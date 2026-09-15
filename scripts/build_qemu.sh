#!/bin/bash
# scripts/build_qemu.sh - Build the QEMU emulator with the GPU device

set -e
source "$(dirname "$0")/common.sh"

BUILD_TYPE="$(parse_build_type "$1")"
LAST_TYPE="$(get_last_build_type)"

cd "$QEMU_DIR"

# Check if QEMU configuration is missing or needs updating
NEED_CONFIGURE=0
if [ ! -f "$QEMU_DIR/build/build.ninja" ] && [ ! -f "$QEMU_DIR/build/config-host.mak" ]; then
    log_warn "QEMU build files not found. Configuration required."
    NEED_CONFIGURE=1
elif [ "$BUILD_TYPE" != "$LAST_TYPE" ]; then
    log_step "QEMU configuration changed ($LAST_TYPE -> $BUILD_TYPE). Reconfiguring..."
    NEED_CONFIGURE=1
fi

if [ "$NEED_CONFIGURE" -eq 1 ]; then
    if [ "$BUILD_TYPE" == "RELEASE" ]; then
        CONFIG_FLAGS="--extra-cflags='-Wno-error=redundant-decls -O3 -march=native -fno-plt' --enable-lto --enable-gtk -Wno-error=discarded-qualifiers"
    else
        CONFIG_FLAGS="--enable-debug --extra-cflags='-Wno-error=redundant-decls' --enable-gtk -Wno-error=discarded-qualifiers"
    fi

    eval ./configure --target-list="x86_64-softmmu" $CONFIG_FLAGS
    set_build_type "$BUILD_TYPE"
else
    log_info "QEMU configuration ($BUILD_TYPE) is up to date."
fi

log_step "Building QEMU [$BUILD_TYPE] using $(nproc) parallel jobs..."
make -j"$(nproc)"

set_build_type "$BUILD_TYPE"
log_success "QEMU built successfully [$BUILD_TYPE]"
