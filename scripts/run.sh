#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CWD="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$CWD"

./qemu/build/qemu-system-x86_64 \
                    -m 512M \
                    -bios /usr/share/edk2-ovmf/x64/OVMF.4m.fd \
                    -device AREK \
                    -monitor stdio \
                    -nodefaults