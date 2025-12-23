#!/bin/bash
# libVLC Cross-Compilation Build Script for webOS ARMv7
#
# This script builds libVLC and its dependencies for the HP TouchPad.
# It's a complex multi-step process that requires several hours and ~10GB disk space.

set -e

echo "=== libVLC webOS ARM Build ==="
echo ""

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_BASE="${SCRIPT_DIR}/vlc-build"
INSTALL_PREFIX="${SCRIPT_DIR}/vlc-arm"
SOURCES_DIR="${BUILD_BASE}/sources"
BUILD_DIR="${BUILD_BASE}/build"

# Toolchain
TOOLCHAIN_ROOT="/opt/gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabi"
CROSS_PREFIX="arm-linux-gnueabi"
CC="${TOOLCHAIN_ROOT}/bin/${CROSS_PREFIX}-gcc"
CXX="${TOOLCHAIN_ROOT}/bin/${CROSS_PREFIX}-g++"

# ARM flags
ARM_CFLAGS="-march=armv7-a -mtune=cortex-a8 -mfpu=neon -mfloat-abi=softfp -O2"

# Device libraries from webOS SDK
DEVICE_PATH="${SCRIPT_DIR}/device"
SYSROOT_PATH="${DEVICE_PATH}/sysroot/usr"

# VLC version to build (2.2.x recommended for stability, 3.0.x for newer features)
VLC_VERSION="2.2.8"
VLC_URL="https://download.videolan.org/vlc/${VLC_VERSION}/vlc-${VLC_VERSION}.tar.xz"

# FFmpeg version (critical dependency)
FFMPEG_VERSION="3.4.13"
FFMPEG_URL="https://ffmpeg.org/releases/ffmpeg-${FFMPEG_VERSION}.tar.xz"

echo "Configuration:"
echo "  VLC Version: ${VLC_VERSION}"
echo "  FFmpeg Version: ${FFMPEG_VERSION}"
echo "  Install Prefix: ${INSTALL_PREFIX}"
echo "  Toolchain: ${TOOLCHAIN_ROOT}"
echo ""

# Check toolchain
if [ ! -f "${CC}" ]; then
    echo "ERROR: Cross-compiler not found at ${CC}"
    echo "Please install Linaro GCC 4.9.4 to /opt/"
    exit 1
fi

# Create directories
mkdir -p "${SOURCES_DIR}" "${BUILD_DIR}" "${INSTALL_PREFIX}"

# Export cross-compilation environment
export PATH="${TOOLCHAIN_ROOT}/bin:${PATH}"
export CC="${CC}"
export CXX="${CXX}"
export AR="${TOOLCHAIN_ROOT}/bin/${CROSS_PREFIX}-ar"
export RANLIB="${TOOLCHAIN_ROOT}/bin/${CROSS_PREFIX}-ranlib"
export STRIP="${TOOLCHAIN_ROOT}/bin/${CROSS_PREFIX}-strip"
export PKG_CONFIG_PATH="${INSTALL_PREFIX}/lib/pkgconfig:${SYSROOT_PATH}/lib/pkgconfig"
export PKG_CONFIG_LIBDIR="${INSTALL_PREFIX}/lib/pkgconfig"

export CFLAGS="${ARM_CFLAGS} -I${INSTALL_PREFIX}/include -I${DEVICE_PATH}/include -I${SYSROOT_PATH}/include"
export CXXFLAGS="${CFLAGS}"
export LDFLAGS="-L${INSTALL_PREFIX}/lib -L${DEVICE_PATH}/lib -L${SYSROOT_PATH}/lib"

#############################################################################
# Step 1: Build FFmpeg (VLC's primary decoder)
#############################################################################
build_ffmpeg() {
    echo ""
    echo "=== Building FFmpeg ${FFMPEG_VERSION} ==="
    echo ""

    cd "${SOURCES_DIR}"

    if [ ! -f "ffmpeg-${FFMPEG_VERSION}.tar.xz" ]; then
        echo "Downloading FFmpeg..."
        wget "${FFMPEG_URL}"
    fi

    if [ ! -d "ffmpeg-${FFMPEG_VERSION}" ]; then
        echo "Extracting FFmpeg..."
        tar xf "ffmpeg-${FFMPEG_VERSION}.tar.xz"
    fi

    cd "ffmpeg-${FFMPEG_VERSION}"
    mkdir -p build-arm && cd build-arm

    echo "Configuring FFmpeg..."
    ../configure \
        --prefix="${INSTALL_PREFIX}" \
        --cross-prefix="${CROSS_PREFIX}-" \
        --arch=arm \
        --cpu=cortex-a8 \
        --target-os=linux \
        --enable-cross-compile \
        --enable-shared \
        --disable-static \
        --enable-gpl \
        --enable-neon \
        --enable-pic \
        --disable-doc \
        --disable-htmlpages \
        --disable-manpages \
        --disable-podpages \
        --disable-txtpages \
        --disable-programs \
        --disable-debug \
        --extra-cflags="${CFLAGS}" \
        --extra-ldflags="${LDFLAGS}"

    echo "Building FFmpeg..."
    make -j$(nproc)
    make install

    echo "FFmpeg build complete!"
}

#############################################################################
# Step 2: Build VLC
#############################################################################
build_vlc() {
    echo ""
    echo "=== Building VLC ${VLC_VERSION} ==="
    echo ""

    cd "${SOURCES_DIR}"

    if [ ! -f "vlc-${VLC_VERSION}.tar.xz" ]; then
        echo "Downloading VLC..."
        wget "${VLC_URL}"
    fi

    if [ ! -d "vlc-${VLC_VERSION}" ]; then
        echo "Extracting VLC..."
        tar xf "vlc-${VLC_VERSION}.tar.xz"
    fi

    cd "vlc-${VLC_VERSION}"
    mkdir -p build-arm && cd build-arm

    # Update PKG_CONFIG_PATH to include FFmpeg
    export PKG_CONFIG_PATH="${INSTALL_PREFIX}/lib/pkgconfig:${PKG_CONFIG_PATH}"

    echo "Configuring VLC..."
    ../configure \
        --host="${CROSS_PREFIX}" \
        --prefix="${INSTALL_PREFIX}" \
        --enable-shared \
        --disable-static \
        --disable-debug \
        --disable-maintainer-mode \
        --enable-run-as-root \
        --enable-optimizations \
        --enable-neon \
        --with-pic \
        \
        --disable-a52 \
        --disable-lua \
        --disable-vlc \
        --disable-skins2 \
        --disable-qt \
        --disable-x11 \
        --disable-xcb \
        --disable-xvideo \
        --disable-glx \
        --disable-wayland \
        --disable-pulse \
        --disable-jack \
        --disable-dbus \
        --disable-gnutls \
        --disable-taglib \
        --disable-nls \
        \
        --enable-alsa \
        --enable-gles2 \
        --enable-avcodec \
        --enable-avformat \
        --enable-swscale \
        --enable-postproc

    echo "Building VLC..."
    make -j$(nproc)
    make install

    echo "VLC build complete!"
}

#############################################################################
# Main
#############################################################################

echo "This script will build libVLC for webOS ARMv7."
echo ""
echo "Steps:"
echo "  1. Build FFmpeg (audio/video codecs)"
echo "  2. Build VLC (media framework)"
echo ""
echo "This may take 1-2 hours depending on your system."
echo ""
read -p "Continue? [y/N] " -n 1 -r
echo ""

if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "Aborted."
    exit 1
fi

# Check for zlib (required)
if [ ! -f "${SYSROOT_PATH}/lib/libz.so" ]; then
    echo "WARNING: zlib not found in sysroot. FFmpeg may fail to build."
    echo "You may need to cross-compile zlib first."
fi

build_ffmpeg
build_vlc

echo ""
echo "=== Build Complete ==="
echo ""
echo "libVLC installed to: ${INSTALL_PREFIX}"
echo ""
echo "Contents:"
ls -la "${INSTALL_PREFIX}/lib/"*.so* 2>/dev/null || echo "  (no .so files found)"
echo ""
echo "Next step: Run build-vlc-qt.sh to build VLC-Qt"
