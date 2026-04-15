Import("env")
import subprocess, os, sys, pathlib

ROOT        = pathlib.Path(env["PROJECT_DIR"])
CORE1_SRC   = ROOT / "core1_project"
BUILD_DIR   = ROOT / ".pio" / "core1_build"
CORE1_BIN   = BUILD_DIR / "core1.bin"
CORE1_HDR   = ROOT / "include" / "core1_bin.h"

cmake_path = r"C:\Program Files\CMake\bin"
os.environ["PATH"] = cmake_path + ";" + os.environ.get("PATH", "")

def run(cmd, cwd=None):
    result = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)
    if result.returncode != 0:
        print(result.stdout)
        print(result.stderr)
        sys.exit(result.returncode)
    return result

def bin_to_header(bin_path, hdr_path):
    data = bin_path.read_bytes()
    lines = [
        "#pragma once",
        "#include <stdint.h>",
        "",
        f"// Auto-generated from {bin_path.name} — do not edit",
        f"#define CORE1_BIN_LEN {len(data)}u",
        "",
        f"static const uint8_t core1_bin[{len(data)}] = {{",
    ]
    # 12 bytes per row
    for i in range(0, len(data), 12):
        row = data[i:i+12]
        lines.append("    " + ", ".join(f"0x{b:02x}" for b in row) + ",")
    lines += ["};", ""]
    hdr_path.write_text("\n".join(lines))
    print(f"[core1] Embedded {len(data)} bytes → {hdr_path.name}")

BUILD_DIR.mkdir(parents=True, exist_ok=True)

print("[core1] Running cmake configure...")
run([
    "cmake",
    "-S", str(CORE1_SRC),
    "-B", str(BUILD_DIR),
    "-DCMAKE_BUILD_TYPE=Release",
    "-DPICO_BOARD=pico2",          # adjust if using a different board
], cwd=str(BUILD_DIR))

print("[core1] Building core1 project...")
run(["cmake", "--build", str(BUILD_DIR), "--parallel"])

if not CORE1_BIN.exists():
    print(f"[core1] ERROR: {CORE1_BIN} not found after build")
    sys.exit(1)

bin_to_header(CORE1_BIN, CORE1_HDR)