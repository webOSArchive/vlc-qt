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
- `com.nizovn.glibc` - GNU C Library (newer than device's built-in glibc)
- `com.nizovn.openssl` - OpenSSL libraries
- `com.nizovn.qt5qpaplugins` - Qt5 platform plugins for webOS
- `com.nizovn.qt5sdk` - **Critical**: Provides custom jailer wrapper and jail_qt5.conf
- `com.nizovn.cacert` - CA certificates
- `org.webosinternals.dbus` - D-Bus for webOS

All packages must be installed on the device before running the VLC app.

### webOS App Sandbox (Jail System)

webOS runs PDK apps in a jail/sandbox as the `prisoner` user (UID 5xxx). By default, apps can only see their own directory under `/media/cryptofs/apps/usr/palm/applications/`.

**The Problem**: Qt5 apps need access to external packages (glibc, Qt5 libs) which are installed as separate apps.

**The Solution**: The `com.nizovn.qt5sdk` package installs:
1. A wrapper script at `/usr/bin/jailer` that intercepts app launches
2. A jail config at `/etc/jail_qt5.conf` that mounts external packages

When an app has a `qt5sdk` section in `appinfo.json`, the jailer uses `jail_qt5.conf` which mounts:
- `com.nizovn.glibc`
- `com.nizovn.qt5`
- `com.nizovn.qt5qpaplugins`
- `com.nizovn.openssl`
- `com.nizovn.cacert`
- `org.webosinternals.dbus`

### appinfo.json Configuration

```json
{
    "id": "org.webosarchive.vlcplayer",
    "type": "pdk",
    "main": "bin/vlcplayer",
    "qt5sdk": {
        "exports": [
            "QMLSCENE_DEVICE=softwarecontext",
            "VLC_PLUGIN_PATH=/media/cryptofs/apps/usr/palm/applications/org.webosarchive.vlcplayer/plugins/vlc"
        ]
    },
    "requiredMemory": 150
}
```

**Key points:**
- `"main": "bin/vlcplayer"` - Point directly to the binary (not a shell script)
- `"qt5sdk": {}` - Presence of this section triggers qt5 jail usage
- `"exports"` - Environment variables set before app launch (parsed by jailer wrapper)

**Warning**: An empty exports array `"exports": []` will break the launcher. Either omit exports entirely or include at least one value.

### Jail Caching Issue

webOS caches jail directory structures at `/var/palm/jail/<appid>/`. If you add `qt5sdk` to an app that was previously installed without it, the cached jail won't have the external package mounts.

**Fix**: Delete the stale jail and relaunch:
```bash
novacom run file:///usr/bin/jailer -- -N -i org.webosarchive.vlcplayer
palm-launch org.webosarchive.vlcplayer
```

You can verify the jail has correct mounts:
```bash
ls /var/palm/jail/org.webosarchive.vlcplayer/media/cryptofs/apps/usr/palm/applications/
# Should show: com.nizovn.glibc, com.nizovn.qt5, etc.
```

### Binary Linking

The vlcplayer binary must be linked with:
- **ELF interpreter**: `/media/cryptofs/apps/usr/palm/applications/com.nizovn.glibc/lib/ld.so`
- **RPATH**: Paths to app libs, Qt5, glibc, and OpenSSL

This is configured in `webos/cmake/webos-arm-toolchain.cmake`.

### Build Commands

```bash
cd webos
./build-and-package.sh    # Build app and create IPK
./package-ipk.sh          # Package only (after building)
```

### Installation & Testing

```bash
palm-install webos/package-staging/org.webosarchive.vlcplayer_1.0.0_all.ipk
palm-launch org.webosarchive.vlcplayer
```

### Debugging

Check jailer logs:
```bash
novacom run file:///bin/sh -- -c 'grep jailer /var/log/messages | tail -20'
```

Check if app entered qt5 jail (look for "entering qt5 jail"):
```bash
novacom run file:///bin/sh -- -c 'grep vlcplayer /var/log/messages | tail -10'
```

### Framebuffer Architecture (Triple Buffering)

The HP TouchPad uses a **triple-buffered framebuffer** for smooth display updates:

```
/dev/fb0 layout:
┌─────────────────┐  yoffset=0    (Page 0)
│   1024 x 768    │
├─────────────────┤  yoffset=768  (Page 1)
│   1024 x 768    │
├─────────────────┤  yoffset=1536 (Page 2)
│   1024 x 768    │
└─────────────────┘
Total: 1024 x 2304 (yres_virtual = 768 * 3)
```

**Key framebuffer info** (from `FBIOGET_VSCREENINFO`):
- `xres`, `yres`: Visible screen size (1024x768)
- `xres_virtual`, `yres_virtual`: Total buffer size (1024x2304)
- `yoffset`: Currently displayed page (0, 768, or 1536)

**Critical**: When rendering directly to `/dev/fb0`, you must:
1. Query current `yoffset` via `FBIOGET_VSCREENINFO` before each frame
2. Add `yoffset` to your target Y coordinates
3. Clip rendering to the current page bounds (`yoffset` to `yoffset + yres`)

```cpp
struct fb_var_screeninfo vinfo;
ioctl(m_fbFd, FBIOGET_VSCREENINFO, &vinfo);
unsigned int pageYOffset = vinfo.yoffset;  // 0, 768, or 1536

// Adjust target coordinates for current page
targetY += pageYOffset;

// Clip to current page bounds
int pageTop = pageYOffset;
int pageBottom = pageYOffset + m_fbHeight;
if (fbY < pageTop || fbY >= pageBottom) continue;
```

Writing to the wrong page results in invisible output or flickering as the display hardware cycles through pages.

### Qt UI vs Direct Framebuffer Rendering

When bypassing Qt to render video directly to `/dev/fb0`, there's a **layer conflict** between Qt's rendering and framebuffer writes. Qt and the video both fight for the same framebuffer, causing flickering.

**Failed approaches**:
- `Qt::WA_PaintOnScreen` - Still conflicts
- `Qt::WA_OpaquePaintEvent` + `Qt::WA_NoSystemBackground` - Helps but insufficient
- Timer-based re-rendering - Just makes flickering faster
- Event filters to intercept Qt paint events - Too complex, still flickers

**Working solution**: Complete layer separation by hiding Qt during playback:

1. **During video playback**: Hide all Qt UI widgets (title bar, controls). Only keep the video widget visible (to capture touch events). Render video fullscreen directly to framebuffer.

2. **When paused/stopped**: Show Qt UI widgets. Clear the framebuffer region so Qt can paint.

3. **Touch to pause**: Detect taps on the video widget, pause playback, and show UI.

```cpp
// In MainWindow - hide UI when playing, show when paused
connect(m_fbVideoWidget, &FBVideoWidget::firstFrameReady, this, &MainWindow::hideForPlayback);
connect(m_player, &VlcMediaPlayer::paused, this, &MainWindow::showForUI);

void MainWindow::hideForPlayback() {
    m_titleLabel->hide();
    m_controlsWidget->hide();
}

void MainWindow::showForUI() {
    m_titleLabel->show();
    m_controlsWidget->show();
}
```

### VLC Frame Delivery Timing

VLC's frame delivery has specific timing characteristics that affect UI coordination:

**Problem 1: Frames arrive before "playing" signal**
- VLC calls `formatCallback` to negotiate video format
- Frames start arriving via `lock/unlock/display` callbacks
- THEN the `playing` signal is emitted
- If you hide UI on `playing`, user sees black screen briefly

**Problem 2: Frames continue after pause**
- VLC may deliver buffered frames after pause is requested
- If you render these frames, they overwrite Qt's UI
- Results in flickering between video and UI when paused

**Solution**: Track frame state explicitly:

```cpp
// Only render when actually playing
void FBVideoWidget::onFrameReady() {
    if (m_isPlaying) {
        renderToFramebuffer();

        // Signal first frame for UI coordination
        if (!m_firstFrameRendered && m_hasFrame) {
            m_firstFrameRendered = true;
            emit firstFrameReady();
        }
    }
}

// Reset on each new playback
void FBVideoWidget::onPlaybackStarted() {
    m_isPlaying = true;
    m_firstFrameRendered = false;
}

void FBVideoWidget::onPlaybackStopped() {
    m_isPlaying = false;
    clearVideoRegion();  // Let Qt paint
}
```

**Key insight**: Use `firstFrameReady` signal (not `playing`) to hide UI. This ensures video is visible before UI disappears, avoiding the black screen flash.

### Debugging on webOS

stderr doesn't appear in `/var/log/messages` on webOS. Use file-based logging:

```cpp
static FILE *g_logFile = nullptr;
static void logMsg(const char *fmt, ...) {
    if (!g_logFile) {
        g_logFile = fopen("/media/internal/vlcplayer.log", "a");
    }
    if (g_logFile) {
        va_list args;
        va_start(args, fmt);
        vfprintf(g_logFile, fmt, args);
        va_end(args);
        fflush(g_logFile);
    }
}
```

Log file location: `/media/internal/vlcplayer.log` (persists across app restarts, accessible via USB)
