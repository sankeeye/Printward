#!/usr/bin/env python3
"""Merge the scale firmware's flash parts into one image for ESP Web Tools.

PlatformIO builds bootloader / partitions / boot_app0 / firmware as separate .bin
files, each flashed at its own offset. A browser flasher is simplest with a single
image written at offset 0, so this reads the exact offsets + flash settings from
PlatformIO's own flasher_args.json (no hardcoded offsets to drift) and runs
esptool's merge_bin.

Usage: python tools/ci_merge_scale.py <build_dir> <output.bin>
"""
import json
import os
import subprocess
import sys

build_dir = sys.argv[1] if len(sys.argv) > 1 else "scale/.pio/build/filatrack_scale"
out = sys.argv[2] if len(sys.argv) > 2 else os.path.join(build_dir, "filatrack-scale-merged.bin")

fa = json.load(open(os.path.join(build_dir, "flasher_args.json"), encoding="utf-8"))
flash_files = fa["flash_files"]          # {"0x0": "bootloader.bin", ...}
write_args = fa.get("write_flash_args", [])  # ["--flash_mode","dio","--flash_freq","80m","--flash_size","4MB"]

# Keep only the settings merge_bin understands; the rest of write_flash_args is for
# writing to a real chip.
keep = {"--flash_mode", "--flash_freq", "--flash_size"}
merge_opts, i = [], 0
while i < len(write_args):
    if write_args[i] in keep and i + 1 < len(write_args):
        merge_opts += [write_args[i], write_args[i + 1]]
        i += 2
    else:
        i += 1

parts = []
for offset, fname in flash_files.items():
    path = fname if os.path.isabs(fname) else os.path.join(build_dir, fname)
    if not os.path.exists(path):
        sys.exit(f"missing flash part: {path}")
    parts += [offset, path]
    print(f"  {offset}\t{path}")

cmd = ["python", "-m", "esptool", "--chip", "esp32s3", "merge_bin", "-o", out] + merge_opts + parts
print("run:", " ".join(cmd))
subprocess.check_call(cmd)
print("merged ->", out, f"({os.path.getsize(out)} bytes)")
