/**
 * Framebuffer Video Widget for webOS
 * Bypasses Qt rendering - writes directly to /dev/fb0
 *
 * When playing: Qt UI is hidden, video renders fullscreen
 * When paused: Qt UI is shown, video clears
 */

#include "FBVideoWidget.h"

#include <QDebug>
#include <QFile>
#include <QTextStream>

#include <sys/ioctl.h>

// Debug logging to file (stderr doesn't go to syslog on webOS)
static FILE *g_logFile = nullptr;

static void logMsg(const char *fmt, ...) {
    if (!g_logFile) {
        g_logFile = fopen("/media/internal/vlcplayer.log", "a");
        if (g_logFile) {
            fprintf(g_logFile, "\n=== VLC Player Started ===\n");
            fflush(g_logFile);
        }
    }
    if (g_logFile) {
        va_list args;
        va_start(args, fmt);
        vfprintf(g_logFile, fmt, args);
        va_end(args);
        fflush(g_logFile);
    }
}

#include <sys/mman.h>
#include <linux/fb.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>

#include "MediaPlayer.h"

// Scale factors for reduced resolution based on source size
// 480p and below: scale by 2 (~240x180)
// 720p: scale by 5 (~256x144)
// 1080p+: scale by 8 (~240x135)
#define VIDEO_SCALE_FACTOR_SD 2
#define VIDEO_SCALE_FACTOR_HD 5
#define VIDEO_SCALE_FACTOR_FHD 8

FBVideoWidget::FBVideoWidget(QWidget *parent)
    : QWidget(parent),
      m_player(nullptr),
      m_writeBuffer(0),
      m_readBuffer(1),
      m_videoWidth(0),
      m_videoHeight(0),
      m_hasFrame(false),
      m_fbFd(-1),
      m_fbMem(nullptr),
      m_fbSize(0),
      m_fbWidth(0),
      m_fbHeight(0),
      m_fbStride(0),
      m_fbBpp(32),
      m_fbOpen(false),
      m_screenX(0),
      m_screenY(0),
      m_renderWidth(0),
      m_renderHeight(0),
      m_isPlaying(false),
      m_firstFrameRendered(false)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_NoSystemBackground);
    setAutoFillBackground(false);

    openFramebuffer();

    // When playing, we render fullscreen - set render region to full FB
    m_screenX = 0;
    m_screenY = 0;
    m_renderWidth = m_fbWidth;
    m_renderHeight = m_fbHeight;
}

FBVideoWidget::~FBVideoWidget()
{
    if (m_player) {
        libvlc_video_set_callbacks(m_player->core(), nullptr, nullptr, nullptr, nullptr);
        libvlc_video_set_format_callbacks(m_player->core(), nullptr, nullptr);
    }
    clearVideoRegion();
    closeFramebuffer();
}

bool FBVideoWidget::openFramebuffer()
{
    m_fbFd = open("/dev/fb0", O_RDWR);
    if (m_fbFd < 0) {
        logMsg( "FBVideoWidget: Failed to open /dev/fb0: %s\n", strerror(errno));
                return false;
    }

    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;

    if (ioctl(m_fbFd, FBIOGET_FSCREENINFO, &finfo) < 0) {
        logMsg( "FBVideoWidget: FBIOGET_FSCREENINFO failed\n");
                ::close(m_fbFd);
        m_fbFd = -1;
        return false;
    }

    if (ioctl(m_fbFd, FBIOGET_VSCREENINFO, &vinfo) < 0) {
        logMsg( "FBVideoWidget: FBIOGET_VSCREENINFO failed\n");
                ::close(m_fbFd);
        m_fbFd = -1;
        return false;
    }

    m_fbWidth = vinfo.xres;
    m_fbHeight = vinfo.yres;
    m_fbBpp = vinfo.bits_per_pixel;
    m_fbStride = finfo.line_length;
    m_fbSize = finfo.smem_len;

    logMsg( "FBVideoWidget: FB info: %ux%u, %u bpp, stride=%u, size=%zu\n",
            m_fbWidth, m_fbHeight, m_fbBpp, m_fbStride, m_fbSize);
    logMsg( "FBVideoWidget: FB virtual: %ux%u, offset: %u,%u\n",
            vinfo.xres_virtual, vinfo.yres_virtual,
            vinfo.xoffset, vinfo.yoffset);
    
    m_fbMem = (unsigned char *)mmap(nullptr, m_fbSize, PROT_READ | PROT_WRITE, MAP_SHARED, m_fbFd, 0);
    if (m_fbMem == MAP_FAILED) {
        logMsg( "FBVideoWidget: mmap failed: %s\n", strerror(errno));
                ::close(m_fbFd);
        m_fbFd = -1;
        m_fbMem = nullptr;
        return false;
    }

    m_fbOpen = true;
    logMsg( "FBVideoWidget: Framebuffer opened successfully\n");
        return true;
}

void FBVideoWidget::closeFramebuffer()
{
    if (m_fbMem && m_fbMem != MAP_FAILED) {
        munmap(m_fbMem, m_fbSize);
        m_fbMem = nullptr;
    }
    if (m_fbFd >= 0) {
        ::close(m_fbFd);
        m_fbFd = -1;
    }
    m_fbOpen = false;
}

void FBVideoWidget::setMediaPlayer(VlcMediaPlayer *player)
{
    if (m_player) {
        libvlc_video_set_callbacks(m_player->core(), nullptr, nullptr, nullptr, nullptr);
        libvlc_video_set_format_callbacks(m_player->core(), nullptr, nullptr);
    }

    m_player = player;

    if (m_player) {
        libvlc_media_player_t *mp = m_player->core();
        logMsg( "FBVideoWidget: Setting callbacks on player %p\n", (void*)mp);
        
        libvlc_video_set_callbacks(mp,
                                   lockCallback,
                                   unlockCallback,
                                   displayCallback,
                                   this);
        libvlc_video_set_format_callbacks(mp,
                                          formatCallback,
                                          formatCleanupCallback);
        logMsg( "FBVideoWidget: Callbacks set successfully\n");
            }
}

void FBVideoWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    // Do nothing - we render directly to framebuffer when playing
}

void FBVideoWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
}

void FBVideoWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
}

void FBVideoWidget::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
}

void FBVideoWidget::mousePressEvent(QMouseEvent *event)
{
    Q_UNUSED(event);

    // If playing, user tapped - emit signal so MainWindow can pause and show UI
    if (m_isPlaying) {
        logMsg( "FBVideoWidget: Tapped during playback\n");
                emit tapped();
    }
}

void FBVideoWidget::updateRenderPosition()
{
    // Always render fullscreen when playing
    m_screenX = 0;
    m_screenY = 0;
    m_renderWidth = m_fbWidth;
    m_renderHeight = m_fbHeight;

    logMsg( "FBVideoWidget: Render region (fullscreen): %d,%d %dx%d\n",
            m_screenX, m_screenY, m_renderWidth, m_renderHeight);
    }

void FBVideoWidget::clearVideoRegion()
{
    if (!m_fbOpen || !m_fbMem) return;

    logMsg( "FBVideoWidget: Clearing video region\n");
    
    // Clear entire framebuffer to black
    memset(m_fbMem, 0, m_fbSize);
}

void FBVideoWidget::renderToFramebuffer()
{
    static int renderCount = 0;

    if (!m_fbOpen || !m_fbMem) {
        if (renderCount < 5) logMsg("FBVideoWidget: renderToFB - FB not open\n");
        return;
    }
    if (!m_hasFrame) {
        if (renderCount < 5) logMsg("FBVideoWidget: renderToFB - no frame yet\n");
        return;
    }
    if (m_videoWidth == 0 || m_videoHeight == 0) {
        if (renderCount < 5) logMsg("FBVideoWidget: renderToFB - video size 0\n");
        return;
    }

    // Get current display page offset (for triple buffering)
    struct fb_var_screeninfo vinfo;
    unsigned int pageYOffset = 0;
    if (ioctl(m_fbFd, FBIOGET_VSCREENINFO, &vinfo) == 0) {
        pageYOffset = vinfo.yoffset;
        if (renderCount < 5) {
            logMsg("FBVideoWidget: FB yoffset=%u (page %u)\n",
                   pageYOffset, pageYOffset / m_fbHeight);
        }
    }

    renderCount++;
    if (renderCount <= 10 || renderCount % 100 == 0) {
        logMsg("FBVideoWidget: renderToFB #%d, video %ux%u -> FB %ux%u (yoff=%u)\n",
               renderCount, m_videoWidth, m_videoHeight, m_fbWidth, m_fbHeight, pageYOffset);
    }

    m_mutex.lock();

    const unsigned char *src = reinterpret_cast<const unsigned char*>(m_buffer[m_readBuffer].constData());
    unsigned srcWidth = m_videoWidth;
    unsigned srcHeight = m_videoHeight;
    unsigned srcStride = srcWidth * 4;

    // Calculate aspect-correct target rectangle (fullscreen)
    float videoAspect = (float)srcWidth / (float)srcHeight;
    float screenAspect = (float)m_fbWidth / (float)m_fbHeight;

    int targetW, targetH, targetX, targetY;
    if (videoAspect > screenAspect) {
        targetW = m_fbWidth;
        targetH = (int)(m_fbWidth / videoAspect);
        targetX = 0;
        targetY = (m_fbHeight - targetH) / 2;
    } else {
        targetH = m_fbHeight;
        targetW = (int)(m_fbHeight * videoAspect);
        targetX = (m_fbWidth - targetW) / 2;
        targetY = 0;
    }

    // Adjust for current page offset (triple buffering)
    targetY += pageYOffset;

    // Fixed-point scale factors (16.16 format for speed)
    unsigned int scaleX_fp = (srcWidth << 16) / targetW;
    unsigned int scaleY_fp = (srcHeight << 16) / targetH;

    // Page bounds for triple buffering
    int pageTop = pageYOffset;
    int pageBottom = pageYOffset + m_fbHeight;

    // Render directly to framebuffer with nearest-neighbor scaling
    for (int y = 0; y < targetH; y++) {
        int fbY = targetY + y;
        if (fbY < pageTop || fbY >= pageBottom) continue;

        unsigned int *dstRow = (unsigned int *)(m_fbMem + fbY * m_fbStride + targetX * 4);
        int srcY = (y * scaleY_fp) >> 16;
        if (srcY >= (int)srcHeight) srcY = srcHeight - 1;
        const unsigned int *srcRow = (const unsigned int *)(src + srcY * srcStride);

        unsigned int srcX_fp = 0;
        for (int x = 0; x < targetW; x++) {
            int srcX = srcX_fp >> 16;
            dstRow[x] = srcRow[srcX];
            srcX_fp += scaleX_fp;
        }
    }

    // Fill black bars (letterbox/pillarbox) - adjusted for page offset
    // Top bar
    for (int y = pageTop; y < targetY && y < pageBottom; y++) {
        memset(m_fbMem + y * m_fbStride, 0, m_fbWidth * 4);
    }
    // Bottom bar
    for (int y = targetY + targetH; y < pageBottom; y++) {
        memset(m_fbMem + y * m_fbStride, 0, m_fbWidth * 4);
    }
    // Left bar
    if (targetX > 0) {
        for (int y = targetY; y < targetY + targetH && y < pageBottom; y++) {
            if (y >= pageTop) memset(m_fbMem + y * m_fbStride, 0, targetX * 4);
        }
    }
    // Right bar
    if (targetX + targetW < (int)m_fbWidth) {
        for (int y = targetY; y < targetY + targetH && y < pageBottom; y++) {
            if (y >= pageTop) memset(m_fbMem + y * m_fbStride + (targetX + targetW) * 4, 0, (m_fbWidth - targetX - targetW) * 4);
        }
    }

    m_mutex.unlock();
}

static int frameCount = 0;

void FBVideoWidget::onFrameReady()
{
    frameCount++;

    // Only render when playing - prevents flickering when paused
    if (m_isPlaying) {
        renderToFramebuffer();

        // Emit firstFrameReady after we've actually rendered a frame
        if (!m_firstFrameRendered && m_hasFrame) {
            m_firstFrameRendered = true;
            logMsg("FBVideoWidget: First frame rendered - emitting firstFrameReady\n");
            emit firstFrameReady();
        }
    }

    if (frameCount <= 10 || frameCount % 100 == 0) {
        logMsg("FBVideoWidget: onFrameReady %d, isPlaying=%d, hasFrame=%d\n",
               frameCount, m_isPlaying, m_hasFrame);
    }
}

void FBVideoWidget::onPlaybackStarted()
{
    logMsg("FBVideoWidget: Playback started - entering fullscreen video mode\n");
    m_isPlaying = true;
    m_firstFrameRendered = false;  // Reset for new playback

    // Set fullscreen render region
    updateRenderPosition();

    // Render current frame if we have one (and trigger firstFrameReady if so)
    if (m_hasFrame) {
        renderToFramebuffer();
        if (!m_firstFrameRendered) {
            m_firstFrameRendered = true;
            logMsg("FBVideoWidget: First frame rendered (immediate) - emitting firstFrameReady\n");
            emit firstFrameReady();
        }
    }
}

void FBVideoWidget::forceSeek()
{
    // Force a micro-seek to kick-start VLC frame delivery
    if (m_player && m_isPlaying) {
        logMsg("FBVideoWidget: Executing micro-seek to kick-start frames\n");
        float pos = m_player->position();
        if (pos < 0.001f) pos = 0.001f;
        if (pos > 0.999f) pos = 0.999f;
        m_player->setPosition(pos);
    }
}

void FBVideoWidget::onPlaybackStopped()
{
    logMsg( "FBVideoWidget: Playback stopped - clearing FB for Qt UI\n");
        m_isPlaying = false;

    // Clear the framebuffer so Qt can paint
    clearVideoRegion();
}

// Static callbacks

void *FBVideoWidget::lockCallback(void *opaque, void **planes)
{
    FBVideoWidget *self = static_cast<FBVideoWidget*>(opaque);
    planes[0] = self->m_buffer[self->m_writeBuffer].data();
    return nullptr;
}

void FBVideoWidget::unlockCallback(void *opaque, void *picture, void *const *planes)
{
    Q_UNUSED(picture);
    Q_UNUSED(planes);

    FBVideoWidget *self = static_cast<FBVideoWidget*>(opaque);

    if (self->m_videoWidth > 0 && self->m_videoHeight > 0) {
        self->m_mutex.lock();
        int temp = self->m_writeBuffer;
        self->m_writeBuffer = self->m_readBuffer;
        self->m_readBuffer = temp;
        self->m_hasFrame = true;
        self->m_mutex.unlock();
    }

    QMetaObject::invokeMethod(self, "onFrameReady", Qt::QueuedConnection);
}

void FBVideoWidget::displayCallback(void *opaque, void *picture)
{
    Q_UNUSED(opaque);
    Q_UNUSED(picture);
}

unsigned FBVideoWidget::formatCallback(void **opaque, char *chroma,
                                        unsigned *width, unsigned *height,
                                        unsigned *pitches, unsigned *lines)
{
    FBVideoWidget *self = static_cast<FBVideoWidget*>(*opaque);

    logMsg( "FBVideoWidget::formatCallback %ux%u incoming chroma=%.4s\n", *width, *height, chroma);

    // Request BGRA format
    memcpy(chroma, "BGRA", 4);

    // Choose scale factor based on source resolution
    // Higher resolution = more aggressive scaling to maintain performance
    unsigned sourceHeight = *height;
    unsigned scaleFactor;
    if (sourceHeight > 900) {
        scaleFactor = VIDEO_SCALE_FACTOR_FHD;  // 1080p+ -> /6
    } else if (sourceHeight > 600) {
        scaleFactor = VIDEO_SCALE_FACTOR_HD;   // 720p -> /4
    } else {
        scaleFactor = VIDEO_SCALE_FACTOR_SD;   // 480p and below -> /2
    }

    unsigned scaledWidth = *width / scaleFactor;
    unsigned scaledHeight = *height / scaleFactor;

    // Ensure dimensions are even (required for many codecs)
    scaledWidth = (scaledWidth / 2) * 2;
    scaledHeight = (scaledHeight / 2) * 2;

    // Minimum size to avoid degenerate cases
    if (scaledWidth < 160) scaledWidth = 160;
    if (scaledHeight < 90) scaledHeight = 90;

    *width = scaledWidth;
    *height = scaledHeight;

    self->m_videoWidth = scaledWidth;
    self->m_videoHeight = scaledHeight;

    *pitches = scaledWidth * 4;
    *lines = scaledHeight;

    unsigned bufferSize = (*pitches) * (*lines);
    self->m_buffer[0].resize(bufferSize);
    self->m_buffer[0].fill(0);
    self->m_buffer[1].resize(bufferSize);
    self->m_buffer[1].fill(0);
    self->m_writeBuffer = 0;
    self->m_readBuffer = 1;

    self->updateRenderPosition();

    logMsg("FBVideoWidget: Requested BGRA at %ux%u (1/%d for %up), buffer=%u bytes\n",
           scaledWidth, scaledHeight, scaleFactor, sourceHeight, bufferSize);

    // WORKAROUND: Force a micro-seek to kick-start frame delivery
    // This needs to happen after format is negotiated
    if (self->m_player) {
        logMsg("FBVideoWidget: Format ready - forcing micro-seek to start frames\n");
        QMetaObject::invokeMethod(self, "forceSeek", Qt::QueuedConnection);
    }

    return bufferSize;
}

void FBVideoWidget::formatCleanupCallback(void *opaque)
{
    FBVideoWidget *self = static_cast<FBVideoWidget*>(opaque);

    self->m_mutex.lock();
    self->m_buffer[0].clear();
    self->m_buffer[1].clear();
    self->m_hasFrame = false;
    self->m_videoWidth = 0;
    self->m_videoHeight = 0;
    self->m_mutex.unlock();

    self->clearVideoRegion();
}
