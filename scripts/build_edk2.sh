#!/bin/bash

set -e  # Exit immediately if any command fails

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CWD="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$CWD/edk2"

echo "=== Setting up EDK2 environment ==="
source ./edksetup.sh
if [ $? -ne 0 ]; then
    echo "ERROR: Failed to setup EDK2 environment"
    exit 1
fi

export EDK2_TOOLCHAIN=GCC5

echo ""
echo "=== Building OptionRom ==="
build -a X64 -t GCC5 -p OvmfPkg/OvmfPkgX64.dsc -m OptionRom/Rom.inf
if [ $? -ne 0 ]; then
    echo "ERROR: Failed to build OptionRom"
    exit 1
fi

echo ""
echo "=== Building DemoApp ==="
build -a X64 -t GCC5 -p OvmfPkg/OvmfPkgX64.dsc -m DemoApp/DemoApp.inf
if [ $? -ne 0 ]; then
    echo "ERROR: Failed to build DemoApp"
    exit 1
fi

echo ""
echo "=== Building FrameBenchmark ==="
build -a X64 -t GCC5 -p OvmfPkg/OvmfPkgX64.dsc -m FrameBenchmark/FrameBenchmark.inf
if [ $? -ne 0 ]; then
    echo "ERROR: Failed to build FrameBenchmark"
    exit 1
fi

echo ""
echo "=== Creating Option ROM image ==="
./BaseTools/Source/C/bin/EfiRom -f 0x6969 -i 0x2137 -o ./Build/OptionRom.rom -e ./Build/OvmfX64/DEBUG_GCC5/X64/OptionRom.efi
if [ $? -ne 0 ]; then
    echo "ERROR: Failed to create Option ROM image"
    exit 1
fi

echo ""
echo "=== Build completed successfully! ==="