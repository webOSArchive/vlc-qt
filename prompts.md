# VLC-Qt webOS Port - Prompt History

This document attempts to reconstructs the prompt history used to develop the webOS port of VLC-Qt for legacy devices (HP TouchPad, Pre3) over several days. This is not a verbatim, rather Claude's memory of our interactions. Still it may be directionally useful for similar efforts.

---

## Session 1: Initial webOS Port Setup

### Prompt 1
> I want to create a VLC video player app for legacy webOS devices, specifically the HP TouchPad. The TouchPad runs webOS 3.0.5 with an ARM Cortex-A9 CPU. I have the VLC-Qt library which wraps libVLC for Qt. Help me set up a webOS app that can play video files.

### Prompt 2
> The webOS device runs apps in a sandbox (jail). My app can't find the Qt5 libraries because they're installed as a separate package at `/media/cryptofs/apps/usr/palm/applications/com.nizovn.qt5/`. How do I configure the app to access external packages?

### Prompt 3
> I found that webOS has a custom jailer that reads `jail_qt5.conf` to mount external packages. The app needs a `qt5sdk` section in `appinfo.json` to use this. Set up the appinfo.json correctly.

### Prompt 4
> The app binary needs to use a custom ELF interpreter that points to the glibc package at `/media/cryptofs/apps/usr/palm/applications/com.nizovn.glibc/lib/ld.so`. How do I configure the CMake toolchain to link with the correct RPATH and interpreter?

---

## Session 2: Video Rendering - Finding a Working Approach

### Prompt 5
> I have the basic app running but video playback shows a black screen. I'm using VLC's vmem callbacks to get video frames into Qt. The app uses QPainter to draw frames via VideoWidget. Check if the callbacks are being called and if frames are arriving.

### Prompt 6
> Frames are arriving but the performance is terrible - only about 4 FPS. The TouchPad's CPU is too slow for Qt's software rendering path. What are my options for faster rendering?

### Prompt 7
> Try OpenGL rendering. Create a GLVideoWidget that uses Qt's OpenGL widget to render video frames as textures. The TouchPad has Adreno 220 GPU which should support OpenGL ES 2.0.

### Prompt 8
> The OpenGL widget crashes the app. The error log shows issues with the GL context. Let me try a different approach - create a GLESVideoWidget that uses EGL directly without going through Qt's OpenGL abstraction.

### Prompt 9
> The EGL/GLES2 approach renders video fast but there's a layer conflict - the video and Qt UI are fighting for the same framebuffer, causing flickering. Qt is painting over my video frames.

### Prompt 10
> Try bypassing Qt completely for video rendering. Create an FBVideoWidget that writes directly to `/dev/fb0`. Use mmap to access the framebuffer memory and render frames directly.

### Prompt 11
> The framebuffer rendering works! I'm getting 20 FPS now. But the video only appears in one corner of the screen and sometimes in the wrong place. I think the TouchPad uses triple buffering.

### Prompt 12
> You're right - the TouchPad framebuffer has yres_virtual = 2304 (3x768) for triple buffering. I need to query the current yoffset before each frame and render to the correct page. Fix the rendering code to handle triple buffering.

---

## Session 3: Launching from the App Launcher

### Prompt 13
> Video playback works when I run the app from the command line via novacom, but when I launch from the webOS app launcher, nothing happens - the app doesn't start. Check the jailer logs.

### Prompt 14
> The logs show the app is entering the jail but immediately exiting. The problem might be with how appinfo.json is configured. The `main` field might need to point directly to the binary, not a shell script.

### Prompt 15
> The app still doesn't launch. I added `qt5sdk` section to appinfo.json but the cached jail doesn't have the external package mounts. How do I clear the jail cache?

### Prompt 16
> Document the solution: delete the stale jail with `novacom run file:///usr/bin/jailer -- -N -i org.webosarchive.vlcplayer` then relaunch. Also document that an empty `exports: []` array breaks the launcher.

---

## Session 4: Fixing the Qt/Framebuffer Render Layer War

### Prompt 17
> When video is playing, there's terrible flickering. The video frames appear briefly then get overwritten by Qt's black background. The Qt painting and my framebuffer writes are fighting each other.

### Prompt 18
> I tried `Qt::WA_PaintOnScreen`, `Qt::WA_OpaquePaintEvent`, and `Qt::WA_NoSystemBackground` but the flickering continues. Even blocking Qt paint events with an event filter doesn't work cleanly.

### Prompt 19
> Try a different approach: complete layer separation. When video is playing, hide all Qt UI widgets (title bar, controls) and render video fullscreen. When paused, show the Qt UI and clear the framebuffer.

### Prompt 20
> The layer separation works! But there's a brief black screen flash when video starts. The UI hides before the first video frame is rendered.

### Prompt 21
> The issue is that VLC calls formatCallback and starts delivering frames BEFORE emitting the "playing" signal. Add a `firstFrameReady` signal that fires after we've actually rendered a frame, and hide the UI on that signal instead of "playing".

### Prompt 22
> Also, when I pause the video, there's flickering as a few buffered frames continue to render over the Qt UI. Track the playing state and only render frames when `m_isPlaying` is true.

---

## Session 5: Fixing Video Decode for Non-Native Formats

### Prompt 23
> Many video files show a black screen even though the app seems to be playing. The callbacks are being called but the frame buffers are all zeros. Some videos work fine (mp4 with h264) but others don't (mkv, avi).

### Prompt 24
> The TouchPad's omxil hardware decoder only supports certain formats. When it gets a format it can't handle, it outputs NV12 with empty frames instead of failing. Force VLC to use FFmpeg's software decoder instead with `--codec=avcodec,none` and `--avcodec-hw=none`.

### Prompt 25
> Video works now but HD videos (720p, 1080p) play too slowly - the CPU can't keep up with software decoding. Add VLC decoder optimizations: skip loop filter, skip IDCT on all frames, allow frame skipping, enable fast mode and direct rendering.

### Prompt 26
> Add dynamic resolution scaling based on source resolution. For 480p videos scale by /2, for 720p scale by /5, for 1080p+ scale by /8. This keeps decoded resolution manageable for the slow CPU.

---

## Session 6: I420 YUV->RGB Optimization Attempt

### Prompt 27
> Currently VLC uses swscale to convert from YUV to BGRA before giving us frames. We're scaling the BGRA frames when rendering to framebuffer. Could we avoid this by requesting I420 directly and doing our own YUV->RGB conversion?

### Prompt 28
> Implement an I420 code path: request I420 chroma from VLC, receive Y/U/V planes, do YUV->RGB conversion during the framebuffer render pass. This combines color conversion and scaling in one loop.

### Prompt 29
> The I420 code path is slower than BGRA! VLC's swscale is NEON-optimized on ARM and beats my simple C loop. Keep the I420 code for reference but disable it with USE_I420_CONVERSION=0.

### Prompt 30
> Document this in CLAUDE.md: swscale is faster, but a potential future optimization would be ARM NEON assembly for YUV->RGB that combines color conversion and scaling in one pass with SIMD.

---

## Session 7: HD Video Re-encoding Feature

### Prompt 31
> Even with all the optimizations, HD videos (720p+) are still choppy on the TouchPad. Instead of trying to play them directly, detect HD videos and offer to re-encode them to 480p for smooth playback.

### Prompt 32
> Create a VideoProber class that uses ffprobe to get video metadata (resolution, duration). Create an `isHD()` function that returns true for videos over 720p.

### Prompt 33
> Create a Transcoder class that runs ffmpeg to re-encode videos to 480p. Use mpeg4 codec (libx264 not available in this ffmpeg build) with reasonable bitrate. Parse ffmpeg's progress output to show percentage.

### Prompt 34
> ffmpeg needs to run via glibc's ld.so to use the newer glibc libraries. Set up the QProcess to run `/media/cryptofs/apps/usr/palm/applications/com.nizovn.glibc/lib/ld.so --library-path <paths> <ffmpeg> <args>`.

### Prompt 35
> Create a TranscodeDialog that: 1) Shows the video info and offer to transcode, 2) Shows progress during transcoding, 3) Handles completion or cancellation. After transcoding, automatically play the 480p version.

### Prompt 36
> Save the 480p version next to the original with "_480p" suffix (e.g., `movie.mp4` -> `movie_480p.mp4`). When opening an HD video, first check if the 480p version already exists and play that instead.

---

## Session 8: Documentation and Cleanup

### Prompt 37
> Update CLAUDE.md with comprehensive documentation of everything we learned: the jail system, appinfo.json configuration, framebuffer triple buffering, Qt/FB layer separation, VLC frame timing, debugging tips.

### Prompt 38
> Document the debugging approach for webOS - stderr doesn't go to syslog, so use file-based logging to `/media/internal/vlcplayer.log`.

### Prompt 39
> Create build scripts for the webOS app: `build-and-package.sh` that compiles the app and creates an IPK, and `package-ipk.sh` for packaging only.

### Prompt 40
> Commit all the changes with appropriate commit messages. Group related changes together and write clear descriptions of what each commit does.

---

## Key Technical Discoveries

1. **webOS Jail System**: Apps run sandboxed but can access external packages via `qt5sdk` section in appinfo.json and jail_qt5.conf
2. **Video Rendering**: Direct framebuffer writes (20 FPS) >> Qt QPainter (4 FPS). OpenGL has layer conflicts.
3. **Triple Buffering**: Must query yoffset before each frame and render to correct page (0, 768, or 1536)
4. **Qt/FB Coexistence**: Hide Qt UI during video, show during pause. Use firstFrameReady not playing signal.
5. **Video Decoding**: Force FFmpeg decoder (omxil silently fails on many formats). Add aggressive skip/fast options.
6. **Resolution Scaling**: Scale decoded video based on source: 480p/2, 720p/5, 1080p/8
7. **YUV->RGB**: VLC's swscale (NEON-optimized) beats custom C loop. NEON assembly could potentially beat it.
8. **HD Videos**: Re-encode to 480p with ffmpeg for smooth playback on slow CPU.
