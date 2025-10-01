#!/bin/bash

set -e

TARGETS=("chimera-host" "chimera-convolve" "chimera-open")

for TARGET in "${TARGETS[@]}"; do
    echo "Building target: $TARGET"
    VERBOSE=1 cmake -D TARGET_PLATFORM="$TARGET" \
        -D TOOLCHAIN_DIR=/app/install/llvm \
        -D SIMULATION_BACKEND=RTL \
        -B build-"$TARGET"
    cmake --build build-"$TARGET" -j -t clean
    cmake --build build-"$TARGET" -j
done