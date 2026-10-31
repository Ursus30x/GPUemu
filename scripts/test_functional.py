#!/usr/bin/env python3
"""
scripts/test_functional.py - Headless automated test runner for GPUemu TestHarness.

Executes the UEFI TestHarness inside QEMU headlessly and reports verdicts from serial.log.
"""

import argparse
import atexit
import os
import re
import shutil
import signal
import subprocess
import sys
from pathlib import Path

# ANSI Color Codes
COLOR_RESET = "\033[0m"
COLOR_BOLD = "\033[1m"
COLOR_RED = "\033[31m"
COLOR_GREEN = "\033[32m"
COLOR_YELLOW = "\033[33m"
COLOR_BLUE = "\033[34m"
COLOR_CYAN = "\033[36m"


def log_step(msg: str) -> None:
    print(f"{COLOR_BOLD}{COLOR_BLUE}==>{COLOR_RESET} {COLOR_BOLD}{msg}{COLOR_RESET}")


def log_info(msg: str) -> None:
    print(f"{COLOR_CYAN}[INFO]{COLOR_RESET} {msg}")


def log_warn(msg: str) -> None:
    print(f"{COLOR_YELLOW}[WARNING]{COLOR_RESET} {msg}")


def log_error(msg: str) -> None:
    print(f"{COLOR_RED}[ERROR]{COLOR_RESET} {msg}", file=sys.stderr)


def log_success(msg: str) -> None:
    print(f"{COLOR_GREEN}[SUCCESS]{COLOR_RESET} {msg}")


def find_ovmf_firmware() -> Path | None:
    """Locate OVMF x64 firmware binary across common system paths."""
    candidates = [
        Path("/usr/share/edk2-ovmf/x64/OVMF.4m.fd"),
        Path("/usr/share/ovmf/OVMF.fd"),
        Path("/usr/share/edk2/x64/OVMF.4m.fd"),
        Path("/usr/share/edk2/ovmf/OVMF.fd"),
    ]
    for path in candidates:
        if path.is_file():
            return path
    return None


def get_last_build_type(repo_root: Path) -> str:
    """Read .last_build_type marker file if present."""
    marker = repo_root / ".last_build_type"
    if marker.is_file():
        try:
            content = marker.read_text().strip().upper()
            if content in ("DEBUG", "RELEASE"):
                return content
        except OSError:
            pass
    return "RELEASE"


def parse_args() -> tuple[argparse.Namespace, list[str]]:
    parser = argparse.ArgumentParser(
        description="Headless functional test runner for GPUemu TestHarness.",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument(
        "build_type",
        nargs="?",
        choices=["DEBUG", "RELEASE", "debug", "release"],
        default=None,
        help="Build configuration type (defaults to last build or RELEASE).",
    )
    parser.add_argument(
        "--timeout",
        type=int,
        default=90,
        help="Maximum execution timeout in seconds.",
    )
    parser.add_argument(
        "--qemu-bin",
        type=Path,
        default=None,
        help="Explicit path to qemu-system-x86_64 binary.",
    )
    parser.add_argument(
        "--ovmf",
        type=Path,
        default=None,
        help="Explicit path to OVMF firmware file.",
    )

    args, extra_args = parser.parse_known_args()
    return args, extra_args


def check_disk(repo_root: Path, build_type: str) -> tuple[Path, str] | None:
    """Verify UEFI disk directory exists, falling back to alternate build type if needed."""
    disk_dir = repo_root / "edk2" / "Build" / "OvmfX64" / f"{build_type}_GCC" / "X64"
    if not disk_dir.is_dir():
        alt_type = "DEBUG" if build_type == "RELEASE" else "RELEASE"
        alt_dir = repo_root / "edk2" / "Build" / "OvmfX64" / f"{alt_type}_GCC" / "X64"
        if alt_dir.is_dir():
            log_warn(f"Disk directory for '{build_type}' not found, falling back to '{alt_type}'.")
            return alt_dir, alt_type

        log_error(f"UEFI disk directory not found: {disk_dir}")
        log_error(f"Please build EDK2 components first: ./scripts/build_edk2.sh {build_type}")
        return None

    return disk_dir, build_type


def verify_binaries(
    qemu_bin: Path,
    rom_file: Path,
    test_harness_bin: Path,
    ovmf_bios: Path | None,
    build_type: str,
) -> bool:
    """Verify that all required binary files and firmware exist."""
    if not qemu_bin.is_file():
        log_error(f"QEMU binary not found at: {qemu_bin}")
        log_error(f"Please build QEMU first: ./scripts/build_qemu.sh {build_type}")
        return False

    if not rom_file.is_file():
        log_error(f"Option ROM not found at: {rom_file}")
        log_error(f"Please build EDK2 first: ./scripts/build_edk2.sh {build_type}")
        return False

    target_harness = (
        test_harness_bin / "TestHarness.efi"
        if test_harness_bin.is_dir()
        else test_harness_bin
    )
    if not target_harness.is_file():
        log_error(f"TestHarness.efi not found at: {target_harness}")
        log_error(f"Please build EDK2 first: ./scripts/build_edk2.sh {build_type}")
        return False

    if not ovmf_bios or not ovmf_bios.is_file():
        log_error("OVMF firmware binary not found in system paths.")
        log_error("Please install the ovmf package (e.g. 'sudo apt-get install ovmf').")
        return False

    return True


def check_virt(kvm_path: Path = Path("/dev/kvm")) -> list[str]:
    """Check for KVM hardware virtualization availability and return QEMU accelerator arguments."""
    if kvm_path.exists() and os.access(kvm_path, os.R_OK | os.W_OK):
        log_info("Hardware virtualization (KVM) detected and enabled.")
        return ["-enable-kvm", "-cpu", "host"]

    log_warn("KVM not available or not writable. Falling back to TCG CPU emulation (-cpu max).")
    return ["-cpu", "max"]


def check_bootloader(disk_dir: Path) -> Path:
    """Ensure UEFI bootloader directory and BOOTX64.EFI exist."""
    efi_boot_dir = disk_dir / "EFI" / "BOOT"
    efi_boot_dir.mkdir(parents=True, exist_ok=True)
    shell_efi = disk_dir / "Shell.efi"
    boot_efi = efi_boot_dir / "BOOTX64.EFI"

    if shell_efi.is_file():
        shutil.copy2(shell_efi, boot_efi)
    elif not boot_efi.is_file():
        log_warn("UEFI Shell binary (Shell.efi) not found; BOOTX64.EFI may be missing.")

    return efi_boot_dir


def manage_startup(
    disk_dir: Path,
    extra_args: list[str] | str = "",
    efi_boot_dir: Path | None = None,
) -> None:
    """Manage startup.nsh generation, backup, and automatic restoration on exit."""
    if efi_boot_dir is None:
        efi_boot_dir = disk_dir / "EFI" / "BOOT"

    startup_file = disk_dir / "startup.nsh"
    boot_startup_file = efi_boot_dir / "startup.nsh"
    startup_backup = disk_dir / "startup.nsh.test_backup"

    if startup_file.is_file():
        shutil.copy2(startup_file, startup_backup)

    def restore_startup() -> None:
        if startup_backup.is_file():
            shutil.move(str(startup_backup), str(startup_file))
            if boot_startup_file.parent.is_dir():
                shutil.copy2(startup_file, boot_startup_file)

    # Register restoration of startup file on exit
    atexit.register(restore_startup)

    # Write test startup.nsh: executes TestHarness and shuts down via 'reset -s'
    extra_cmd = " ".join(extra_args) if isinstance(extra_args, list) else str(extra_args)
    startup_content = (
        "@echo -off\n"
        "FS0:\n"
        f"TestHarness.efi {extra_cmd}\n"
        "reset -s\n"
    )
    startup_file.write_text(startup_content)
    boot_startup_file.write_text(startup_content)


def run_qemu(
    qemu_bin: Path,
    ovmf_bios: Path,
    rom_file: Path,
    disk_dir: Path,
    serial_log: Path,
    debug_log: Path,
    accel_args: list[str],
    build_type: str,
    extra_cmd: str = "",
    timeout: int = 90,
) -> int:
    """Launch QEMU headlessly with UEFI firmware, option ROM, and virtual disk."""
    # Clean old logs
    if serial_log.exists():
        serial_log.unlink()
    if debug_log.exists():
        debug_log.unlink()

    # Configure temporary directory for QEMU vvfat block driver
    qemu_tmp = Path(os.environ.get("TMPDIR", "/tmp")) / "qemu_tmp"
    qemu_tmp.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    env["TMPDIR"] = str(qemu_tmp)

    log_step(f"Launching headless QEMU test runner ({build_type})...")
    if extra_cmd.strip():
        log_info(f"TestHarness arguments: {extra_cmd.strip()}")

    qemu_cmd = [
        str(qemu_bin),
        "-m", "4G",
        "-smp", "4",
        *accel_args,
        "-bios", str(ovmf_bios),
        "-device", f"AREK,romfile={rom_file},legacy_asm=off",
        "-display", "none",
        "-nodefaults",
        "-serial", f"file:{serial_log}",
        "-debugcon", f"file:{debug_log}",
        "-global", "isa-debugcon.iobase=0x402",
        "-drive", f"file=fat:rw:{disk_dir},format=raw,media=disk",
    ]

    try:
        proc = subprocess.run(qemu_cmd, env=env, timeout=timeout)
        return proc.returncode
    except subprocess.TimeoutExpired:
        log_error(f"QEMU execution timed out after {timeout} seconds!")
        return 124
    except Exception as e:
        log_error(f"Failed to run QEMU: {e}")
        return 1


def validate_output(serial_log: Path, qemu_exit_code: int = 0) -> bool:
    """Validate test results from QEMU serial output log."""
    # Verify serial.log exists
    if not serial_log.is_file():
        log_error(f"serial.log was not created. QEMU failed to initialize or execute (exit code: {qemu_exit_code}).")
        return False

    try:
        content = serial_log.read_text(encoding="utf-8", errors="ignore")
    except OSError as e:
        log_error(f"Failed to read serial.log: {e}")
        return False

    # Extract test harness block
    pattern = r"(=+\s+GPUemu Test Harness v1\.0.*?(?:OVERALL RESULT: (?:PASSED|FAILED)))"
    match = re.search(pattern, content, re.DOTALL)

    if match:
        # Filter terminal control codes (cursor jumps, clears), preserving ANSI colors
        cleaned = re.sub(r"\x1b\[[0-9;]*[A-HJKSTfhilmn]", lambda m: m.group(0) if m.group(0).endswith("m") else "", match.group(1))
        print(cleaned)

        if "OVERALL RESULT: PASSED" in cleaned:
            log_success("All tests passed successfully.")
            return True
        else:
            log_error("One or more tests failed.")
            return False
    else:
        log_error(f"Test harness did not complete normally or timed out (QEMU exit code: {qemu_exit_code})!")
        print("\n--- Recent serial.log output ---", file=sys.stderr)
        lines = content.splitlines()[-40:]
        for line in lines:
            cleaned_line = re.sub(r"\x1b\[[0-9;]*[a-zA-Z]", "", line)
            if cleaned_line.strip():
                print(f"  {cleaned_line}", file=sys.stderr)
        return False


def main() -> int:
    script_dir = Path(__file__).resolve().parent
    repo_root = script_dir.parent

    args, extra_args = parse_args()

    # Determine build type
    build_type = (args.build_type or get_last_build_type(repo_root)).upper()

    # Paths
    qemu_bin = args.qemu_bin or (repo_root / "qemu" / "build" / "qemu-system-x86_64")
    rom_file = repo_root / "edk2" / "Build" / "OptionRom.rom"
    serial_log = repo_root / "serial.log"
    debug_log = repo_root / "debug.log"

    # Disk check
    disk_info = check_disk(repo_root, build_type)
    if not disk_info:
        return 1
    disk_dir, build_type = disk_info

    ovmf_bios = args.ovmf or find_ovmf_firmware()
    test_harness_bin = disk_dir / "TestHarness.efi"

    # Validate binaries
    if not verify_binaries(qemu_bin, rom_file, test_harness_bin, ovmf_bios, build_type):
        return 1

    # Virtualization check (KVM vs TCG)
    accel_args = check_virt()

    # Bootloader check
    efi_boot_dir = check_bootloader(disk_dir)

    # Startup management
    extra_cmd = " ".join(extra_args)
    manage_startup(disk_dir, extra_cmd, efi_boot_dir)

    # Launch QEMU
    qemu_exit_code = run_qemu(
        qemu_bin=qemu_bin,
        ovmf_bios=ovmf_bios,
        rom_file=rom_file,
        disk_dir=disk_dir,
        serial_log=serial_log,
        debug_log=debug_log,
        accel_args=accel_args,
        build_type=build_type,
        extra_cmd=extra_cmd,
        timeout=args.timeout,
    )

    # Validate test results
    if not validate_output(serial_log, qemu_exit_code):
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
