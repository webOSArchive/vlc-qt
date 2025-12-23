#!/bin/bash
#
# Setup Device SDK for webOS VLC-Qt Cross-Compilation
#
# This script populates the webos/device folder with headers and libraries
# needed for cross-compiling. It tries sources in this order:
#   1. Local IPKs in related-packages/
#   2. Palm PDK (/opt/PalmPDK)
#   3. Device extraction via novacom
#   4. Download from Khronos registry
#
# NOTE: The sysroot headers (pthread.h, etc.) require a complete extraction
# from a webOS device. The simple extraction in this script may be incomplete.
# If builds fail, you may need to manually extract /usr/include from a device:
#   novacom run file:///bin/tar -- czf - -C / usr/include | tar xzf - -C device/sysroot/
#
# Usage: ./setup-device-sdk.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
DEVICE_DIR="${SCRIPT_DIR}/device"
PACKAGES_DIR="${SCRIPT_DIR}/related-packages"
TEMP_DIR="${SCRIPT_DIR}/.setup-temp"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

info() { echo -e "${GREEN}[INFO]${NC} $1"; }
warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
error() { echo -e "${RED}[ERROR]${NC} $1"; }

cleanup() {
    rm -rf "${TEMP_DIR}"
}
trap cleanup EXIT

# Check if device SDK already exists
check_existing() {
    if [ -f "${DEVICE_DIR}/include/KHR/khrplatform.h" ] && \
       [ -f "${DEVICE_DIR}/lib/libEGL.so" ] && \
       [ -f "${DEVICE_DIR}/include/openssl/ssl.h" ]; then
        info "Device SDK already set up at ${DEVICE_DIR}"
        read -p "Recreate? (y/N) " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            exit 0
        fi
    fi
}

# Extract an IPK file (ar archive containing data.tar.gz)
extract_ipk() {
    local ipk="$1"
    local dest="$2"

    mkdir -p "${dest}"
    cd "${dest}"

    # IPK is an ar archive
    ar x "${ipk}"

    # Extract data tarball (may be .tar.gz or .tar.xz)
    if [ -f data.tar.gz ]; then
        tar xzf data.tar.gz
        rm -f data.tar.gz
    elif [ -f data.tar.xz ]; then
        tar xJf data.tar.xz
        rm -f data.tar.xz
    fi

    rm -f control.tar.gz control.tar.xz debian-binary
    cd - > /dev/null
}

# Setup OpenSSL from IPK
setup_openssl_from_ipk() {
    local ipk="${PACKAGES_DIR}/com.nizovn.openssl_1.0.2p_armv7.ipk"

    if [ ! -f "${ipk}" ]; then
        return 1
    fi

    info "Extracting OpenSSL from IPK..."
    mkdir -p "${TEMP_DIR}/openssl"
    extract_ipk "${ipk}" "${TEMP_DIR}/openssl"

    # Copy libraries
    mkdir -p "${DEVICE_DIR}/lib"
    cp -P "${TEMP_DIR}/openssl/lib/"libssl* "${DEVICE_DIR}/lib/" 2>/dev/null || true
    cp -P "${TEMP_DIR}/openssl/lib/"libcrypto* "${DEVICE_DIR}/lib/" 2>/dev/null || true

    # Copy headers if present
    if [ -d "${TEMP_DIR}/openssl/include/openssl" ]; then
        mkdir -p "${DEVICE_DIR}/include"
        cp -r "${TEMP_DIR}/openssl/include/openssl" "${DEVICE_DIR}/include/"
    fi

    return 0
}

# Check if novacom is available and device is connected
check_novacom() {
    if ! command -v novacom &> /dev/null; then
        return 1
    fi

    if ! novacom -l 2>/dev/null | grep -q "usb"; then
        return 1
    fi

    return 0
}

# Extract files from device via novacom
setup_from_device() {
    info "Extracting SDK files from device via novacom..."

    mkdir -p "${DEVICE_DIR}/include"
    mkdir -p "${DEVICE_DIR}/lib"
    mkdir -p "${DEVICE_DIR}/sysroot/usr/include"
    mkdir -p "${DEVICE_DIR}/sysroot/usr/lib"

    # EGL/KHR/GLES headers
    info "  Extracting EGL/KHR headers..."
    novacom run file:///bin/tar -- czf - -C /usr/include EGL KHR 2>/dev/null | \
        tar xzf - -C "${DEVICE_DIR}/include/" 2>/dev/null || warn "EGL/KHR headers not found"

    # ALSA headers
    info "  Extracting ALSA headers..."
    novacom run file:///bin/tar -- czf - -C /usr/include alsa 2>/dev/null | \
        tar xzf - -C "${DEVICE_DIR}/include/" 2>/dev/null || warn "ALSA headers not found"

    # OMX headers
    info "  Extracting OMX headers..."
    novacom run file:///bin/tar -- czf - -C /usr/include OMX 2>/dev/null | \
        tar xzf - -C "${DEVICE_DIR}/include/" 2>/dev/null || warn "OMX headers not found"

    # OpenSSL headers (if not already from IPK)
    if [ ! -d "${DEVICE_DIR}/include/openssl" ]; then
        info "  Extracting OpenSSL headers..."
        novacom run file:///bin/tar -- czf - -C /usr/include openssl 2>/dev/null | \
            tar xzf - -C "${DEVICE_DIR}/include/" 2>/dev/null || warn "OpenSSL headers not found"
    fi

    # Libraries
    info "  Extracting device libraries..."

    # libEGL
    novacom run file:///bin/cat -- /usr/lib/libEGL.so > "${DEVICE_DIR}/lib/libEGL.so" 2>/dev/null || \
        warn "libEGL.so not found"

    # libasound
    for lib in libasound.so libasound.so.2 libasound.so.2.0.0; do
        novacom run file:///bin/cat -- /usr/lib/${lib} > "${DEVICE_DIR}/lib/${lib}" 2>/dev/null || true
    done

    # OpenSSL libs (if not already from IPK)
    if [ ! -f "${DEVICE_DIR}/lib/libssl.so" ]; then
        for lib in libssl.so libssl.so.1.0.0 libcrypto.so libcrypto.so.1.0.0; do
            novacom run file:///bin/cat -- /usr/lib/${lib} > "${DEVICE_DIR}/lib/${lib}" 2>/dev/null || true
        done
        # Create symlinks
        cd "${DEVICE_DIR}/lib"
        [ -f libssl.so.1.0.0 ] && [ ! -L libssl.so ] && ln -sf libssl.so.1.0.0 libssl.so
        [ -f libcrypto.so.1.0.0 ] && [ ! -L libcrypto.so ] && ln -sf libcrypto.so.1.0.0 libcrypto.so
        cd - > /dev/null
    fi

    # OMX libraries
    mkdir -p "${DEVICE_DIR}/lib/omx"
    novacom run file:///bin/tar -- czf - -C /usr/lib omx 2>/dev/null | \
        tar xzf - -C "${DEVICE_DIR}/lib/" 2>/dev/null || warn "OMX libs not found"

    # Sysroot - glibc/kernel headers needed for compilation
    # This is the most complex part - we need a fairly complete /usr/include
    info "  Extracting sysroot (this may take a minute)..."

    mkdir -p "${DEVICE_DIR}/sysroot/usr"

    # Try to extract complete /usr/include
    if novacom run file:///bin/tar -- czf - -C / usr/include 2>/dev/null | \
            tar xzf - -C "${DEVICE_DIR}/sysroot/" 2>/dev/null; then
        info "  Full sysroot extracted successfully"
    else
        warn "  Full sysroot extraction failed, trying individual headers..."

        # Fallback: extract key headers individually
        for header in pthread.h sched.h limits.h errno.h signal.h time.h unistd.h \
                      stdlib.h stdio.h string.h stdint.h stddef.h fcntl.h aio.h \
                      sys/types.h sys/stat.h sys/time.h sys/socket.h sys/select.h \
                      linux/types.h asm/types.h bits/pthreadtypes.h; do
            dir=$(dirname "${header}")
            mkdir -p "${DEVICE_DIR}/sysroot/usr/include/${dir}"
            novacom run file:///bin/cat -- /usr/include/${header} > \
                "${DEVICE_DIR}/sysroot/usr/include/${header}" 2>/dev/null || true
        done

        # Try to get bits/ and sys/ directories
        for subdir in bits sys linux asm; do
            novacom run file:///bin/tar -- czf - -C /usr/include ${subdir} 2>/dev/null | \
                tar xzf - -C "${DEVICE_DIR}/sysroot/usr/include/" 2>/dev/null || true
        done
    fi

    return 0
}

# Copy headers from Palm PDK
setup_from_palmpdk() {
    local pdk_include="/opt/PalmPDK/include"

    if [ ! -d "${pdk_include}" ]; then
        warn "Palm PDK not found at /opt/PalmPDK"
        return 1
    fi

    info "Copying headers from Palm PDK..."

    # OpenSSL headers
    if [ -d "${pdk_include}/openssl" ]; then
        mkdir -p "${DEVICE_DIR}/include"
        cp -r "${pdk_include}/openssl" "${DEVICE_DIR}/include/"
        info "  OpenSSL headers copied from Palm PDK"
    fi

    # GLES headers (may be useful)
    if [ -d "${pdk_include}/GLES" ]; then
        cp -r "${pdk_include}/GLES" "${DEVICE_DIR}/include/"
        cp -r "${pdk_include}/GLES2" "${DEVICE_DIR}/include/"
        info "  GLES headers copied from Palm PDK"
    fi

    return 0
}

# Download headers from Khronos (last resort)
download_khronos_headers() {
    info "Downloading headers from Khronos registry..."
    mkdir -p "${DEVICE_DIR}/include/KHR"
    mkdir -p "${DEVICE_DIR}/include/EGL"

    # KHR platform header
    if [ ! -f "${DEVICE_DIR}/include/KHR/khrplatform.h" ]; then
        curl -sL "https://registry.khronos.org/EGL/api/KHR/khrplatform.h" \
            -o "${DEVICE_DIR}/include/KHR/khrplatform.h" 2>/dev/null || \
            warn "Could not download khrplatform.h"
    fi

    # EGL headers
    for header in egl.h eglext.h eglplatform.h; do
        if [ ! -f "${DEVICE_DIR}/include/EGL/${header}" ]; then
            curl -sL "https://registry.khronos.org/EGL/api/EGL/${header}" \
                -o "${DEVICE_DIR}/include/EGL/${header}" 2>/dev/null || \
                warn "Could not download ${header}"
        fi
    done

    return 0
}

# Main
main() {
    echo "=================================="
    echo "webOS Device SDK Setup"
    echo "=================================="
    echo ""

    check_existing

    mkdir -p "${TEMP_DIR}"
    mkdir -p "${DEVICE_DIR}"

    # Step 1: Try OpenSSL from IPK
    if setup_openssl_from_ipk; then
        info "OpenSSL extracted from IPK"
    else
        warn "OpenSSL IPK not found, will try device extraction"
    fi

    # Step 2: Try device extraction via novacom
    if check_novacom; then
        setup_from_device
    else
        warn "novacom not available or no device connected"
        warn "Connect a webOS device and run this script again to extract device headers"
    fi

    # Step 3: Copy from Palm PDK
    setup_from_palmpdk || true

    # Step 4: Download missing critical headers from Khronos
    download_khronos_headers || true

    # Verify setup
    echo ""
    echo "=================================="
    echo "Setup Summary"
    echo "=================================="

    local success=true

    if [ -f "${DEVICE_DIR}/include/KHR/khrplatform.h" ]; then
        info "KHR headers: OK"
    else
        error "KHR headers: MISSING"
        success=false
    fi

    if [ -f "${DEVICE_DIR}/include/EGL/egl.h" ]; then
        info "EGL headers: OK"
    else
        warn "EGL headers: MISSING (optional)"
    fi

    if [ -f "${DEVICE_DIR}/include/alsa/asoundlib.h" ]; then
        info "ALSA headers: OK"
    else
        warn "ALSA headers: MISSING (optional)"
    fi

    if [ -f "${DEVICE_DIR}/include/openssl/ssl.h" ]; then
        info "OpenSSL headers: OK"
    else
        warn "OpenSSL headers: MISSING (optional)"
    fi

    if [ -f "${DEVICE_DIR}/lib/libEGL.so" ]; then
        info "libEGL: OK"
    else
        warn "libEGL: MISSING (optional)"
    fi

    if [ -f "${DEVICE_DIR}/lib/libssl.so" ] || [ -f "${DEVICE_DIR}/lib/libssl.so.1.0.0" ]; then
        info "OpenSSL libs: OK"
    else
        warn "OpenSSL libs: MISSING (optional)"
    fi

    echo ""
    if [ "$success" = true ]; then
        info "Device SDK setup complete!"
    else
        error "Some required components are missing."
        error "Connect a webOS device and run this script again."
        exit 1
    fi
}

main "$@"
