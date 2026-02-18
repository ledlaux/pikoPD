#!/usr/bin/env python3
import os
import sys
import json
import shutil
import subprocess
import time
import jinja2
from hvcc.core.hv2ir.HeavyLangObject import HeavyLangObject
from hvcc.types.IR import IRGraph

heavy_hash = HeavyLangObject.get_hash

class PicoUF2Generator:
    def __init__(self, pd_path, project_root, src_dir):
        self.pd_path = os.path.abspath(pd_path)
        self.project_root = os.path.abspath(project_root)
        self.src_dir = os.path.abspath(src_dir)
        self.ir_dir = os.path.join(project_root, "ir")
        self.hv_dir = os.path.join(project_root, "hv")
        self.c_dir = os.path.join(project_root, "c")
        self.settings_file = os.path.abspath("settings.json")
        self.patch_name = os.path.splitext(os.path.basename(pd_path))[0]

    def run_hvcc(self):
        subprocess.run([
            "hvcc",
            self.pd_path,
            "-o", self.project_root,   # generate all output into project root
            "-n", self.patch_name
        ], check=True)

    def update_settings(self, max_voices=None):
        with open(self.settings_file) as f:
            settings = json.load(f)

    # Always take max_voices from settings.json; don't overwrite if provided
    #    if max_voices is None:
    #        max_voices = settings.get("max_voices", 4)
    #    else:
    #        settings["max_voices"] = max_voices
        max_voices = settings.get("max_voices", 4)


    # Load IR to get receiver hashes
        ir_path = os.path.join(self.ir_dir, f"{self.patch_name}.heavy.ir.json")
        with open(ir_path) as f:
            ir = json.load(f)

        receivers = ir["control"]["receivers"]

    # Prepare a list of dicts with name + hash
        voice_list = []
        for name, recv in receivers.items():
            if name.startswith("__"):
                continue
            h = heavy_hash(name)
            voice_list.append({
                "name": name,
                "hash": f"0x{h:08X}"
        })
            if len(voice_list) >= max_voices:
                break

    # Save both voice hashes and names
        settings["voice_hashes"] = voice_list

        with open(self.settings_file, "w") as f:
            json.dump(settings, f, indent=4)

        print(f"[SETTINGS] Updated {self.settings_file} with {len(voice_list)} voice hashes")
        return settings

    def copy_src(self):
        """Copy src files into c/ folder."""
        if not os.path.exists(self.c_dir):
            os.makedirs(self.c_dir)

        for fname in os.listdir(self.src_dir):
            src = os.path.join(self.src_dir, fname)
            dst = os.path.join(self.c_dir, fname)
            if os.path.isdir(src):
                shutil.copytree(src, dst, dirs_exist_ok=True)
            else:
                shutil.copy(src, dst)
        print(f"[SRC] Copied src files from {self.src_dir} to {self.c_dir}")

    def render_main(self, settings):
        """Render main.cpp from template."""
        env = jinja2.Environment(
            loader=jinja2.FileSystemLoader(os.path.dirname(__file__))
        )
        template = env.get_template("main.cpp.j2")
        output_cpp = os.path.join(self.c_dir, "main.cpp")
        
        settings["voice_hashes_cpp"] = ",\n    ".join(v["hash"] for v in settings["voice_hashes"])

        with open(output_cpp, "w") as f:
            f.write(template.render(name=self.patch_name, settings=settings))
        print(f"[TEMPLATE] Rendered main.cpp to {output_cpp}")

    def build_project(self, cmake_path=None, build_type="Release"):
        """Run CMake and make."""
        build_dir = os.path.join(self.c_dir, "build")
        os.makedirs(build_dir, exist_ok=True)
        cmake_cmd = [
            "cmake",
            f"-DPICO_SDK_PATH={os.environ.get('PICO_SDK_PATH', '/Users/lidlaux/pico-sdk')}",
            f"-DPICO_BOARD=pico2",
            f"-DCMAKE_BUILD_TYPE={build_type}",
            "-DCMAKE_TOOLCHAIN_FILE=${PICO_SDK_PATH}/cmake/preload/toolchains/pico_arm_cortex_m33_gcc.cmake",
            ".."
        ]
        print("[CMAKE] Configuring...")
        subprocess.run(" ".join(cmake_cmd), cwd=build_dir, shell=True, check=True)
        print("[MAKE] Building project...")
        subprocess.run(["make", "-j8"], cwd=build_dir, check=True)
        print("[BUILD] Finished building")

    def flash_uf2(self, uf2_path=None):
        """Flash the built UF2 file from the build/ folder."""
        build_dir = os.path.join(self.c_dir, "build")
        if uf2_path is None:
            # automatically find any .uf2 file in the build/ folder
            uf2_files = [f for f in os.listdir(build_dir) if f.endswith(".uf2")]
            if not uf2_files:
                raise FileNotFoundError(f"No UF2 file found in {build_dir}")
            uf2_path = os.path.join(build_dir, uf2_files[0])  # pick the first one found

        print(f"[PICOTOOL] Flashing UF2: {uf2_path}")
        subprocess.run(["picotool", "load", "-f", uf2_path], check=True)

    def run_all(self, max_voices=None, flash=False):
        start = time.time()
        self.run_hvcc()
        settings = self.update_settings(max_voices=max_voices)
        self.copy_src()
        self.render_main(settings)
        self.build_project()
        if flash:
            self.flash_uf2()
        print(f"[DONE] Total time: {time.time() - start:.1f}s")


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: generator.py <pd_patch> <project_root> [max_voices] [--flash]")
        sys.exit(1)

    pd_patch = sys.argv[1]
    project_root = sys.argv[2]
    max_voices = int(sys.argv[3]) if len(sys.argv) > 3 and sys.argv[3].isdigit() else None
    flash = "--flash" in sys.argv

    gen = PicoUF2Generator(
        pd_patch,
        project_root,
        src_dir=os.path.join(os.path.dirname(os.path.abspath(__file__)), "src")
    )
    gen.run_all(max_voices=max_voices, flash=flash)
