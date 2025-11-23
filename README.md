# GPUemu

### What is this project?

This is a uni project for System Firmware (Oprogramowanie Systemowe) course, that targets to emulate a PCI device in UEFI using QEMU. It's used as a learning tool to understand how PCI devices work and some quirks of creating a valid PCI devices.

This GPU will present 2 modes, as a GPU passthrough for VGA and a simple 3D renderer.

This project could be expanded upon to accept kernel code and implement more complex grahpics pipelines.

## Building

Since most of this project relies on modification od QEMU and EDK2 projetcs, instead of creating two diffrent forks and mixing our code with their relative sources, we've decided to write our code as patches for both QEMU and EDK2.

Here are the instructions:
- Make sure that all submodules are downloaded and uptodate
- Run `scripts/apply_patches.sh` 
- Run `scripts/build.sh`

## Running

`scripts/run.sh`
