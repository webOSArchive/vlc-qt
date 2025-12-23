# VLC-Qt for webOS - Next Steps

## Current Status (December 2024)

**The VLC player is working!** Basic video playback with software decoding is functional:

- Video plays at ~20 FPS (SD content) via direct framebuffer rendering
- Audio works via ALSA
- File picker allows selecting media files
- Touch to pause/resume works correctly
- Qt UI and video layers properly separated (no flickering)

See `README.md` "Current Status" section for full details.

---

## Remaining Work

### High Priority

1. **Improve UI/UX**
   - Better file browser (currently uses Qt's basic file dialog)
   - Display video metadata (duration, resolution, codec)
   - Remember last played file/position

2. **Performance optimization**
   - Profile CPU usage during playback
   - Consider NEON SIMD for color conversion (NV12→BGRA)
   - Experiment with different `VIDEO_SCALE_FACTOR` values

3. **Error handling**
   - Handle unsupported codecs gracefully
   - Show error messages for failed playback
   - Handle file access errors

### Medium Priority

4. **Hardware acceleration** (see `experiments/HARDWARE_DECODING_NOTES.md`)
   - Investigate OMX video output (`--vout=omxil_vout`)
   - Research GStreamer + libpalmvideodecoder.so
   - Look at mplayer-webos for hardware decoding approach

5. **Additional features**
   - Subtitle support
   - Audio track selection
   - Playlist support
   - Resume playback from last position

### Low Priority

6. **Polish**
   - App icon design
   - About dialog
   - Settings persistence

---

## Project Structure Created

```
vlc-qt/webos/
├── sdk/qt5-arm/           # Qt 5.9.7 ARM SDK (copied from qupzilla-webos)
├── device/                # Device headers (ALSA, OpenGL ES, OpenSSL)
├── packages/              # Runtime dependencies (.ipk files)
├── cmake/
│   └── webos-arm-toolchain.cmake   # CMake cross-compilation toolchain
├── app/                   # Sample VLC Player application
│   ├── CMakeLists.txt
│   ├── main.cpp
│   ├── MainWindow.h/cpp
│   └── appinfo.json       # webOS app manifest
├── build-vlc-qt.sh        # Build VLC-Qt libraries
├── build-libvlc.sh        # Build libVLC from source
└── package-ipk.sh         # Create webOS IPK package
```

## Key Files

| File | Purpose |
|------|---------|
| `WEBOS_BUILD_PLAN.md` | Comprehensive build plan |
| `webos/cmake/webos-arm-toolchain.cmake` | CMake toolchain for ARMv7 |
| `webos/build-vlc-qt.sh` | Script to build VLC-Qt |
| `webos/build-libvlc.sh` | Script to build libVLC + FFmpeg |
| `webos/app/*` | Sample Qt5 VLC player app |
| `webos/package-ipk.sh` | Create webOS IPK package |

---

## Build Steps (Critical Path)

### Step 1: Build libVLC for ARM

**This is the most complex and time-consuming step.**

```bash
cd /home/jonwise/Projects/vlc-qt/webos
./build-libvlc.sh
```

This script will:
- Download FFmpeg 3.4.13 and VLC 2.2.8 source code
- Cross-compile FFmpeg for ARMv7
- Cross-compile VLC with minimal plugins for webOS
- Install to `webos/vlc-arm/`

**Time required**: 1-2 hours
**Disk space**: ~10GB

**Alternative**: If you have pre-built libVLC ARM binaries, place them in:
```
webos/vlc-arm/
├── include/
│   └── vlc/
│       ├── vlc.h
│       ├── libvlc.h
│       └── ...
├── lib/
│   ├── libvlc.so
│   ├── libvlccore.so
│   └── ...
└── plugins/
    └── ... (VLC plugins)
```

### Step 2: Build VLC-Qt Libraries

```bash
cd /home/jonwise/Projects/vlc-qt/webos
./build-vlc-qt.sh
```

This builds the VLC-Qt wrapper libraries:
- `libVLCQtCore.so` - Core VLC wrapper
- `libVLCQtWidgets.so` - Qt widget components

Output will be in `build-webos/`.

### Step 3: Build the Player Application

```bash
cd /home/jonwise/Projects/vlc-qt/webos/app
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=../../cmake/webos-arm-toolchain.cmake
make
```

This builds the sample VLC player application (`vlcplayer`).

### Step 4: Create webOS Package

```bash
cd /home/jonwise/Projects/vlc-qt/webos
./package-ipk.sh
```

This creates `org.webosarchive.vlcplayer_1.0.0_armv7.ipk` in `package-staging/`.

---

## Device Installation

### Prerequisites

Install these packages on the webOS device first (from `webos/related-packages/`):

1. `com.nizovn.glibc_4.8-2015.06-0_armv7.ipk` - Modern glibc
2. `com.nizovn.qt5_5.9.7-0_armv7.ipk` - Qt 5.9.7 runtime
3. `com.nizovn.qt5qpaplugins_1.0.3_armv7.ipk` - QPA plugins
4. `com.nizovn.openssl_1.0.2p_armv7.ipk` - OpenSSL
5. `org.webosinternals.dbus_1.4.16-3_armv7.ipk` - D-Bus

### Install VLC Player

```bash
# Install using Palm SDK (device must be connected via USB or novacom)
/opt/PalmSDK/0.2/bin/palm-install package-staging/org.webosarchive.vlcplayer_1.0.0_all.ipk
```

---

## Troubleshooting

### libVLC build fails

Common issues:
- Missing zlib: Cross-compile zlib first or use system headers
- Linker errors: Check `LDFLAGS` include device library paths
- FFmpeg not found: Ensure `PKG_CONFIG_PATH` includes FFmpeg install

### VLC-Qt build fails

- Check that libVLC was built and installed to `webos/vlc-arm/`
- Verify toolchain file paths are correct for your system

### App crashes on device

- Check VLC_PLUGIN_PATH environment variable
- Verify all .so dependencies are in the package
- Check `journalctl` or `/var/log/messages` for errors

---

## Customization

### Change App ID

Edit these files:
- `webos/app/appinfo.json` - Change `"id"` field
- `webos/cmake/webos-arm-toolchain.cmake` - Change `WEBOS_APP_ID`
- `webos/package-ipk.sh` - Change `APP_ID` variable

### Add App Icon

Replace `webos/app/icon.png` with a 64x64 PNG icon.

### Modify VLC Arguments

Edit `MainWindow.cpp` in the `setupVLC()` function to change VLC initialization arguments.

---

## Architecture Notes

### Video Output
The app uses **direct framebuffer rendering** (`FBVideoWidget`), not OpenGL:
- Writes directly to `/dev/fb0` memory
- Bypasses Qt's rendering to avoid z-order conflicts
- TouchPad framebuffer: 1024×768, 32bpp, triple-buffered (yres_virtual=2304)
- Must handle `yoffset` for correct page targeting

VLC is configured with `--vout=vmem` (memory video output) which provides decoded frames via callbacks. The `FBVideoWidget` receives these frames and renders them to the framebuffer.

### Audio Output
- Uses ALSA (`--aout=alsa`)
- webOS routes through PulseAudio internally

### Memory
- HP TouchPad has ~1GB RAM
- VLC + Qt5 can use 150-300MB
- Video scaled down by `VIDEO_SCALE_FACTOR` to reduce memory usage
- Avoid HD video if memory-constrained

### Qt/Video Layer Management
Critical insight: Qt widgets and direct framebuffer writes conflict. Solution:
- Hide Qt UI during video playback
- Show Qt UI when paused/stopped
- Use `firstFrameReady` signal to coordinate UI hiding (not `playing` signal)
- Gate frame rendering on `m_isPlaying` to prevent overwriting Qt when paused

---

## References

- [VLC-Qt Documentation](https://vlc-qt.tano.si/)
- [VLC Compilation Guide](https://wiki.videolan.org/Compile_VLC/)
- [webOS Homebrew](https://www.webos-ports.org/)
- [Qt Cross-Compilation](https://doc.qt.io/qt-5/configure-linux-device.html)
