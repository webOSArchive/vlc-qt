# VLC-Qt for webOS Build Plan

## Overview

Build a VLC media player application for legacy webOS (HP TouchPad), an ARMv7 Linux environment using Qt5.

## Target Platform

- **Device**: HP TouchPad (and compatible legacy webOS devices)
- **Architecture**: ARMv7-A, Cortex-A8, NEON, softfp ABI
- **OS**: webOS 3.x (Linux-based)
- **GPU**: PowerVR SGX540 (OpenGL ES 2.0 only)
- **Audio**: ALSA

## Available Resources from qupzilla-webos

### Cross-Compilation SDK (Ready to Use)
- **Qt 5.9.7 ARM SDK**: `/home/jonwise/Projects/qupzilla-webos/sdk/qt5-arm/`
- **Qt Libraries**: `/tmp/qt5-arm-runtime/lib/` (actual ARM .so files)
- **Qt Headers**: `sdk/qt5-arm/include/`
- **mkspec**: `sdk/qt5-arm/mkspecs/linux-webos-arm-g++/`

### Toolchain
- **Compiler**: Linaro GCC 4.9.4 at `/opt/gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabi/`
- **Target triple**: `arm-linux-gnueabi`
- **Compiler flags**: `-march=armv7-a -mtune=cortex-a8 -mfpu=neon -mfloat-abi=softfp`

### Device Headers & Libraries
- **Location**: `/home/jonwise/Projects/qupzilla-webos/source/qt5-webos-sdk/files/device/`
- **Available**: ALSA, OpenGL ES (EGL/KHR), OpenSSL, fontconfig, freetype, dbus, png, jpeg

### Runtime Dependencies (IPK packages)
```
related-packages/
├── com.nizovn.glibc_4.8-2015.06-0_armv7.ipk    # Modern glibc
├── com.nizovn.qt5_5.9.7-0_armv7.ipk            # Qt 5.9.7 runtime
├── com.nizovn.qt5qpaplugins_1.0.3_armv7.ipk    # QPA plugins
├── com.nizovn.qt5sdk_1.0.2_armv7.ipk           # Jailer wrapper
├── com.nizovn.openssl_1.0.2p_armv7.ipk         # OpenSSL
├── com.nizovn.cacert_2021-09-30_armv7.ipk      # CA certificates
└── org.webosinternals.dbus_1.4.16-3_armv7.ipk  # D-Bus
```

---

## Implementation Plan

### Phase 1: Setup Project Structure

**Step 1.1**: Copy SDK components to vlc-qt project
```bash
# Create webos directory structure
mkdir -p webos/{sdk,device,toolchain,packages}

# Copy Qt5 ARM SDK
cp -r ../qupzilla-webos/sdk/qt5-arm webos/sdk/

# Copy device headers/libs
cp -r ../qupzilla-webos/source/qt5-webos-sdk/files/device webos/

# Copy related packages for reference
cp -r ../qupzilla-webos/related-packages webos/related-packages/
```

**Step 1.2**: Initialize libvlc-headers submodule
```bash
git submodule update --init libvlc-headers
```

### Phase 2: Build/Acquire libVLC for ARMv7

**This is the critical dependency.** Options:

#### Option A: Build VLC from Source (Recommended)
1. Clone VLC source (version 2.2.x or 3.0.x)
2. Set up cross-compilation environment
3. Configure with minimal plugins for webOS:
   - Video output: OpenGL ES 2.0 / software
   - Audio output: ALSA
   - Demuxers: mkv, mp4, avi, ts
   - Codecs: ffmpeg (cross-compiled)
4. Build libvlc.so, libvlccore.so, and essential plugins

**VLC Build Configuration Example:**
```bash
./configure --host=arm-linux-gnueabi \
    --prefix=/opt/vlc-webos \
    --disable-a52 \
    --disable-lua \
    --enable-alsa \
    --enable-gles2 \
    --disable-x11 \
    --disable-xcb \
    --disable-qt \
    --disable-skins2 \
    --with-pic \
    CC=arm-linux-gnueabi-gcc \
    CXX=arm-linux-gnueabi-g++ \
    CFLAGS="-march=armv7-a -mtune=cortex-a8 -mfpu=neon -mfloat-abi=softfp" \
    PKG_CONFIG_PATH=/path/to/cross-compiled-deps
```

**VLC Dependencies to Cross-Compile:**
- ffmpeg (with ARM NEON optimizations)
- libogg, libvorbis (audio)
- libpng, libjpeg (already in sysroot)
- freetype (already in sysroot)
- OpenSSL (already available)

#### Option B: Use Pre-built ARM VLC
- Check Debian armhf packages for libvlc
- May need to adjust GLIBC compatibility

### Phase 3: Create CMake Toolchain File

**File**: `webos/cmake/webos-arm-toolchain.cmake`

```cmake
# webOS ARMv7 Cross-Compilation Toolchain
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

# Toolchain paths
set(TOOLCHAIN_ROOT /opt/gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabi)
set(CMAKE_C_COMPILER ${TOOLCHAIN_ROOT}/bin/arm-linux-gnueabi-gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_ROOT}/bin/arm-linux-gnueabi-g++)

# Project paths (adjust based on your setup)
set(WEBOS_SDK_PATH ${CMAKE_SOURCE_DIR}/webos/sdk/qt5-arm)
set(WEBOS_DEVICE_PATH ${CMAKE_SOURCE_DIR}/webos/device)

# Compiler flags
set(CMAKE_C_FLAGS "-march=armv7-a -mtune=cortex-a8 -mfpu=neon -mfloat-abi=softfp")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -std=gnu++11")

# Search paths
set(CMAKE_FIND_ROOT_PATH ${WEBOS_SDK_PATH} ${WEBOS_DEVICE_PATH})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Qt5 configuration
set(Qt5_DIR ${WEBOS_SDK_PATH}/lib/cmake/Qt5)
set(Qt5Core_DIR ${WEBOS_SDK_PATH}/lib/cmake/Qt5Core)
set(Qt5Gui_DIR ${WEBOS_SDK_PATH}/lib/cmake/Qt5Gui)
set(Qt5Widgets_DIR ${WEBOS_SDK_PATH}/lib/cmake/Qt5Widgets)
set(Qt5Quick_DIR ${WEBOS_SDK_PATH}/lib/cmake/Qt5Quick)

# RPATH for webOS
set(CMAKE_INSTALL_RPATH
    "/media/cryptofs/apps/usr/palm/applications/com.example.vlc/lib"
    "/media/cryptofs/apps/usr/palm/applications/com.nizovn.qt5/lib"
    "/media/cryptofs/apps/usr/palm/applications/com.nizovn.glibc/lib"
)
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)

# Dynamic linker
set(CMAKE_EXE_LINKER_FLAGS
    "-Wl,--dynamic-linker=/media/cryptofs/apps/usr/palm/applications/com.nizovn.glibc/lib/ld.so"
    "-Wl,--allow-shlib-undefined"
)
```

### Phase 4: Build VLC-Qt Libraries

**Step 4.1**: Configure and build
```bash
mkdir build-webos && cd build-webos
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE=../webos/cmake/webos-arm-toolchain.cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DLIBVLC_INCLUDE_DIR=/path/to/vlc-arm/include \
    -DLIBVLC_LIBRARY=/path/to/vlc-arm/lib/libvlc.so \
    -DLIBVLCCORE_LIBRARY=/path/to/vlc-arm/lib/libvlccore.so \
    -DSTATIC=OFF \
    -DWITH_X11=OFF

make -j$(nproc)
```

**Step 4.2**: Verify ARM binaries
```bash
file src/core/libVLCQtCore.so
# Should show: ELF 32-bit LSB shared object, ARM, EABI5
```

### Phase 5: Create VLC Player Application

**Step 5.1**: Create simple Qt5 Widgets-based player

**Directory**: `webos/app/`

**Main components:**
- Main window with video widget
- Playback controls (play, pause, stop, seek)
- Volume control
- File browser / URL input
- Touch-friendly UI for tablet

**Key classes to use:**
- `VlcInstance` - Initialize libVLC
- `VlcMedia` - Load media files
- `VlcMediaPlayer` - Control playback
- `VlcWidgetVideo` - Video rendering widget

**Example main.cpp:**
```cpp
#include <QApplication>
#include <VLCQtCore/Instance.h>
#include <VLCQtCore/Media.h>
#include <VLCQtCore/MediaPlayer.h>
#include <VLCQtWidgets/WidgetVideo.h>
#include "MainWindow.h"

int main(int argc, char *argv[])
{
    // webOS environment setup
    qputenv("QT_QPA_FONTDIR", "/usr/share/fonts");

    QApplication app(argc, argv);

    // Create VLC instance with appropriate args for webOS
    VlcInstance *instance = new VlcInstance(VlcCommon::args(), nullptr);

    MainWindow window(instance);
    window.showFullScreen();

    return app.exec();
}
```

### Phase 6: Package for webOS

**Step 6.1**: Create package structure
```
org.webosarchive.vlcplayer/
├── appinfo.json
├── package.properties
├── icon.png
├── bin/
│   └── vlcplayer          # Main executable
├── lib/
│   ├── libVLCQtCore.so
│   ├── libVLCQtWidgets.so
│   ├── libvlc.so
│   ├── libvlccore.so
│   └── ... (other deps)
└── plugins/
    └── vlc/               # VLC plugins
        ├── access/
        ├── audio_output/
        ├── codec/
        ├── demux/
        └── video_output/
```

**Step 6.2**: appinfo.json
```json
{
    "title": "VLC Player",
    "id": "org.webosarchive.vlcplayer",
    "version": "1.0.0",
    "vendor": "Your Name",
    "type": "pdk",
    "main": "bin/vlcplayer",
    "icon": "icon.png",
    "keywords": ["video", "media", "player", "vlc"],
    "qt5sdk": {
        "exports": [
            "VLC_PLUGIN_PATH=/media/cryptofs/apps/usr/palm/applications/org.webosarchive.vlcplayer/plugins/vlc",
            "LD_LIBRARY_PATH=/media/cryptofs/apps/usr/palm/applications/org.webosarchive.vlcplayer/lib:$LD_LIBRARY_PATH"
        ]
    },
    "requiredMemory": 150
}
```

**Step 6.3**: Build IPK package
```bash
# Install palm-package tool or use ar/tar manually
palm-package org.webosarchive.vlcplayer/
# Or manually:
# tar -cvzf data.tar.gz -C org.webosarchive.vlcplayer .
# echo "2.0" > debian-binary
# ar -r org.webosarchive.vlcplayer.ipk debian-binary control.tar.gz data.tar.gz
```

---

## Build Scripts to Create

### webos/build-vlc-deps.sh
Cross-compile VLC dependencies (ffmpeg, etc.)

### webos/build-vlc.sh
Cross-compile libVLC for ARMv7

### webos/build-vlc-qt.sh
Build VLC-Qt libraries with CMake toolchain

### webos/build-app.sh
Build the VLC player application

### webos/package-ipk.sh
Create webOS IPK package (requires Palm SDK)

---

## Technical Challenges

### 1. Video Output
- webOS uses OpenGL ES 2.0 (SGX540)
- VLC-Qt's video widgets may need adaptation
- Consider software rendering fallback

### 2. Hardware Decoding
- TouchPad has TI OMAP 4430 with hardware decode
- May need to use VLC's omxil or gstreamer plugins
- Alternative: software decoding (may be slow for HD)

### 3. Memory Constraints
- ~1GB RAM on TouchPad
- VLC + Qt5 can be memory-hungry
- Optimize build, strip symbols, use STATIC builds

### 4. Audio Output
- Use ALSA audio output
- webOS audio routing via PulseAudio (through ALSA)

### 5. File Access
- USB storage: `/media/internal/`
- May need webOS file picker integration

---

## Timeline Estimate

| Phase | Description |
|-------|-------------|
| 1 | Setup project structure |
| 2 | Build/acquire libVLC for ARM (most complex) |
| 3 | Create CMake toolchain |
| 4 | Build VLC-Qt libraries |
| 5 | Create player application |
| 6 | Package and test on device |

---

## Alternative Approach: Minimal VLC

If full VLC is too complex, consider:
1. Build only libvlc + libvlccore + minimal plugins
2. Use software decoding only
3. Target common formats: MP4, MKV, MP3, FLAC

---

## Next Steps

1. **Decision Point**: Build VLC from source vs. find pre-built ARM binaries
2. Set up the cross-compilation environment
3. Start with libVLC build (the critical path)
4. Then build VLC-Qt libraries
5. Create minimal player app
6. Test on device
7. Iterate and optimize

---

## References

- VLC-Qt documentation: https://vlc-qt.tano.si/
- VLC compilation guide: https://wiki.videolan.org/Compile_VLC/
- webOS homebrew: https://www.webos-ports.org/
- Qt cross-compilation: https://doc.qt.io/qt-5/configure-linux-device.html
