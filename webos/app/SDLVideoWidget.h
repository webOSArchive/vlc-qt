/**
 * SDL/OpenGL ES Video Widget for webOS
 *
 * Uses SDL's built-in OpenGL support to avoid touch flicker on webOS.
 * Key insight from PCSX-ReARMed: SDL properly integrates with webOS's
 * 3-layer display system, while direct EGL usage causes flicker.
 *
 * Requirements:
 * - PDL_Init() must be called before SDL_Init()
 * - Link directly to libGLES_CM.so (NOT libEGL.so)
 * - Use SDL_GL_SwapBuffers() (NOT eglSwapBuffers())
 */

#ifndef SDLVIDEOWIDGET_H
#define SDLVIDEOWIDGET_H

#include <QWidget>
#include <QMutex>
#include <QByteArray>
#include <QTimer>

#include <vlc/vlc.h>

// Forward declarations
class VlcMediaPlayer;
struct SDL_Surface;

class SDLVideoWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SDLVideoWidget(QWidget *parent = nullptr);
    ~SDLVideoWidget();

    void setMediaPlayer(VlcMediaPlayer *player);

    // Check if SDL/GL initialized successfully
    bool isInitialized() const { return m_initialized; }

    // Static initialization - call before QApplication
    static bool initSDL();
    static void shutdownSDL();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

signals:
    void tapped();           // Emitted when user taps during playback
    void firstFrameReady();  // Emitted when first frame is rendered

public slots:
    void onPlaybackStarted();
    void onPlaybackStopped();

private slots:
    void onFrameReady();
    void renderFrame();

private:
    bool initGL();
    void cleanupGL();
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

    // Double buffering for VLC frames (BGRA format)
    QByteArray m_buffer[2];
    int m_writeBuffer;
    int m_readBuffer;
    unsigned m_videoWidth;
    unsigned m_videoHeight;
    bool m_hasFrame;

    // OpenGL state
    bool m_initialized;
    unsigned int m_texture;
    int m_texWidth;   // Power-of-2 texture dimensions
    int m_texHeight;
    bool m_textureNeedsUpdate;

    // Playback state
    bool m_isPlaying;
    bool m_firstFrameRendered;

    // Render timer for smooth updates
    QTimer *m_renderTimer;

    // Static SDL state
    static bool s_sdlInitialized;
    static SDL_Surface *s_screen;
};

#endif // SDLVIDEOWIDGET_H
