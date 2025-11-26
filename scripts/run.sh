#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CWD="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$CWD"

./qemu/build/qemu-system-x86_64 \
    -m 512M \
    -hda "$CWD/edk2/disk.raw" \
    -bios /usr/share/edk2-ovmf/x64/OVMF.4m.fd \
    -device AREK,romfile=/$CWD/edk2/Build/OptionRom.rom \
    -monitor stdio
    -serial file:serial.log \
    -nodefaults