#!/usr/bin/env python3
import os, json, shutil, subprocess, jinja2, argparse, time, glob, sys


class PicoUF2Generator:
    def __init__(self, pd_path, project_root, src_dir=None, verbose=False):
        self.script_dir = os.path.dirname(os.path.abspath(__file__))
        self.pd_path = os.path.abspath(pd_path)
        self.project_root = os.path.abspath(project_root)
        self.verbose = verbose
        self.templates = os.path.join(self.script_dir, "templates")
        if src_dir is None:
            self.src_dir = os.path.join(self.script_dir, "src")
        else:
            self.src_dir = os.path.abspath(src_dir)
        self.c_dir = os.path.join(self.project_root, "src")
        self.patch_name = os.path.splitext(os.path.basename(self.pd_path))[0]
        self.hvcc_dir = os.path.join(self.project_root, "hvcc")
        self.build_dir = os.path.join(self.project_root, "build")
        self.ir_json = os.path.join(self.hvcc_dir, f"{self.patch_name}.heavy.ir.json")
        self.manifest_out = os.path.join(self.hvcc_dir, f"{self.patch_name}_manifest.json")
        self.hv_lib_path = os.path.abspath(os.path.join(self.project_root, "../lib", "heavylib"))
        
    def print_logo(self):
        logo = r"""
           _  _           _____  _____  
     _ __ (_)| | __  ___ |  __ \|  __ \ 
    | '_ \| || |/ / / _ \| |__) | |  | |
    | |_) | ||   < | (_) |  ___/| |  | |
    | .__/|_||_|\_\ \___/|_|    |_____/ 
    |_|   [hvcc]  RP2040|RP2350  v0.0.0 
        """
        if not self.verbose:
            print(f"\033[36m{logo}\033[0m")

    def print_progress(self, percent, task, error=False):
        if self.verbose:
            return
        bar_length = 20
        filled = int(round(bar_length * percent))
        color = "\033[91m" if error else "\033[34m"
        reset = "\033[0m"
        dim = "\033[2m"
        bar = "█" * filled + dim + "░" * (bar_length - filled) + reset
        sys.stdout.write(
            f"\r{color}{bar}{reset} {int(percent * 100):>3}% | {task}\033[K"
        )
        sys.stdout.flush()

    def run_cmd(self, cmd, cwd=None, step_name="Command"):
        if self.verbose:
            print(f"\n--- [ {step_name.upper()} ] ---")
            res = subprocess.run(cmd, cwd=cwd)
            if res.returncode != 0:
                sys.exit(1)
            return res
        else:
            res = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)
            if res.returncode != 0:
                self.handle_error(step_name, res)
            return res

    def handle_error(self, step_name, result):
        print(f"\n\n\033[91m❌ ERROR during {step_name}:\033[0m")
        print("-" * 50)
        print(
            result.stderr
            if hasattr(result, "stderr") and result.stderr
            else result.stdout
        )
        print("-" * 50)
        sys.exit(1)

    def check_pico_bootsel(self):
        try:
            res = subprocess.run(["picotool", "info"], capture_output=True, text=True)
            return (
                "No accessible RP-series devices" not in res.stdout
                and res.returncode == 0
            )
        except:
            return False

    def open_serial(self):
        sys.stdout.write("\n[SERIAL] Connecting...")
        sys.stdout.flush()
        subprocess.run(
            "pkill -f 'screen /dev/tty.usbmodem'", shell=True, capture_output=True
        )
        port = None
        for _ in range(60):
            ports = glob.glob("/dev/tty.usbmodem*")
            if ports:
                port = ports[0]
                break
            time.sleep(0.1)
        if port:
            script = f'tell application "Terminal" to do script "screen {port} 115200"'
            subprocess.run(["osascript", "-e", script])
            print(f" Connected to {port}")
        else:
            print(" Error: Device serial port not found.")

    def collect_and_save_manifest(self):
        if not os.path.exists(self.ir_json):
            return {"receives": [], "sends": [], "tables": [], "prints": []}

        with open(self.ir_json, "r") as f:
            data = json.load(f)

        manifest = {
            "patch_name": self.patch_name,
            "receives": [],
            "sends": [],
            "tables": [],
            "prints": [],
        }

        control = data.get("control", {})
        for r_name, r_body in control.get("receivers", {}).items():
            if "__hv_" in r_name: continue
            manifest["receives"].append({
                "name": r_name,
                "hash": r_body.get("hash", "0")
            })

        for obj_id, obj_body in data.get("objects", {}).items():
            t = obj_body.get("type", "")
            args = obj_body.get("args", {})
            
            name = args.get("name") or args.get("label")
            if not name or "__hv_" in name: continue

            obj_hash = args.get("hash", "0")

            entry = {"name": name, "hash": obj_hash}

            if t == "__send":
                if not any(s['name'] == name for s in manifest["sends"]):
                    manifest["sends"].append(entry)
            elif t == "__print":
                if not any(p['name'] == name for p in manifest["prints"]):
                    manifest["prints"].append(entry)
            elif t == "__table":
                manifest["tables"].append(entry)

        with open(self.manifest_out, "w") as f:
            json.dump(manifest, f, indent=2)

        return manifest

    def flatten_hvcc_output(self):
        """Move hvcc outputs into a single directory."""
        for sub in ["c", "ir", "hv"]:
            subdir = os.path.join(self.hvcc_dir, sub)
            if not os.path.exists(subdir):
                continue

            for root, _, files in os.walk(subdir):
                for f in files:
                    src = os.path.join(root, f)
                    dst = os.path.join(self.hvcc_dir, f)

                    # overwrite if newer
                    if not os.path.exists(dst) or (
                        os.path.getmtime(src) > os.path.getmtime(dst)
                    ):
                        shutil.move(src, dst)

            shutil.rmtree(subdir, ignore_errors=True)

    def samples_inflash(self):
        """Finds generated Heavy tables and adds 'static const' to move them to Flash."""
        # Find the main generated C++ file (usually Heavy_patchname.cpp)
        cpp_file = os.path.join(self.hvcc_dir, f"Heavy_{self.patch_name}.cpp")
        
        if not os.path.exists(cpp_file):
            if self.verbose: print(f"Warning: {cpp_file} not found for patching.")
            return

        with open(cpp_file, 'r') as f:
            lines = f.readlines()

        patched = False
        with open(cpp_file, 'w') as f:
            for line in lines:
                # Target the hTable definitions
                if line.startswith("float hTable") and "[" in line:
                    f.write("static const " + line)
                    patched = True
                else:
                    f.write(line)
        
        if patched and self.verbose:
            print(f"  -> Patched {cpp_file}: Tables moved to Flash memory.")

    def run_all(self, flash=False, board_config=None, serial=False, skip_hvcc=False, midi_host=None):
        self.print_logo()
        start_time = time.time()
        print(f"\033[1mBuilding: {self.patch_name}\033[0m")

        if not os.path.exists(self.hv_lib_path):
            print(f"❌ Heavy library path not found: {self.hv_lib_path}")
            sys.exit(1)

        # 1. Heavy Compiler Step
        if not skip_hvcc:
            self.print_progress(0.1, "Heavy Compiler")
            hvcc_cmd = [
                "hvcc", self.pd_path,
                "-o", self.hvcc_dir,
                "-n", self.patch_name,
                "-g", "c",
                "-p", self.hv_lib_path,
            ]
            self.run_cmd(hvcc_cmd, step_name="HVCC")
            self.flatten_hvcc_output()
            self.samples_inflash()
        else:
            print("\033[33m⚠️  Skipping HVCC file regeneration (--skip-hvcc enabled)\033[0m")

        # 2. Load Configuration
        self.print_progress(0.3, "Setup")
        settings = {"pico_board": "pico2", "midi_mode": "usb"} # Default base settings

        config_filename = board_config if board_config else "board.json"
        config_path = os.path.join(self.script_dir, config_filename)
        
        if not os.path.exists(config_path):
            print(f"\033[91m❌ Configuration file not found: {config_filename}\033[0m")
            sys.exit(1)

        with open(config_path) as f:
            settings.update(json.load(f))
            
        print(f"\033[32m  -> Using config: {config_filename}\033[0m")
        midi_mode = midi_host if midi_host else settings.get("midi_mode")

        # 3. Source File Sync (Conditional Folders)
        os.makedirs(self.c_dir, exist_ok=True)
        web_enabled = settings.get("web", {}).get("enabled", False)
        conditional_folders = {"web": "web", "screen": "screen", "usb": not web_enabled}

        for root, dirs, files in os.walk(self.src_dir):
            rel_dir = os.path.relpath(root, self.src_dir)
            should_copy = True
            
            for folder_prefix, setting_key in conditional_folders.items():
                if rel_dir.startswith(folder_prefix):
                    val = settings.get(setting_key)
                    # Check if it's a dict with "enabled": true or just a boolean
                    is_enabled = val.get("enabled") if isinstance(val, dict) else val
                    if not is_enabled:
                        should_copy = False
                        break
            
            if not should_copy: continue

            for f in files:
                src_file = os.path.join(root, f)
                rel_path = os.path.relpath(src_file, self.src_dir)
                dest_file = os.path.join(self.project_root, f) if f == "CMakeLists.txt" else os.path.join(self.c_dir, rel_path)
                
                os.makedirs(os.path.dirname(dest_file), exist_ok=True)
                if not os.path.exists(dest_file) or open(src_file, "rb").read() != open(dest_file, "rb").read():
                    shutil.copy2(src_file, dest_file)

        # 4. Generate main.cpp Template
        self.print_progress(0.5, "Updating C++ & Manifest")
        manifest = self.collect_and_save_manifest()
        env = jinja2.Environment(loader=jinja2.FileSystemLoader(self.templates))
        new_main = env.get_template("main.cpp").render(name=self.patch_name, hv_manifest=manifest, board=settings)
        
        with open(os.path.join(self.c_dir, "main.cpp"), "w") as f:
            f.write(new_main)

        # 5. CMake Configuration
        sdk = os.environ.get("PICO_SDK_PATH")
        board = settings.get("pico_board", "pico")
        sdk_target = "pico_w" if board == "pico_w" else ("pico" if board == "zero" else board)

        if board == "pico2":
            toolchain_file = os.path.join(sdk, "cmake/preload/toolchains/pico_arm_cortex_m33_gcc.cmake")
        else:
            toolchain_file = os.path.join(sdk, "cmake/preload/toolchains/pico_arm_cortex_m0plus_gcc.cmake")

        os.makedirs(self.build_dir, exist_ok=True)
        self.print_progress(0.7, "Configuring CMake")

        cmake_cmd = [
            "cmake", "-G", "Unix Makefiles",
            f"-DPICO_SDK_PATH={sdk}",
            f"-DPICO_BOARD={sdk_target}",
            f"-DCMAKE_TOOLCHAIN_FILE={toolchain_file}",
            self.project_root,
        ]

        # Web & MIDI Logic
        web_cfg = settings.get("web", {})
        if web_cfg.get("enabled"):
            mode = web_cfg.get("active_mode", 0)
            creds = web_cfg.get("ap" if mode == 0 else "sta", {})
            osc = web_cfg.get("osc", {})
            
            cmake_cmd.extend([
                "-DWEB=1",
                f"-DACTIVE_MODE={mode}",
                f'-DWIFI_SSID="{creds.get("ssid", "")}"',
                f'-DWIFI_PASSWORD="{creds.get("password", "")}"',
                f'-DMDNS_NAME="{web_cfg.get("mdns_name", "pikopd")}"',
                f"-DOSC_ENABLED={1 if osc.get('enabled', True) else 0}",
                f'-DOSC_PORT={web_cfg.get("osc_port", 8000)}',
                "-DMIDI_HOST_ENABLED=0",  
                "-DMIDI_UART_ENABLED=1"   
            ])
            print("\033[32m  -> Web Enabled: MIDI Host disabled, UART MIDI enabled\033[0m")
        else:
            cmake_cmd.append("-DWEB=0")
            if midi_mode == "host":
                cmake_cmd.extend(["-DMIDI_HOST=1", "-DMIDI_UART_ENABLED=0"])
                print("\033[32m  -> MIDI Host Mode enabled\033[0m")
            elif midi_mode == "uart":
                cmake_cmd.extend(["-DMIDI_HOST=0", "-DMIDI_UART_ENABLED=1"])
                print("\033[32m  -> MIDI UART Mode enabled\033[0m")
            else:
                cmake_cmd.extend(["-DMIDI_HOST=0", "-DMIDI_UART_ENABLED=0"])
                print("\033[32m  -> USB MIDI Device Mode enabled\033[0m")

        # Console & Extra Flags
        cmake_cmd.append(f"-DENABLE_DEBUG={'1' if settings.get('console') else '0'}")
        if board == "zero": cmake_cmd.append("-DPICO_ZERO_BOARD=1")
        cmake_cmd.append(f"-DMAX_VOICES={settings.get('voice_count', 1)}")

        self.run_cmd(cmake_cmd, cwd=self.build_dir, step_name="CMake")

        # 6. Compilation & Flash
        self.print_progress(0.85, "Compiling")
        self.run_cmd(["make", "-j10"], cwd=self.build_dir, step_name="Make")

        flash_success = False
        if flash:
            if self.check_pico_bootsel():
                self.print_progress(0.95, "Flashing")
                uf2 = glob.glob(os.path.join(self.build_dir, "*.uf2"))[0]
                self.run_cmd(["picotool", "load", "-f", "-x", uf2], step_name="Flash")
                flash_success = True
            else:
                print("\033[91m❌ STOP: Pico not in BOOTSEL mode.\033[0m")
                sys.exit(1)

        duration = time.time() - start_time
        self.print_progress(1.0, f"Finished in {duration:.1f}s")
        sys.stdout.write("\n")
        if serial and flash_success: self.open_serial()

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Upload Heavy Pd patch to Pico")
    parser.add_argument("-b", "--board", help="Path to custom board configuration json file") 
    parser.add_argument("pd_patch", help="Pure Data patch file (e.g., heavy.pd)")
    parser.add_argument("project_root", help="Project folder")
    parser.add_argument("-f", "--flash", action="store_true", help="Flash UF2 to Pico")
    parser.add_argument(
        "-s", "--serial", action="store_true", help="Open serial console after reboot"
    )
    parser.add_argument(
        "-v", "--verbose", action="store_true", help="Enable debug output"
    )
    parser.add_argument(
    "-x", "--skip-hvcc",
    action="store_true",
    help="Skip running HVCC (useful for manual edits of C/C++ files)"
    )
    args = parser.parse_args()

    cmake_path = os.path.join(args.project_root, "CMakeLists.txt")
    if args.skip_hvcc and not os.path.exists(cmake_path):
        print("\033[33m⚠️  --skip-hvcc (-x) ignored: CMakeLists.txt not found in project root\033[0m")
        args.skip_hvcc = False

    src = os.path.join(os.path.dirname(os.path.abspath(__file__)), "src")
    gen = PicoUF2Generator(args.pd_patch, args.project_root, src, verbose=args.verbose)
    gen.run_all(skip_hvcc=args.skip_hvcc, flash=args.flash, serial=args.serial, board_config=args.board)
