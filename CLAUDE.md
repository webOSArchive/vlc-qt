# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

VLC-Qt is a C++ library that wraps libVLC to provide Qt-friendly classes for multimedia playback. It supports Qt 5.5+ (Qt 4 deprecated) and libVLC 2.1+.

## Build Commands

```bash
# Standard build
mkdir build && cd build
cmake ..
make -j$(nproc)

# Run tests (Qt 5 only)
make test

# Generate documentation
make docs
```

### CMake Configuration Options

- `STATIC=ON` - Build static libraries
- `LIBVLC_VERSION=0x020200` - Target specific libVLC version (hex format)
- `COVERAGE=ON` - Enable code coverage
- `WITH_X11=ON` - Enable X11 linking on Linux
- `WITH_HOMEBREW=ON` - Use Homebrew Qt on macOS

## Architecture

### Library Modules

The project builds four libraries with this dependency hierarchy:

```
VLCQtCore (base wrapper around libVLC)
├── VLCQtWidgets (Qt widget components for desktop)
└── VLCQtQml (Qt Quick integration, Qt 5 only)
    └── VLCQtPlugin (QML plugin)
```

### Source Organization

- `src/core/` - Core libVLC wrapper classes (VlcInstance, VlcMedia, VlcMediaPlayer, VlcAudio, VlcVideo)
- `src/widgets/` - Desktop UI widgets (VlcWidgetVideo, VlcWidgetSeek, VlcWidgetVolumeSlider)
- `src/qml/` - QML integration with rendering engine in `rendering/` subdirectory
- `tests/` - Qt Test-based test suite

### Key Classes

- **VlcInstance** - Initializes libVLC, entry point for all operations
- **VlcMedia** - Represents playable media (file, URL, stream)
- **VlcMediaPlayer** - Controls playback, emits Qt signals for state changes
- **VlcAudio/VlcVideo** - Track and output management
- **Vlc** (in Enums.h) - All enumeration definitions (State, PlaybackMode, Ratio, etc.)

### Playback Flow

1. Create `VlcInstance` with libVLC arguments
2. Create `VlcMedia` from file/URL
3. Create `VlcMediaPlayer` and set media
4. Connect to Qt signals for events
5. Call `play()` to start

## Code Style

Uses clang-format with LLVM base style:
- 4-space indentation (no tabs)
- Private members prefixed with underscore: `_memberName`
- Opening braces on same line (Linux style)

## Version Compatibility

When adding features, maintain backward compatibility with libVLC 2.1+:

```cpp
#if LIBVLC_VERSION >= 0x020200
    // Feature only available in libVLC 2.2+
#endif
```

## Submodules

Initialize after cloning:
```bash
git submodule init && git submodule update
```

- `libvlc-headers/` - libVLC header files
- `packaging/` - Platform-specific packaging scripts

## webOS Port (webos/ directory)

The `webos/` directory contains a port of VLC-Qt for legacy webOS devices (TouchPad, Pre3).

### Dependencies

The `webos/related-packages/` folder contains all required dependency IPKs:
- `com.nizovn.qt5` - Qt5 libraries
- `com.nizovn.glibc` - GNU C Library (compatible with webOS)
- `com.nizovn.openssl` - OpenSSL libraries
- `com.nizovn.qt5qpaplugins` - Qt5 platform plugins for webOS
- `com.nizovn.qt5sdk` - **Critical**: Provides custom jailer and jail_qt5.conf that grants apps access to external package directories
- `com.nizovn.cacert` - CA certificates
- `org.webosinternals.dbus` - D-Bus for webOS

These packages must be installed on the device. The `qt5sdk` package is especially important as it modifies the webOS jail system to allow Qt5 apps to access shared libraries.

### webOS App Sandbox (Jail)

webOS runs apps in a sandbox as the `prisoner` user, which restricts access to other app directories. Qt5 apps require access to the `com.nizovn.*` packages.

**Solution**: Add `qt5sdk` section to `appinfo.json`:
```json
{
    "type": "pdk",
    "main": "start",
    "qt5sdk": {
        "exports": [
            "VLC_PLUGIN_PATH=/media/cryptofs/apps/usr/palm/applications/org.webosarchive.vlcplayer/plugins/vlc"
        ]
    }
}
```

This tells webOS to use the qt5sdk jail configuration, which mounts the external package directories.

### Reference Implementation

See `~/Projects/qupzilla` for a working Qt5 webOS app example:
- Uses `qt5sdk` section in appinfo.json
- Points `main` directly to binary or uses start script
- Does NOT bundle glibc/Qt5 (uses external packages)

### Build Commands

```bash
cd webos
./build-and-package.sh    # Build VLC-Qt for ARM and package IPK
./package-ipk.sh          # Package only (after building)
```

### Debugging

Logs are written to `/media/internal/vlc.log` on the device.
