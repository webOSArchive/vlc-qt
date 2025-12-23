#!/bin/bash
# libVLC Cross-Compilation Build Script for webOS ARMv7
# Non-interactive version for automated builds

set -e

echo "=== libVLC webOS ARM Build (Automated) ==="
echo "Started at: $(date)"
echo ""

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_BASE="${SCRIPT_DIR}/vlc-build"
INSTALL_PREFIX="${SCRIPT_DIR}/vlc-arm"
SOURCES_DIR="${BUILD_BASE}/sources"

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

# VLC version to build (2.2.x is more compatible with older systems)
VLC_VERSION="2.2.8"
VLC_URL="https://download.videolan.org/vlc/${VLC_VERSION}/vlc-${VLC_VERSION}.tar.xz"

# FFmpeg version (2.8.x is compatible with VLC 2.2.x - libavutil 54)
FFMPEG_VERSION="2.8.22"
FFMPEG_URL="https://ffmpeg.org/releases/ffmpeg-${FFMPEG_VERSION}.tar.xz"

# Log file
LOG_FILE="${BUILD_BASE}/build.log"

echo "Configuration:"
echo "  VLC Version: ${VLC_VERSION}"
echo "  FFmpeg Version: ${FFMPEG_VERSION}"
echo "  Install Prefix: ${INSTALL_PREFIX}"
echo "  Toolchain: ${TOOLCHAIN_ROOT}"
echo "  Log file: ${LOG_FILE}"
echo ""

# Check toolchain
if [ ! -f "${CC}" ]; then
    echo "ERROR: Cross-compiler not found at ${CC}"
    exit 1
fi

# Create directories
mkdir -p "${SOURCES_DIR}" "${INSTALL_PREFIX}"
exec > >(tee -a "${LOG_FILE}") 2>&1

# Export cross-compilation environment
export PATH="${TOOLCHAIN_ROOT}/bin:${PATH}"
export CC="${CC}"
export CXX="${CXX}"
export AR="${TOOLCHAIN_ROOT}/bin/${CROSS_PREFIX}-ar"
export RANLIB="${TOOLCHAIN_ROOT}/bin/${CROSS_PREFIX}-ranlib"
export STRIP="${TOOLCHAIN_ROOT}/bin/${CROSS_PREFIX}-strip"
export PKG_CONFIG_PATH="${INSTALL_PREFIX}/lib/pkgconfig:${SYSROOT_PATH}/lib/pkgconfig"
export PKG_CONFIG_LIBDIR="${INSTALL_PREFIX}/lib/pkgconfig:${SYSROOT_PATH}/lib/pkgconfig"

export CFLAGS="${ARM_CFLAGS} -I${INSTALL_PREFIX}/include -I${DEVICE_PATH}/include -I${SYSROOT_PATH}/include -I/opt/PalmPDK/include"
export CXXFLAGS="${CFLAGS}"
export LDFLAGS="-L${INSTALL_PREFIX}/lib -L${DEVICE_PATH}/lib -L${SYSROOT_PATH}/lib -L/opt/PalmPDK/device/lib"

#############################################################################
# Step 1: Build FFmpeg
#############################################################################
build_ffmpeg() {
    echo ""
    echo "=== Building FFmpeg ${FFMPEG_VERSION} ==="
    echo "Started at: $(date)"
    echo ""

    cd "${SOURCES_DIR}"

    if [ ! -f "ffmpeg-${FFMPEG_VERSION}.tar.xz" ]; then
        echo "Downloading FFmpeg..."
        wget --no-verbose "${FFMPEG_URL}"
    fi

    if [ ! -d "ffmpeg-${FFMPEG_VERSION}" ]; then
        echo "Extracting FFmpeg..."
        tar xf "ffmpeg-${FFMPEG_VERSION}.tar.xz"
    fi

    cd "ffmpeg-${FFMPEG_VERSION}"

    # Clean previous build if exists
    rm -rf build-arm
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
        --disable-stripping \
        --extra-cflags="${CFLAGS}" \
        --extra-ldflags="${LDFLAGS}"

    echo "Building FFmpeg (this may take 15-30 minutes)..."
    make -j$(nproc)

    echo "Installing FFmpeg..."
    make install

    echo "FFmpeg build complete at: $(date)"
}

#############################################################################
# Step 2: Build VLC
#############################################################################
build_vlc() {
    echo ""
    echo "=== Building VLC ${VLC_VERSION} ==="
    echo "Started at: $(date)"
    echo ""

    cd "${SOURCES_DIR}"

    if [ ! -f "vlc-${VLC_VERSION}.tar.xz" ]; then
        echo "Downloading VLC..."
        wget --no-verbose "${VLC_URL}"
    fi

    if [ ! -d "vlc-${VLC_VERSION}" ]; then
        echo "Extracting VLC..."
        tar xf "vlc-${VLC_VERSION}.tar.xz"
    fi

    cd "vlc-${VLC_VERSION}"

    # Clean previous build if exists
    rm -rf build-arm
    mkdir -p build-arm && cd build-arm

    # Update PKG_CONFIG_PATH to include FFmpeg
    export PKG_CONFIG_PATH="${INSTALL_PREFIX}/lib/pkgconfig:${PKG_CONFIG_PATH}"

    echo "Configuring VLC..."
    echo "PKG_CONFIG_PATH=${PKG_CONFIG_PATH}"

    # VLC 2.2.x configure options
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
        --disable-macosx \
        --disable-macosx-dialog-provider \
        --disable-x11 \
        --disable-xcb \
        --disable-xvideo \
        --disable-glx \
        --disable-wayland \
        --disable-sdl \
        --disable-sdl-image \
        --disable-pulse \
        --disable-jack \
        --disable-dbus \
        --disable-gnutls \
        --disable-taglib \
        --disable-nls \
        --disable-notify \
        --disable-libgcrypt \
        --disable-update-check \
        --disable-vlm \
        --disable-addonmanagermodules \
        --disable-live555 \
        --disable-dc1394 \
        --disable-dv1394 \
        --disable-linsys \
        --disable-dvdread \
        --disable-dvdnav \
        --disable-bluray \
        --disable-opencv \
        --disable-smbclient \
        --disable-sftp \
        --disable-v4l2 \
        --disable-decklink \
        --disable-vcd \
        --disable-libcddb \
        --disable-screen \
        --disable-vnc \
        --disable-freerdp \
        --disable-realrtsp \
        --disable-asdcp \
        --disable-dvbpsi \
        --disable-gme \
        --disable-sid \
        --disable-ogg \
        --disable-shout \
        --disable-mod \
        --disable-mpc \
        --disable-mad \
        --disable-mpg123 \
        --disable-gst-decode \
        --disable-merge-ffmpeg \
        --disable-faad \
        --disable-flac \
        --disable-twolame \
        --disable-quicktime \
        --disable-dca \
        --disable-fluidsynth \
        --disable-zvbi \
        --disable-telx \
        --disable-libass \
        --disable-kate \
        --disable-tiger \
        --disable-css \
        --disable-jpeg \
        \
        --enable-alsa \
        --disable-oss \
        --enable-avcodec \
        --enable-avformat \
        --enable-swscale \
        --enable-postproc

    echo "Building VLC (this may take 30-60 minutes)..."
    make -j$(nproc)

    echo "Installing VLC..."
    make install

    echo "VLC build complete at: $(date)"
}

#############################################################################
# Main
#############################################################################

echo "Build started at: $(date)"
echo ""

# Build steps
# Skip FFmpeg if already built
if [ -f "${INSTALL_PREFIX}/lib/libavcodec.so" ]; then
    echo "FFmpeg already built, skipping..."
else
    build_ffmpeg
fi

build_vlc

echo ""
echo "=== Build Complete ==="
echo "Finished at: $(date)"
echo ""
echo "libVLC installed to: ${INSTALL_PREFIX}"
echo ""
echo "Contents:"
ls -la "${INSTALL_PREFIX}/lib/"*.so* 2>/dev/null | head -20 || echo "  (no .so files found)"
echo ""
echo "Plugins:"
ls -la "${INSTALL_PREFIX}/lib/vlc/plugins/" 2>/dev/null | head -10 || echo "  (no plugins found)"
