#!/usr/bin/env python3
"""Stage the scale firmware for ESP Web Tools: the flash parts + a manifest.

ESP Web Tools writes each part at its own offset, so there's no merged image to
build and no chip flash-size to guess. Offsets are the ESP32-S3 Arduino defaults.
boot_app0.bin ships with the Arduino-ESP32 framework, not the build, so it's found
in the PlatformIO packages. Prints the build dir so a bad path is obvious in the log.

Usage: python tools/ci_stage_scale.py <build_dir> <out_dir>
"""
import glob
import json
import os
import shutil
import sys

build_dir = sys.argv[1] if len(sys.argv) > 1 else "scale/.pio/build/filatrack_scale"
out_dir = sys.argv[2] if len(sys.argv) > 2 else "dist/scale"

print("=== build dir contents ===")
for f in sorted(glob.glob(os.path.join(build_dir, "*"))):
    print(f"  {os.path.getsize(f):>9}  {os.path.basename(f)}")

boot_app0 = next(iter(glob.glob(os.path.expanduser(
    "~/.platformio/packages/framework-arduinoespressif32*/tools/partitions/boot_app0.bin"))), None)
print("boot_app0:", boot_app0)

# offset -> source. ESP32-S3 Arduino default flash layout.
layout = [
    (0x0,     os.path.join(build_dir, "bootloader.bin")),
    (0x8000,  os.path.join(build_dir, "partitions.bin")),
    (0xe000,  boot_app0),
    (0x10000, os.path.join(build_dir, "firmware.bin")),
]

os.makedirs(out_dir, exist_ok=True)
parts = []
for offset, src in layout:
    if not src or not os.path.exists(src):
        sys.exit(f"missing flash part for offset {hex(offset)}: {src}")
    name = os.path.basename(src)
    shutil.copy(src, os.path.join(out_dir, name))
    parts.append({"path": name, "offset": offset})

manifest = {
    "name": "FilaTrack Scale",
    "new_install_prompt_erase": True,
    "builds": [{"chipFamily": "ESP32-S3", "parts": parts}],
}
json.dump(manifest, open(os.path.join(out_dir, "manifest.json"), "w", encoding="utf-8"), indent=2)
print(f"staged {len(parts)} parts + manifest.json -> {out_dir}")
