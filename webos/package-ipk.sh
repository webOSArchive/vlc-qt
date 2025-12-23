#!/bin/bash
# Package VLC Player for webOS as IPK using Palm SDK

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
APP_ID="org.webosarchive.vlcplayer"
VERSION="1.0.0"

# Source directories
VLC_ARM="${SCRIPT_DIR}/vlc-arm"
VLCQT_BUILD="${SCRIPT_DIR}/../build-webos"
APP_BUILD="${SCRIPT_DIR}/app/build"

# Package staging directory (use package-staging to preserve user resources)
STAGING_ROOT="${SCRIPT_DIR}/package-staging"
STAGE_DIR="${STAGING_ROOT}/${APP_ID}"
OUTPUT_DIR="${SCRIPT_DIR}"

echo "=== Creating webOS Package ==="
echo ""

# Clean app staging directory (preserve resources folder)
RESOURCES_BACKUP=""
if [ -d "${STAGING_ROOT}/resources" ]; then
    RESOURCES_BACKUP=$(mktemp -d)
    cp -r "${STAGING_ROOT}/resources" "${RESOURCES_BACKUP}/"
fi

rm -rf "${STAGE_DIR}"
mkdir -p "${STAGE_DIR}/bin"
mkdir -p "${STAGE_DIR}/lib"
mkdir -p "${STAGE_DIR}/plugins/vlc"

# Restore user resources
if [ -n "${RESOURCES_BACKUP}" ] && [ -d "${RESOURCES_BACKUP}/resources" ]; then
    cp -r "${RESOURCES_BACKUP}/resources" "${STAGING_ROOT}/"
    rm -rf "${RESOURCES_BACKUP}"
fi

echo "Copying executable..."
cp "${APP_BUILD}/vlcplayer" "${STAGE_DIR}/bin/"
chmod +x "${STAGE_DIR}/bin/vlcplayer"

echo "Copying launcher script..."
cp "${SCRIPT_DIR}/app/vlcplayer.sh" "${STAGE_DIR}/"
chmod +x "${STAGE_DIR}/vlcplayer.sh"

echo "Copying VLC-Qt libraries..."
cp -P "${VLCQT_BUILD}/src/core/libVLCQtCore.so"* "${STAGE_DIR}/lib/"
cp -P "${VLCQT_BUILD}/src/widgets/libVLCQtWidgets.so"* "${STAGE_DIR}/lib/"

echo "Copying libVLC libraries..."
cp -P "${VLC_ARM}/lib/libvlc.so"* "${STAGE_DIR}/lib/"
cp -P "${VLC_ARM}/lib/libvlccore.so"* "${STAGE_DIR}/lib/"

# Copy FFmpeg libraries needed by VLC
echo "Copying FFmpeg libraries..."
for lib in libavcodec libavformat libavutil libswscale libswresample libpostproc; do
    if ls "${VLC_ARM}/lib/${lib}.so"* 1>/dev/null 2>&1; then
        cp -P "${VLC_ARM}/lib/${lib}.so"* "${STAGE_DIR}/lib/"
    fi
done

echo "Copying VLC plugins..."
# Copy plugins preserving directory structure (only .so files)
if [ -d "${VLC_ARM}/lib/vlc/plugins" ]; then
    cd "${VLC_ARM}/lib/vlc/plugins"
    find . -name "*.so" | while read plugin; do
        plugin_dir=$(dirname "$plugin")
        mkdir -p "${STAGE_DIR}/plugins/vlc/${plugin_dir}"
        cp "$plugin" "${STAGE_DIR}/plugins/vlc/${plugin_dir}/"
    done
    cd - > /dev/null
fi

echo "Copying appinfo.json..."
cp "${SCRIPT_DIR}/app/appinfo.json" "${STAGE_DIR}/"

# Copy icon - check multiple locations
echo "Copying icon..."
if [ -f "${STAGING_ROOT}/resources/icon.png" ]; then
    echo "Using custom icon from resources folder."
    cp "${STAGING_ROOT}/resources/icon.png" "${STAGE_DIR}/"
elif [ -f "${SCRIPT_DIR}/app/icon.png" ]; then
    cp "${SCRIPT_DIR}/app/icon.png" "${STAGE_DIR}/"
else
    echo "Creating placeholder icon..."
    # Use base64 encoded minimal 64x64 PNG
    echo "iVBORw0KGgoAAAANSUhEUgAAAEAAAABACAYAAACqaXHeAAAA2klEQVR42u3bMQ6AIBCF4XcT78C5OIKJxoTE6BG8gLXdFhYWJP5fBQU8hkd4BwAAAAAAAPyPvDhSvfp6+7m+N0kWO95uo5kFoZlNP12lYfT1uQmI1LPTj7mB4J6JiB6PW7/r8l8GQCT6eqvdOG+Xe1UmIH6H8Xed7kJnvz+rTECKoqjXMl0m4K5zWW8CchdJ8zABVZ/Lq05AuiKVZULeZKX9uUxX2Qi4iCR5u12lIpKeZEWeABRFkT4B0U5kNk/ArJcJiFAkaQLmbJaJdJVJOgEI5e+XySQdAAAAAAAA/9kHZDM+RaA3wIIAAAAASUVORK5CYII=" | base64 -d > "${STAGE_DIR}/icon.png"
fi

echo ""
echo "Package contents:"
find "${STAGE_DIR}" -type f | wc -l
echo "files in staging directory"
echo ""
echo "Library files:"
ls -la "${STAGE_DIR}/lib/"*.so* 2>/dev/null | head -10
echo ""

echo "Creating IPK package with palm-package..."
cd "${OUTPUT_DIR}"

# Use palm-package to create the IPK
/opt/PalmSDK/0.2/bin/palm-package -o "${OUTPUT_DIR}" "${STAGE_DIR}"

echo ""
echo "=== Package Created ==="
echo ""
ls -lh "${OUTPUT_DIR}/${APP_ID}"*.ipk 2>/dev/null || echo "IPK not found!"
echo ""
echo "To install on device:"
echo "  scp ${OUTPUT_DIR}/${APP_ID}_${VERSION}_all.ipk root@touchpad:/tmp/"
echo "  ssh root@touchpad 'ipkg install /tmp/${APP_ID}_${VERSION}_all.ipk'"
echo ""
