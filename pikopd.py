#!/usr/bin/env python3
import os, json, shutil, subprocess, jinja2, argparse, time, glob, sys


class PicoUF2Generator:
    def __init__(self, pd_path, project_root, src_dir=None, verbose=False):
        self.pd_path = os.path.abspath(pd_path)
        self.project_root = os.path.abspath(project_root)
        self.verbose = verbose

        script_dir = os.path.dirname(os.path.abspath(__file__))

        self.templates = os.path.join(script_dir, "templates")

        if src_dir is None:
            self.src_dir = os.path.join(script_dir, "src")
        else:
            self.src_dir = os.path.abspath(src_dir)

        self.c_dir = os.path.join(self.project_root, "src")

        self.patch_name = os.path.splitext(os.path.basename(self.pd_path))[0]

        self.hvcc_dir = os.path.join(self.project_root, "hvcc")
        self.build_dir = os.path.join(self.project_root, "build")

        self.ir_json = os.path.join(self.hvcc_dir, f"{self.patch_name}.heavy.ir.json")
        self.manifest_out = os.path.join(self.hvcc_dir, f"{self.patch_name}_manifest.json")

        self.hv_lib_path = os.path.abspath(
            os.path.join(self.project_root, "../lib", "heavylib")
        )
        
    def print_logo(self):
        logo = r"""
           _  _           _____  _____  
     _ __ (_)| | __  ___ |  __ \|  __ \ 
    | '_ \| || |/ / / _ \| |__) | |  | |
    | |_) | ||   < | (_) |  ___/| |  | |
    | .__/|_||_|\_\ \___/|_|    |_____/ 
    |_|   [hvcc]  RP2040|RP2350  v0.0.1 
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

    def run_all(self, flash=False, serial=False, skip_hvcc=False):
        self.print_logo()
        start_time = time.time()
        print(f"\033[1mBuilding: {self.patch_name}\033[0m")

        if not os.path.exists(self.hv_lib_path):
            print(f"❌ Heavy library path not found: {self.hv_lib_path}")
            sys.exit(1)

        if not skip_hvcc:
            self.print_progress(0.1, "Heavy Compiler")
            hvcc_cmd = [
            "hvcc",
            self.pd_path,
            "-o",
            self.hvcc_dir,
            "-n",
            self.patch_name,
            "-g",
            "c",
            "-p",
            self.hv_lib_path,
            ]
            self.run_cmd(hvcc_cmd, step_name="HVCC")
            self.flatten_hvcc_output()
        else:
            print("\033[33m⚠️  Skipping HVCC file regeneration (--skip-hvcc enabled)\033[0m")

        self.print_progress(0.3, "Syncing Source")
        settings = {"pico_board": "pico2"}
        if os.path.exists("board.json"):
            with open("board.json") as f:
                settings.update(json.load(f))

        # ---- Copy sources ----
        os.makedirs(self.c_dir, exist_ok=True)
        for f in os.listdir(self.src_dir):
            s = os.path.join(self.src_dir, f)
            if f == "CMakeLists.txt":
                d = os.path.join(self.project_root, f)  # copy CMakeLists.txt to project root
            else:
                d = os.path.join(self.c_dir, f)  # all other files go to src/

            if os.path.isfile(s) and (
                not os.path.exists(d) or open(s, "rb").read() != open(d, "rb").read()
            ):
                shutil.copy2(s, d)

        os.makedirs(self.build_dir, exist_ok=True)

        self.print_progress(0.5, "Updating C++ & Manifest")
        manifest = self.collect_and_save_manifest()

        env = jinja2.Environment(loader=jinja2.FileSystemLoader(self.templates))

        new_main = env.get_template("main.cpp").render(
        name=self.patch_name,
        hv_manifest=manifest,
        board=settings
        )
        m_path = os.path.join(self.c_dir, "main.cpp") 
        with open(m_path, "w") as f:
            f.write(new_main)

        sdk = os.environ.get("PICO_SDK_PATH")
        board = settings.get("pico_board", "pico")
        sdk_target = "pico" if board == "zero" else board

        if board == "pico2":
            toolchain_file = os.path.join(sdk, "cmake/preload/toolchains/pico_arm_cortex_m33_gcc.cmake")
        else:
            toolchain_file = os.path.join(sdk, "cmake/preload/toolchains/pico_arm_cortex_m0plus_gcc.cmake")
        if board == "pico_w":
            sdk_target = "pico_w"
        elif board == "zero":
            sdk_target = "pico"
            extra_args = ["-DPICO_ZERO_BOARD=1"]
        else:
            sdk_target = board

        os.makedirs(self.build_dir, exist_ok=True)
        if not os.path.exists(os.path.join(self.build_dir, "Makefile")):
            self.print_progress(0.7, "Configuring CMake")
            cmake_cmd = [
                "cmake",
                "-G", "Unix Makefiles",
                f"-DPICO_SDK_PATH={sdk}",
                f"-DPICO_BOARD={sdk_target}",
                f"-DCMAKE_TOOLCHAIN_FILE={toolchain_file}",
                self.project_root,
            ]

            if board == "zero":
                cmake_cmd.append("-DPICO_ZERO_BOARD=1")

            self.run_cmd(
                cmake_cmd,
                cwd=self.build_dir,
                step_name="CMake",
            )

        self.print_progress(0.85, "Compiling")
        self.run_cmd(["make", "-j10"], cwd=self.build_dir, step_name="Make")

        flash_success = False
        if flash:
            if self.check_pico_bootsel():
                self.print_progress(0.95, "Flashing")
                uf2 = glob.glob(os.path.join(self.build_dir, "*.uf2"))[0]
                self.run_cmd(["picotool", "load", "-f", "-x", uf2], step_name="Flash")
                duration = time.time() - start_time
                self.print_progress(1.0, f"Finished in {duration:.1f}s | Rebooting...")
                time.sleep(1.5)
                sys.stdout.write("\n")
                flash_success = True
            else:
                sys.stdout.write("\n")
                print("\033[91m❌ STOP: Pico not in BOOTSEL mode.\033[0m")
                sys.exit(1)
        else:
            duration = time.time() - start_time
            self.print_progress(1.0, f"Finished in {duration:.1f}s")
            sys.stdout.write("\n")

        if serial and flash_success:
            self.open_serial()

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Upload Heavy Pd patch to Pico")
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
    gen.run_all(skip_hvcc=args.skip_hvcc, flash=args.flash, serial=args.serial)
