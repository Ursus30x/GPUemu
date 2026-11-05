#!/bin/bash
# scripts/build_compiler.sh - Build the custom shader assembler/compiler

set -e
source "$(dirname "$0")/common.sh"

log_step "Building Shader Compiler (output: tools/compiler)..."
make -C "$COMPILER_DIR"

log_success "Shader compiler built and installed to tools/compiler"