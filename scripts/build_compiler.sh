#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CWD="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$CWD/compiler"
make

mkdir -p ../tools
mv compiler ../tools/compiler