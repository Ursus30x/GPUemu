# Agent Instructions for GPUemu

This file provides context, tech stack details, build commands, and coding guidelines for AI development agents working on the GPUemu repository.

---

## 1. Project Context
GPUemu emulates a custom PCI GPU card (codename `AREK`, PCI Vendor `0x6969`, Device `0x2137`) under QEMU and exposes 2D GOP framebuffers and custom 3D drawing capabilities to EDK2/UEFI guest environments.
It features:
- A multithreaded software rasterizer running inside QEMU.
- A custom 4x4 matrix/scalar shader instruction set (ISA).
- A two-pass assembler/compiler to translate `.asm` shaders to binary formats.
- A UEFI OptionRom driver utilizing a buddy memory allocator for VRAM.
- UEFI benchmark and demonstration apps drawing 3D graphics in a virtual machine.

---

## 2. Tech Stack
- **QEMU Emulation (Host)**: C (QEMU APIs, POSIX Threads, QemuMutex/QemuCond).
- **UEFI Driver & Apps (Guest)**: C (EDK2 environment, UEFI Driver Model, UEFI Boot Services/Runtime Services).
- **Shader Compiler**: C (Standard C99).
- **Shader Code**: Custom GPU matrix shader assembly (documented in [doc/shaderSpec.md](doc/shaderSpec.md)).
- **Orchestration**: Bash scripts.

---

## 3. Core Development Commands

### 3.1 Initial Setup / Patches
Apply submodules configuration and setup symbolic links from core directories (`gpu/`, `UEFI/`, `include/`) to their patched locations:
```bash
./scripts/apply_patches.sh
```

### 3.2 Compilation
Build QEMU, EDK2, and the custom shader compiler. Default build type is `DEBUG`.
```bash
# Build all components
./scripts/build_all.sh [DEBUG|RELEASE]

# Build individual components
./scripts/build_qemu.sh [DEBUG|RELEASE]
./scripts/build_edk2.sh [DEBUG|RELEASE]
./scripts/build_compiler.sh
```

### 3.3 Running & Debugging
```bash
# Launch the QEMU machine running UEFI shell with the GPU emulation card
./scripts/run.sh

# Run QEMU with GDB/Monitor attachments
./scripts/debug.sh
```
*Note: Once inside the UEFI shell, run the DemoApp by typing `FS0:` and executing `DemoApp.efi`.*

---

## 4. Code Style & Design Rules

### 4.1 Submodule Integration (CRITICAL)
- **Do NOT commit directly inside submodule directories** (`qemu/` or `edk2/`).
- Write/edit code in the root directories: [gpu/](gpu/), [UEFI/](UEFI/), and [include/](include/).
- Use [scripts/apply_patches.sh](scripts/apply_patches.sh) to update files in submodules.
- If you edit configuration files in submodules (e.g., Meson build files or EDK2 .dsc/.dec configs), regenerate the respective patches:
  - QEMU Patch: Save changes as [gpu/qemu.patch](gpu/qemu.patch).
  - EDK2 Patch: Save changes as [UEFI/OvmfPkg.patch](UEFI/OvmfPkg.patch).

### 4.2 Shared Architecture & Headers
- Shared constants, registers offsets, and ISA definitions must reside in the [include/](include/) directory:
  - `gpu_hw.h` (Hardware MMIO registers offsets, IDs)
  - `gpu_isa.h` (Shader instructions opcodes, structures)
  - `vram.h` (Command ring buffer, data layout)
- Ensure changes in these headers are symlinked to `compiler/`, `gpu/`, and `UEFI/OptionRom/` via [scripts/apply_patches.sh](scripts/apply_patches.sh).

### 4.3 Threading & Synchronization (Host Renderer)
- The software rasterizer ([gpu/renderer.c](gpu/renderer.c)) processes vertex shading and band rasterization concurrently.
- Vertex Shader virtualization: To prevent registry corruption, copy `GpuState` onto the thread's local stack during `exec_shader` execution.
- Scanline bands: Divide the screen vertically into non-overlapping horizontal bands. Ensure worker threads write strictly within their assigned boundaries to maintain lock-free and atomic-free operations.

### 4.4 VRAM & Memory Allocator (Guest Driver)
- VRAM (32 MB) is managed via a buddy allocator in [UEFI/OptionRom/gpu_memory.c](UEFI/OptionRom/gpu_memory.c).
- Allocate VRAM via `GpuAllocateMem(Size, Tag)` and free it using `GpuFreeMem(Addr)`. Avoid hardcoded VRAM address offsets.

### 4.5 UEFI Coding Standard
- Follow EDK2 coding conventions (e.g., `EFIAPI`, `IN`, `OUT`, `STATIC`, CamelCase naming, and EDK2-defined primitive types like `UINT32`, `CHAR8`, `EFI_STATUS`) when modifying files under [UEFI/](UEFI/).

---

## 5. References & Documentation
Before making architectural or compiler updates, consult:
- GPU Specification: [doc/gpuSpec.md](doc/gpuSpec.md)
- Shader ISA Reference: [doc/shaderSpec.md](doc/shaderSpec.md)
- Rendering Pipeline Architecture: [doc/renderThreads.md](doc/renderThreads.md)
- Compiler Specification: [doc/compilerSpec.md](doc/compilerSpec.md)
