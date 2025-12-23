/**
 * Software Video Widget for webOS
 * Uses VLC's vmem output to render video frames via QPainter
 */

#include "SoftwareVideoWidget.h"

#include <QPainter>
#include <QDebug>

#include "MediaPlayer.h"

SoftwareVideoWidget::SoftwareVideoWidget(VlcMediaPlayer *player, QWidget *parent)
    : QWidget(parent),
      m_player(nullptr),
      m_videoWidth(0),
      m_videoHeight(0),
      m_frameReady(false)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAutoFillBackground(false);

    // Set black background
    QPalette pal = palette();
    pal.setColor(QPalette::Window, Qt::black);
    setPalette(pal);

    setMediaPlayer(player);
}

SoftwareVideoWidget::~SoftwareVideoWidget()
{
    if (m_player) {
        unsetCallbacks(m_player);
    }
}

void SoftwareVideoWidget::setMediaPlayer(VlcMediaPlayer *player)
{
    if (m_player) {
        unsetCallbacks(m_player);
    }

    m_player = player;

    if (m_player) {
        setCallbacks(m_player);
    }
}

void SoftwareVideoWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    m_mutex.lock();

    if (!m_frame.isNull() && m_frameReady) {
        // Scale frame to widget size while maintaining aspect ratio
        QSize frameSize = m_frame.size();
        QSize widgetSize = size();

        frameSize.scale(widgetSize, Qt::KeepAspectRatio);

        // Center the frame
        int x = (widgetSize.width() - frameSize.width()) / 2;
        int y = (widgetSize.height() - frameSize.height()) / 2;

        // Fill letterbox/pillarbox areas with black
        painter.fillRect(rect(), Qt::black);

        // Draw the frame
        QRect targetRect(x, y, frameSize.width(), frameSize.height());
        painter.drawImage(targetRect, m_frame);
    } else {
        // No frame yet, fill with black
        painter.fillRect(rect(), Qt::black);
    }

    m_mutex.unlock();
}

void SoftwareVideoWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    update();
}

void *SoftwareVideoWidget::lockCallback(void **planes)
{
    m_mutex.lock();

    // Provide the buffer for VLC to write into
    planes[0] = m_buffer.data();

    return nullptr;
}

void SoftwareVideoWidget::unlockCallback(void *picture, void *const *planes)
{
    Q_UNUSED(picture);
    Q_UNUSED(planes);

    // Convert buffer to QImage
    if (m_videoWidth > 0 && m_videoHeight > 0 && !m_buffer.isEmpty()) {
        m_frame = QImage(reinterpret_cast<const uchar*>(m_buffer.constData()),
                         m_videoWidth, m_videoHeight,
                         m_videoWidth * 4,  // bytes per line for RGBA
                         QImage::Format_RGBA8888);
        // Make a deep copy so the buffer can be reused
        m_frame = m_frame.copy();
        m_frameReady = true;
    }

    m_mutex.unlock();

    // Schedule a repaint via event loop
    QMetaObject::invokeMethod(this, "frameReady", Qt::QueuedConnection);
}

void SoftwareVideoWidget::displayCallback(void *picture)
{
    Q_UNUSED(picture);
    // Frame is ready to display - already handled in unlockCallback
}

unsigned SoftwareVideoWidget::formatCallback(char *chroma,
                                              unsigned *width, unsigned *height,
                                              unsigned *pitches, unsigned *lines)
{
    qDebug() << "Video format:" << *width << "x" << *height;

    // Request RGBA format for easy conversion to QImage
    memcpy(chroma, "RGBA", 4);

    m_videoWidth = *width;
    m_videoHeight = *height;

    // Calculate buffer size
    unsigned pitch = m_videoWidth * 4;  // 4 bytes per pixel (RGBA)
    *pitches = pitch;
    *lines = m_videoHeight;

    unsigned bufferSize = pitch * m_videoHeight;
    m_buffer.resize(bufferSize);
    m_buffer.fill(0);

    qDebug() << "Video buffer size:" << bufferSize << "bytes";

    return bufferSize;
}

void SoftwareVideoWidget::formatCleanUpCallback()
{
    m_mutex.lock();
    m_buffer.clear();
    m_frame = QImage();
    m_frameReady = false;
    m_videoWidth = 0;
    m_videoHeight = 0;
    m_mutex.unlock();
}

void SoftwareVideoWidget::frameReady()
{
    update();
}
