#!/bin/bash
# scripts/common.sh - Shared definitions and helper functions for GPUemu

set -e # Exit immediately on error

# Resolve repository root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Shared directories
QEMU_DIR="$REPO_ROOT/qemu"
EDK2_DIR="$REPO_ROOT/edk2"
GPU_DIR="$REPO_ROOT/gpu"
UEFI_DIR="$REPO_ROOT/UEFI"
INCLUDE_DIR="$REPO_ROOT/include"
COMPILER_DIR="$REPO_ROOT/compiler"
TOOLS_DIR="$REPO_ROOT/tools"

# Shared marker file for tracking the active build configuration
MARKER_FILE="$REPO_ROOT/.last_build_type"

# Terminal formatting
if [ -t 1 ]; then
    COLOR_RESET="\033[0m"
    COLOR_BOLD="\033[1m"
    COLOR_RED="\033[31m"
    COLOR_GREEN="\033[32m"
    COLOR_YELLOW="\033[33m"
    COLOR_BLUE="\033[34m"
    COLOR_CYAN="\033[36m"
else
    COLOR_RESET=""
    COLOR_BOLD=""
    COLOR_RED=""
    COLOR_GREEN=""
    COLOR_YELLOW=""
    COLOR_BLUE=""
    COLOR_CYAN=""
fi

# Logging helpers
log_step()    { echo -e "${COLOR_BOLD}${COLOR_BLUE}==>${COLOR_RESET} ${COLOR_BOLD}$*${COLOR_RESET}"; }
log_info()    { echo -e "${COLOR_CYAN}[INFO]${COLOR_RESET} $*"; }
log_success() { echo -e "${COLOR_GREEN}[SUCCESS]${COLOR_RESET} $*"; }
log_warn()    { echo -e "${COLOR_YELLOW}[WARNING]${COLOR_RESET} $*"; }
log_error()   { echo -e "${COLOR_RED}[ERROR]${COLOR_RESET} $*" >&2; }

# Read the last recorded build type from the marker file (trimmed, normalized)
get_last_build_type() {
    if [ -f "$MARKER_FILE" ]; then
        local raw
        raw="$(tr -d '[:space:]' < "$MARKER_FILE" | tr '[:lower:]' '[:upper:]')"
        case "$raw" in
            RELEASE) echo "RELEASE" ;;
            DEBUG)   echo "DEBUG" ;;
            *)       echo "DEBUG" ;;
        esac
    else
        echo "DEBUG"
    fi
}

# Record the active build type into the shared marker file
set_build_type() {
    local type="${1^^}"
    case "$type" in
        RELEASE) echo "RELEASE" > "$MARKER_FILE" ;;
        *)       echo "DEBUG" > "$MARKER_FILE" ;;
    esac
}

# Parse and normalize build type argument (defaults to last used, or DEBUG)
parse_build_type() {
    local type="${1:-}"
    if [ -z "$type" ]; then
        get_last_build_type
    else
        case "${type^^}" in
            RELEASE) echo "RELEASE" ;;
            DEBUG|*) echo "DEBUG" ;;
        esac
    fi
}

# Safely create a symbolic link (ensures parent dir exists, replaces existing link/file)
create_symlink() {
    local src="$1"
    local dst="$2"

    if [ ! -e "$src" ] && [ ! -d "$src" ]; then
        log_error "Symlink source not found: $src"
        return 1
    fi

    # Skip if already pointing to the correct source
    if [ -L "$dst" ] && [ "$(readlink "$dst")" = "$src" ]; then
        return 0
    fi

    mkdir -p "$(dirname "$dst")"
    if [ -L "$dst" ] || [ -f "$dst" ]; then
        rm -f "$dst"
    elif [ -d "$dst" ]; then
        rmdir "$dst" 2>/dev/null || rm -f "$dst"
    fi
    ln -sf "$src" "$dst"
}
