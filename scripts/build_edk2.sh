#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CWD="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$CWD"


cd "$CWD/edk2"

source ./edksetup.sh

export EDK2_TOOLCHAIN=GCC5
build -a X64 -t GCC5 -p OvmfPkg/OvmfPkgX64.dsc -m OptionRom/Rom.inf
./BaseTools/Source/C/bin/EfiRom -f 0x6969 -i 0x2137 -o ./Build/OptionRom.rom -e ./Build/OvmfX64/DEBUG_GCC5/X64/OptionRom.efi
