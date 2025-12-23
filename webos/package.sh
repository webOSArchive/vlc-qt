#!/bin/bash
# webOS IPK Package Creation Script for VLC Player

set -e

echo "=== VLC Player webOS Package Builder ==="
echo ""

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="${SCRIPT_DIR}/.."
BUILD_DIR="${PROJECT_ROOT}/build-webos"
STAGING_DIR="${SCRIPT_DIR}/package-staging"
APP_ID="org.webosarchive.vlcplayer"

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
    # Create a simple placeholder (1x1 pixel transparent PNG)
    printf '\x89PNG\r\n\x1a\n\x00\x00\x00\rIHDR\x00\x00\x00@\x00\x00\x00@\x08\x06\x00\x00\x00\xaaiq\xde\x00\x00\x00\x1fIDATx\x9c\xed\xc1\x01\r\x00\x00\x00\xc2\xa0\xf7Om\x0e7\xa0\x00\x00\x00\x00\x00\x00\x00\x00\xbe\r!\x00\x00\x01\x9a`\xe1\xd5\x00\x00\x00\x00IEND\xaeB`\x82' > "${STAGING_DIR}/${APP_ID}/icon.png"
fi

# Copy main executable
echo "Copying executable..."
cp "${APP_BUILD}/vlcplayer" "${STAGING_DIR}/${APP_ID}/bin/"

# Copy launcher script
echo "Copying launcher script..."
cp "${SCRIPT_DIR}/app/vlcplayer.sh" "${STAGING_DIR}/${APP_ID}/"
chmod +x "${STAGING_DIR}/${APP_ID}/vlcplayer.sh"

# Copy VLC-Qt libraries
echo "Copying VLC-Qt libraries..."
cp "${VLCQT_BUILD}/src/core/libVLCQtCore.so"* "${STAGING_DIR}/${APP_ID}/lib/" 2>/dev/null || true
cp "${VLCQT_BUILD}/src/widgets/libVLCQtWidgets.so"* "${STAGING_DIR}/${APP_ID}/lib/" 2>/dev/null || true

# Copy libVLC libraries
echo "Copying libVLC libraries..."
cp "${LIBVLC_PATH}/lib/libvlc.so"* "${STAGING_DIR}/${APP_ID}/lib/"
cp "${LIBVLC_PATH}/lib/libvlccore.so"* "${STAGING_DIR}/${APP_ID}/lib/"

# Copy VLC plugins
echo "Copying VLC plugins..."
if [ -d "${LIBVLC_PATH}/lib/vlc/plugins" ]; then
    cp -r "${LIBVLC_PATH}/lib/vlc/plugins"/* "${STAGING_DIR}/${APP_ID}/plugins/vlc/"
elif [ -d "${LIBVLC_PATH}/plugins" ]; then
    cp -r "${LIBVLC_PATH}/plugins"/* "${STAGING_DIR}/${APP_ID}/plugins/vlc/"
else
    echo "WARNING: VLC plugins directory not found!"
fi

# Strip binaries to reduce size
echo "Stripping binaries..."
STRIP="/opt/gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabi/bin/arm-linux-gnueabi-strip"
if [ -f "${STRIP}" ]; then
    find "${STAGING_DIR}/${APP_ID}" -name "*.so*" -exec ${STRIP} --strip-unneeded {} \; 2>/dev/null || true
    ${STRIP} "${STAGING_DIR}/${APP_ID}/bin/vlcplayer" 2>/dev/null || true
else
    echo "WARNING: Strip tool not found. Package will be larger."
fi

# Create IPK package
echo ""
echo "Creating IPK package..."
cd "${STAGING_DIR}"

# Check for palm-package tool
if command -v palm-package &> /dev/null; then
    palm-package "${APP_ID}"
else
    # Manual IPK creation
    echo "palm-package not found. Creating IPK manually..."

    # Create control file
    mkdir -p CONTROL
    cat > CONTROL/control << EOF
Package: ${APP_ID}
Version: 1.0.0
Section: multimedia
Priority: optional
Architecture: armv7
Maintainer: VLC-Qt
Description: VLC Media Player for webOS
Depends: com.nizovn.qt5, com.nizovn.glibc
EOF

    # Create data tarball
    tar czf data.tar.gz -C "${APP_ID}" .

    # Create control tarball
    tar czf control.tar.gz -C CONTROL .

    # Create debian-binary
    echo "2.0" > debian-binary

    # Create IPK (ar archive)
    ar r "${APP_ID}_1.0.0_armv7.ipk" debian-binary control.tar.gz data.tar.gz

    # Cleanup
    rm -rf CONTROL debian-binary control.tar.gz data.tar.gz
fi

echo ""
echo "=== Package Complete ==="
echo ""
echo "Package created: ${STAGING_DIR}/${APP_ID}_1.0.0_armv7.ipk"
echo ""
echo "To install on webOS device:"
echo "  1. Copy the .ipk file to the device"
echo "  2. Install with: luna-send -n 1 palm://com.palm.appinstaller/installNoVerify '{\"target\":\"/path/to/ipk\"}'"
echo ""
echo "Dependencies required on device:"
echo "  - com.nizovn.qt5"
echo "  - com.nizovn.glibc"
echo "  - com.nizovn.openssl"
