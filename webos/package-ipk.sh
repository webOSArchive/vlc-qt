#!/bin/bash
# webOS IPK Package Creation Script for VLC Player
# Requires Palm SDK installed at /opt/PalmSDK

set -e

PALM_SDK="/opt/PalmSDK/Current/bin"
PALM_PACKAGE="${PALM_SDK}/palm-package"
PALM_INSTALL="${PALM_SDK}/palm-install"

echo "=== VLC Player webOS Package Builder ==="
echo ""

# Check for Palm SDK
if [ ! -x "${PALM_PACKAGE}" ]; then
    echo "ERROR: Palm SDK not found at ${PALM_SDK}"
    echo "Please install the Palm SDK to /opt/PalmSDK"
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="${SCRIPT_DIR}/.."
BUILD_DIR="${PROJECT_ROOT}/build-webos"
STAGING_DIR="${SCRIPT_DIR}/package-staging"
APP_ID="org.webosarchive.vlcplayer"
VERSION="1.0.0"

# Paths to built components
VLCQT_BUILD="${BUILD_DIR}"
LIBVLC_PATH="${SCRIPT_DIR}/vlc-arm"
APP_BUILD="${SCRIPT_DIR}/app/build"

# Check if VLC-Qt was built
if [ ! -f "${VLCQT_BUILD}/src/core/libVLCQtCore.so" ]; then
    echo "ERROR: VLC-Qt not built. Run build-vlc-qt.sh first."
    exit 1
fi

# Check if app was built
if [ ! -f "${APP_BUILD}/vlcplayer" ]; then
    echo "ERROR: VLC Player app not built."
    echo "Build with: cd ${SCRIPT_DIR}/app && mkdir build && cd build && cmake .. && make"
    exit 1
fi

# Check if libVLC exists
if [ ! -d "${LIBVLC_PATH}/lib" ]; then
    echo "ERROR: libVLC not found at ${LIBVLC_PATH}"
    echo "Run build-libvlc.sh or provide pre-built ARM libVLC."
    exit 1
fi

echo "Creating package staging directory..."
# Preserve user resources folder if it exists
RESOURCES_BACKUP=""
if [ -d "${STAGING_DIR}/resources" ]; then
    RESOURCES_BACKUP=$(mktemp -d)
    cp -r "${STAGING_DIR}/resources" "${RESOURCES_BACKUP}/"
fi

rm -rf "${STAGING_DIR}/${APP_ID}"
mkdir -p "${STAGING_DIR}/${APP_ID}/bin"
mkdir -p "${STAGING_DIR}/${APP_ID}/lib"
mkdir -p "${STAGING_DIR}/${APP_ID}/plugins/vlc"

# Restore user resources
if [ -n "${RESOURCES_BACKUP}" ] && [ -d "${RESOURCES_BACKUP}/resources" ]; then
    cp -r "${RESOURCES_BACKUP}/resources" "${STAGING_DIR}/"
    rm -rf "${RESOURCES_BACKUP}"
fi

# Copy app metadata
echo "Copying app metadata..."
cp "${SCRIPT_DIR}/app/appinfo.json" "${STAGING_DIR}/${APP_ID}/"
echo "2.0" > "${STAGING_DIR}/${APP_ID}/package.properties"

# Copy icon - check multiple locations
if [ -f "${STAGING_DIR}/resources/icon.png" ]; then
    echo "Using custom icon from resources folder."
    cp "${STAGING_DIR}/resources/icon.png" "${STAGING_DIR}/${APP_ID}/"
elif [ -f "${SCRIPT_DIR}/app/icon.png" ]; then
    cp "${SCRIPT_DIR}/app/icon.png" "${STAGING_DIR}/${APP_ID}/"
else
    echo "Note: No icon.png found. Creating placeholder."
    echo "iVBORw0KGgoAAAANSUhEUgAAAEAAAABACAYAAACqaXHeAAAA2klEQVR42u3bMQ6AIBCF4XcT78C5OIKJxoTE6BG8gLXdFhYWJP5fBQU8hkd4BwAAAAAAAPyPvDhSvfp6+7m+N0kWO95uo5kFoZlNP12lYfT1uQmI1LPTj7mB4J6JiB6PW7/r8l8GQCT6eqvdOG+Xe1UmIH6H8Xed7kJnvz+rTECKoqjXMl0m4K5zWW8CchdJ8zABVZ/Lq05AuiKVZULeZKX9uUxX2Qi4iCR5u12lIpKeZEWeABRFkT4B0U5kNk/ArJcJiFAkaQLmbJaJdJVJOgEI5e+XySQdAAAAAAAA/9kHZDM+RaA3wIIAAAAASUVORK5CYII=" | base64 -d > "${STAGING_DIR}/${APP_ID}/icon.png"
fi

# Copy main executable
echo "Copying executable..."
cp "${APP_BUILD}/vlcplayer" "${STAGING_DIR}/${APP_ID}/bin/"
chmod +x "${STAGING_DIR}/${APP_ID}/bin/vlcplayer"

# Copy ffmpeg tools (for video probing and transcoding)
echo "Copying ffmpeg tools..."
if [ -f "${LIBVLC_PATH}/bin/ffmpeg" ]; then
    cp "${LIBVLC_PATH}/bin/ffmpeg" "${STAGING_DIR}/${APP_ID}/bin/"
    chmod +x "${STAGING_DIR}/${APP_ID}/bin/ffmpeg"
fi
if [ -f "${LIBVLC_PATH}/bin/ffprobe" ]; then
    cp "${LIBVLC_PATH}/bin/ffprobe" "${STAGING_DIR}/${APP_ID}/bin/"
    chmod +x "${STAGING_DIR}/${APP_ID}/bin/ffprobe"
fi

# Copy launcher script
echo "Copying launcher script..."
cp "${SCRIPT_DIR}/app/start" "${STAGING_DIR}/${APP_ID}/"
chmod +x "${STAGING_DIR}/${APP_ID}/start"

# Copy VLC-Qt libraries (preserve symlinks)
echo "Copying VLC-Qt libraries..."
cp -P "${VLCQT_BUILD}/src/core/libVLCQtCore.so"* "${STAGING_DIR}/${APP_ID}/lib/" 2>/dev/null || true
cp -P "${VLCQT_BUILD}/src/widgets/libVLCQtWidgets.so"* "${STAGING_DIR}/${APP_ID}/lib/" 2>/dev/null || true

# Copy libVLC libraries (preserve symlinks)
echo "Copying libVLC libraries..."
cp -P "${LIBVLC_PATH}/lib/libvlc.so"* "${STAGING_DIR}/${APP_ID}/lib/"
cp -P "${LIBVLC_PATH}/lib/libvlccore.so"* "${STAGING_DIR}/${APP_ID}/lib/"

# Copy FFmpeg libraries needed by VLC and ffmpeg/ffprobe tools
echo "Copying FFmpeg libraries..."
for lib in libavcodec libavformat libavutil libswscale libswresample libpostproc libavdevice libavfilter; do
    if ls "${LIBVLC_PATH}/lib/${lib}.so"* 1>/dev/null 2>&1; then
        cp -P "${LIBVLC_PATH}/lib/${lib}.so"* "${STAGING_DIR}/${APP_ID}/lib/"
    fi
done

# NOTE: glibc and Qt5 are NOT bundled - they come from external packages:
#   com.nizovn.glibc, com.nizovn.qt5, com.nizovn.qt5qpaplugins
# The "qt5sdk" section in appinfo.json grants jail access to these packages.

# Copy VLC plugins (only .so files, preserve directory structure)
echo "Copying VLC plugins..."
if [ -d "${LIBVLC_PATH}/lib/vlc/plugins" ]; then
    cd "${LIBVLC_PATH}/lib/vlc/plugins"
    find . -name "*.so" | while read plugin; do
        plugin_dir=$(dirname "$plugin")
        mkdir -p "${STAGING_DIR}/${APP_ID}/plugins/vlc/${plugin_dir}"
        cp "$plugin" "${STAGING_DIR}/${APP_ID}/plugins/vlc/${plugin_dir}/"
    done
    cd - > /dev/null
elif [ -d "${LIBVLC_PATH}/plugins" ]; then
    cd "${LIBVLC_PATH}/plugins"
    find . -name "*.so" | while read plugin; do
        plugin_dir=$(dirname "$plugin")
        mkdir -p "${STAGING_DIR}/${APP_ID}/plugins/vlc/${plugin_dir}"
        cp "$plugin" "${STAGING_DIR}/${APP_ID}/plugins/vlc/${plugin_dir}/"
    done
    cd - > /dev/null
else
    echo "WARNING: VLC plugins directory not found!"
fi

# Strip binaries to reduce size
echo "Stripping binaries..."
STRIP="/opt/gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabi/bin/arm-linux-gnueabi-strip"
if [ -f "${STRIP}" ]; then
    find "${STAGING_DIR}/${APP_ID}" -name "*.so*" -type f -exec ${STRIP} --strip-unneeded {} \; 2>/dev/null || true
    ${STRIP} "${STAGING_DIR}/${APP_ID}/bin/vlcplayer" 2>/dev/null || true
else
    echo "WARNING: Strip tool not found. Package will be larger."
fi

echo ""
echo "Package contents:"
find "${STAGING_DIR}/${APP_ID}" -type f | wc -l
echo "files in staging directory"
echo ""

# Create IPK package using Palm SDK
echo "Creating IPK package..."
cd "${STAGING_DIR}"
"${PALM_PACKAGE}" -o "${STAGING_DIR}" "${APP_ID}"

echo ""
echo "=== Package Complete ==="
echo ""
ls -lh "${STAGING_DIR}/${APP_ID}"*.ipk 2>/dev/null || echo "IPK not found!"
echo ""
echo "To install on webOS device:"
echo "  palm-install ${STAGING_DIR}/${APP_ID}_${VERSION}_all.ipk"
echo ""
echo "Dependencies required on device:"
echo "  - com.nizovn.qt5"
echo "  - com.nizovn.glibc"
echo "  - com.nizovn.openssl"
