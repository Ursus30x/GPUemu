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
    COMMON_FLAGS="--target-list=x86_64-softmmu --disable-werror --enable-gtk --disable-docs --disable-tools \
--disable-vnc --disable-spice --disable-slirp --disable-xen --disable-curl --audio-drv-list=\"\""

    if [ "$BUILD_TYPE" == "RELEASE" ]; then
        RELEASE_CFLAGS="-O3 -march=native -mtune=native -fno-plt -fno-math-errno -fno-trapping-math"
        CONFIG_FLAGS="$COMMON_FLAGS --disable-debug-info --enable-malloc=jemalloc --enable-lto --extra-cflags='$RELEASE_CFLAGS' --extra-ldflags='-flto=auto'"
    else
        CONFIG_FLAGS="$COMMON_FLAGS --enable-debug"
    fi

    eval ./configure $CONFIG_FLAGS
    set_build_type "$BUILD_TYPE"
else
    log_info "QEMU configuration ($BUILD_TYPE) is up to date."
fi

log_step "Building QEMU [$BUILD_TYPE] using $(nproc) parallel jobs..."
make -j"$(nproc)" qemu-system-x86_64

set_build_type "$BUILD_TYPE"
log_success "QEMU built successfully [$BUILD_TYPE]"
