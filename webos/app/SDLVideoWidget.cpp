/**
 * SDL/OpenGL ES Video Widget for webOS
 *
 * This implementation uses SDL's built-in OpenGL support which properly
 * integrates with webOS's 3-layer display system, eliminating touch flicker.
 *
 * Based on the solution discovered in PCSX-ReARMed:
 * - SDL manages the GL context correctly with webOS
 * - Direct EGL usage causes flicker on touch events
 * - PDL must be initialized before SDL
 */

#include "SDLVideoWidget.h"

#include <QDebug>
#include <QApplication>

#include <SDL.h>

#include <dlfcn.h>
#include <string.h>
#include <stdarg.h>

#include "MediaPlayer.h"

// Debug logging to file (stderr doesn't go to syslog on webOS)
static FILE *g_logFile = nullptr;

static void logMsg(const char *fmt, ...) {
    if (!g_logFile) {
        g_logFile = fopen("/media/internal/vlcplayer.log", "a");
        if (g_logFile) {
            fprintf(g_logFile, "\n=== SDLVideoWidget Started ===\n");
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

// Scale factors for reduced resolution based on source size
#define VIDEO_SCALE_FACTOR_SD 2
#define VIDEO_SCALE_FACTOR_HD 5
#define VIDEO_SCALE_FACTOR_FHD 8

// Static members
bool SDLVideoWidget::s_sdlInitialized = false;
SDL_Surface *SDLVideoWidget::s_screen = nullptr;

// PDL function pointers (loaded dynamically)
typedef int (*PDL_Init_t)(unsigned int flags);
typedef void (*PDL_Quit_t)(void);
typedef int (*PDL_SetTouchAggression_t)(int aggression);

static void *s_pdlLib = nullptr;
static PDL_Init_t pPDL_Init = nullptr;
static PDL_Quit_t pPDL_Quit = nullptr;
static PDL_SetTouchAggression_t pPDL_SetTouchAggression = nullptr;

#define PDL_AGGRESSION_MORETOUCHES 2

bool SDLVideoWidget::initSDL()
{
    if (s_sdlInitialized) {
        return true;
    }

    logMsg("SDLVideoWidget: Initializing PDL and SDL...\n");

    // Step 1: Initialize PDL BEFORE SDL (critical for webOS GPU access)
    s_pdlLib = dlopen("libpdl.so", RTLD_LAZY);
    if (s_pdlLib) {
        pPDL_Init = (PDL_Init_t)dlsym(s_pdlLib, "PDL_Init");
        pPDL_Quit = (PDL_Quit_t)dlsym(s_pdlLib, "PDL_Quit");
        pPDL_SetTouchAggression = (PDL_SetTouchAggression_t)dlsym(s_pdlLib, "PDL_SetTouchAggression");

        if (pPDL_Init) {
            int ret = pPDL_Init(0);
            if (ret == 0) {
                logMsg("SDLVideoWidget: PDL initialized successfully\n");

                // Set touch aggression for better multitouch
                if (pPDL_SetTouchAggression) {
                    pPDL_SetTouchAggression(PDL_AGGRESSION_MORETOUCHES);
                    logMsg("SDLVideoWidget: Touch aggression set to MORETOUCHES\n");
                }
            } else {
                logMsg("SDLVideoWidget: PDL_Init failed with code %d\n", ret);
            }
        }
    } else {
        logMsg("SDLVideoWidget: libpdl.so not found - not running on webOS?\n");
    }

    // Step 2: Don't initialize SDL video - it conflicts with Qt on webOS
    // SDL_SetVideoMode tries to create an EGL context which conflicts with Qt's display
    // We'll use PDL benefits (touch handling) and framebuffer for video
    logMsg("SDLVideoWidget: Skipping SDL video init (conflicts with Qt), using framebuffer\n");

    // Mark as initialized - we'll use framebuffer rendering
    s_screen = nullptr;  // No SDL surface

    s_sdlInitialized = true;
    return true;
}

void SDLVideoWidget::shutdownSDL()
{
    if (!s_sdlInitialized) {
        return;
    }

    logMsg("SDLVideoWidget: Shutting down SDL...\n");

    SDL_Quit();

    if (pPDL_Quit) {
        pPDL_Quit();
    }
    if (s_pdlLib) {
        dlclose(s_pdlLib);
        s_pdlLib = nullptr;
    }

    s_sdlInitialized = false;
    s_screen = nullptr;
}

SDLVideoWidget::SDLVideoWidget(QWidget *parent)
    : QWidget(parent),
      m_player(nullptr),
      m_writeBuffer(0),
      m_readBuffer(1),
      m_videoWidth(0),
      m_videoHeight(0),
      m_hasFrame(false),
      m_initialized(false),
      m_texture(0),
      m_texWidth(0),
      m_texHeight(0),
      m_textureNeedsUpdate(false),
      m_isPlaying(false),
      m_firstFrameRendered(false),
      m_renderTimer(nullptr)
{
    // Widget attributes - we handle our own painting
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_NoSystemBackground);
    setAutoFillBackground(false);

    // Initialize GL state if SDL is ready
    if (s_sdlInitialized) {
        initGL();
    }

    // Create render timer for smooth frame updates
    m_renderTimer = new QTimer(this);
    m_renderTimer->setInterval(16);  // ~60fps target
    connect(m_renderTimer, &QTimer::timeout, this, &SDLVideoWidget::renderFrame);
}

SDLVideoWidget::~SDLVideoWidget()
{
    if (m_renderTimer) {
        m_renderTimer->stop();
    }

    if (m_player) {
        libvlc_video_set_callbacks(m_player->core(), nullptr, nullptr, nullptr, nullptr);
        libvlc_video_set_format_callbacks(m_player->core(), nullptr, nullptr);
    }

    cleanupGL();
}

bool SDLVideoWidget::initGL()
{
    if (m_initialized) {
        return true;
    }

    if (!s_sdlInitialized || !s_screen) {
        logMsg("SDLVideoWidget::initGL: SDL not initialized\n");
        return false;
    }

    logMsg("SDLVideoWidget: Initializing SDL software rendering...\n");

    // Clear screen to black
    SDL_FillRect(s_screen, NULL, SDL_MapRGB(s_screen->format, 0, 0, 0));
    SDL_Flip(s_screen);

    m_initialized = true;
    logMsg("SDLVideoWidget: SDL software rendering initialized successfully\n");

    return true;
}

void SDLVideoWidget::cleanupGL()
{
    m_texture = 0;
    m_initialized = false;
}

void SDLVideoWidget::setMediaPlayer(VlcMediaPlayer *player)
{
    if (m_player) {
        libvlc_video_set_callbacks(m_player->core(), nullptr, nullptr, nullptr, nullptr);
        libvlc_video_set_format_callbacks(m_player->core(), nullptr, nullptr);
    }

    m_player = player;

    if (m_player) {
        libvlc_media_player_t *mp = m_player->core();
        logMsg("SDLVideoWidget: Setting callbacks on player %p\n", (void*)mp);

        libvlc_video_set_callbacks(mp,
                                   lockCallback,
                                   unlockCallback,
                                   displayCallback,
                                   this);
        libvlc_video_set_format_callbacks(mp,
                                          formatCallback,
                                          formatCleanupCallback);
        logMsg("SDLVideoWidget: Callbacks set successfully\n");
    }
}

void SDLVideoWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    // We render via SDL/GL, not Qt paint events
    // But we still need this widget to capture mouse events
}

void SDLVideoWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
}

void SDLVideoWidget::mousePressEvent(QMouseEvent *event)
{
    Q_UNUSED(event);

    // If playing, user tapped - emit signal so MainWindow can pause and show UI
    if (m_isPlaying) {
        logMsg("SDLVideoWidget: Tapped during playback\n");
        emit tapped();
    }
}

void SDLVideoWidget::updateTexture()
{
    // Not used in software rendering mode
}

void SDLVideoWidget::renderFrame()
{
    static int renderCount = 0;

    if (!m_initialized || !s_screen || !m_isPlaying || !m_hasFrame) {
        return;
    }

    if (m_videoWidth == 0 || m_videoHeight == 0) {
        return;
    }

    renderCount++;
    if (renderCount <= 10 || renderCount % 100 == 0) {
        logMsg("SDLVideoWidget: renderFrame #%d, video %ux%u\n",
               renderCount, m_videoWidth, m_videoHeight);
    }

    m_mutex.lock();

    const unsigned char *src = reinterpret_cast<const unsigned char*>(
        m_buffer[m_readBuffer].constData());

    // Create SDL surface from video frame data
    SDL_Surface *frameSurface = SDL_CreateRGBSurfaceFrom(
        (void*)src,
        m_videoWidth, m_videoHeight,
        32,                          // bits per pixel
        m_videoWidth * 4,            // pitch
        0x000000FF,                  // R mask
        0x0000FF00,                  // G mask
        0x00FF0000,                  // B mask
        0xFF000000                   // A mask
    );

    if (!frameSurface) {
        m_mutex.unlock();
        logMsg("SDLVideoWidget: Failed to create frame surface: %s\n", SDL_GetError());
        return;
    }

    // Calculate aspect-correct destination rectangle
    float videoAspect = (float)m_videoWidth / (float)m_videoHeight;
    float screenAspect = (float)s_screen->w / (float)s_screen->h;

    SDL_Rect destRect;
    if (videoAspect > screenAspect) {
        // Video is wider - letterbox (black bars top/bottom)
        destRect.w = s_screen->w;
        destRect.h = (int)(s_screen->w / videoAspect);
        destRect.x = 0;
        destRect.y = (s_screen->h - destRect.h) / 2;
    } else {
        // Video is taller - pillarbox (black bars left/right)
        destRect.h = s_screen->h;
        destRect.w = (int)(s_screen->h * videoAspect);
        destRect.x = (s_screen->w - destRect.w) / 2;
        destRect.y = 0;
    }

    // Clear screen to black
    SDL_FillRect(s_screen, NULL, SDL_MapRGB(s_screen->format, 0, 0, 0));

    // Scale and blit the frame
    SDL_SoftStretch(frameSurface, NULL, s_screen, &destRect);

    // Flip the display
    SDL_Flip(s_screen);

    SDL_FreeSurface(frameSurface);
    m_mutex.unlock();

    // Emit firstFrameReady after we've actually rendered
    if (!m_firstFrameRendered) {
        m_firstFrameRendered = true;
        logMsg("SDLVideoWidget: First frame rendered - emitting firstFrameReady\n");
        emit firstFrameReady();
    }
}

void SDLVideoWidget::onFrameReady()
{
    m_textureNeedsUpdate = true;

    // Render immediately if playing
    if (m_isPlaying) {
        renderFrame();
    }
}

void SDLVideoWidget::onPlaybackStarted()
{
    logMsg("SDLVideoWidget: Playback started\n");
    m_isPlaying = true;
    m_firstFrameRendered = false;

    // Start render timer for smooth playback
    if (m_renderTimer) {
        m_renderTimer->start();
    }

    // Render current frame if we have one
    if (m_hasFrame) {
        renderFrame();
    }
}

void SDLVideoWidget::onPlaybackStopped()
{
    logMsg("SDLVideoWidget: Playback stopped\n");
    m_isPlaying = false;

    // Stop render timer
    if (m_renderTimer) {
        m_renderTimer->stop();
    }

    // Clear to black
    if (m_initialized && s_screen) {
        SDL_FillRect(s_screen, NULL, SDL_MapRGB(s_screen->format, 0, 0, 0));
        SDL_Flip(s_screen);
    }
}

// Static callbacks for VLC

void *SDLVideoWidget::lockCallback(void *opaque, void **planes)
{
    SDLVideoWidget *self = static_cast<SDLVideoWidget*>(opaque);
    planes[0] = self->m_buffer[self->m_writeBuffer].data();
    return nullptr;
}

void SDLVideoWidget::unlockCallback(void *opaque, void *picture, void *const *planes)
{
    Q_UNUSED(picture);
    Q_UNUSED(planes);

    SDLVideoWidget *self = static_cast<SDLVideoWidget*>(opaque);

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

void SDLVideoWidget::displayCallback(void *opaque, void *picture)
{
    Q_UNUSED(opaque);
    Q_UNUSED(picture);
}

unsigned SDLVideoWidget::formatCallback(void **opaque, char *chroma,
                                         unsigned *width, unsigned *height,
                                         unsigned *pitches, unsigned *lines)
{
    SDLVideoWidget *self = static_cast<SDLVideoWidget*>(*opaque);

    logMsg("SDLVideoWidget::formatCallback %ux%u incoming chroma=%.4s\n",
           *width, *height, chroma);

    // Choose scale factor based on source resolution
    unsigned sourceHeight = *height;
    unsigned scaleFactor;
    if (sourceHeight > 900) {
        scaleFactor = VIDEO_SCALE_FACTOR_FHD;  // 1080p+ -> /8
    } else if (sourceHeight > 600) {
        scaleFactor = VIDEO_SCALE_FACTOR_HD;   // 720p -> /5
    } else {
        scaleFactor = VIDEO_SCALE_FACTOR_SD;   // 480p and below -> /2
    }

    unsigned scaledWidth = *width / scaleFactor;
    unsigned scaledHeight = *height / scaleFactor;

    // Ensure dimensions are even
    scaledWidth = (scaledWidth / 2) * 2;
    scaledHeight = (scaledHeight / 2) * 2;

    // Minimum size
    if (scaledWidth < 160) scaledWidth = 160;
    if (scaledHeight < 90) scaledHeight = 90;

    *width = scaledWidth;
    *height = scaledHeight;

    self->m_videoWidth = scaledWidth;
    self->m_videoHeight = scaledHeight;

    // Reset texture dimensions so they get recalculated
    self->m_texWidth = 0;
    self->m_texHeight = 0;

    // Request RGBA format for OpenGL ES
    memcpy(chroma, "RGBA", 4);

    *pitches = scaledWidth * 4;
    *lines = scaledHeight;

    unsigned bufferSize = (*pitches) * (*lines);
    self->m_buffer[0].resize(bufferSize);
    self->m_buffer[0].fill(0);
    self->m_buffer[1].resize(bufferSize);
    self->m_buffer[1].fill(0);
    self->m_writeBuffer = 0;
    self->m_readBuffer = 1;

    logMsg("SDLVideoWidget: Requested RGBA at %ux%u (1/%d for %up), buffer=%u bytes\n",
           scaledWidth, scaledHeight, scaleFactor, sourceHeight, bufferSize);

    return bufferSize;
}

void SDLVideoWidget::formatCleanupCallback(void *opaque)
{
    SDLVideoWidget *self = static_cast<SDLVideoWidget*>(opaque);

    self->m_mutex.lock();
    self->m_buffer[0].clear();
    self->m_buffer[1].clear();
    self->m_hasFrame = false;
    self->m_videoWidth = 0;
    self->m_videoHeight = 0;
    self->m_texWidth = 0;
    self->m_texHeight = 0;
    self->m_mutex.unlock();
}
