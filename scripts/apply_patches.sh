#!/bin/bash
set -e # Exit immediately if a command exits with a non-zero status

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CWD="$(cd "$SCRIPT_DIR/.." && pwd)"

################################################
################# GPU CODE GEN #################
################################################
cd "$CWD/gpu/gen"

CORE_JSON="spirv.core.grammar.json"
CORE_URL="https://raw.githubusercontent.com/KhronosGroup/SPIRV-Headers/main/include/spirv/unified1/spirv.core.grammar.json"

if [ ! -f "$CORE_JSON" ]; then
    echo "Downloading $CORE_JSON..."
    wget -q "$CORE_URL" -O "$CORE_JSON"
fi
python3 gen_spirv_header.py

GLSL_JSON="extinst.glsl.std.450.grammar.json"
GLSL_URL="https://raw.githubusercontent.com/KhronosGroup/SPIRV-Headers/main/include/spirv/unified1/extinst.glsl.std.450.grammar.json"

if [ ! -f "$GLSL_JSON" ]; then
    echo "Downloading $GLSL_JSON..."
    wget -q "$GLSL_URL" -O "$GLSL_JSON"
fi
python3 gen_glsl_header.py
################################################
#################### COMMON ####################
################################################

git submodule init
git submodule update --recursive

rm -f .last_build_type
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
ln -sf "$CWD/gpu/debug_gpu.h" "$CWD/qemu/hw/misc/debug_gpu.h"
ln -sf "$CWD/gpu/renderer.c" "$CWD/qemu/hw/misc/renderer.c"
ln -sf "$CWD/gpu/renderer.h" "$CWD/qemu/hw/misc/renderer.h"
ln -sf "$CWD/gpu/math3d.c" "$CWD/qemu/hw/misc/math3d.c"
ln -sf "$CWD/gpu/math3d.h" "$CWD/qemu/hw/misc/math3d.h"
ln -sf "$CWD/gpu/jit/jit.c" "$CWD/qemu/hw/misc/jit.c"
ln -sf "$CWD/gpu/jit/jit.h" "$CWD/qemu/hw/misc/jit.h"
ln -sf "$CWD/gpu/jit/jit_alu.c" "$CWD/qemu/hw/misc/jit_alu.c"
ln -sf "$CWD/gpu/jit/jit_alu.h" "$CWD/qemu/hw/misc/jit_alu.h"
ln -sf "$CWD/gpu/jit/jit_decorators.c" "$CWD/qemu/hw/misc/jit_decorators.c"
ln -sf "$CWD/gpu/jit/jit_decorators.h" "$CWD/qemu/hw/misc/jit_decorators.h"
ln -sf "$CWD/gpu/gen/spirv_jit_meta.h" "$CWD/qemu/hw/misc/spirv_jit_meta.h"
ln -sf "$CWD/gpu/gen/glsl_std_450.h" "$CWD/qemu/hw/misc/glsl_std_450.h"
ln -sf "$CWD/gpu/jit/jit_flow.c" "$CWD/qemu/hw/misc/jit_flow.c"
ln -sf "$CWD/gpu/jit/jit_flow.h" "$CWD/qemu/hw/misc/jit_flow.h"
ln -sf "$CWD/gpu/jit/jit_mem.c" "$CWD/qemu/hw/misc/jit_mem.c"
ln -sf "$CWD/gpu/jit/jit_mem.h" "$CWD/qemu/hw/misc/jit_mem.h"
ln -sf "$CWD/include/gpu_isa.h" "$CWD/qemu/hw/misc/gpu_isa.h"
ln -sf "$CWD/include/gpu_hw.h" "$CWD/qemu/hw/misc/gpu_hw.h"
ln -sf "$CWD/include/vram.h" "$CWD/qemu/hw/misc/vram.h"

# Apply config patches
git apply "$CWD/gpu/qemu.patch"

################################################
################# EDK2 PATCHES #################
################################################

# Enter EDK2 repo
cd "$CWD/edk2"

# Restore EDK2 repo to avoid conflicts
git restore .
git clean -qfdx

# Build stuff nedeed for edk2 to build project
git submodule init
git submodule update --recursive

make -C BaseTools

# Remove old driver symlinks/files for driver implementation
rm -f "$CWD/edk2/OptionRom"
rm -f "$CWD/edk2/DemoApp"
rm -f "$CWD/edk2/FrameBenchmark"
rm -f "$CWD/edk2/OvmfPkg/Include/Protocol/Gop3D.h"
rm -f "$CWD/edk2/OptionRom/gpu_isa.h"
rm -f "$CWD/edk2/OptionRom/gpu_hw.h"
rm -f "$CWD/edk2/OptionRom/varm.h"
rm -f "$CWD/edk2/Conf/target.txt"

# Symlink driver implementation to EDK2
ln -sf "$CWD/UEFI/OptionRom"                        "$CWD/edk2/OptionRom"
ln -sf "$CWD/UEFI/DemoApp"                          "$CWD/edk2/DemoApp"
ln -sf "$CWD/UEFI/FrameBenchmark"                   "$CWD/edk2/FrameBenchmark"
ln -sf "$CWD/UEFI/OvmfPkg/Include/Protocol/Gop3D.h" "$CWD/edk2/OvmfPkg/Include/Protocol/Gop3D.h"
ln -sf "$CWD/include/gpu_isa.h"                     "$CWD/edk2/OptionRom/gpu_isa.h"
ln -sf "$CWD/include/gpu_hw.h"                      "$CWD/edk2/OptionRom/gpu_hw.h"
ln -sf "$CWD/include/vram.h"                        "$CWD/edk2/OptionRom/vram.h"
ln -sf "$CWD/UEFI/target.txt"                       "$CWD/edk2/Conf/target.txt"

# Apply dsc patches
git apply "$CWD/UEFI/OvmfPkg.patch"

################################################
################################################

# Compiler symlink
rm -f  "$CWD/compiler/gpu_isa.h"
rm -f  "$CWD/compiler/gpu_hw.h"
rm -f  "$CWD/compiler/vram.h"
ln -sf "$CWD/include/gpu_isa.h" "$CWD/compiler/gpu_isa.h"
ln -sf "$CWD/include/gpu_hw.h" "$CWD/compiler/gpu_hw.h"
ln -sf "$CWD/include/vram.h" "$CWD/compiler/vram.h"

cd "$CWD"