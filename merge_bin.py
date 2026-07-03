# merge_bin.py
# PlatformIO post-build script for C.P.S. CardPuter Synth
#
# After each successful build, merges:
#   0x0000  bootloader.bin
#   0x8000  partitions.bin
#   0x10000 firmware.bin
# into a single file: .pio/build/cps/merge.bin
#
# This merged binary can be uploaded directly via M5Burner or any
# other ESP32 flash tool that accepts a single file at offset 0x0.
#
# Usage: automatically invoked by PlatformIO via extra_scripts in
# platformio.ini — no manual steps required.

import os
Import("env")  # PlatformIO build environment (SCons)

def merge_bins(source, target, env):
    build_dir   = env.subst("$BUILD_DIR")
    project_dir = env.subst("$PROJECT_DIR")

    bootloader  = os.path.join(build_dir, "bootloader.bin")
    partitions  = os.path.join(build_dir, "partitions.bin")
    firmware    = os.path.join(build_dir, "firmware.bin")
    output      = os.path.join(build_dir, "merge.bin")

    # esptool is bundled with PlatformIO; invoke via the Python interpreter
    # and the OBJCOPY variable which points to esptool.py inside the toolchain.
    cmd = " ".join([
        '"$PYTHONEXE"',
        '"$OBJCOPY"',
        "--chip", "esp32s3",
        "merge_bin",
        "--output", f'"{output}"',
        "0x0000", f'"{bootloader}"',
        "0x8000", f'"{partitions}"',
        "0x10000", f'"{firmware}"',
    ])

    print(f"\n[C.P.S. CardPuter Synth] Merging binaries -> {output}")
    ret = env.Execute(env.subst(cmd))
    if ret == 0:
        print(f"[C.P.S. CardPuter Synth] merge.bin generated successfully.")
    else:
        print(f"[C.P.S. CardPuter Synth] merge_bin FAILED (exit code {ret})")

# Register the function to run after firmware.bin is produced
env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", merge_bins)
