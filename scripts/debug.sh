#!/bin/bash
# scripts/debug.sh - Run QEMU with GDB attached for debugging GPUemu

set -e
source "$(dirname "$0")/common.sh"

CMD_TO_RUN=""
if [ -n "$1" ]; then
    if [ "$1" = "DEBUG" ] || [ "$1" = "RELEASE" ]; then
        LAST_TYPE="$1"
        shift
        CMD_TO_RUN="$*"
    else
        LAST_TYPE="$(get_last_build_type)"
        CMD_TO_RUN="$*"
    fi
else
    LAST_TYPE="$(get_last_build_type)"
fi

QEMU_BIN="$QEMU_DIR/build/qemu-system-x86_64"
ROM_FILE="$EDK2_DIR/Build/OptionRom.rom"
DISK_DIR="$EDK2_DIR/Build/OvmfX64/${LAST_TYPE}_GCC/X64"

# Check if QEMU binary exists
if [ ! -f "$QEMU_BIN" ]; then
    log_error "QEMU binary not found at $QEMU_BIN. Please run: ./scripts/build_qemu.sh $LAST_TYPE"
    exit 1
fi

# Check if Option ROM exists
if [ ! -f "$ROM_FILE" ]; then
    log_error "Option ROM not found at $ROM_FILE. Please run: ./scripts/build_edk2.sh $LAST_TYPE"
    exit 1
fi

# Check if the UEFI disk directory exists for the selected build type; fallback if alternative exists
if [ ! -d "$DISK_DIR" ]; then
    ALT_TYPE="DEBUG"
    if [ "$LAST_TYPE" = "DEBUG" ]; then
        ALT_TYPE="RELEASE"
    fi
    ALT_DIR="$EDK2_DIR/Build/OvmfX64/${ALT_TYPE}_GCC/X64"

    if [ -d "$ALT_DIR" ]; then
        log_warn "Disk directory for '$LAST_TYPE' ($DISK_DIR) not found, but '$ALT_TYPE' exists."
        log_warn "Automatically switching to '$ALT_TYPE' build directory."
        LAST_TYPE="$ALT_TYPE"
        DISK_DIR="$ALT_DIR"
        set_build_type "$LAST_TYPE"
    else
        log_error "UEFI application directory not found: $DISK_DIR"
        log_error "Please run: ./scripts/build_edk2.sh $LAST_TYPE"
        exit 1
    fi
fi

log_step "Starting GDB for QEMU (GPUemu $LAST_TYPE)..."
log_info "UEFI Shell drive mapped to: $DISK_DIR"

# Ensure EFI bootloader exists to boot directly into UEFI Shell without BDS boot menu
mkdir -p "$DISK_DIR/EFI/BOOT"
if [ -f "$DISK_DIR/Shell.efi" ]; then
    cp -f "$DISK_DIR/Shell.efi" "$DISK_DIR/EFI/BOOT/BOOTX64.EFI"
elif [ -f "$EDK2_DIR/Build/OvmfX64/DEBUG_GCC/X64/Shell.efi" ]; then
    cp -f "$EDK2_DIR/Build/OvmfX64/DEBUG_GCC/X64/Shell.efi" "$DISK_DIR/EFI/BOOT/BOOTX64.EFI"
elif [ -f "$EDK2_DIR/Build/OvmfX64/RELEASE_GCC/X64/Shell.efi" ]; then
    cp -f "$EDK2_DIR/Build/OvmfX64/RELEASE_GCC/X64/Shell.efi" "$DISK_DIR/EFI/BOOT/BOOTX64.EFI"
fi

# Ensure startup.nsh switches to FS0: and optionally runs the requested command
write_startup_nsh() {
    local target="$1"
    if [ -n "$CMD_TO_RUN" ]; then
        cat << EOF > "$target"
@echo -off
FS0:
$CMD_TO_RUN
EOF
    else
        cat << 'EOF' > "$target"
@echo -off
FS0:
EOF
    fi
}

if [ -n "$CMD_TO_RUN" ]; then
    log_info "Configured startup.nsh to auto-run: $CMD_TO_RUN"
fi
write_startup_nsh "$DISK_DIR/startup.nsh"
write_startup_nsh "$DISK_DIR/EFI/BOOT/startup.nsh"

gdb -ex "handle SIGUSR1 noprint nostop pass" \
    -ex "handle SIGUSR2 noprint nostop pass" \
    --args "$QEMU_BIN" \
    -m 4G \
    -smp 4 \
    -cpu host \
    -enable-kvm \
    -bios /usr/share/edk2-ovmf/x64/OVMF.4m.fd \
    -device AREK,romfile="$ROM_FILE" \
    -monitor stdio \
    -nodefaults \
    -serial file:"$REPO_ROOT/serial.log" \
    -debugcon file:"$REPO_ROOT/debug.log" \
    -global isa-debugcon.iobase=0x402 \
    -drive file=fat:rw:"$DISK_DIR",format=raw,media=disk
