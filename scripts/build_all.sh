#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CWD="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$CWD/scripts"

./build_qemu.sh
./build_edk2.sh
./build_compiler.sh