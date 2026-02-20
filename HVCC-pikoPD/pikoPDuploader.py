#!/usr/bin/env python3
import os
import sys
import json
import shutil
import subprocess
import time
import jinja2
import glob
import re
import argparse

sdk_path = os.environ.get("PICO_SDK_PATH")
if not sdk_path:
    raise RuntimeError("PICO_SDK_PATH environment variable is not set.")

# HEAVY PARSER 
def parse_heavy_receiver_hashes(c_dir):
    cpp_files = [f for f in os.listdir(c_dir) if f.startswith("Heavy_") and f.endswith(".cpp")]
    if not cpp_files:
        raise FileNotFoundError("No Heavy CPP found")
    cpp_path = os.path.join(c_dir, cpp_files[0])
    print(f"[DEBUG] Using {cpp_path}")

    hashes = []
    pattern = re.compile(r"case\s+(0x[0-9A-F]+):\s*{?\s*//\s*(\w+)")
    ignore_prefix = "__hv"

    with open(cpp_path, "r") as f:
        for line in f:
            m = pattern.search(line)
            if m:
                name = m.group(2)
                if name.startswith(ignore_prefix):
                    continue
                hashes.append({"name": name, "hash": m.group(1)})

    print(f"[DEBUG] Found {len(hashes)} receiver hashes")
    return hashes

# GENERATOR
class PicoUF2Generator:

    def __init__(self, pd_path, project_root, src_dir):
        self.pd_path = os.path.abspath(pd_path)
        self.project_root = os.path.abspath(project_root)
        self.src_dir = os.path.abspath(src_dir)
        self.c_dir = os.path.join(self.project_root, "c")
        self.settings_file = os.path.join(os.path.dirname(os.path.abspath(__file__)), "settings.json")
        self.patch_name = os.path.splitext(os.path.basename(self.pd_path))[0]

    def run_hvcc(self):
        print(f"[HVCC] Compiling {self.pd_path} → {self.project_root}")
        subprocess.run([
            "hvcc",
            self.pd_path,
            "-o", self.project_root,
            "-n", self.patch_name
        ], check=True)

    def update_settings(self):
        # Load settings from script folder
        if os.path.exists(self.settings_file):
            with open(self.settings_file) as f:
                settings = json.load(f)
        else:
            settings = {"max_voices": 4}

        max_voices = settings.get("max_voices", 4)

        # Parse Heavy CPP receiver hashes
        voice_list = parse_heavy_receiver_hashes(self.c_dir)
        settings["voice_hashes"] = voice_list[:max_voices]

        with open(self.settings_file, "w") as f:
            json.dump(settings, f, indent=4)

        print(f"[SETTINGS] Updated {self.settings_file} with {len(settings['voice_hashes'])} voice hashes")
        return settings

    def copy_src(self):
        os.makedirs(self.c_dir, exist_ok=True)
        for fname in os.listdir(self.src_dir):
            src = os.path.join(self.src_dir, fname)
            dst = os.path.join(self.c_dir, fname)
            if os.path.isdir(src):
                shutil.copytree(src, dst, dirs_exist_ok=True)
            else:
                shutil.copy2(src, dst)
        print(f"[SRC] Copied src files → {self.c_dir}")

    def render_main(self, settings):
        env = jinja2.Environment(loader=jinja2.FileSystemLoader(os.path.dirname(__file__)))
        template = env.get_template("main.cpp")
        output_cpp = os.path.join(self.c_dir, "main.cpp")

        settings["voice_hashes_cpp"] = ",\n    ".join(v["hash"] for v in settings["voice_hashes"])

        with open(output_cpp, "w") as f:
            f.write(template.render(name=self.patch_name, settings=settings))
        print(f"[TEMPLATE] Rendered main.cpp")

    def build_project(self, build_type="Release"):
        build_dir = os.path.join(self.c_dir, "build")
        os.makedirs(build_dir, exist_ok=True)

        cmake_cmd = [
            "cmake",
            f"-DPICO_SDK_PATH={sdk_path}",
            "-DPICO_BOARD=pico2",
            f"-DCMAKE_BUILD_TYPE={build_type}",
            f"-DCMAKE_TOOLCHAIN_FILE={sdk_path}/cmake/preload/toolchains/pico_arm_cortex_m33_gcc.cmake",
            ".."
        ]

        print("[CMAKE] Configuring...")
        subprocess.run(cmake_cmd, cwd=build_dir, check=True)

        print("[MAKE] Building...")
        subprocess.run(["make", "-j8"], cwd=build_dir, check=True)

        print("[BUILD] Done")

    def flash_uf2(self):
        build_dir = os.path.join(self.c_dir, "build")
        uf2_files = [f for f in os.listdir(build_dir) if f.endswith(".uf2")]
        if not uf2_files:
            raise FileNotFoundError("No UF2 file found")
        uf2_path = os.path.join(build_dir, uf2_files[0])
        print(f"[PICOTOOL] Flashing {uf2_path}")
        subprocess.run(["picotool", "load", "-f", "-x", uf2_path], check=True)

    def run_all(self, flash=False):
        start = time.time()
        self.run_hvcc()
        settings = self.update_settings()
        self.copy_src()
        self.render_main(settings)
        self.build_project()
        if flash:
            self.flash_uf2()
        print(f"[DONE] Total time: {time.time() - start:.1f}s")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Build and flash a pikoPD Heavy patch")
    parser.add_argument("pd_patch", help="Path to the Pure Data patch (.pd)")
    parser.add_argument("project_root", help="Root folder for the generated project")
    parser.add_argument("--flash", action="store_true", help="Flash the UF2 to the Pico")
    args = parser.parse_args()

    generator = PicoUF2Generator(
        pd_path=args.pd_patch,
        project_root=args.project_root,
        src_dir=os.path.join(os.path.dirname(os.path.abspath(__file__)), "src")
    )

    generator.run_all(flash=args.flash)
