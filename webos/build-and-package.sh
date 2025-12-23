#!/bin/bash
# Build VLC Player app and package for webOS
# Usage: ./build-and-package.sh [clean]

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
APP_DIR="${SCRIPT_DIR}/app"
BUILD_DIR="${APP_DIR}/build"
TOOLCHAIN="${SCRIPT_DIR}/cmake/webos-arm-toolchain.cmake"

echo "=== VLC Player Build Script ==="
echo ""

# Check for clean parameter
if [ "$1" = "clean" ]; then
    echo "Performing clean build..."
    rm -rf "${BUILD_DIR}"
fi

# Create build directory if needed
if [ ! -d "${BUILD_DIR}" ]; then
    echo "Creating build directory..."
    mkdir -p "${BUILD_DIR}"
fi

# Configure if needed
if [ ! -f "${BUILD_DIR}/Makefile" ]; then
    echo "Configuring with CMake..."
    cd "${BUILD_DIR}"
    cmake .. -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN}"
fi

# Build
echo "Building app..."
cd "${BUILD_DIR}"
make -j$(nproc)

echo ""
echo "Build complete. Packaging..."
echo ""

# Package
cd "${SCRIPT_DIR}"
./package-ipk.sh
