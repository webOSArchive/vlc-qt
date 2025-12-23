/**
 * Video Widget for webOS - Software Rendering
 * Uses libvlc vmem callbacks directly
 */

#ifndef VIDEOWIDGET_H
#define VIDEOWIDGET_H

#include <QWidget>
#include <QImage>
#include <QMutex>

#include <vlc/vlc.h>

class VlcMediaPlayer;

class VideoWidget : public QWidget
{
    Q_OBJECT

public:
    explicit VideoWidget(QWidget *parent = nullptr);
    ~VideoWidget();

    void setMediaPlayer(VlcMediaPlayer *player);

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void onFrameReady();

private:
    // Static callbacks for libvlc
    static void *lockCallback(void *opaque, void **planes);
    static void unlockCallback(void *opaque, void *picture, void *const *planes);
    static void displayCallback(void *opaque, void *picture);
    static unsigned formatCallback(void **opaque, char *chroma,
                                   unsigned *width, unsigned *height,
                                   unsigned *pitches, unsigned *lines);
    static void formatCleanupCallback(void *opaque);

    VlcMediaPlayer *m_player;
    QMutex m_mutex;
    QImage m_frame;
    QByteArray m_buffer[2];  // Double buffer
    int m_writeBuffer;        // Buffer VLC writes to
    int m_readBuffer;         // Buffer we read from for display
    unsigned m_width;
    unsigned m_height;
    bool m_hasFrame;
    bool m_frameReady;        // New frame available for display
};

#endif // VIDEOWIDGET_H
