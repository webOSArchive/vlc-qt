/**
 * OpenGL ES 2.0 Video Widget for webOS
 * Uses EGL for hardware-accelerated rendering
 */

#ifndef GLESVIDEOWIDGET_H
#define GLESVIDEOWIDGET_H

#include <QWidget>
#include <QMutex>
#include <QByteArray>

#include <vlc/vlc.h>

// EGL/GLES types
typedef void *EGLDisplay;
typedef void *EGLSurface;
typedef void *EGLContext;
typedef void *EGLConfig;
typedef void *EGLNativeWindowType;
typedef void *EGLNativeDisplayType;

typedef unsigned int GLuint;
typedef int GLint;
typedef float GLfloat;
typedef unsigned int GLenum;

class VlcMediaPlayer;

class GLESVideoWidget : public QWidget
{
    Q_OBJECT

public:
    explicit GLESVideoWidget(QWidget *parent = nullptr);
    ~GLESVideoWidget();

    void setMediaPlayer(VlcMediaPlayer *player);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private slots:
    void onFrameReady();

private:
    bool initEGL();
    void cleanupEGL();
    bool initShaders();
    void renderFrame();
    void updateTexture();

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
    bool m_textureNeedsUpdate;

    // EGL handles
    EGLDisplay m_eglDisplay;
    EGLSurface m_eglSurface;
    EGLContext m_eglContext;
    EGLConfig m_eglConfig;
    bool m_eglInitialized;

    // OpenGL ES handles
    GLuint m_texture;
    GLuint m_program;
    GLuint m_vertexShader;
    GLuint m_fragmentShader;
    GLuint m_positionAttr;
    GLuint m_texCoordAttr;
    GLuint m_textureUniform;

    // Dynamic library handles
    void *m_eglLib;
    void *m_eglWebosLib;
    void *m_glesLib;

    // EGL function pointers
    EGLDisplay (*eglGetDisplay)(EGLNativeDisplayType);
    int (*eglInitialize)(EGLDisplay, int*, int*);
    int (*eglChooseConfig)(EGLDisplay, const int*, EGLConfig*, int, int*);
    EGLSurface (*eglCreateWindowSurface)(EGLDisplay, EGLConfig, EGLNativeWindowType, const int*);
    EGLContext (*eglCreateContext)(EGLDisplay, EGLConfig, EGLContext, const int*);
    int (*eglMakeCurrent)(EGLDisplay, EGLSurface, EGLSurface, EGLContext);
    int (*eglSwapBuffers)(EGLDisplay, EGLSurface);
    int (*eglDestroyContext)(EGLDisplay, EGLContext);
    int (*eglDestroySurface)(EGLDisplay, EGLSurface);
    int (*eglTerminate)(EGLDisplay);
    int (*eglGetError)();
};

#endif // GLESVIDEOWIDGET_H
