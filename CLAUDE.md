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
