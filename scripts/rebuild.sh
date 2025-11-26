#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CWD="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$CWD"

# Copy gpu implementation to QEMU hardware
cp "$CWD/gpu/gpu.c" "$CWD/qemu/hw/misc/gpu.c"
cp "$CWD/gpu/gpu.h" "$CWD/qemu/hw/misc/gpu.h"
cp "$CWD/gpu/renderer.c" "$CWD/qemu/hw/misc/renderer.c"
cp "$CWD/gpu/renderer.h" "$CWD/qemu/hw/misc/renderer.h"
cp "$CWD/gpu/math3d.c" "$CWD/qemu/hw/misc/math3d.c"
cp "$CWD/gpu/math3d.h" "$CWD/qemu/hw/misc/math3d.h"
cp "$CWD/include/isa.h" "$CWD/qemu/hw/misc/isa.h"

# Rebuild qemu
cd "$CWD/qemu"
make -j"$(nproc)"
