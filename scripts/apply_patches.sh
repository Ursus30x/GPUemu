#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CWD="$(cd "$SCRIPT_DIR/.." && pwd)"

################################################
#################### COMMON ####################
################################################

git submodule init 
git submodule update --recursive

################################################
################# QEMU PATCHES #################
################################################

# Enter QEMU repo
cd "$CWD/qemu"

# Restore QEMU repo to avoid conflicts
git restore .
git clean -qfdx

# Symlink gpu implementation to QEMU hardware
ln -sf "$CWD/gpu/gpu.c" "$CWD/qemu/hw/misc/gpu.c"
ln -sf "$CWD/gpu/gpu.h" "$CWD/qemu/hw/misc/gpu.h"
ln -sf "$CWD/gpu/renderer.c" "$CWD/qemu/hw/misc/renderer.c"
ln -sf "$CWD/gpu/renderer.h" "$CWD/qemu/hw/misc/renderer.h"
ln -sf "$CWD/gpu/math3d.c" "$CWD/qemu/hw/misc/math3d.c"
ln -sf "$CWD/gpu/math3d.h" "$CWD/qemu/hw/misc/math3d.h"
ln -sf "$CWD/include/isa.h" "$CWD/qemu/hw/misc/isa.h"

# Apply config patches
git apply "$CWD/gpu/qemu.patch"

# Configure build 
./configure --target-list="x86_64-softmmu" --enable-debug \
 --extra-cflags="-Wno-error=redundant-decls" --enable-gtk

################################################
################# EDK2 PATCHES #################
################################################

# Enter EDK2 repo
cd "$CWD/edk2"

# Restore EDK2 repo to avoid conflicts
git restore .
git clean -qfdx

# Build stuff nedeed for edk2 to build project
# TODO: Should we do it in the script? Or rather a README.md instruction
# either way its comfortable this way for now
git submodule init 
git submodule update --recursive

make -C BaseTools

# Symlink driver implementation to EDK2
rm -f "$CWD/edk2/OptionRom"
rm -f "$CWD/edk2/DemoApp"
ln -sf "$CWD/UEFI/OptionRom" "$CWD/edk2/OptionRom"
ln -sf "$CWD/UEFI/DemoApp" "$CWD/edk2/DemoApp"

# Apply dsc patches
git apply "$CWD/UEFI/OvmfPkg.patch"

################################################
################################################
cd "$CWD"