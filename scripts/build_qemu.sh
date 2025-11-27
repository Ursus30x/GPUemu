#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CWD="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$CWD"


cd "$CWD/qemu"
./configure --target-list="x86_64-softmmu" --enable-debug \
 --extra-cflags="-Wno-error=redundant-decls" --enable-gtk  

make -j"$(nproc)"
