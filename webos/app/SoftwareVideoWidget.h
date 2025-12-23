/**
 * Software Video Widget for webOS
 * Uses VLC's vmem output to render video frames via QPainter
 */

#ifndef SOFTWAREVIDEOWIDGET_H
#define SOFTWAREVIDEOWIDGET_H

#include <QWidget>
#include <QImage>
#include <QMutex>

#include "AbstractVideoStream.h"

class VlcMediaPlayer;

class SoftwareVideoWidget : public QWidget, public VlcAbstractVideoStream
{
    Q_OBJECT

public:
    explicit SoftwareVideoWidget(VlcMediaPlayer *player, QWidget *parent = nullptr);
    ~SoftwareVideoWidget();

    void setMediaPlayer(VlcMediaPlayer *player);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

    // VlcAbstractVideoStream callbacks
    void *lockCallback(void **planes) override;
    void unlockCallback(void *picture, void *const *planes) override;
    void displayCallback(void *picture) override;
    unsigned formatCallback(char *chroma, unsigned *width, unsigned *height,
                           unsigned *pitches, unsigned *lines) override;
    void formatCleanUpCallback() override;

private slots:
    void frameReady();

private:
    VlcMediaPlayer *m_player;
    QMutex m_mutex;

    // Frame buffer
    QImage m_frame;
    QByteArray m_buffer;
    unsigned m_videoWidth;
    unsigned m_videoHeight;
    bool m_frameReady;
};

#endif // SOFTWAREVIDEOWIDGET_H
