#!/usr/bin/env python3
import os, json, shutil, subprocess, jinja2, argparse, time, glob, sys

class PicoUF2Generator:
    def __init__(self, pd_path, project_root, src_dir):
        self.pd_path = os.path.abspath(pd_path)
        self.project_root = os.path.abspath(project_root)
        self.src_dir = src_dir
        self.patch_name = os.path.splitext(os.path.basename(self.pd_path))[0]
        self.ir_json = os.path.join(self.project_root, "ir", f"{self.patch_name}.heavy.ir.json")
        self.c_dir = os.path.join(self.project_root, "c")
        self.build_dir = os.path.join(self.c_dir, "build")

    def print_progress(self, percent, task):
        bar_length = 20
        filled = int(round(bar_length * percent))
        bar = '█' * filled + '-' * (bar_length - filled)
        sys.stdout.write(f'\r[{bar}] {int(percent * 100)}% | {task}')
        sys.stdout.flush()

    def check_pico_bootsel(self):
        try:
            result = subprocess.run(["picotool", "info"], capture_output=True, text=True)
            return "No accessible RP-series devices" not in result.stdout and result.returncode == 0
        except:
            return False

    def collect_hv_manifest(self):
        if not os.path.exists(self.ir_json): return {"patch_name": self.patch_name, "receives": [], "sends": [], "tables": [], "prints": []}
        with open(self.ir_json, "r") as f:
            data = json.load(f)
        manifest = {"patch_name": self.patch_name, "receives": [], "sends": [], "tables": [], "prints": []}
        objects = data.get("objects", {})
        for obj_id, obj_body in objects.items():
            obj_type = obj_body.get("type", "")
            args = obj_body.get("args", {})
            if not isinstance(args, dict): continue
            name = args.get("name") or args.get("label")
            if not name or any(x in name for x in ["__hv_"]): continue
            
            entry = {"name": name, "hash": args.get("hash", "0")}
            if obj_type == "__send": manifest["sends"].append(entry)
            elif obj_type == "__receive" or args.get("extern") == "param": manifest["receives"].append(entry)
            elif obj_type == "__print": manifest["prints"].append(entry)
            elif obj_type == "__table": manifest["tables"].append(entry)
        return manifest

    def build_project(self, settings):
        sdk_path = os.environ.get("PICO_SDK_PATH")
        board = settings.get("pico_board", "pico")
        toolchain = f"{sdk_path}/cmake/preload/toolchains/pico_arm_cortex_m33_gcc.cmake" if board == "pico2" else f"{sdk_path}/cmake/preload/toolchains/pico_arm_gcc.cmake"

        board_record = os.path.join(self.build_dir, ".last_board")
        if os.path.exists(board_record):
            with open(board_record, "r") as f:
                if f.read().strip() != board:
                    shutil.rmtree(self.build_dir, ignore_errors=True)
        
        os.makedirs(self.build_dir, exist_ok=True)
        with open(board_record, "w") as f: f.write(board)

        if not os.path.exists(os.path.join(self.build_dir, "Makefile")):
            self.print_progress(0.60, f"Configuring {board}...")
            subprocess.run(["cmake", f"-DPICO_SDK_PATH={sdk_path}", f"-DPICO_BOARD={board}", f"-DCMAKE_TOOLCHAIN_FILE={toolchain}", ".."], 
                           cwd=self.build_dir, capture_output=True, check=True)

        self.print_progress(0.80, "Compiling...")
        subprocess.run(["make", "-j10"], cwd=self.build_dir, capture_output=True, check=True)

    def run_all(self, flash=False):
        start = time.time()
        print(f"--- pikoPD Build: {self.patch_name} ---")
        
        # 1. HVCC
        self.print_progress(0.10, "Heavy Compiler...")
        subprocess.run(["hvcc", self.pd_path, "-o", self.project_root, "-n", self.patch_name], capture_output=True, check=True)

        # 2. Settings & Sync
        self.print_progress(0.30, "Syncing Source...")
        settings = {"pico_board": "pico2"}
        if os.path.exists("settings.json"):
            with open("settings.json") as f: settings.update(json.load(f))
        
        os.makedirs(self.c_dir, exist_ok=True)
        if os.path.exists(self.src_dir):
            for f in os.listdir(self.src_dir):
                s, d = os.path.join(self.src_dir, f), os.path.join(self.c_dir, f)
                if os.path.isfile(s):
                    if not os.path.exists(d) or open(s,'rb').read() != open(d,'rb').read():
                        shutil.copy2(s, d)

        # 3. Render
        self.print_progress(0.45, "Updating Manifest...")
        manifest = self.collect_hv_manifest()
        env = jinja2.Environment(loader=jinja2.FileSystemLoader(os.path.dirname(__file__)))
        new_main = env.get_template("main.cpp").render(name=self.patch_name, hv_manifest=manifest, settings=settings)
        m_path = os.path.join(self.c_dir, "main.cpp")
        if not os.path.exists(m_path) or open(m_path).read() != new_main:
            with open(m_path, "w") as f: f.write(new_main)

        # 4. Build
        self.build_project(settings)

        # 5. Flash with restored check
        flash_status = ""
        if flash:
            self.print_progress(0.90, "Checking Device...")
            if self.check_pico_bootsel():
                self.print_progress(0.95, "Flashing...")
                uf2 = glob.glob(os.path.join(self.build_dir, "*.uf2"))[0]
                subprocess.run(["picotool", "load", "-f", "-x", uf2], capture_output=True)
                flash_status = " | Flashed!"
            else:
                flash_status = " | SKIP (No pico in BOOTSEL mode)"

        self.print_progress(1.0, f"Done! ({time.time() - start:.1f}s){flash_status}\n")

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("pd_patch"); parser.add_argument("project_root"); parser.add_argument("--flash", action="store_true")
    args = parser.parse_args()
    gen = PicoUF2Generator(args.pd_patch, args.project_root, os.path.join(os.path.dirname(__file__), "src"))
    gen.run_all(flash=args.flash)
