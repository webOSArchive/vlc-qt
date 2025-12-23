/**
 * Framebuffer Video Widget for webOS
 * Bypasses Qt rendering - writes directly to /dev/fb0
 */

#include "FBVideoWidget.h"

#include <QPainter>
#include <QDebug>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "MediaPlayer.h"

// Scale factor for reduced resolution
#define VIDEO_SCALE_FACTOR 2

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
      m_renderHeight(0)
{
    // Make widget transparent - we render directly to FB
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_NoSystemBackground);

    QPalette pal = palette();
    pal.setColor(QPalette::Window, Qt::black);
    setPalette(pal);
    setAutoFillBackground(true);

    openFramebuffer();
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
        fprintf(stderr, "FBVideoWidget: Failed to open /dev/fb0: %s\n", strerror(errno));
        fflush(stderr);
        return false;
    }

    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;

    if (ioctl(m_fbFd, FBIOGET_FSCREENINFO, &finfo) < 0) {
        fprintf(stderr, "FBVideoWidget: FBIOGET_FSCREENINFO failed\n");
        fflush(stderr);
        ::close(m_fbFd);
        m_fbFd = -1;
        return false;
    }

    if (ioctl(m_fbFd, FBIOGET_VSCREENINFO, &vinfo) < 0) {
        fprintf(stderr, "FBVideoWidget: FBIOGET_VSCREENINFO failed\n");
        fflush(stderr);
        ::close(m_fbFd);
        m_fbFd = -1;
        return false;
    }

    m_fbWidth = vinfo.xres;
    m_fbHeight = vinfo.yres;
    m_fbBpp = vinfo.bits_per_pixel;
    m_fbStride = finfo.line_length;
    m_fbSize = finfo.smem_len;

    fprintf(stderr, "FBVideoWidget: FB info: %ux%u, %u bpp, stride=%u, size=%zu\n",
            m_fbWidth, m_fbHeight, m_fbBpp, m_fbStride, m_fbSize);
    fprintf(stderr, "FBVideoWidget: FB format: R=%u/%u G=%u/%u B=%u/%u A=%u/%u\n",
            vinfo.red.offset, vinfo.red.length,
            vinfo.green.offset, vinfo.green.length,
            vinfo.blue.offset, vinfo.blue.length,
            vinfo.transp.offset, vinfo.transp.length);
    fflush(stderr);

    m_fbMem = (unsigned char *)mmap(nullptr, m_fbSize, PROT_READ | PROT_WRITE, MAP_SHARED, m_fbFd, 0);
    if (m_fbMem == MAP_FAILED) {
        fprintf(stderr, "FBVideoWidget: mmap failed: %s\n", strerror(errno));
        fflush(stderr);
        ::close(m_fbFd);
        m_fbFd = -1;
        m_fbMem = nullptr;
        return false;
    }

    m_fbOpen = true;
    fprintf(stderr, "FBVideoWidget: Framebuffer opened successfully\n");
    fflush(stderr);
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
        fprintf(stderr, "FBVideoWidget: Setting callbacks on player %p\n", (void*)mp);
        fflush(stderr);

        libvlc_video_set_callbacks(mp,
                                   lockCallback,
                                   unlockCallback,
                                   displayCallback,
                                   this);
        libvlc_video_set_format_callbacks(mp,
                                          formatCallback,
                                          formatCleanupCallback);
        fprintf(stderr, "FBVideoWidget: Callbacks set successfully\n");
        fflush(stderr);
    }
}

void FBVideoWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    // Just fill with black - actual video is rendered directly to FB
    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);
}

void FBVideoWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateRenderPosition();
}

void FBVideoWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    updateRenderPosition();
}

void FBVideoWidget::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    clearVideoRegion();
}

void FBVideoWidget::updateRenderPosition()
{
    // Get widget position in screen coordinates
    QPoint globalPos = mapToGlobal(QPoint(0, 0));
    m_screenX = globalPos.x();
    m_screenY = globalPos.y();
    m_renderWidth = width();
    m_renderHeight = height();

    fprintf(stderr, "FBVideoWidget: Render region: %d,%d %dx%d\n",
            m_screenX, m_screenY, m_renderWidth, m_renderHeight);
    fflush(stderr);
}

void FBVideoWidget::clearVideoRegion()
{
    if (!m_fbOpen || !m_fbMem) return;

    // Clear the video region to black
    for (int y = m_screenY; y < m_screenY + m_renderHeight && y < (int)m_fbHeight; y++) {
        if (y < 0) continue;
        unsigned char *row = m_fbMem + y * m_fbStride + m_screenX * 4;
        int clearWidth = m_renderWidth;
        if (m_screenX + clearWidth > (int)m_fbWidth) {
            clearWidth = m_fbWidth - m_screenX;
        }
        if (m_screenX < 0) {
            row -= m_screenX * 4;
            clearWidth += m_screenX;
        }
        if (clearWidth > 0) {
            memset(row, 0, clearWidth * 4);
        }
    }
}

void FBVideoWidget::renderToFramebuffer()
{
    if (!m_fbOpen || !m_fbMem || !m_hasFrame || m_videoWidth == 0 || m_videoHeight == 0) {
        return;
    }

    m_mutex.lock();

    const unsigned char *src = reinterpret_cast<const unsigned char*>(m_buffer[m_readBuffer].constData());
    unsigned srcWidth = m_videoWidth;
    unsigned srcHeight = m_videoHeight;
    unsigned srcStride = srcWidth * 4;

    // Calculate aspect-correct target rectangle
    float videoAspect = (float)srcWidth / (float)srcHeight;
    float widgetAspect = (float)m_renderWidth / (float)m_renderHeight;

    int targetW, targetH, targetX, targetY;
    if (videoAspect > widgetAspect) {
        targetW = m_renderWidth;
        targetH = (int)(m_renderWidth / videoAspect);
        targetX = 0;
        targetY = (m_renderHeight - targetH) / 2;
    } else {
        targetH = m_renderHeight;
        targetW = (int)(m_renderHeight * videoAspect);
        targetX = (m_renderWidth - targetW) / 2;
        targetY = 0;
    }

    // Fixed-point scale factors (16.16 format for speed)
    unsigned int scaleX_fp = (srcWidth << 16) / targetW;
    unsigned int scaleY_fp = (srcHeight << 16) / targetH;

    // Render directly to framebuffer with nearest-neighbor scaling
    // Optimized with fixed-point math and 32-bit word copies
    for (int y = 0; y < targetH; y++) {
        int fbY = m_screenY + targetY + y;
        if (fbY < 0 || fbY >= (int)m_fbHeight) continue;

        unsigned int *dstRow = (unsigned int *)(m_fbMem + fbY * m_fbStride + (m_screenX + targetX) * 4);
        int srcY = (y * scaleY_fp) >> 16;
        if (srcY >= (int)srcHeight) srcY = srcHeight - 1;
        const unsigned int *srcRow = (const unsigned int *)(src + srcY * srcStride);

        // Calculate valid pixel range
        int startX = 0;
        int endX = targetW;
        if (m_screenX + targetX < 0) startX = -(m_screenX + targetX);
        if (m_screenX + targetX + targetW > (int)m_fbWidth) endX = m_fbWidth - m_screenX - targetX;

        unsigned int srcX_fp = startX * scaleX_fp;
        for (int x = startX; x < endX; x++) {
            int srcX = srcX_fp >> 16;
            // Copy pixel as 32-bit word (BGRA)
            dstRow[x] = srcRow[srcX];
            srcX_fp += scaleX_fp;
        }
    }

    // Fill black bars
    // Top bar
    for (int y = 0; y < targetY; y++) {
        int fbY = m_screenY + y;
        if (fbY < 0 || fbY >= (int)m_fbHeight) continue;
        unsigned char *dstRow = m_fbMem + fbY * m_fbStride + m_screenX * 4;
        memset(dstRow, 0, m_renderWidth * 4);
    }
    // Bottom bar
    for (int y = targetY + targetH; y < m_renderHeight; y++) {
        int fbY = m_screenY + y;
        if (fbY < 0 || fbY >= (int)m_fbHeight) continue;
        unsigned char *dstRow = m_fbMem + fbY * m_fbStride + m_screenX * 4;
        memset(dstRow, 0, m_renderWidth * 4);
    }
    // Left bar
    if (targetX > 0) {
        for (int y = targetY; y < targetY + targetH; y++) {
            int fbY = m_screenY + y;
            if (fbY < 0 || fbY >= (int)m_fbHeight) continue;
            unsigned char *dstRow = m_fbMem + fbY * m_fbStride + m_screenX * 4;
            memset(dstRow, 0, targetX * 4);
        }
    }
    // Right bar
    if (targetX + targetW < m_renderWidth) {
        int rightBarStart = targetX + targetW;
        int rightBarWidth = m_renderWidth - rightBarStart;
        for (int y = targetY; y < targetY + targetH; y++) {
            int fbY = m_screenY + y;
            if (fbY < 0 || fbY >= (int)m_fbHeight) continue;
            unsigned char *dstRow = m_fbMem + fbY * m_fbStride + (m_screenX + rightBarStart) * 4;
            memset(dstRow, 0, rightBarWidth * 4);
        }
    }

    m_mutex.unlock();
}

static int frameCount = 0;

void FBVideoWidget::onFrameReady()
{
    frameCount++;

    // Render every frame directly to framebuffer
    renderToFramebuffer();

    if (frameCount <= 5 || frameCount % 100 == 0) {
        fprintf(stderr, "FBVideoWidget: onFrameReady %d rendered to FB\n", frameCount);
        fflush(stderr);
    }
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

    fprintf(stderr, "FBVideoWidget::formatCallback %ux%u incoming chroma=%.4s\n", *width, *height, chroma);
    fflush(stderr);

    // Request BGRA format
    memcpy(chroma, "BGRA", 4);

    // Scale down resolution
    unsigned scaledWidth = *width / VIDEO_SCALE_FACTOR;
    unsigned scaledHeight = *height / VIDEO_SCALE_FACTOR;
    scaledWidth = (scaledWidth / 2) * 2;
    scaledHeight = (scaledHeight / 2) * 2;

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

    // Update render position now that we know video size
    self->updateRenderPosition();

    fprintf(stderr, "FBVideoWidget: Requested BGRA at %ux%u (1/%d), buffer=%u bytes\n",
            scaledWidth, scaledHeight, VIDEO_SCALE_FACTOR, bufferSize);
    fflush(stderr);

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
