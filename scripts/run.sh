#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CWD="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$CWD"

# Fix: Use absolute path consistent with build scripts
MARKER_FILE="$CWD/.last_build_type"

if [ -f "$MARKER_FILE" ]; then
    LAST_TYPE=$(cat "$MARKER_FILE")
else
    # Default to DEBUG if file is missing (e.g. first run or cleaned)
    LAST_TYPE="DEBUG" 
fi

./qemu/build/qemu-system-x86_64 \
    -m 4G \
    -smp 4 \
    -cpu host \
    -enable-kvm \
    -bios /usr/share/edk2-ovmf/x64/OVMF.4m.fd \
    -device AREK,romfile=$CWD/edk2/Build/OptionRom.rom \
    -monitor stdio \
    -nodefaults \
    -serial file:serial.log \
    -debugcon file:debug.log \
    -global isa-debugcon.iobase=0x402 \
    -drive file=fat:rw:$CWD/edk2/Build/OvmfX64/${LAST_TYPE}_GCC/X64,format=raw,media=disk