#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CWD="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$CWD"

./qemu/build/qemu-system-x86_64 \
    -m 512M \
    -bios /usr/share/edk2-ovmf/x64/OVMF.4m.fd \
    -device AREK,romfile=/$CWD/edk2/Build/OptionRom.rom -monitor stdio -nodefaults -serial file:serial.log \
    -debugcon file:debug.log -global isa-debugcon.iobase=0x402 \
    -drive file=fat:rw:$CWD/edk2/Build/OvmfX64/DEBUG_GCC5/X64,format=raw,media=disk
   