#!/bin/bash

# Build the 3BX compiler
# Usage: ./build.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"

# Create build directory if needed
mkdir -p "$BUILD_DIR"

# Configure if needed
if [ ! -f "$BUILD_DIR/Makefile" ]; then
    echo "Configuring build..."
    cd "$BUILD_DIR"
    cmake ..
    cd "$SCRIPT_DIR"
fi

# Build
echo "Building 3bx compiler..."
cd "$BUILD_DIR"
make -j$(nproc)

echo "Build complete."
