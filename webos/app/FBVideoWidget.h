/**
 * Framebuffer Video Widget for webOS
 * Bypasses Qt rendering - writes directly to /dev/fb0
 */

#ifndef FBVIDEOWIDGET_H
#define FBVIDEOWIDGET_H

#include <QWidget>
#include <QMutex>
#include <QByteArray>
#include <QMouseEvent>

#include <vlc/vlc.h>

class VlcMediaPlayer;

class FBVideoWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FBVideoWidget(QWidget *parent = nullptr);
    ~FBVideoWidget();

    void setMediaPlayer(VlcMediaPlayer *player);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

signals:
    void tapped();           // Emitted when user taps during playback
    void firstFrameReady();  // Emitted when first frame is rendered

public slots:
    void onPlaybackStarted();
    void onPlaybackStopped();

private slots:
    void onFrameReady();
    void forceSeek();

private:
    void updateRenderPosition();

    // Framebuffer management
    bool openFramebuffer();
    void closeFramebuffer();
    void renderToFramebuffer();
    void clearVideoRegion();

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

    // Double buffering for VLC frames
    QByteArray m_buffer[2];
    int m_writeBuffer;
    int m_readBuffer;
    unsigned m_videoWidth;
    unsigned m_videoHeight;
    bool m_hasFrame;

    // Framebuffer
    int m_fbFd;
    unsigned char *m_fbMem;
    size_t m_fbSize;
    unsigned m_fbWidth;
    unsigned m_fbHeight;
    unsigned m_fbStride;
    unsigned m_fbBpp;
    bool m_fbOpen;

    // Widget position on screen (for direct FB rendering)
    int m_screenX;
    int m_screenY;
    int m_renderWidth;
    int m_renderHeight;

    // Playback state
    bool m_isPlaying;
    bool m_firstFrameRendered;
};

#endif // FBVIDEOWIDGET_H
