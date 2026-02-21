#!/bin/bash
set -e # Exit immediately if a command exits with a non-zero status

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CWD="$(cd "$SCRIPT_DIR/.." && pwd)"

if [ -z "$1" ]; then
    BUILD_TYPE="DEBUG"
else
    BUILD_TYPE="$1"
fi

# Fix: Use absolute path for marker file so it is shared correctly
MARKER_FILE="$CWD/.last_build_type"

if [ -f "$MARKER_FILE" ]; then
    LAST_TYPE=$(cat "$MARKER_FILE")
else
    LAST_TYPE=""
fi

cd "$CWD/qemu"

if [ "$BUILD_TYPE" != "$LAST_TYPE" ]; then
    echo "Configuration changed ($LAST_TYPE -> $BUILD_TYPE). Running ./configure..."

    # Define flags based on type
    if [ "$BUILD_TYPE" == "RELEASE" ]; then
        CONFIG_FLAGS="--extra-cflags='-Wno-error=redundant-decls -O3 -march=native -fno-plt' --enable-lto --enable-gtk"
    else
        CONFIG_FLAGS="--enable-debug --extra-cflags='-Wno-error=redundant-decls' --enable-gtk"
    fi

    # Run configure
    eval ./configure --target-list="x86_64-softmmu" $CONFIG_FLAGS
    
    # Fix: Update the marker file ONLY after we have detected the change and configured
    echo "$BUILD_TYPE" > "$MARKER_FILE"
else
    echo "Configuration ($BUILD_TYPE) is up to date. Skipping ./configure."
fi

make -j"$(nproc)"