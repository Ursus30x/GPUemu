#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CWD="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$CWD"

# Copy gpu implementation to QEMU hardware
cp "$CWD/gpu/gpu.c" "$CWD/qemu/hw/misc/gpu.c"

# Rebuild qemu
cd "$CWD/qemu"
make -j"$(nproc)"
