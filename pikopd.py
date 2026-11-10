#!/usr/bin/env python3

import os
import re
import json
import shutil
import subprocess
import jinja2
import argparse
import time
import glob
import sys
import serial


class PicoUF2Generator:

    def __init__(self, pd_path, project_root, src_dir=None, verbose=False):
        self.script_dir = os.path.dirname(os.path.abspath(__file__))
        self.pd_path = os.path.abspath(pd_path)
        self.project_root = os.path.abspath(project_root)
        self.verbose = verbose
        self.templates = os.path.join(self.script_dir, "templates")
        self.src_dir = os.path.abspath(src_dir) if src_dir else os.path.join(self.script_dir, "src")

        self.c_dir = self.project_root
        self.patch_name = os.path.splitext(os.path.basename(self.pd_path))[0]
        self.hvcc_dir = os.path.join(self.project_root, "hvcc")
        self.build_dir = os.path.join(self.project_root, "build")
        self.ir_json = os.path.join(self.hvcc_dir, f"{self.patch_name}.heavy.ir.json")
        self.manifest_out = os.path.join(self.hvcc_dir, f"{self.patch_name}_manifest.json")
        self.hv_lib_path = os.path.abspath(os.path.join(self.project_root, "../lib", "heavylib"))

        # Overall progress
        self.progress = 0
        self.progress_stage = "Starting"
        self.progress_active = False
        self.progress_width = 30

    # ---------------------------------------------------------
    # TERMINAL OUTPUT
    # ---------------------------------------------------------

    def print_logo(self):
        if self.verbose:
            return

        logo = r"""
           _  _           _____  _____
     _ __ (_)| | __  ___ |  __ \|  __ \
    | '_ \| || |/ / / _ \| |__) | |  | |
    | |_) | ||   < | (_) |  ___/| |  | |
    | .__/|_||_|\_\ \___/|_|    |_____/
    |_|   [hvcc]  RP2040|RP2350  v0.0.6
        """
        print(f"\033[36m{logo}\033[0m")

    def progress_status(self, message=None, percent=None, stage=None, color=None):
        """
        Unified terminal output.

        Normal mode:
            progress_status("Compiling...")
            progress_status(percent=52, stage="Compiling firmware")
            progress_status(percent=100, stage="Complete")
            progress_status()  # Stop progress without completing.

        Verbose mode:
            All custom output is suppressed.
        """

        if self.verbose:
            return

        # Update progress.
        if percent is not None:
            self.progress = max(self.progress, min(100, percent))
            self.progress_active = self.progress < 100

        if stage:
            self.progress_stage = stage

        # Print a status message.
        if message is not None:
            if self.progress_active:
                sys.stdout.write("\r\033[2K")

            if color:
                print(f"\033[{color}m  -> {message}\033[0m")
            else:
                print(f"  -> {message}")

        # Draw progress bar.
        if percent is not None:
            filled = int(self.progress_width * self.progress / 100)
            bar = "█" * filled + "░" * (self.progress_width - filled)

            sys.stdout.write(
                f"\r\033[2K"
                f"  -> [{bar}] {self.progress:3d}%  {self.progress_stage}"
            )
            sys.stdout.flush()

            if percent >= 100:
                self.progress_active = False
                sys.stdout.write("\n")
                sys.stdout.flush()

        # Stop progress without completing.
        elif message is None:
            if self.progress_active:
                sys.stdout.write("\n")
                sys.stdout.flush()

            self.progress_active = False

        # Redraw progress after a status message.
        elif self.progress_active:
            filled = int(self.progress_width * self.progress / 100)
            bar = "█" * filled + "░" * (self.progress_width - filled)

            sys.stdout.write(
                f"\r\033[2K"
                f"  -> [{bar}] {self.progress:3d}%  {self.progress_stage}"
            )
            sys.stdout.flush()

    # ---------------------------------------------------------
    # COMMAND EXECUTION
    # ---------------------------------------------------------

    def run_cmd(self, cmd, cwd=None, step_name="Command"):

        if self.verbose:
            # Live external command output only.
            result = subprocess.run(cmd, cwd=cwd)

            if result.returncode:
                sys.exit(result.returncode)

            return result

        result = subprocess.run(
            cmd,
            cwd=cwd,
            capture_output=True,
            text=True
        )

        if result.returncode:
            self.handle_error(step_name, result)

        return result

    def handle_error(self, step_name, result):
        self.progress_status()

        self.progress_status(f"ERROR during {step_name}:", color="91")

        print("-" * 50)
        print(result.stderr if result.stderr else result.stdout)
        print("-" * 50)

        sys.exit(1)

    def copy_if_changed(self, src, dst):
        """Copy a file only when its contents have changed."""
        if not os.path.exists(dst):
            shutil.copy2(src, dst)
            return True

        with open(src, "rb") as source, open(dst, "rb") as destination:
            if source.read() == destination.read():
                return False

        shutil.copy2(src, dst)
        return True

    # ---------------------------------------------------------
    # BOOTSEL
    # ---------------------------------------------------------

    def check_pico_bootsel(self):
        result = subprocess.run(
            ["picotool", "info"],
            capture_output=True,
            text=True
        )

        return result.returncode == 0

    def validate_flash_size(self, set_mb):

        result = subprocess.run(
            ["picotool", "info", "-a"],
            capture_output=True,
            text=True
        )

        if result.returncode != 0:
            raise RuntimeError(
                f"Could not read flash information:\n{result.stderr}"
            )

        output = result.stdout.replace("\u00a0", " ")

        matches = re.findall(
            r"flash\s+size\s*:\s*(\d+)\s*([KMG])?",
            output,
            re.IGNORECASE
        )

        if len(matches) != 1:
            raise RuntimeError(
                f"Expected exactly one flash-size result, found {len(matches)}."
            )

        value, unit = matches[0]

        multiplier = {
            "": 1,
            "K": 1024,
            "M": 1024 * 1024,
            "G": 1024 * 1024 * 1024
        }[unit.upper()]

        detected_bytes = int(value) * multiplier
        detected_mb = detected_bytes / (1024 * 1024)
        expected_bytes = set_mb * 1024 * 1024

        if detected_bytes < expected_bytes:
            raise RuntimeError(
                f"FLASH TOO SMALL! Required: {set_mb} MB, "
                f"detected: {detected_mb:g} MB. Flashing aborted."
            )

        self.progress_status(
            f"Device flash detected: {detected_mb:g} MB"
        )

        return True

    def enter_bootsel(self):

        # Method 1: serial 1200-baud touch
        if self.enter_bootsel_1200():
            return True

       # self.progress_status("1200-baud touch unavailable.", color="33")
        self.progress_status("Trying picotool reboot...")

        # Method 2: picotool 
        subprocess.run(
            ["picotool", "reboot", "-u", "-f"],
            capture_output=True,
            text=True
        )

        for _ in range(30):
            if self.check_pico_bootsel():
                self.progress_status("Entered BOOTSEL mode using picotool")
                return True

            time.sleep(0.2)

        # Method 3: manual BOOTSEL
        self.progress_status()
        self.progress_status(
            "Automatic BOOTSEL entry failed.",
            color="91"
        )
        self.progress_status(
            "Please hold BOOTSEL, reconnect the Pico, and release the button."
        )
        self.progress_status("Waiting for manual BOOTSEL connection...")

        for _ in range(20):
            if self.check_pico_bootsel():
                self.progress_status("Pico detected in BOOTSEL mode")
                return True

            time.sleep(0.5)

        self.progress_status()
        self.progress_status("Manual BOOTSEL timed out.", color="91")

        return False

    def enter_bootsel_1200(self):
        ports = glob.glob("/dev/cu.usbmodem*")

        if not ports:
        #    self.progress_status("CDC port not found.", color="33")
            return False

        port = ports[0]
        self.progress_status(f"Sending 1200-baud touch to RPi Pico: {port}")

        try:
            with serial.Serial(port, 1200, timeout=1):
                time.sleep(0.1)

        except (serial.SerialException, OSError):
            pass

        # Check for BOOTSEL even if the serial port disconnected.
        for _ in range(15):
            if self.check_pico_bootsel():
                self.progress_status("Entered BOOTSEL mode using 1200-baud")
                return True

            time.sleep(0.2)

        return False

    # ---------------------------------------------------------
    # SERIAL MONITOR
    # ---------------------------------------------------------

    def open_serial(self):
        if not self.verbose:
            sys.stdout.write("\n[SERIAL] Connecting...")
            sys.stdout.flush()

        subprocess.run(
            "pkill -f 'screen /dev/tty.usbmodem'",
            shell=True,
            capture_output=True
        )

        port = None

        for _ in range(60):
            ports = glob.glob("/dev/tty.usbmodem*")

            if ports:
                port = ports[0]
                break

            time.sleep(0.1)

        if port:
            script = (
                f'tell application "Terminal" to '
                f'do script "screen {port} 115200"'
            )
            subprocess.run(["osascript", "-e", script])
            self.progress_status(f"Connected to {port}", color="32")
        else:
            self.progress_status("Device serial port not found.", color="91")

    # ---------------------------------------------------------
    # HVCC MANIFEST
    # ---------------------------------------------------------

    def collect_and_save_manifest(self):
        empty_manifest = {
            "receives": [],
            "sends": [],
            "tables": [],
            "prints": []
        }

        if not os.path.exists(self.ir_json):
            return empty_manifest

        with open(self.ir_json, "r") as file:
            data = json.load(file)

        manifest = {"patch_name": self.patch_name, **empty_manifest}
        control = data.get("control", {})

        for name, body in control.get("receivers", {}).items():
            if "__hv_" not in name:
                manifest["receives"].append({
                    "name": name,
                    "hash": body.get("hash", "0")
                })

        for obj_body in data.get("objects", {}).values():
            obj_type = obj_body.get("type", "")
            args = obj_body.get("args", {})
            name = args.get("name") or args.get("label")

            if not name or "__hv_" in name:
                continue

            entry = {
                "name": name,
                "hash": args.get("hash", "0")
            }

            if obj_type == "__send":
                target = manifest["sends"]
            elif obj_type == "__print":
                target = manifest["prints"]
            elif obj_type == "__table":
                manifest["tables"].append(entry)
                continue
            else:
                continue

            if not any(item["name"] == name for item in target):
                target.append(entry)

        with open(self.manifest_out, "w") as file:
            json.dump(manifest, file, indent=2)

        return manifest

    # ---------------------------------------------------------
    # HVCC OUTPUT
    # ---------------------------------------------------------

    def flatten_hvcc_output(self):
        """Move HVCC outputs from subdirectories into one directory."""
        for sub in ("c", "ir", "hv"):
            subdir = os.path.join(self.hvcc_dir, sub)

            if not os.path.exists(subdir):
                continue

            for root, _, files in os.walk(subdir):
                for filename in files:
                    src = os.path.join(root, filename)
                    dst = os.path.join(self.hvcc_dir, filename)

                    if (
                        not os.path.exists(dst)
                        or os.path.getmtime(src) > os.path.getmtime(dst)
                    ):
                        shutil.move(src, dst)

            shutil.rmtree(subdir, ignore_errors=True)

    def samples_inflash(self):
        """Add static const to generated Heavy tables so they reside in Flash."""
        cpp_file = os.path.join(
            self.hvcc_dir,
            f"Heavy_{self.patch_name}.cpp"
        )

        if not os.path.exists(cpp_file):
            if self.verbose:
                print(
                    f"Warning: {cpp_file} not found for patching.",
                    file=sys.stderr
                )
            return

        with open(cpp_file, "r") as file:
            lines = file.readlines()

        patched = False

        with open(cpp_file, "w") as file:
            for line in lines:
                if line.startswith("float hTable") and "[" in line:
                    file.write("static const " + line)
                    patched = True
                else:
                    file.write(line)

        if patched and self.verbose:
            print(
                f"Patched {cpp_file}: Tables moved to Flash memory.",
                file=sys.stderr
            )

    # ---------------------------------------------------------
    # MAIN BUILD PROCESS
    # ---------------------------------------------------------

    def run_all(
        self,
        flash=False,
        board_config=None,
        serial=False,
        skip_hvcc=False,
        midi_host=None
    ):

        self.print_logo()
        start_time = time.time()

        if not self.verbose:
            print(f"\033[1mBuilding project: {self.patch_name}\033[0m")

        self.progress_status(percent=0, stage="Starting")

        if not os.path.exists(self.hv_lib_path):
            self.progress_status(
                f"Heavy library path not found: {self.hv_lib_path}",
                color="91"
            )
            self.progress_status()
            sys.exit(1)

        # 1. HEAVY COMPILER
        if not skip_hvcc:
            hvcc_cmd = [
                "hvcc", self.pd_path,
                "-o", self.hvcc_dir,
                "-n", self.patch_name,
                "-g", "c",
                "-p", self.hv_lib_path
            ]

            self.run_cmd(hvcc_cmd, step_name="HVCC")
            self.flatten_hvcc_output()
            self.samples_inflash()

        else:
            self.progress_status(
                "Skipping HVCC file regeneration (--skip-hvcc enabled)",
                color="33"
            )

        self.progress_status(percent=12, stage="Loading configuration")

        # 2. LOAD CONFIGURATION
        settings = {
            "pico_board": "pico2",
            "midi_mode": "usb"
        }

        config_filename = board_config or "board.json"
        config_path = os.path.join(self.script_dir, config_filename)

        if not os.path.exists(config_path):
            self.progress_status(
                f"Configuration file not found: {config_filename}",
                color="91"
            )
            self.progress_status()
            sys.exit(1)

        with open(config_path) as file:
            settings.update(json.load(file))

        self.progress_status(f"Using config: {config_filename}", color="32")
        self.progress_status(percent=18, stage="Preparing configuration")

        # 3. CONFIGURATION FLAGS
        masterfx = settings.get("masterfx") or {}
        inputs = settings.get("inputs") or {}
        sensors = inputs.get("sensors") or {}
        web_config = settings.get("web") or {}
        display_config = settings.get("display") or {}

        midi_mode = midi_host if midi_host else settings.get("midi_mode")
        web_enabled = web_config.get("enabled", False)
        display_enabled = display_config.get("enabled", False)

        distance_enabled = (
            isinstance(sensors.get("hc-sr04"), list)
            and bool(sensors.get("hc-sr04"))
        )
        cny_enabled = (
            isinstance(sensors.get("cny70"), list)
            and bool(sensors.get("cny70"))
        )
        mpr121_enabled = (
            isinstance(sensors.get("mpr121"), list)
            and bool(sensors.get("mpr121"))
        )
        air_enabled = (
            isinstance(sensors.get("hx710"), list)
            and bool(sensors.get("hx710"))
        )
        loadcell_enabled = (
            isinstance(sensors.get("hx711"), list)
            and bool(sensors.get("hx711"))
        )

        # 4. SYNC SOURCE FILES
        os.makedirs(self.c_dir, exist_ok=True)

        if os.path.exists(self.src_dir):
            for root, _, files in os.walk(self.src_dir):
                for filename in files:

                    if filename == "CMakeLists.txt" and root == self.src_dir:
                        continue

                    if not web_enabled and "web" in root:
                        continue

                    if web_enabled and "usb" in root:
                        continue

                    if not display_enabled and "screen" in root:
                        continue

                    if not air_enabled and "hx710" in filename:
                        continue

                    if not loadcell_enabled and "hx711" in filename:
                        continue

                    if not mpr121_enabled and "mpr121" in filename:
                        continue

                    if not cny_enabled and "cny70" in filename:
                        continue

                    if not distance_enabled and (
                        "hc-sr04" in filename or filename.endswith(".pio")
                    ):
                        continue

                    if (
                        not masterfx.get("delay", False)
                        and "delay" in filename.lower()
                    ):
                        continue

                    if not masterfx.get("reverb", False) and (
                        "reverb" in filename.lower()
                        or "freeverb" in filename.lower()
                    ):
                        continue

                    self.copy_if_changed(
                        os.path.join(root, filename),
                        os.path.join(self.c_dir, filename)
                    )

        # Copy CMakeLists.txt separately.
        cmake_src = os.path.join(self.src_dir, "CMakeLists.txt")
        cmake_dest = os.path.join(self.project_root, "CMakeLists.txt")

        if os.path.exists(cmake_src):
            self.copy_if_changed(cmake_src, cmake_dest)

        self.progress_status(percent=35, stage="Generating firmware")

        # 5. GENERATE MAIN CPP AND MANIFEST
        manifest = self.collect_and_save_manifest()

        env = jinja2.Environment(
            loader=jinja2.FileSystemLoader(self.templates)
        )

        new_main = env.get_template("main.cpp").render(
            name=self.patch_name,
            hv_manifest=manifest,
            board=settings
        )

        with open(os.path.join(self.c_dir, "main.cpp"), "w") as file:
            file.write(new_main)

        self.progress_status(percent=42, stage="Configuring CMake")

        # 6. CMAKE CONFIGURATION
        sdk = os.environ.get("PICO_SDK_PATH")

        if not sdk:
            self.progress_status(
                "PICO_SDK_PATH environment variable is not set.",
                color="91"
            )
            self.progress_status()
            sys.exit(1)

        board = settings.get("pico_board", "pico")
        sdk_target = (
            "pico_w" if board == "pico_w"
            else ("pico" if board == "zero" else board)
        )

        if board in ("pico2", "pico2_w"):
            toolchain = "pico_arm_cortex_m33_gcc.cmake"
        else:
            toolchain = "pico_arm_cortex_m0plus_gcc.cmake"

        toolchain_file = os.path.join(
            sdk,
            "cmake/preload/toolchains",
            toolchain
        )

        os.makedirs(self.build_dir, exist_ok=True)

        flash_size = settings.get("flash_size", 2)

        if type(flash_size) is not int or flash_size not in (2, 4, 8, 16, 32):
            raise ValueError("flash_size must be 2, 4, 8, 16 or 32 MB")

        cmake_cmd = [
            "cmake", "-G", "Ninja",
            f"-DPICO_SDK_PATH={sdk}",
            f"-DPICO_BOARD={sdk_target}",
            f"-DCMAKE_TOOLCHAIN_FILE={toolchain_file}",
            self.project_root
        ]

        cmake_cmd.append(
            f"-DPICO_FLASH_SIZE_BYTES={flash_size * 1024 * 1024}"
        )

        # Web & MIDI logic.
        if web_enabled:
            mode = web_config.get("active_mode", 0)
            creds = web_config.get("ap" if mode == 0 else "sta", {})
            osc_enabled = web_config.get("osc_enabled", False)

            cmake_cmd.extend([
                "-DWEB=1",
                f"-DACTIVE_MODE={mode}",
                f"-DWIFI_SSID={creds.get('ssid', '')}",
                f"-DWIFI_PASSWORD={creds.get('password', '')}",
                f"-DMDNS_NAME={web_config.get('mdns_name', 'pikopd')}",
                f"-DOSC_ENABLED={1 if osc_enabled else 0}",
                f"-DOSC_PORT={web_config.get('osc_port', 8000)}",
                "-DMIDI_HOST_ENABLED=0",
                "-DMIDI_UART_ENABLED=1"
            ])

            self.progress_status(
                "Web Enabled: MIDI Host disabled, UART MIDI enabled",
                color="32"
            )

        else:
            cmake_cmd.append("-DWEB=0")

            if midi_mode == "host":
                cmake_cmd.extend([
                    "-DMIDI_HOST_ENABLED=1",
                    "-DMIDI_UART_ENABLED=0"
                ])
                self.progress_status("MIDI Host Mode enabled", color="32")

            elif midi_mode == "uart":
                cmake_cmd.extend([
                    "-DMIDI_HOST_ENABLED=0",
                    "-DMIDI_UART_ENABLED=1"
                ])
                self.progress_status("MIDI UART Mode enabled", color="32")

            else:
                cmake_cmd.extend([
                    "-DMIDI_HOST_ENABLED=0",
                    "-DMIDI_UART_ENABLED=0"
                ])
                self.progress_status("USB MIDI Device Mode enabled", color="32")

        # Console and extra flags.
        cmake_cmd.extend([
            f"-DENABLE_DEBUG={1 if settings.get('console') else 0}",
            f"-DENABLE_DISPLAY={1 if display_enabled else 0}",
            f"-DENABLE_DISTANCE_SENSOR={1 if distance_enabled else 0}",
            f"-DCNY70_ENABLED={1 if cny_enabled else 0}",
            f"-DMPR121_ENABLED={1 if mpr121_enabled else 0}",
            f"-DHX710_ENABLED={1 if air_enabled else 0}",
            f"-DHX711_ENABLED={1 if loadcell_enabled else 0}",
            f"-DUSE_DELAY={1 if masterfx.get('delay', False) else 0}",
            f"-DUSE_REVERB={1 if masterfx.get('reverb', False) else 0}",
            f"-DUSE_LIMITER={1 if masterfx.get('limiter', False) else 0}",
            f"-DMAX_VOICES={settings.get('voice_count', 1)}"
        ])

        if board == "zero":
            cmake_cmd.append("-DPICO_ZERO_BOARD=1")

        self.run_cmd(cmake_cmd, cwd=self.build_dir, step_name="CMake")

        self.progress_status(percent=52, stage="Compiling firmware")

        # 7. COMPILATION
        self.progress_status("Compiling...")

        self.run_cmd(
            ["ninja", "-j10"],
            cwd=self.build_dir,
            step_name="Compilation"
        )

       # self.progress_status(percent=85, stage="Preparing firmware upload")

        # 8. FLASHING
        flash_success = False

        if flash:
            uf2_files = glob.glob(os.path.join(self.build_dir, "*.uf2"))

            if not uf2_files:
                self.progress_status("ERROR: No UF2 firmware found.", color="91")
                self.progress_status()
                sys.exit(1)

            uf2 = uf2_files[0]

            try:
                # Enter BOOTSEL using automatic detection/fallback.
                if not self.enter_bootsel():
                    raise RuntimeError("Could not enter BOOTSEL mode")

                # Verify physical flash capacity before writing firmware.
                self.validate_flash_size(flash_size)

                self.progress_status(percent=93, stage="Flashing firmware")

                # Flash and request execution.
                self.run_cmd(
                    ["picotool", "load", "-f", "-x", uf2],
                    step_name="Flash"
                )

                flash_success = True

                self.progress_status("Firmware flashed successfully")
                self.progress_status("Rebooting...")
                self.progress_status(percent=99, stage="Finishing")

            except (Exception, SystemExit) as error:
                self.progress_status()

                self.progress_status(f"Flash failed: {error}", color="91")

                if not self.check_pico_bootsel():
                    self.progress_status(
                        "Pico is not detected by picotool.",
                        color="33"
                    )
                else:
                    self.progress_status(
                        "Pico is accessible, but flashing failed.",
                        color="33"
                    )

                sys.exit(1)

        # 9. FINISH
        duration = time.time() - start_time

        self.progress_status(percent=100, stage="Complete")

        if not self.verbose:
            print(f"\n\033[32m✔ Finished in {duration:.1f}s\033[0m")

        if serial and flash_success:
            self.open_serial()


# =============================================================
# CLI
# =============================================================

if __name__ == "__main__":

    parser = argparse.ArgumentParser(description="Upload Heavy Pd patch to Pico")

    parser.add_argument("-b", "--board", help="Path to custom board configuration json file")
    parser.add_argument("pd_patch", help="Pure Data patch file (e.g., heavy.pd)")
    parser.add_argument("project_root", help="Project folder")
    parser.add_argument("-f", "--flash", action="store_true", help="Flash UF2 to Pico")
    parser.add_argument("-v", "--verbose", action="store_true", help="Show external command output")
    parser.add_argument("-c", "--clean", action="store_true", help="Clean the project folder and recompile")
    parser.add_argument("-x", "--skip-hvcc", action="store_true", help="Skip running HVCC (useful for manual edits of C/C++ files)")
    parser.add_argument("-s", "--serial", action="store_true", help="Open serial console after reboot")

    args = parser.parse_args()

    # CLEAN PROJECT BEFORE BUILD
    if args.clean:
        project_root = os.path.abspath(args.project_root)
        script_dir = os.path.dirname(os.path.abspath(__file__))

        if project_root == script_dir or project_root == os.path.dirname(script_dir):
            parser.error("Refusing to clean the generator directory or its parent")

        os.makedirs(project_root, exist_ok=True)

        for item in os.listdir(project_root):
            path = os.path.join(project_root, item)

            if os.path.isdir(path):
                shutil.rmtree(path)
            else:
                os.remove(path)

        print(f"✔ Project folder cleaned: {project_root}")

    cmake_path = os.path.join(args.project_root, "CMakeLists.txt")

    if args.skip_hvcc and not os.path.exists(cmake_path):
        print("\033[33m⚠️  --skip-hvcc (-x) ignored: CMakeLists.txt not found in project root\033[0m")
        args.skip_hvcc = False

    src = os.path.join(os.path.dirname(os.path.abspath(__file__)), "src")

    generator = PicoUF2Generator(
        args.pd_patch,
        args.project_root,
        src,
        verbose=args.verbose
    )

    generator.run_all(
        skip_hvcc=args.skip_hvcc,
        flash=args.flash,
        serial=args.serial,
        board_config=args.board
    )
