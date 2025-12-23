/**
 * OpenGL ES 2.0 Video Widget for webOS
 * Uses EGL with libeglwebos.so for hardware-accelerated rendering
 */

#include "GLESVideoWidget.h"

#include <QPainter>
#include <QDebug>
#include <QApplication>

#include <dlfcn.h>
#include <string.h>

#include "MediaPlayer.h"

// OpenGL ES 2.0 constants
#define GL_TEXTURE_2D         0x0DE1
#define GL_RGBA               0x1908
#define GL_UNSIGNED_BYTE      0x1401
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_LINEAR             0x2601
#define GL_NEAREST            0x2600
#define GL_FRAGMENT_SHADER    0x8B30
#define GL_VERTEX_SHADER      0x8B31
#define GL_COMPILE_STATUS     0x8B81
#define GL_LINK_STATUS        0x8B82
#define GL_COLOR_BUFFER_BIT   0x4000
#define GL_TRIANGLE_STRIP     0x0005
#define GL_FLOAT              0x1406
#define GL_FALSE              0
#define GL_TRUE               1

// EGL constants
#define EGL_SURFACE_TYPE      0x3033
#define EGL_WINDOW_BIT        0x0004
#define EGL_RENDERABLE_TYPE   0x3040
#define EGL_OPENGL_ES2_BIT    0x0004
#define EGL_RED_SIZE          0x3024
#define EGL_GREEN_SIZE        0x3023
#define EGL_BLUE_SIZE         0x3022
#define EGL_ALPHA_SIZE        0x3021
#define EGL_DEPTH_SIZE        0x3025
#define EGL_NONE              0x3038
#define EGL_CONTEXT_CLIENT_VERSION 0x3098
#define EGL_NO_CONTEXT        ((EGLContext)0)
#define EGL_NO_SURFACE        ((EGLSurface)0)
#define EGL_NO_DISPLAY        ((EGLDisplay)0)
#define EGL_DEFAULT_DISPLAY   ((EGLNativeDisplayType)0)
#define EGL_TRUE              1
#define EGL_FALSE             0

// GLES function pointer types
typedef void (*PFNGLVIEWPORTPROC)(GLint, GLint, GLint, GLint);
typedef void (*PFNGLCLEARCOLORPROC)(GLfloat, GLfloat, GLfloat, GLfloat);
typedef void (*PFNGLCLEARPROC)(GLuint);
typedef void (*PFNGLGENTEXTURESPROC)(GLint, GLuint*);
typedef void (*PFNGLBINDTEXTUREPROC)(GLenum, GLuint);
typedef void (*PFNGLTEXIMAGE2DPROC)(GLenum, GLint, GLint, GLint, GLint, GLint, GLenum, GLenum, const void*);
typedef void (*PFNGLTEXPARAMETERIPROC)(GLenum, GLenum, GLint);
typedef GLuint (*PFNGLCREATESHADERPROC)(GLenum);
typedef void (*PFNGLSHADERSOURCEPROC)(GLuint, GLint, const char**, const GLint*);
typedef void (*PFNGLCOMPILESHADERPROC)(GLuint);
typedef void (*PFNGLGETSHADERIVPROC)(GLuint, GLenum, GLint*);
typedef GLuint (*PFNGLCREATEPROGRAMPROC)();
typedef void (*PFNGLATTACHSHADERPROC)(GLuint, GLuint);
typedef void (*PFNGLLINKPROGRAMPROC)(GLuint);
typedef void (*PFNGLGETPROGRAMIVPROC)(GLuint, GLenum, GLint*);
typedef void (*PFNGLUSEPROGRAMPROC)(GLuint);
typedef GLint (*PFNGLGETATTRIBLOCATIONPROC)(GLuint, const char*);
typedef GLint (*PFNGLGETUNIFORMLOCATIONPROC)(GLuint, const char*);
typedef void (*PFNGLENABLEVERTEXATTRIBARRAYPROC)(GLuint);
typedef void (*PFNGLVERTEXATTRIBPOINTERPROC)(GLuint, GLint, GLenum, GLint, GLint, const void*);
typedef void (*PFNGLUNIFORM1IPROC)(GLint, GLint);
typedef void (*PFNGLDRAWARRAYSPROC)(GLenum, GLint, GLint);
typedef void (*PFNGLDISABLEVERTEXATTRIBARRAYPROC)(GLuint);
typedef void (*PFNGLDELETESHADERPROC)(GLuint);
typedef void (*PFNGLDELETEPROGRAMPROC)(GLuint);
typedef void (*PFNGLDELETETEXTURESPROC)(GLint, const GLuint*);
typedef void (*PFNGLACTIVETEXTUREPROC)(GLenum);
typedef void (*PFNGLTEXSUBIMAGE2DPROC)(GLenum, GLint, GLint, GLint, GLint, GLint, GLenum, GLenum, const void*);

// GLES function pointers
static PFNGLVIEWPORTPROC glViewport = nullptr;
static PFNGLCLEARCOLORPROC glClearColor = nullptr;
static PFNGLCLEARPROC glClear = nullptr;
static PFNGLGENTEXTURESPROC glGenTextures = nullptr;
static PFNGLBINDTEXTUREPROC glBindTexture = nullptr;
static PFNGLTEXIMAGE2DPROC glTexImage2D = nullptr;
static PFNGLTEXPARAMETERIPROC glTexParameteri = nullptr;
static PFNGLCREATESHADERPROC glCreateShader = nullptr;
static PFNGLSHADERSOURCEPROC glShaderSource = nullptr;
static PFNGLCOMPILESHADERPROC glCompileShader = nullptr;
static PFNGLGETSHADERIVPROC glGetShaderiv = nullptr;
static PFNGLCREATEPROGRAMPROC glCreateProgram = nullptr;
static PFNGLATTACHSHADERPROC glAttachShader = nullptr;
static PFNGLLINKPROGRAMPROC glLinkProgram = nullptr;
static PFNGLGETPROGRAMIVPROC glGetProgramiv = nullptr;
static PFNGLUSEPROGRAMPROC glUseProgram = nullptr;
static PFNGLGETATTRIBLOCATIONPROC glGetAttribLocation = nullptr;
static PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation = nullptr;
static PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray = nullptr;
static PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer = nullptr;
static PFNGLUNIFORM1IPROC glUniform1i = nullptr;
static PFNGLDRAWARRAYSPROC glDrawArrays = nullptr;
static PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray = nullptr;
static PFNGLDELETESHADERPROC glDeleteShader = nullptr;
static PFNGLDELETEPROGRAMPROC glDeleteProgram = nullptr;
static PFNGLDELETETEXTURESPROC glDeleteTextures = nullptr;
static PFNGLACTIVETEXTUREPROC glActiveTexture = nullptr;
static PFNGLTEXSUBIMAGE2DPROC glTexSubImage2D = nullptr;

// Vertex shader - simple passthrough
static const char *vertexShaderSource =
    "attribute vec4 a_position;\n"
    "attribute vec2 a_texCoord;\n"
    "varying vec2 v_texCoord;\n"
    "void main() {\n"
    "    gl_Position = a_position;\n"
    "    v_texCoord = a_texCoord;\n"
    "}\n";

// Fragment shader - simple texture lookup
static const char *fragmentShaderSource =
    "precision mediump float;\n"
    "varying vec2 v_texCoord;\n"
    "uniform sampler2D u_texture;\n"
    "void main() {\n"
    "    gl_FragColor = texture2D(u_texture, v_texCoord);\n"
    "}\n";

// Scale factor for reduced resolution
#define VIDEO_SCALE_FACTOR 2

GLESVideoWidget::GLESVideoWidget(QWidget *parent)
    : QWidget(parent),
      m_player(nullptr),
      m_writeBuffer(0),
      m_readBuffer(1),
      m_videoWidth(0),
      m_videoHeight(0),
      m_hasFrame(false),
      m_textureNeedsUpdate(false),
      m_eglDisplay(EGL_NO_DISPLAY),
      m_eglSurface(EGL_NO_SURFACE),
      m_eglContext(EGL_NO_CONTEXT),
      m_eglConfig(nullptr),
      m_eglInitialized(false),
      m_texture(0),
      m_program(0),
      m_vertexShader(0),
      m_fragmentShader(0),
      m_positionAttr(0),
      m_texCoordAttr(0),
      m_textureUniform(0),
      m_eglLib(nullptr),
      m_eglWebosLib(nullptr),
      m_glesLib(nullptr),
      eglGetDisplay(nullptr),
      eglInitialize(nullptr),
      eglChooseConfig(nullptr),
      eglCreateWindowSurface(nullptr),
      eglCreateContext(nullptr),
      eglMakeCurrent(nullptr),
      eglSwapBuffers(nullptr),
      eglDestroyContext(nullptr),
      eglDestroySurface(nullptr),
      eglTerminate(nullptr),
      eglGetError(nullptr)
{
    // Make widget suitable for OpenGL
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_PaintOnScreen);

    QPalette pal = palette();
    pal.setColor(QPalette::Window, Qt::black);
    setPalette(pal);
    setAutoFillBackground(true);

    fprintf(stderr, "GLESVideoWidget: Initializing...\n");
    fflush(stderr);

    if (!initEGL()) {
        fprintf(stderr, "GLESVideoWidget: EGL initialization failed, falling back to software\n");
        fflush(stderr);
    }
}

GLESVideoWidget::~GLESVideoWidget()
{
    if (m_player) {
        libvlc_video_set_callbacks(m_player->core(), nullptr, nullptr, nullptr, nullptr);
        libvlc_video_set_format_callbacks(m_player->core(), nullptr, nullptr);
    }
    cleanupEGL();
}

bool GLESVideoWidget::initEGL()
{
    fprintf(stderr, "GLESVideoWidget: Loading EGL libraries...\n");
    fflush(stderr);

    // Load EGL library
    m_eglLib = dlopen("libEGL.so", RTLD_NOW | RTLD_GLOBAL);
    if (!m_eglLib) {
        fprintf(stderr, "GLESVideoWidget: Failed to load libEGL.so: %s\n", dlerror());
        fflush(stderr);
        return false;
    }

    // Load webOS EGL subdriver
    m_eglWebosLib = dlopen("/usr/lib/libeglwebos.so", RTLD_NOW | RTLD_GLOBAL);
    if (!m_eglWebosLib) {
        fprintf(stderr, "GLESVideoWidget: Failed to load libeglwebos.so: %s\n", dlerror());
        fflush(stderr);
        // Continue without it - might work on some systems
    } else {
        fprintf(stderr, "GLESVideoWidget: Loaded libeglwebos.so\n");
        fflush(stderr);
    }

    // Load GLES2 library
    m_glesLib = dlopen("libGLESv2.so", RTLD_NOW | RTLD_GLOBAL);
    if (!m_glesLib) {
        fprintf(stderr, "GLESVideoWidget: Failed to load libGLESv2.so: %s\n", dlerror());
        fflush(stderr);
        return false;
    }

    // Load EGL function pointers
    #define LOAD_EGL(name) \
        name = (decltype(name))dlsym(m_eglLib, #name); \
        if (!name) { fprintf(stderr, "Failed to load " #name "\n"); fflush(stderr); return false; }

    LOAD_EGL(eglGetDisplay);
    LOAD_EGL(eglInitialize);
    LOAD_EGL(eglChooseConfig);
    LOAD_EGL(eglCreateWindowSurface);
    LOAD_EGL(eglCreateContext);
    LOAD_EGL(eglMakeCurrent);
    LOAD_EGL(eglSwapBuffers);
    LOAD_EGL(eglDestroyContext);
    LOAD_EGL(eglDestroySurface);
    LOAD_EGL(eglTerminate);
    eglGetError = (decltype(eglGetError))dlsym(m_eglLib, "eglGetError");

    #undef LOAD_EGL

    // Load GLES function pointers
    #define LOAD_GLES(name) \
        name = (decltype(name))dlsym(m_glesLib, #name); \
        if (!name) { fprintf(stderr, "Failed to load " #name "\n"); fflush(stderr); return false; }

    LOAD_GLES(glViewport);
    LOAD_GLES(glClearColor);
    LOAD_GLES(glClear);
    LOAD_GLES(glGenTextures);
    LOAD_GLES(glBindTexture);
    LOAD_GLES(glTexImage2D);
    LOAD_GLES(glTexParameteri);
    LOAD_GLES(glCreateShader);
    LOAD_GLES(glShaderSource);
    LOAD_GLES(glCompileShader);
    LOAD_GLES(glGetShaderiv);
    LOAD_GLES(glCreateProgram);
    LOAD_GLES(glAttachShader);
    LOAD_GLES(glLinkProgram);
    LOAD_GLES(glGetProgramiv);
    LOAD_GLES(glUseProgram);
    LOAD_GLES(glGetAttribLocation);
    LOAD_GLES(glGetUniformLocation);
    LOAD_GLES(glEnableVertexAttribArray);
    LOAD_GLES(glVertexAttribPointer);
    LOAD_GLES(glUniform1i);
    LOAD_GLES(glDrawArrays);
    LOAD_GLES(glDisableVertexAttribArray);
    LOAD_GLES(glDeleteShader);
    LOAD_GLES(glDeleteProgram);
    LOAD_GLES(glDeleteTextures);
    LOAD_GLES(glActiveTexture);
    LOAD_GLES(glTexSubImage2D);

    #undef LOAD_GLES

    fprintf(stderr, "GLESVideoWidget: Libraries loaded, getting display...\n");
    fflush(stderr);

    // Get EGL display - use default display for now
    m_eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (m_eglDisplay == EGL_NO_DISPLAY) {
        fprintf(stderr, "GLESVideoWidget: eglGetDisplay failed\n");
        fflush(stderr);
        return false;
    }

    // Initialize EGL
    int major, minor;
    if (eglInitialize(m_eglDisplay, &major, &minor) != EGL_TRUE) {
        fprintf(stderr, "GLESVideoWidget: eglInitialize failed: 0x%x\n", eglGetError ? eglGetError() : -1);
        fflush(stderr);
        return false;
    }
    fprintf(stderr, "GLESVideoWidget: EGL initialized version %d.%d\n", major, minor);
    fflush(stderr);

    // Choose config
    int configAttribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 0,
        EGL_DEPTH_SIZE, 0,
        EGL_NONE
    };

    int numConfigs;
    if (eglChooseConfig(m_eglDisplay, configAttribs, &m_eglConfig, 1, &numConfigs) != EGL_TRUE || numConfigs == 0) {
        fprintf(stderr, "GLESVideoWidget: eglChooseConfig failed\n");
        fflush(stderr);
        return false;
    }
    fprintf(stderr, "GLESVideoWidget: Found %d EGL configs\n", numConfigs);
    fflush(stderr);

    // Create window surface - for webOS, pass NULL for fullscreen window
    // The webOS EGL driver (libeglwebos.so) creates the native window internally
    fprintf(stderr, "GLESVideoWidget: Creating fullscreen EGL surface (NULL window)\n");
    fflush(stderr);

    m_eglSurface = eglCreateWindowSurface(m_eglDisplay, m_eglConfig, (EGLNativeWindowType)0, nullptr);
    if (m_eglSurface == EGL_NO_SURFACE) {
        fprintf(stderr, "GLESVideoWidget: eglCreateWindowSurface failed: 0x%x\n", eglGetError ? eglGetError() : -1);
        fflush(stderr);
        return false;
    }

    // Create context
    int contextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };

    m_eglContext = eglCreateContext(m_eglDisplay, m_eglConfig, EGL_NO_CONTEXT, contextAttribs);
    if (m_eglContext == EGL_NO_CONTEXT) {
        fprintf(stderr, "GLESVideoWidget: eglCreateContext failed: 0x%x\n", eglGetError ? eglGetError() : -1);
        fflush(stderr);
        return false;
    }

    // Make context current
    if (eglMakeCurrent(m_eglDisplay, m_eglSurface, m_eglSurface, m_eglContext) != EGL_TRUE) {
        fprintf(stderr, "GLESVideoWidget: eglMakeCurrent failed: 0x%x\n", eglGetError ? eglGetError() : -1);
        fflush(stderr);
        return false;
    }

    // Initialize shaders
    if (!initShaders()) {
        fprintf(stderr, "GLESVideoWidget: Shader initialization failed\n");
        fflush(stderr);
        return false;
    }

    m_eglInitialized = true;
    fprintf(stderr, "GLESVideoWidget: EGL initialized successfully!\n");
    fflush(stderr);
    return true;
}

bool GLESVideoWidget::initShaders()
{
    GLint status;

    // Vertex shader
    m_vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(m_vertexShader, 1, &vertexShaderSource, nullptr);
    glCompileShader(m_vertexShader);
    glGetShaderiv(m_vertexShader, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE) {
        fprintf(stderr, "GLESVideoWidget: Vertex shader compilation failed\n");
        fflush(stderr);
        return false;
    }

    // Fragment shader
    m_fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(m_fragmentShader, 1, &fragmentShaderSource, nullptr);
    glCompileShader(m_fragmentShader);
    glGetShaderiv(m_fragmentShader, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE) {
        fprintf(stderr, "GLESVideoWidget: Fragment shader compilation failed\n");
        fflush(stderr);
        return false;
    }

    // Program
    m_program = glCreateProgram();
    glAttachShader(m_program, m_vertexShader);
    glAttachShader(m_program, m_fragmentShader);
    glLinkProgram(m_program);
    glGetProgramiv(m_program, GL_LINK_STATUS, &status);
    if (status != GL_TRUE) {
        fprintf(stderr, "GLESVideoWidget: Shader program linking failed\n");
        fflush(stderr);
        return false;
    }

    // Get attribute/uniform locations
    m_positionAttr = glGetAttribLocation(m_program, "a_position");
    m_texCoordAttr = glGetAttribLocation(m_program, "a_texCoord");
    m_textureUniform = glGetUniformLocation(m_program, "u_texture");

    // Create texture
    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    fprintf(stderr, "GLESVideoWidget: Shaders initialized\n");
    fflush(stderr);
    return true;
}

void GLESVideoWidget::cleanupEGL()
{
    if (m_eglDisplay != EGL_NO_DISPLAY) {
        eglMakeCurrent(m_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

        if (m_texture) {
            glDeleteTextures(1, &m_texture);
            m_texture = 0;
        }
        if (m_program) {
            glDeleteProgram(m_program);
            m_program = 0;
        }
        if (m_vertexShader) {
            glDeleteShader(m_vertexShader);
            m_vertexShader = 0;
        }
        if (m_fragmentShader) {
            glDeleteShader(m_fragmentShader);
            m_fragmentShader = 0;
        }

        if (m_eglContext != EGL_NO_CONTEXT) {
            eglDestroyContext(m_eglDisplay, m_eglContext);
        }
        if (m_eglSurface != EGL_NO_SURFACE) {
            eglDestroySurface(m_eglDisplay, m_eglSurface);
        }
        eglTerminate(m_eglDisplay);
    }

    if (m_glesLib) dlclose(m_glesLib);
    if (m_eglWebosLib) dlclose(m_eglWebosLib);
    if (m_eglLib) dlclose(m_eglLib);

    m_eglInitialized = false;
}

void GLESVideoWidget::setMediaPlayer(VlcMediaPlayer *player)
{
    if (m_player) {
        libvlc_video_set_callbacks(m_player->core(), nullptr, nullptr, nullptr, nullptr);
        libvlc_video_set_format_callbacks(m_player->core(), nullptr, nullptr);
    }

    m_player = player;

    if (m_player) {
        libvlc_media_player_t *mp = m_player->core();
        fprintf(stderr, "GLESVideoWidget: Setting callbacks on player %p\n", (void*)mp);
        fflush(stderr);

        libvlc_video_set_callbacks(mp,
                                   lockCallback,
                                   unlockCallback,
                                   displayCallback,
                                   this);
        libvlc_video_set_format_callbacks(mp,
                                          formatCallback,
                                          formatCleanupCallback);
    }
}

void GLESVideoWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    if (m_eglInitialized && m_hasFrame) {
        renderFrame();
    } else {
        // Fallback to black fill
        QPainter painter(this);
        painter.fillRect(rect(), Qt::black);
    }
}

void GLESVideoWidget::renderFrame()
{
    if (!m_eglInitialized || !m_hasFrame || m_videoWidth == 0 || m_videoHeight == 0) {
        return;
    }

    // Make context current
    eglMakeCurrent(m_eglDisplay, m_eglSurface, m_eglSurface, m_eglContext);

    // Update texture if needed
    if (m_textureNeedsUpdate) {
        updateTexture();
        m_textureNeedsUpdate = false;
    }

    // Set viewport
    glViewport(0, 0, width(), height());

    // Clear
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Use program
    glUseProgram(m_program);

    // Calculate aspect ratio correct coordinates
    float videoAspect = (float)m_videoWidth / (float)m_videoHeight;
    float widgetAspect = (float)width() / (float)height();

    float scaleX = 1.0f, scaleY = 1.0f;
    if (videoAspect > widgetAspect) {
        scaleY = widgetAspect / videoAspect;
    } else {
        scaleX = videoAspect / widgetAspect;
    }

    // Vertex data (position + texcoord interleaved)
    GLfloat vertices[] = {
        // Position         // TexCoord
        -scaleX, -scaleY,   0.0f, 1.0f,  // bottom-left
         scaleX, -scaleY,   1.0f, 1.0f,  // bottom-right
        -scaleX,  scaleY,   0.0f, 0.0f,  // top-left
         scaleX,  scaleY,   1.0f, 0.0f,  // top-right
    };

    // Bind texture
    glActiveTexture(0x84C0); // GL_TEXTURE0
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glUniform1i(m_textureUniform, 0);

    // Set up vertex attributes
    glEnableVertexAttribArray(m_positionAttr);
    glEnableVertexAttribArray(m_texCoordAttr);
    glVertexAttribPointer(m_positionAttr, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), vertices);
    glVertexAttribPointer(m_texCoordAttr, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), vertices + 2);

    // Draw
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // Cleanup
    glDisableVertexAttribArray(m_positionAttr);
    glDisableVertexAttribArray(m_texCoordAttr);

    // Swap buffers
    eglSwapBuffers(m_eglDisplay, m_eglSurface);
}

void GLESVideoWidget::updateTexture()
{
    m_mutex.lock();
    const unsigned char *src = reinterpret_cast<const unsigned char*>(m_buffer[m_readBuffer].constData());
    
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_videoWidth, m_videoHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, src);

    m_mutex.unlock();
}

void GLESVideoWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_eglInitialized) {
        eglMakeCurrent(m_eglDisplay, m_eglSurface, m_eglSurface, m_eglContext);
        glViewport(0, 0, width(), height());
    }
}

void GLESVideoWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
}

void GLESVideoWidget::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
}

static int frameCount = 0;

void GLESVideoWidget::onFrameReady()
{
    frameCount++;
    m_textureNeedsUpdate = true;
    update();

    if (frameCount <= 5 || frameCount % 100 == 0) {
        fprintf(stderr, "GLESVideoWidget: onFrameReady %d\n", frameCount);
        fflush(stderr);
    }
}

// Static callbacks

void *GLESVideoWidget::lockCallback(void *opaque, void **planes)
{
    GLESVideoWidget *self = static_cast<GLESVideoWidget*>(opaque);
    planes[0] = self->m_buffer[self->m_writeBuffer].data();
    return nullptr;
}

void GLESVideoWidget::unlockCallback(void *opaque, void *picture, void *const *planes)
{
    Q_UNUSED(picture);
    Q_UNUSED(planes);

    GLESVideoWidget *self = static_cast<GLESVideoWidget*>(opaque);

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

void GLESVideoWidget::displayCallback(void *opaque, void *picture)
{
    Q_UNUSED(opaque);
    Q_UNUSED(picture);
}

unsigned GLESVideoWidget::formatCallback(void **opaque, char *chroma,
                                         unsigned *width, unsigned *height,
                                         unsigned *pitches, unsigned *lines)
{
    GLESVideoWidget *self = static_cast<GLESVideoWidget*>(*opaque);

    fprintf(stderr, "GLESVideoWidget::formatCallback %ux%u incoming chroma=%.4s\n", *width, *height, chroma);
    fflush(stderr);

    // Request RGBA format for OpenGL ES
    memcpy(chroma, "RGBA", 4);

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

    fprintf(stderr, "GLESVideoWidget: Requested RGBA at %ux%u, buffer=%u bytes\n",
            scaledWidth, scaledHeight, bufferSize);
    fflush(stderr);

    return bufferSize;
}

void GLESVideoWidget::formatCleanupCallback(void *opaque)
{
    GLESVideoWidget *self = static_cast<GLESVideoWidget*>(opaque);

    self->m_mutex.lock();
    self->m_buffer[0].clear();
    self->m_buffer[1].clear();
    self->m_hasFrame = false;
    self->m_videoWidth = 0;
    self->m_videoHeight = 0;
    self->m_mutex.unlock();
}
