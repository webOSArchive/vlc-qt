#!/bin/sh
# VLC Player Launcher Script for webOS
# Sets up environment before running the actual binary

LOG="/media/internal/vlcplayer-log.txt"

echo "=== VLC Player starting ===" >> $LOG
echo "Date: $(date)" >> $LOG

APP_ID="org.webosarchive.vlcplayer"
APP_DIR="/media/cryptofs/apps/usr/palm/applications/${APP_ID}"

# Set up library paths
QT5_DIR="/media/cryptofs/apps/usr/palm/applications/com.nizovn.qt5/lib"
GLIBC_DIR="/media/cryptofs/apps/usr/palm/applications/com.nizovn.glibc/lib"
OPENSSL_DIR="/media/cryptofs/apps/usr/palm/applications/com.nizovn.openssl/lib"

export LD_LIBRARY_PATH="${APP_DIR}/lib:${QT5_DIR}:${GLIBC_DIR}:${OPENSSL_DIR}:${LD_LIBRARY_PATH}"
echo "LD_LIBRARY_PATH: $LD_LIBRARY_PATH" >> $LOG

# Set up VLC plugin path
export VLC_PLUGIN_PATH="${APP_DIR}/plugins/vlc"
export VLC_VERBOSE=0
echo "VLC_PLUGIN_PATH: $VLC_PLUGIN_PATH" >> $LOG

# Set up Qt environment
export QT_QPA_PLATFORM=webos
export QT_QPA_FONTDIR=/usr/share/fonts
export QT_PLUGIN_PATH="/media/cryptofs/apps/usr/palm/applications/com.nizovn.qt5qpaplugins/plugins"
echo "QT_PLUGIN_PATH: $QT_PLUGIN_PATH" >> $LOG

# Check if binary exists
BINARY="${APP_DIR}/bin/vlcplayer"
if [ ! -f "$BINARY" ]; then
    echo "ERROR: Binary not found at $BINARY" >> $LOG
    exit 1
fi

echo "Launching $BINARY" >> $LOG

# Execute via glibc's ld.so (needed for cross-compiled binaries)
LD_SO="/media/cryptofs/apps/usr/palm/applications/com.nizovn.glibc/lib/ld.so"
exec "$LD_SO" "$BINARY" "$@" >> $LOG 2>&1
