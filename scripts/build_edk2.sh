#!/bin/bash
# scripts/build_edk2.sh - Build UEFI Option ROM driver and applications

set -e
source "$(dirname "$0")/common.sh"

BUILD_TYPE="$(parse_build_type "$1")"

cd "$EDK2_DIR"

# Ensure BaseTools binaries exist
if [ ! -x "./BaseTools/Source/C/bin/EfiRom" ]; then
    log_warn "BaseTools binaries missing. Building BaseTools..."
    make -C BaseTools
fi

log_step "Setting up EDK2 build environment..."
source ./edksetup.sh BaseTools

# List of components to build
EDK2_MODULES=(
    "OptionRom/Rom.inf:OptionRom"
    "DemoApp/DemoApp.inf:DemoApp"
    "LegacyAsmApp/LegacyAsmApp.inf:LegacyAsmApp"
    "SpirvApp/SpirvApp.inf:SpirvApp"
    "FrameBenchmark/FrameBenchmark.inf:FrameBenchmark"
)

for entry in "${EDK2_MODULES[@]}"; do
    inf="${entry%%:*}"
    name="${entry##*:}"
    log_step "Building EDK2 component: $name [$BUILD_TYPE]..."
    build -p OvmfPkg/OvmfPkgX64.dsc -m "$inf" -b "$BUILD_TYPE"
done

log_step "Packaging Option ROM image..."
ROM_OUTPUT="./Build/OptionRom.rom"
EFI_INPUT="./Build/OvmfX64/${BUILD_TYPE}_GCC/X64/OptionRom.efi"

if [ ! -f "$EFI_INPUT" ]; then
    log_error "Expected driver binary not found: $EFI_INPUT"
    exit 1
fi

./BaseTools/Source/C/bin/EfiRom \
    -f 0x6969 -i 0x2137 \
    -o "$ROM_OUTPUT" \
    -e "$EFI_INPUT"

set_build_type "$BUILD_TYPE"
log_success "EDK2 build and Option ROM generation completed successfully [$BUILD_TYPE]"