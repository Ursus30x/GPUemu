#!/bin/bash
# scripts/build_all.sh - Build all GPUemu components (QEMU, EDK2, Compiler)

set -e
source "$(dirname "$0")/common.sh"

BUILD_TYPE="$(parse_build_type "$1")"

log_step "============================================================"
log_step " Building all GPUemu components [$BUILD_TYPE]"
log_step "============================================================"

"$SCRIPT_DIR/build_qemu.sh" "$BUILD_TYPE"
"$SCRIPT_DIR/build_edk2.sh" "$BUILD_TYPE"
"$SCRIPT_DIR/build_compiler.sh"

log_step "============================================================"
log_success "All GPUemu components built successfully [$BUILD_TYPE]"
log_step "============================================================"