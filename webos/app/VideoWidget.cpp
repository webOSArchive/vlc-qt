/**
 * Video Widget for webOS - Software Rendering
 * Uses libvlc vmem callbacks directly
 */

#include "VideoWidget.h"

#include <QPainter>
#include <QDebug>

#include "MediaPlayer.h"

VideoWidget::VideoWidget(QWidget *parent)
    : QWidget(parent),
      m_player(nullptr),
      m_writeBuffer(0),
      m_readBuffer(1),
      m_width(0),
      m_height(0),
      m_hasFrame(false),
      m_frameReady(false)
{
    setAttribute(Qt::WA_OpaquePaintEvent);

    QPalette pal = palette();
    pal.setColor(QPalette::Window, Qt::black);
    setPalette(pal);
    setAutoFillBackground(true);
}

VideoWidget::~VideoWidget()
{
    if (m_player) {
        libvlc_video_set_callbacks(m_player->core(), nullptr, nullptr, nullptr, nullptr);
        libvlc_video_set_format_callbacks(m_player->core(), nullptr, nullptr);
    }
}

void VideoWidget::setMediaPlayer(VlcMediaPlayer *player)
{
    if (m_player) {
        libvlc_video_set_callbacks(m_player->core(), nullptr, nullptr, nullptr, nullptr);
        libvlc_video_set_format_callbacks(m_player->core(), nullptr, nullptr);
    }

    m_player = player;

    if (m_player) {
        libvlc_media_player_t *mp = m_player->core();
        fprintf(stderr, "VideoWidget: Setting callbacks on player %p\n", (void*)mp);
        fflush(stderr);

        libvlc_video_set_callbacks(mp,
                                   lockCallback,
                                   unlockCallback,
                                   displayCallback,
                                   this);
        libvlc_video_set_format_callbacks(mp,
                                          formatCallback,
                                          formatCleanupCallback);
        fprintf(stderr, "VideoWidget: Callbacks set successfully\n");
        fflush(stderr);
    }
}

static int paintCount = 0;

void VideoWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    paintCount++;

    QPainter painter(this);

    // Use fast rendering - no antialiasing or smoothing
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);

    m_mutex.lock();

    if (paintCount <= 5 || paintCount % 100 == 0) {
        fprintf(stderr, "paintEvent %d: hasFrame=%d widgetSize=%dx%d videoSize=%dx%d readBuf=%d\n",
                paintCount, m_hasFrame, width(), height(),
                m_width, m_height, m_readBuffer);
        fflush(stderr);
    }

    // Fill black background
    painter.fillRect(rect(), Qt::black);

    // Draw the frame if available
    if (m_hasFrame && m_width > 0 && m_height > 0) {
        // Create QImage directly from the read buffer (no copy needed)
        QImage frame(
            reinterpret_cast<const uchar*>(m_buffer[m_readBuffer].constData()),
            m_width,
            m_height,
            m_width * 4,
            QImage::Format_ARGB32
        );

        // Calculate target rectangle for aspect-correct display
        float videoAspect = (float)m_width / (float)m_height;
        float widgetAspect = (float)width() / (float)height();

        int targetW, targetH, targetX, targetY;
        if (videoAspect > widgetAspect) {
            targetW = width();
            targetH = (int)(width() / videoAspect);
            targetX = 0;
            targetY = (height() - targetH) / 2;
        } else {
            targetH = height();
            targetW = (int)(height() * videoAspect);
            targetX = (width() - targetW) / 2;
            targetY = 0;
        }

        // Draw scaled video frame (fast transformation mode)
        painter.drawImage(QRect(targetX, targetY, targetW, targetH), frame);
    }

    m_mutex.unlock();
}

static int updateCount = 0;
void VideoWidget::onFrameReady()
{
    updateCount++;

    // With half-resolution video (320x240), render every frame for smoother playback
    // Skip logging most frames to reduce overhead
    if (updateCount <= 10 || updateCount % 100 == 0) {
        fprintf(stderr, "onFrameReady: updateCount=%d calling update()\n", updateCount);
        fflush(stderr);
    }
    update();
}

// Static callbacks

static int frameCount = 0;

void *VideoWidget::lockCallback(void *opaque, void **planes)
{
    VideoWidget *self = static_cast<VideoWidget*>(opaque);
    // Point VLC to the write buffer - no mutex needed during write
    planes[0] = self->m_buffer[self->m_writeBuffer].data();
    return nullptr;
}

// Log every 30th frame

void VideoWidget::unlockCallback(void *opaque, void *picture, void *const *planes)
{
    Q_UNUSED(picture);
    Q_UNUSED(planes);

    VideoWidget *self = static_cast<VideoWidget*>(opaque);
    frameCount++;

    // Log first few frames unconditionally
    if (frameCount <= 5) {
        const unsigned char* data = reinterpret_cast<const unsigned char*>(self->m_buffer[self->m_writeBuffer].constData());
        fprintf(stderr, "unlockCallback: frame=%d w=%d h=%d bufSize=%d first16bytes: %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x\n",
                frameCount, self->m_width, self->m_height, self->m_buffer[self->m_writeBuffer].size(),
                data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7],
                data[8], data[9], data[10], data[11], data[12], data[13], data[14], data[15]);
        fflush(stderr);
    }

    if (self->m_width > 0 && self->m_height > 0) {
        // Swap buffers atomically
        self->m_mutex.lock();
        int temp = self->m_writeBuffer;
        self->m_writeBuffer = self->m_readBuffer;
        self->m_readBuffer = temp;
        self->m_hasFrame = true;
        self->m_frameReady = true;
        self->m_mutex.unlock();

        if (frameCount % 30 == 1) {
            fprintf(stderr, "Frame %d: %dx%d swapped buffers\n",
                    frameCount, self->m_width, self->m_height);
            fflush(stderr);
        }
    }

    // Schedule repaint
    QMetaObject::invokeMethod(self, "onFrameReady", Qt::QueuedConnection);
}

void VideoWidget::displayCallback(void *opaque, void *picture)
{
    Q_UNUSED(opaque);
    Q_UNUSED(picture);
}

// Scale factor for reduced resolution rendering (1 = full, 2 = half)
// Half resolution (320x240) performs best - more scaling adds overhead
#define VIDEO_SCALE_FACTOR 2

unsigned VideoWidget::formatCallback(void **opaque, char *chroma,
                                      unsigned *width, unsigned *height,
                                      unsigned *pitches, unsigned *lines)
{
    fprintf(stderr, "VideoWidget::formatCallback called! opaque=%p\n", (void*)*opaque);
    fflush(stderr);

    VideoWidget *self = static_cast<VideoWidget*>(*opaque);

    fprintf(stderr, "VideoWidget::formatCallback %ux%u incoming chroma=%.4s\n", *width, *height, chroma);
    fflush(stderr);

    // Request BGRA format (matches Qt's native ARGB32 format on little-endian)
    memcpy(chroma, "BGRA", 4);

    // Scale down the video resolution to reduce CPU load
    // VLC will handle the scaling during decode/convert
    unsigned scaledWidth = *width / VIDEO_SCALE_FACTOR;
    unsigned scaledHeight = *height / VIDEO_SCALE_FACTOR;

    // Ensure dimensions are even (required for some video formats)
    scaledWidth = (scaledWidth / 2) * 2;
    scaledHeight = (scaledHeight / 2) * 2;

    // Tell VLC we want the scaled size
    *width = scaledWidth;
    *height = scaledHeight;

    self->m_width = scaledWidth;
    self->m_height = scaledHeight;

    *pitches = self->m_width * 4;  // 4 bytes per pixel for BGRA
    *lines = self->m_height;

    unsigned bufferSize = (*pitches) * (*lines);
    // Allocate double buffers
    self->m_buffer[0].resize(bufferSize);
    self->m_buffer[0].fill(0);
    self->m_buffer[1].resize(bufferSize);
    self->m_buffer[1].fill(0);
    self->m_writeBuffer = 0;
    self->m_readBuffer = 1;

    fprintf(stderr, "Requested chroma=BGRA at scaled %ux%u (1/%d), buffer=%u bytes\n",
            scaledWidth, scaledHeight, VIDEO_SCALE_FACTOR, bufferSize);
    fflush(stderr);

    return bufferSize;
}

void VideoWidget::formatCleanupCallback(void *opaque)
{
    VideoWidget *self = static_cast<VideoWidget*>(opaque);

    self->m_mutex.lock();
    self->m_buffer[0].clear();
    self->m_buffer[1].clear();
    self->m_frame = QImage();
    self->m_hasFrame = false;
    self->m_frameReady = false;
    self->m_width = 0;
    self->m_height = 0;
    self->m_mutex.unlock();
}
