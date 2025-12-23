/**
 * OpenGL Video Widget for webOS - GPU-accelerated rendering
 * Uses OpenGL ES 2.0 textures for video display
 */

#ifndef GLVIDEOWIDGET_H
#define GLVIDEOWIDGET_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QMutex>

#include <vlc/vlc.h>

class VlcMediaPlayer;

class GLVideoWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    explicit GLVideoWidget(QWidget *parent = nullptr);
    ~GLVideoWidget();

    void setMediaPlayer(VlcMediaPlayer *player);

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;

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
    QByteArray m_buffer[2];
    int m_writeBuffer;
    int m_readBuffer;
    unsigned m_width;
    unsigned m_height;
    unsigned m_textureWidth;   // Allocated texture size
    unsigned m_textureHeight;
    bool m_hasFrame;
    bool m_textureNeedsUpdate;
    bool m_textureAllocated;   // True after first glTexImage2D

    // OpenGL
    GLuint m_textureId;
    GLuint m_program;
    GLuint m_vbo;
    bool m_glInitialized;
};

#endif // GLVIDEOWIDGET_H
