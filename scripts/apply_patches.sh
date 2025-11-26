#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CWD="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$CWD"

# Symlink gpu implementation to QEMU hardware
ln -sf "$CWD/gpu/gpu.c" "$CWD/qemu/hw/misc/gpu.c"

# Apply config patches
cd "$CWD/qemu"
git apply "$CWD/gpu/qemu.patch"

# Configure build 
./configure --target-list="x86_64-softmmu" --enable-debug \
 --extra-cflags="-Wno-error=redundant-decls" --enable-gtk

cd "$CWD"


# Symlink driver implementation to EDK2
ln -sf "$CWD/OptionRom" "$CWD/edk2/OptionRom"

# Apply dsc patches
cd "$CWD/edk2"

git apply "$CWD/OptionRom/OvmfPkg.patch"