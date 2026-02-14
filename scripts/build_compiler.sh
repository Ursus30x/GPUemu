#!/bin/bash
set -e # Exit immediately if a command exits with a non-zero status

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CWD="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$CWD/compiler"
make

mkdir -p ../tools
mv compiler ../tools/compiler