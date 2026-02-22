#!/usr/bin/env python3
import os
import sys
import json
import shutil
import subprocess
import time
import jinja2
import argparse
import zlib
import glob

# -------------------------------
# ENVIRONMENT & CONSTANTS
# -------------------------------
sdk_path = os.environ.get("PICO_SDK_PATH")
if not sdk_path:
    raise RuntimeError("PICO_SDK_PATH environment variable is not set. Please export it.")

HV_INTERNAL_MESSAGES = [
    "__hv_noteout", "__hv_ctlout", "__hv_polytouchout", "__hv_pgmout",
    "__hv_touchout", "__hv_bendout", "__hv_midiout", "__hv_midioutport",
    "__hv_init", "__hv_notein", "__hv_ctlin"
]

class HeavyObject:
    @staticmethod
    def get_hash_string(name: str) -> str:
        return f"0x{zlib.adler32(name.encode()) & 0xFFFFFFFF:08X}"

# -------------------------------
# RECURSIVE HV JSON PARSER
# -------------------------------
def parse_node(node, manifest):
    if isinstance(node, list):
        for item in node: parse_node(item, manifest)
        return
    if not isinstance(node, dict): return

    obj_type = node.get("type", "")
    args = node.get("args") if isinstance(node.get("args"), dict) else {}
    name = args.get("name") or args.get("label") or node.get("name")

    if isinstance(name, str) and not (name.startswith("__hv_") or name in HV_INTERNAL_MESSAGES):
        # Parameters / Receives
        if obj_type in ["receive", "param"]:
            attrs = args.get("attributes", {})
            default_val = attrs.get("default", args.get("default", 0.0))
            vtype = attrs.get("type") or ("bool" if isinstance(default_val, bool) else "float")
            if name not in [r["name"] for r in manifest["receives"]]:
                manifest["receives"].append({
                    "name": name, "hash": HeavyObject.get_hash_string(name),
                    "type": str(vtype), "min": float(attrs.get("min", args.get("min", 0.0))),
                    "max": float(attrs.get("max", args.get("max", 1.0))), "default": default_val
                })
        # Sends, Prints, Tables
        elif obj_type in ["send", "__send"]:
            if name not in [s["name"] for s in manifest["sends"]]:
                manifest["sends"].append({"name": name, "hash": HeavyObject.get_hash_string(name)})
        elif obj_type in ["print", "__print"]:
            if name not in [p["name"] for p in manifest["prints"]]:
                manifest["prints"].append({"name": name, "hash": HeavyObject.get_hash_string(name)})
        elif obj_type in ["table", "__table"]:
            if name not in [t["name"] for t in manifest["tables"]]:
                manifest["tables"].append({
                    "name": name, "hash": HeavyObject.get_hash_string(name), "size": args.get("size", 0)
                })

    for val in node.values():
        if isinstance(val, (dict, list)):
            parse_node(val, manifest)

def collect_hv_manifest(hv_json_path):
    if not os.path.exists(hv_json_path):
        raise FileNotFoundError(f"HV JSON not found: {hv_json_path}")
    with open(hv_json_path, "r") as f:
        data = json.load(f)
    
    patch_name = os.path.basename(hv_json_path).split('.')[0]
    manifest = {
        "patch_name": patch_name,
        "stats": {
            "num_inputs": data.get("stats", {}).get("numInputChannels", 0),
            "num_outputs": data.get("stats", {}).get("numOutputChannels", 2),
            "samplerate": data.get("stats", {}).get("samplerate", 44100)
        },
        "receives": [], "sends": [], "prints": [], "tables": []
    }
    parse_node(data, manifest)
    return manifest

# -------------------------------
# PICO UF2 GENERATOR
# -------------------------------
class PicoUF2Generator:
    def __init__(self, pd_path, project_root, src_dir):
        self.pd_path = os.path.abspath(pd_path)
        self.project_root = os.path.abspath(project_root)
        self.src_dir = os.path.abspath(src_dir)
        self.c_dir = os.path.join(self.project_root, "c")
        self.settings_file = os.path.join(os.path.dirname(os.path.abspath(__file__)), "settings.json")
        self.patch_name = os.path.splitext(os.path.basename(self.pd_path))[0]
        self.hv_json = os.path.join(self.project_root, "hv", f"{self.patch_name}.hv.json")

    def run_hvcc(self):
        print(f"[HVCC] Processing {self.patch_name}...")
        subprocess.run(["hvcc", self.pd_path, "-o", self.project_root, "-n", self.patch_name], check=True)

    def load_settings(self):
        settings = {"max_voices": 4, "pico_board": "pico"}
        if os.path.exists(self.settings_file):
            with open(self.settings_file) as f:
                settings.update(json.load(f))
        
        tc_file = "pico_arm_cortex_m33_gcc.cmake" if settings["pico_board"] == "pico2" else "pico_arm_gcc.cmake"
        tc_path = os.path.join(sdk_path, "cmake/preload/toolchains", tc_file)
        
        if not os.path.exists(tc_path):
            print(f"[SEARCH] Hunting for {tc_file}...")
            found = glob.glob(os.path.join(sdk_path, "**", tc_file), recursive=True)
            if not found: raise FileNotFoundError(f"FATAL: {tc_file} not found in PICO_SDK_PATH.")
            tc_path = found[0]

        settings["toolchain_path"] = tc_path
        print(f"[SETTINGS] Board: {settings['pico_board']} | Toolchain: {os.path.basename(tc_path)}")
        return settings

    def copy_src(self):
        os.makedirs(self.c_dir, exist_ok=True)
        if os.path.exists(self.src_dir):
            shutil.copytree(self.src_dir, self.c_dir, dirs_exist_ok=True)

    def render_and_save_manifest(self, settings):
        manifest = collect_hv_manifest(self.hv_json)
        
        # SAVE JSON TO ROOT
        manifest_path = os.path.join(self.project_root, "pico_manifest.json")
        with open(manifest_path, 'w') as f:
            json.dump(manifest, f, indent=4)
        
        # RENDER main.cpp
        env = jinja2.Environment(loader=jinja2.FileSystemLoader(os.path.dirname(os.path.abspath(__file__))))
        template = env.get_template("main.cpp")
        with open(os.path.join(self.c_dir, "main.cpp"), "w") as f:
            f.write(template.render(name=self.patch_name, settings=settings, hv_manifest=manifest))
        
        print(f"[MANIFEST] Saved to: {manifest_path}")
        print(f"[TEXT] RECV: {', '.join([r['name'] for r in manifest['receives']])}")
        print(f"[TEXT] SEND: {', '.join([s['name'] for s in manifest['sends']])}")

    def build_project(self, settings):
        build_dir = os.path.join(self.c_dir, "build")
        os.makedirs(build_dir, exist_ok=True)
        
        subprocess.run([
            "cmake", f"-DPICO_SDK_PATH={sdk_path}", f"-DPICO_BOARD={settings['pico_board']}",
            f"-DCMAKE_TOOLCHAIN_FILE={settings['toolchain_path']}", "-DCMAKE_BUILD_TYPE=Release", ".."
        ], cwd=build_dir, check=True)
        
        subprocess.run(["make", f"-j{os.cpu_count() or 4}"], cwd=build_dir, check=True)

    def flash_uf2(self):
        uf2s = glob.glob(os.path.join(self.c_dir, "build", "*.uf2"))
        if uf2s:
            print(f"[FLASH] Loading {os.path.basename(uf2s[0])}...")
            subprocess.run(["picotool", "load", "-f", "-x", uf2s[0]], check=False)

    def run_all(self, flash=False):
        start = time.time()
        self.run_hvcc()
        settings = self.load_settings()
        self.copy_src()
        self.render_and_save_manifest(settings)
        self.build_project(settings)
        if flash: self.flash_uf2()
        print(f"[DONE] Time: {time.time() - start:.1f}s")

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("pd_patch")
    parser.add_argument("project_root")
    parser.add_argument("--flash", action="store_true")
    args = parser.parse_args()

    gen = PicoUF2Generator(args.pd_patch, args.project_root, os.path.join(os.path.dirname(__file__), "src"))
    gen.run_all(flash=args.flash)
