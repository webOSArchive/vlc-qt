# VLC-Qt webOS Port

This directory contains the webOS port of VLC-Qt for legacy webOS devices (HP TouchPad, Pre3).

## Prerequisites

### On the Device

Install these packages from `related-packages/` on your webOS device:

```bash
palm-install related-packages/com.nizovn.glibc_*.ipk
palm-install related-packages/com.nizovn.qt5_*.ipk
palm-install related-packages/com.nizovn.qt5qpaplugins_*.ipk
palm-install related-packages/com.nizovn.openssl_*.ipk
palm-install related-packages/com.nizovn.qt5sdk_*.ipk
palm-install related-packages/com.nizovn.cacert_*.ipk
palm-install related-packages/org.webosinternals.dbus_*.ipk
```

### On the Build Machine

- Linaro GCC 4.9.4 toolchain at `/opt/gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabi/`
- Palm SDK at `/opt/PalmSDK/`
- Palm PDK at `/opt/PalmPDK/`
- Qt5 ARM SDK (see `sdk/` folder)
- novacom for device communication

## Building

```bash
./build-and-package.sh        # Full build + package
./build-and-package.sh clean  # Clean build
```

Or manually:
```bash
cd app && mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=../../cmake/webos-arm-toolchain.cmake
make -j$(nproc)
cd ../..
./package-ipk.sh
```

## Installation

```bash
palm-install package-staging/org.webosarchive.vlcplayer_1.0.0_all.ipk
palm-launch org.webosarchive.vlcplayer
```

## Directory Structure

```
webos/
├── app/                    # VLC Player application source
│   ├── appinfo.json       # webOS app manifest
│   ├── CMakeLists.txt
│   └── *.cpp/*.h          # Application code
├── cmake/                  # CMake toolchain files
│   └── webos-arm-toolchain.cmake
├── device/                 # Device SDK (headers & libs for cross-compilation)
│   ├── include/           # EGL, KHR, ALSA, OpenSSL headers
│   ├── lib/               # Device libraries
│   └── sysroot/           # glibc headers
├── related-packages/       # Dependency IPKs
├── sdk/                    # Qt5 ARM SDK for cross-compilation
├── vlc-arm/               # Pre-built libVLC for ARM
├── build-and-package.sh   # Main build script
├── package-ipk.sh         # IPK packaging script
└── setup-device-sdk.sh    # Script to regenerate device/ folder
```

## webOS Jail System & qt5sdk

### The Problem

webOS runs apps in a sandbox ("jail") as the `prisoner` user. Apps can only access their own directory, but Qt5 apps need access to shared libraries installed as separate packages.

### The Solution

The `com.nizovn.qt5sdk` package provides:

1. **Custom jailer wrapper** (`/usr/bin/jailer`) - Intercepts app launches and checks for `qt5sdk` in appinfo.json
2. **Qt5 jail config** (`/etc/jail_qt5.conf`) - Mounts external package directories into the app's jail

### How It Works

1. App is launched via LunaSysMgr
2. Jailer wrapper reads app's `appinfo.json`
3. If `qt5sdk` section exists, jailer:
   - Parses `exports` array and sets environment variables
   - Uses `-t qt5` jail type instead of default
4. `jail_qt5.conf` mounts these directories into the jail:
   - `com.nizovn.glibc`
   - `com.nizovn.qt5`
   - `com.nizovn.qt5qpaplugins`
   - `com.nizovn.openssl`
   - `com.nizovn.cacert`
   - `org.webosinternals.dbus`
5. App binary executes with access to all shared libraries

### appinfo.json Example

```json
{
    "title": "VLC Player",
    "id": "org.webosarchive.vlcplayer",
    "version": "1.0.0",
    "type": "pdk",
    "main": "bin/vlcplayer",
    "icon": "icon.png",
    "qt5sdk": {
        "exports": [
            "QMLSCENE_DEVICE=softwarecontext",
            "VLC_PLUGIN_PATH=/media/cryptofs/apps/usr/palm/applications/org.webosarchive.vlcplayer/plugins/vlc"
        ]
    },
    "requiredMemory": 150
}
```

**Important notes:**
- `main` should point to the binary directly (not a wrapper script)
- `exports` array must not be empty if present (use `{}` or omit entirely if no exports needed)
- Environment variables in `exports` are set before the app launches

## Troubleshooting

### App doesn't launch from icon but works from command line

**Cause**: Stale jail cache from before `qt5sdk` was added.

**Fix**: Nuke the jail and relaunch:
```bash
novacom run file:///usr/bin/jailer -- -N -i org.webosarchive.vlcplayer
palm-launch org.webosarchive.vlcplayer
```

### "not found" error when executing binary

**Cause**: The ELF interpreter (dynamic linker) path in the binary isn't accessible.

**Check**: Verify the binary's interpreter:
```bash
readelf -l app/build/vlcplayer | grep interpreter
# Should show: /media/cryptofs/apps/usr/palm/applications/com.nizovn.glibc/lib/ld.so
```

**Fix**: Ensure `com.nizovn.glibc` is installed and the app uses qt5sdk jail.

### VLC can't find plugins

**Cause**: `VLC_PLUGIN_PATH` environment variable not set.

**Fix**: Add to exports in appinfo.json:
```json
"exports": [
    "VLC_PLUGIN_PATH=/media/cryptofs/apps/usr/palm/applications/org.webosarchive.vlcplayer/plugins/vlc"
]
```

### How to verify jail is correct

Check what's mounted in the jail:
```bash
novacom run file:///bin/sh -- -c 'ls /var/palm/jail/org.webosarchive.vlcplayer/media/cryptofs/apps/usr/palm/applications/'
```

Should show:
```
com.nizovn.cacert
com.nizovn.glibc
com.nizovn.openssl
com.nizovn.qt5
com.nizovn.qt5qpaplugins
org.webosarchive.vlcplayer
org.webosinternals.dbus
```

### Checking logs

```bash
# Jailer logs
novacom run file:///bin/sh -- -c 'tail -50 /var/log/messages | grep jailer'

# Look for "entering qt5 jail"
novacom run file:///bin/sh -- -c 'tail -50 /var/log/messages | grep qt5'
```

## References

- QupZilla browser (`~/Projects/qupzilla`) - Working Qt5 webOS app example
- webOS Internals wiki
- Palm PDK documentation
