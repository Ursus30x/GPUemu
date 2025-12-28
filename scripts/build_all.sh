#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CWD="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$CWD/scripts"

if [ -z "$1" ]; then
    BUILD_TYPE="DEBUG"
else
    BUILD_TYPE="$1"
fi

./build_qemu.sh $BUILD_TYPE
./build_edk2.sh $BUILD_TYPE
./build_compiler.sh