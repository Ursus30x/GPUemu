#!/bin/bash
# scripts/apply_patches.sh - Setup submodules, codegen, symlinks, and patches

set -e
source "$(dirname "$0")/common.sh"

FORCE_RESET=0
if [ "${1:-}" = "--force" ] || [ "${1:-}" = "-f" ]; then
    FORCE_RESET=1
    log_warn "Force mode enabled: submodules will be reset to clean state."
fi

# ============================================================================
# GPU Code Generation (SPIR-V & GLSL grammar headers)
# ============================================================================
generate_gpu_headers() {
    log_step "Checking SPIR-V and GLSL code generation..."
    local gen_dir="$GPU_DIR/gen"

    local core_json="$gen_dir/spirv.core.grammar.json"
    local core_url="https://raw.githubusercontent.com/KhronosGroup/SPIRV-Headers/main/include/spirv/unified1/spirv.core.grammar.json"
    if [ ! -f "$core_json" ]; then
        log_info "Downloading $core_json..."
        wget -q "$core_url" -O "$core_json"
    fi
    (cd "$gen_dir" && python3 gen_spirv_header.py)

    local glsl_json="$gen_dir/extinst.glsl.std.450.grammar.json"
    local glsl_url="https://raw.githubusercontent.com/KhronosGroup/SPIRV-Headers/main/include/spirv/unified1/extinst.glsl.std.450.grammar.json"
    if [ ! -f "$glsl_json" ]; then
        log_info "Downloading $glsl_json..."
        wget -q "$glsl_url" -O "$glsl_json"
    fi
    (cd "$gen_dir" && python3 gen_glsl_header.py)
}

# ============================================================================
# Patch helper: applies patch only if not already applied
# ============================================================================
apply_submodule_patch() {
    local target_dir="$1"
    local patch_file="$2"
    local extra_args="${3:-}"

    (
        cd "$target_dir"
        if [ "$FORCE_RESET" -eq 1 ]; then
            log_warn "Restoring $target_dir to clean state..."
            git restore .
        fi

        if git apply --reverse --check $extra_args "$patch_file" >/dev/null 2>&1; then
            log_info "Patch $(basename "$patch_file") is already applied in $(basename "$target_dir")."
        else
            log_step "Applying $(basename "$patch_file") to $(basename "$target_dir")..."
            git apply $extra_args "$patch_file"
        fi
    )
}

# ============================================================================
# Main Setup Sequence
# ============================================================================
generate_gpu_headers

log_step "Updating git submodules..."
(
    cd "$REPO_ROOT"
    git submodule init
    git submodule update --recursive
)
rm -f "$MARKER_FILE"

# --- Setup QEMU ---
log_step "Setting up QEMU configuration & symlinks..."
apply_submodule_patch "$QEMU_DIR" "$GPU_DIR/qemu.patch"

# JIT internal symlinks
create_symlink "$GPU_DIR/debug_gpu.h"          "$GPU_DIR/jit/debug_gpu.h"
create_symlink "$GPU_DIR/gen/spirv_jit_meta.h" "$GPU_DIR/jit/spirv_jit_meta.h"
create_symlink "$GPU_DIR/gen/glsl_std_450.h"   "$GPU_DIR/jit/glsl_std_450.h"

# QEMU hardware symlinks
QEMU_FILES=(
    "gpu/gpu.c"
    "gpu/gpu.h"
    "gpu/debug_gpu.h"
    "gpu/renderer.c"
    "gpu/renderer.h"
    "gpu/primitive_assembly.c"
    "gpu/primitive_assembly.h"
    "gpu/rasterizer_simt.c"
    "gpu/rasterizer_simt.h"
    "gpu/math3d.c"
    "gpu/math3d.h"
    "gpu/asm.c"
    "gpu/asm.h"
    "gpu/jit/jit.c"
    "gpu/jit/jit.h"
    "gpu/jit/jit_alu.c"
    "gpu/jit/jit_alu.h"
    "gpu/jit/jit_decorators.c"
    "gpu/jit/jit_decorators.h"
    "gpu/gen/spirv_jit_meta.h"
    "gpu/gen/glsl_std_450.h"
    "gpu/jit/jit_flow.c"
    "gpu/jit/jit_flow.h"
    "gpu/jit/jit_mem.c"
    "gpu/jit/jit_mem.h"
    "gpu/jit/jit_smpl.h"
    "gpu/jit/jit_smpl.c"
    "gpu/jit/jit_atomic.h"
    "gpu/jit/jit_atomic.c"
    "include/gpu_isa.h"
    "include/gpu_hw.h"
    "include/vram.h"
)

for file in "${QEMU_FILES[@]}"; do
    filename="$(basename "$file")"
    create_symlink "$REPO_ROOT/$file" "$QEMU_DIR/hw/misc/$filename"
done

# Build and run JIT standalone tests
log_step "Building and verifying JIT engine..."
make -C "$GPU_DIR/jit"
(cd "$GPU_DIR/jit/test" && python3 test.py)

# --- Setup EDK2 ---
log_step "Setting up EDK2 configuration & symlinks..."
(
    cd "$EDK2_DIR"
    git submodule init
    git submodule update --recursive
)
apply_submodule_patch "$EDK2_DIR" "$UEFI_DIR/OvmfPkg.patch" "--ignore-space-change --ignore-whitespace"

EDK2_MAPPINGS=(
    "UEFI/OptionRom:edk2/OptionRom"
    "UEFI/SpirvApp:edk2/SpirvApp"
    "UEFI/FrameBenchmark:edk2/FrameBenchmark"
    "UEFI/OvmfPkg/Include/Protocol/Gop3D.h:edk2/OvmfPkg/Include/Protocol/Gop3D.h"
    "include/gpu_isa.h:edk2/OptionRom/gpu_isa.h"
    "include/gpu_hw.h:edk2/OptionRom/gpu_hw.h"
    "include/vram.h:edk2/OptionRom/vram.h"
    "UEFI/target.txt:edk2/Conf/target.txt"
)

# If TestHarness directory exists on this branch, map it
if [ -d "$UEFI_DIR/TestHarness" ]; then
    EDK2_MAPPINGS+=("UEFI/TestHarness:edk2/TestHarness")
fi

for mapping in "${EDK2_MAPPINGS[@]}"; do
    src="${mapping%%:*}"
    dst="${mapping##*:}"
    create_symlink "$REPO_ROOT/$src" "$REPO_ROOT/$dst"
done

log_step "Building EDK2 BaseTools..."
make -C "$EDK2_DIR/BaseTools"

# --- Setup Compiler ---
log_step "Setting up Compiler headers..."
create_symlink "$INCLUDE_DIR/gpu_isa.h" "$COMPILER_DIR/gpu_isa.h"
create_symlink "$INCLUDE_DIR/gpu_hw.h"  "$COMPILER_DIR/gpu_hw.h"
create_symlink "$INCLUDE_DIR/vram.h"    "$COMPILER_DIR/vram.h"

log_success "Patches, code generation, and symlinks applied"