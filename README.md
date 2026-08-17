# GPUemu

![Icon](GPUemuIcon.png)

### What is this project?

This started as a university project for the System Firmware (*Oprogramowanie Systemowe*) course, intended to emulate a simple PCI device in UEFI using QEMU. The scope eventually expanded to a full GPU emulation, covering everything from PCI communication to shader assembly code, VRAM management, and a graphics API. It is mainly used as a learning tool to understand how GPUs work from the ground up.

Currently, the GPU consists of a simple UEFI Driver, a shader assembler, and a few UEFI benchmark apps to demonstrate functionality.

This project could be extended in many ways, and the final product is not yet fully defined.

## Building

Since most of this project relies on modifying QEMU and EDK2 projects, we decided to avoid creating two different forks. Instead, we write our code as patches for both QEMU and EDK2 sources.

Here are the instructions:
-   Download needed packages:
    -   For Ubuntu/Debian:
    ``` sh
    apt install make g++ gcc python3 uuid-dev nasm python3-pip python3-venv ninja-build libglib2.0-dev cmake libpixman-1-dev libgtk-3-dev
    ```

-   Run `scripts/apply_patches.sh`
    -   This prepares submodules, applies patches to them, and symlinks directories to their respective submodules.

-   Run `scripts/build_all.sh`
    -   This builds both QEMU and EDK2. The default build type is `DEBUG`. You can use different build types by passing the name as a parameter (currently supports only `RELEASE`).

## Running

To run the UEFI Shell using our card, run `scripts/run.sh`.

This launches QEMU with our device as a video output and links the `Build` directory of EDK2 with our benchmark apps.

You can enter this directory from the UEFI shell by typing `FS0:` in the UEFI shell terminal.

<img width="640" height="480" alt="gpu-test" src="https://github.com/user-attachments/assets/7f997a02-8cee-4be2-8ce5-7e9d8b3ab094" />

## Legacy ASM Mode

The GPU supports two shader execution backends. The default path JIT-compiles SPIR-V shaders into native code. The **legacy ASM** path instead interprets shaders written in the GPU's custom ISA (see `compiler/`). To enable it, set `legacy_asm=on` on the device flag in `scripts/run.sh`:

```
-device AREK,...,legacy_asm=on
```

The `LegacyAsmApp` UEFI application (`UEFI/LegacyAsmApp/`) demonstrates this mode with hand-assembled vertex and fragment shaders.

## Documentation

Architectural specifications and choices are documented in `/docs`.

APIs, the Driver, and the GPU are documented via comments within the code itself.
