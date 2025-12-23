/**
 * OpenGL Video Widget for webOS - GPU-accelerated rendering
 */

#include "GLVideoWidget.h"

#include <QDebug>
#include <cstring>

#include "MediaPlayer.h"

// Simple vertex/fragment shaders for texture rendering (GLES 2.0 compatible)
// Based on mplayer-webos implementation
static const char* vertexShaderSource =
    "attribute vec4 a_position;\n"
    "attribute vec2 a_texCoord;\n"
    "varying vec2 v_texCoord;\n"
    "void main() {\n"
    "    gl_Position = a_position;\n"
    "    v_texCoord = a_texCoord;\n"
    "}\n";

static const char* fragmentShaderSource =
    "precision lowp float;\n"  // lowp is faster on mobile GPUs
    "varying vec2 v_texCoord;\n"
    "uniform sampler2D tex0;\n"  // avoid reserved word "texture"
    "void main() {\n"
    "    gl_FragColor = texture2D(tex0, v_texCoord);\n"
    "}\n";

GLVideoWidget::GLVideoWidget(QWidget *parent)
    : QOpenGLWidget(parent),
      m_player(nullptr),
      m_writeBuffer(0),
      m_readBuffer(1),
      m_width(0),
      m_height(0),
      m_textureWidth(0),
      m_textureHeight(0),
      m_hasFrame(false),
      m_textureNeedsUpdate(false),
      m_textureAllocated(false),
      m_textureId(0),
      m_program(0),
      m_vbo(0),
      m_glInitialized(false)
{
    fprintf(stderr, "GLVideoWidget: constructor\n");
    fflush(stderr);
}

GLVideoWidget::~GLVideoWidget()
{
    if (m_player) {
        libvlc_video_set_callbacks(m_player->core(), nullptr, nullptr, nullptr, nullptr);
        libvlc_video_set_format_callbacks(m_player->core(), nullptr, nullptr);
    }

    makeCurrent();
    if (m_textureId) glDeleteTextures(1, &m_textureId);
    if (m_program) glDeleteProgram(m_program);
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    doneCurrent();
}

void GLVideoWidget::setMediaPlayer(VlcMediaPlayer *player)
{
    if (m_player) {
        libvlc_video_set_callbacks(m_player->core(), nullptr, nullptr, nullptr, nullptr);
        libvlc_video_set_format_callbacks(m_player->core(), nullptr, nullptr);
    }

    m_player = player;

    if (m_player) {
        libvlc_media_player_t *mp = m_player->core();
        fprintf(stderr, "GLVideoWidget: Setting callbacks on player %p\n", (void*)mp);
        fflush(stderr);

        libvlc_video_set_callbacks(mp,
                                   lockCallback,
                                   unlockCallback,
                                   displayCallback,
                                   this);
        libvlc_video_set_format_callbacks(mp,
                                          formatCallback,
                                          formatCleanupCallback);
        fprintf(stderr, "GLVideoWidget: Callbacks set successfully\n");
        fflush(stderr);
    }
}

void GLVideoWidget::initializeGL()
{
    fprintf(stderr, "GLVideoWidget::initializeGL\n");
    fflush(stderr);

    initializeOpenGLFunctions();

    // Log GL info
    fprintf(stderr, "GL_VERSION: %s\n", glGetString(GL_VERSION));
    fprintf(stderr, "GL_RENDERER: %s\n", glGetString(GL_RENDERER));
    fflush(stderr);

    // Create texture
    glGenTextures(1, &m_textureId);
    glBindTexture(GL_TEXTURE_2D, m_textureId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Compile vertex shader with error checking
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
    glCompileShader(vertexShader);
    {
        GLint compiled = 0;
        glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &compiled);
        if (!compiled) {
            GLint infoLen = 0;
            glGetShaderiv(vertexShader, GL_INFO_LOG_LENGTH, &infoLen);
            if (infoLen > 1) {
                char infoLog[256];
                glGetShaderInfoLog(vertexShader, sizeof(infoLog), nullptr, infoLog);
                fprintf(stderr, "Vertex shader error: %s\n", infoLog);
            }
        } else {
            fprintf(stderr, "Vertex shader compiled OK\n");
        }
        fflush(stderr);
    }

    // Compile fragment shader with error checking
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr);
    glCompileShader(fragmentShader);
    {
        GLint compiled = 0;
        glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &compiled);
        if (!compiled) {
            GLint infoLen = 0;
            glGetShaderiv(fragmentShader, GL_INFO_LOG_LENGTH, &infoLen);
            if (infoLen > 1) {
                char infoLog[256];
                glGetShaderInfoLog(fragmentShader, sizeof(infoLog), nullptr, infoLog);
                fprintf(stderr, "Fragment shader error: %s\n", infoLog);
            }
        } else {
            fprintf(stderr, "Fragment shader compiled OK\n");
        }
        fflush(stderr);
    }

    // Create and link program
    m_program = glCreateProgram();
    glAttachShader(m_program, vertexShader);
    glAttachShader(m_program, fragmentShader);

    // Bind attribute locations before linking (more reliable on some drivers)
    glBindAttribLocation(m_program, 0, "a_position");
    glBindAttribLocation(m_program, 1, "a_texCoord");

    glLinkProgram(m_program);
    {
        GLint linked = 0;
        glGetProgramiv(m_program, GL_LINK_STATUS, &linked);
        if (!linked) {
            GLint infoLen = 0;
            glGetProgramiv(m_program, GL_INFO_LOG_LENGTH, &infoLen);
            if (infoLen > 1) {
                char infoLog[256];
                glGetProgramInfoLog(m_program, sizeof(infoLog), nullptr, infoLog);
                fprintf(stderr, "Program link error: %s\n", infoLog);
            }
        } else {
            fprintf(stderr, "Program linked OK\n");
        }
        fflush(stderr);
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // Create VBO for fullscreen quad (matches mplayer-webos layout)
    float vertices[] = {
        // Position (x,y,z,w)  // TexCoord (u,v)
        -1.0f, -1.0f, 0.0f, 1.0f,   0.0f, 1.0f,
         1.0f, -1.0f, 0.0f, 1.0f,   1.0f, 1.0f,
        -1.0f,  1.0f, 0.0f, 1.0f,   0.0f, 0.0f,
         1.0f,  1.0f, 0.0f, 1.0f,   1.0f, 0.0f,
    };

    glGenBuffers(1, &m_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    m_textureAllocated = false;
    m_glInitialized = true;
    fprintf(stderr, "GLVideoWidget::initializeGL complete, texture=%u program=%u\n", m_textureId, m_program);
    fflush(stderr);
}

static int glPaintCount = 0;

void GLVideoWidget::paintGL()
{
    glPaintCount++;

    if (glPaintCount <= 20 || glPaintCount % 30 == 1) {
        fprintf(stderr, "paintGL %d: hasFrame=%d width=%d height=%d texAlloc=%d\n",
                glPaintCount, m_hasFrame, m_width, m_height, m_textureAllocated);
        fflush(stderr);
    }

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    if (!m_hasFrame || m_width == 0 || m_height == 0) {
        return;
    }

    // Calculate aspect-correct viewport first (before any texture ops)
    float videoAspect = (float)m_width / (float)m_height;
    float widgetAspect = (float)width() / (float)height();

    int vpX, vpY, vpW, vpH;
    if (videoAspect > widgetAspect) {
        vpW = width();
        vpH = (int)(width() / videoAspect);
        vpX = 0;
        vpY = (height() - vpH) / 2;
    } else {
        vpH = height();
        vpW = (int)(height() * videoAspect);
        vpX = (width() - vpW) / 2;
        vpY = 0;
    }
    glViewport(vpX, vpY, vpW, vpH);

    // Update texture if needed
    m_mutex.lock();

    if (m_textureNeedsUpdate && m_buffer[m_readBuffer].size() > 0) {
        glBindTexture(GL_TEXTURE_2D, m_textureId);

        // Check if we need to reallocate texture (size changed or first time)
        if (!m_textureAllocated || m_textureWidth != m_width || m_textureHeight != m_height) {
            if (glPaintCount <= 20 || glPaintCount % 30 == 1) {
                fprintf(stderr, "paintGL %d: allocating texture %dx%d\n",
                        glPaintCount, m_width, m_height);
                fflush(stderr);
            }
            // First time or size changed - allocate with glTexImage2D
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_width, m_height, 0,
                         GL_RGBA, GL_UNSIGNED_BYTE, m_buffer[m_readBuffer].constData());
            m_textureWidth = m_width;
            m_textureHeight = m_height;
            m_textureAllocated = true;
        } else {
            // Same size - use faster glTexSubImage2D
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_width, m_height,
                            GL_RGBA, GL_UNSIGNED_BYTE, m_buffer[m_readBuffer].constData());
        }

        GLenum err = glGetError();
        if (err != GL_NO_ERROR) {
            fprintf(stderr, "paintGL: texture upload error: 0x%x\n", err);
            fflush(stderr);
        }

        m_textureNeedsUpdate = false;

        if (glPaintCount <= 20 || glPaintCount % 30 == 1) {
            fprintf(stderr, "paintGL %d: texture updated\n", glPaintCount);
            fflush(stderr);
        }
    }

    m_mutex.unlock();

    // Draw textured quad using pre-bound attribute locations
    glUseProgram(m_program);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

    // a_position is at location 0: vec4 (4 floats), stride = 6 floats, offset = 0
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);

    // a_texCoord is at location 1: vec2 (2 floats), stride = 6 floats, offset = 4 floats
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(4 * sizeof(float)));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_textureId);
    glUniform1i(glGetUniformLocation(m_program, "tex0"), 0);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    GLenum drawErr = glGetError();
    if (drawErr != GL_NO_ERROR) {
        fprintf(stderr, "paintGL: glDrawArrays error: 0x%x\n", drawErr);
        fflush(stderr);
    }

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);

    if (glPaintCount <= 20 || glPaintCount % 30 == 1) {
        fprintf(stderr, "paintGL %d: draw complete\n", glPaintCount);
        fflush(stderr);
    }
}

void GLVideoWidget::resizeGL(int w, int h)
{
    fprintf(stderr, "GLVideoWidget::resizeGL %dx%d\n", w, h);
    fflush(stderr);
}

static int glUpdateCount = 0;

void GLVideoWidget::onFrameReady()
{
    glUpdateCount++;

    // Skip frames to reduce GPU load - only render every 2nd frame
    if (glUpdateCount % 2 != 0) {
        return;
    }

    // m_textureNeedsUpdate is already set in unlockCallback
    update();  // Request Qt repaint
}

// Static callbacks
static int frameCount = 0;

void *GLVideoWidget::lockCallback(void *opaque, void **planes)
{
    GLVideoWidget *self = static_cast<GLVideoWidget*>(opaque);
    planes[0] = self->m_buffer[self->m_writeBuffer].data();
    return nullptr;
}

void GLVideoWidget::unlockCallback(void *opaque, void *picture, void *const *planes)
{
    Q_UNUSED(picture);
    Q_UNUSED(planes);

    GLVideoWidget *self = static_cast<GLVideoWidget*>(opaque);
    frameCount++;

    if (frameCount <= 5) {
        const unsigned char* data = reinterpret_cast<const unsigned char*>(self->m_buffer[self->m_writeBuffer].constData());
        fprintf(stderr, "GL unlockCallback: frame=%d w=%d h=%d first8bytes: %02x%02x%02x%02x %02x%02x%02x%02x\n",
                frameCount, self->m_width, self->m_height,
                data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
        fflush(stderr);
    }

    if (self->m_width > 0 && self->m_height > 0) {
        self->m_mutex.lock();
        int temp = self->m_writeBuffer;
        self->m_writeBuffer = self->m_readBuffer;
        self->m_readBuffer = temp;
        self->m_hasFrame = true;
        self->m_textureNeedsUpdate = true;  // Set here to avoid race condition
        self->m_mutex.unlock();

        if (frameCount % 30 == 1) {
            fprintf(stderr, "GL Frame %d: %dx%d swapped\n", frameCount, self->m_width, self->m_height);
            fflush(stderr);
        }

        // Schedule repaint on Qt thread
        QMetaObject::invokeMethod(self, "onFrameReady", Qt::QueuedConnection);
    }
}

void GLVideoWidget::displayCallback(void *opaque, void *picture)
{
    Q_UNUSED(opaque);
    Q_UNUSED(picture);
}

unsigned GLVideoWidget::formatCallback(void **opaque, char *chroma,
                                        unsigned *width, unsigned *height,
                                        unsigned *pitches, unsigned *lines)
{
    fprintf(stderr, "GLVideoWidget::formatCallback called! opaque=%p\n", (void*)*opaque);
    fflush(stderr);

    GLVideoWidget *self = static_cast<GLVideoWidget*>(*opaque);

    fprintf(stderr, "GLVideoWidget::formatCallback %ux%u incoming chroma=%.4s\n", *width, *height, chroma);
    fflush(stderr);

    // Request RGBA format for OpenGL texture
    memcpy(chroma, "RGBA", 4);

    self->m_width = *width;
    self->m_height = *height;

    *pitches = self->m_width * 4;
    *lines = self->m_height;

    unsigned bufferSize = (*pitches) * (*lines);
    self->m_buffer[0].resize(bufferSize);
    self->m_buffer[0].fill(0);
    self->m_buffer[1].resize(bufferSize);
    self->m_buffer[1].fill(0);
    self->m_writeBuffer = 0;
    self->m_readBuffer = 1;

    fprintf(stderr, "GL Requested chroma=RGBA, double buffer=%u bytes each\n", bufferSize);
    fflush(stderr);

    return bufferSize;
}

void GLVideoWidget::formatCleanupCallback(void *opaque)
{
    GLVideoWidget *self = static_cast<GLVideoWidget*>(opaque);

    fprintf(stderr, "GLVideoWidget::formatCleanupCallback\n");
    fflush(stderr);

    self->m_mutex.lock();
    self->m_buffer[0].clear();
    self->m_buffer[1].clear();
    self->m_hasFrame = false;
    self->m_textureNeedsUpdate = false;
    self->m_textureAllocated = false;  // Force texture reallocation on next video
    self->m_width = 0;
    self->m_height = 0;
    self->m_mutex.unlock();
}
