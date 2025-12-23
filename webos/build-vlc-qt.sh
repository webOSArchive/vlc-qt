#!/bin/bash
# VLC-Qt webOS Cross-Compilation Build Script
# Builds VLC-Qt libraries for ARMv7 webOS (HP TouchPad)

set -e

echo "=== VLC-Qt webOS Build ==="
echo ""

# Paths
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="${SCRIPT_DIR}/.."
BUILD_DIR="${PROJECT_ROOT}/build-webos"
TOOLCHAIN_FILE="${SCRIPT_DIR}/cmake/webos-arm-toolchain.cmake"

# libVLC paths - UPDATE THESE after building/acquiring libVLC for ARM
LIBVLC_ARM_PATH="${SCRIPT_DIR}/vlc-arm"

# Check prerequisites
echo "Checking prerequisites..."

if [ ! -f "${TOOLCHAIN_FILE}" ]; then
    echo "ERROR: Toolchain file not found at ${TOOLCHAIN_FILE}"
    exit 1
fi

if [ ! -d "/opt/gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabi" ]; then
    echo "ERROR: Linaro GCC toolchain not found"
    echo "Please install gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabi to /opt/"
    exit 1
fi

if [ ! -d "${SCRIPT_DIR}/sdk/qt5-arm" ]; then
    echo "ERROR: Qt5 ARM SDK not found at ${SCRIPT_DIR}/sdk/qt5-arm"
    exit 1
fi

# Check for libVLC
if [ ! -d "${LIBVLC_ARM_PATH}" ]; then
    echo ""
    echo "WARNING: libVLC for ARM not found at ${LIBVLC_ARM_PATH}"
    echo ""
    echo "You need to either:"
    echo "  1. Build VLC from source for ARMv7 (see build-libvlc.sh)"
    echo "  2. Extract pre-built libVLC ARM binaries to ${LIBVLC_ARM_PATH}"
    echo ""
    echo "Expected structure:"
    echo "  ${LIBVLC_ARM_PATH}/"
    echo "    include/"
    echo "      vlc/"
    echo "        vlc.h, libvlc.h, etc."
    echo "    lib/"
    echo "      libvlc.so"
    echo "      libvlccore.so"
    echo "    plugins/"
    echo "      ... VLC plugins ..."
    echo ""
    echo "Run with --skip-vlc-check to proceed anyway (will fail at link time)"

    if [ "$1" != "--skip-vlc-check" ]; then
        exit 1
    fi
fi

echo "  Toolchain: ${TOOLCHAIN_FILE}"
echo "  Build dir: ${BUILD_DIR}"
echo "  libVLC: ${LIBVLC_ARM_PATH}"
echo ""

# Create build directory
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

echo "Configuring with CMake..."
echo ""

# CMake configuration
cmake "${PROJECT_ROOT}" \
    -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN_FILE}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="${SCRIPT_DIR}/install" \
    -DLIBVLC_INCLUDE_DIR="${LIBVLC_ARM_PATH}/include" \
    -DLIBVLC_LIBRARY="${LIBVLC_ARM_PATH}/lib/libvlc.so" \
    -DLIBVLCCORE_LIBRARY="${LIBVLC_ARM_PATH}/lib/libvlccore.so" \
    -DLIBVLC_VERSION=0x020200 \
    -DSTATIC=OFF \
    -DWITH_X11=OFF \
    -DQT_VERSION=5 \
    "$@"

echo ""
echo "Building..."
echo ""

make -j$(nproc) 2>&1 | tee build.log

echo ""
echo "Checking built libraries..."
echo ""

if [ -f "src/core/libVLCQtCore.so" ]; then
    echo "VLCQtCore:"
    file src/core/libVLCQtCore.so
    echo ""
fi

if [ -f "src/widgets/libVLCQtWidgets.so" ]; then
    echo "VLCQtWidgets:"
    file src/widgets/libVLCQtWidgets.so
    echo ""
fi

echo "Build complete!"
echo ""
echo "To install: cd ${BUILD_DIR} && make install"
