/**
 * VLC Player for webOS - Main Window Implementation
 */

#include "MainWindow.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QStyle>
#include <QApplication>
#include <QFile>
#include <stdarg.h>

// Debug logging to file
static FILE *s_logFile = nullptr;
static void logMsg(const char *fmt, ...) {
    if (!s_logFile) {
        s_logFile = fopen("/media/internal/vlcplayer.log", "a");
    }
    if (s_logFile) {
        va_list args;
        va_start(args, fmt);
        vfprintf(s_logFile, fmt, args);
        va_end(args);
        fflush(s_logFile);
    }
}

#include "Common.h"
#include "Instance.h"
#include "Media.h"
#include "MediaPlayer.h"
#include "Audio.h"

#include "VideoWidget.h"
#include "GLVideoWidget.h"
#include "FBVideoWidget.h"
#include "GLESVideoWidget.h"

// Video rendering mode:
// 0 = Software (VideoWidget with QPainter)
// 1 = OpenGL (GLVideoWidget - crashes on TouchPad)
// 2 = Framebuffer (FBVideoWidget - direct /dev/fb0) - WORKS, shows video
// 3 = OpenGL ES 2.0 (GLESVideoWidget - EGL + GLES2) - fast but layer conflict
#define VIDEO_RENDER_MODE 2

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_instance(nullptr),
      m_media(nullptr),
      m_player(nullptr),
      m_fbVideoWidget(nullptr),
      m_seeking(false)
{
    setupVLC();
    setupUI();
    setupConnections();

    // Position update timer
    m_positionTimer = new QTimer(this);
    connect(m_positionTimer, &QTimer::timeout, this, &MainWindow::updatePosition);
    m_positionTimer->start(100);

    setWindowTitle("VLC Player");
    resize(1024, 768);
}

MainWindow::~MainWindow()
{
    delete m_media;
    delete m_player;
    delete m_instance;
}

void MainWindow::setupVLC()
{
    // VLC arguments optimized for webOS
    QStringList args;
    args << "--no-xlib";             // No X11
    args << "--vout=vmem";           // Use memory video output for software rendering
    args << "--aout=alsa";           // Use ALSA audio output
    args << "--no-video-title-show"; // Don't show title on video
    args << "--no-snapshot-preview"; // No snapshot preview
    args << "--no-osd";              // No on-screen display

    // Force FFmpeg software decoding - hardware decoders (omxil) fail silently
    // on many video formats, outputting NV12 but not actually decoding frames
    args << "--codec=avcodec,none";  // Use FFmpeg avcodec only
    args << "--avcodec-hw=none";     // Disable hardware acceleration
    args << "--avcodec-threads=2";   // Limit threads on slow ARM device

    // Decoder optimizations for slow ARM CPU
    args << "--avcodec-skiploopfilter=4";  // Skip deblocking filter (all frames)
    args << "--avcodec-skip-idct=4";       // Skip IDCT on all frames (faster, lower quality)
    args << "--avcodec-skip-frame=1";      // Skip non-reference frames when behind
    args << "--avcodec-fast";              // Fast decoding mode
    args << "--avcodec-dr";                // Direct rendering (less copying)
    args << "--sout-avcodec-hurry-up";     // Allow skipping when behind

    // Clock/sync adjustments for smoother playback on slow devices
    args << "--clock-jitter=100";          // Allow more timing jitter
    args << "--clock-synchro=0";           // Disable strict sync (smoother on slow CPU)

    m_instance = new VlcInstance(args, this);
    m_player = new VlcMediaPlayer(m_instance);
}

void MainWindow::setupUI()
{
    // Central widget
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    // Main layout
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Title bar
    m_titleLabel = new QLabel("VLC Player for webOS", this);
    m_titleLabel->setStyleSheet(
        "QLabel { background-color: #333; color: white; padding: 10px; font-size: 18px; }");
    m_titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(m_titleLabel);

    // Video widget
#if VIDEO_RENDER_MODE == 3
    // OpenGL ES 2.0 with EGL - hardware accelerated
    logMsg( "MainWindow: Using GLESVideoWidget (EGL + OpenGL ES 2.0)\n");
        GLESVideoWidget *videoWidget = new GLESVideoWidget(this);
    videoWidget->setMediaPlayer(m_player);
    m_videoWidget = videoWidget;
#elif VIDEO_RENDER_MODE == 2
    // Direct framebuffer rendering - bypasses Qt
    logMsg( "MainWindow: Using FBVideoWidget (framebuffer)\n");
        FBVideoWidget *videoWidget = new FBVideoWidget(this);
    videoWidget->setMediaPlayer(m_player);
    m_videoWidget = videoWidget;
    m_fbVideoWidget = videoWidget;  // Keep reference for state connections
#elif VIDEO_RENDER_MODE == 1
    // GPU-accelerated rendering via OpenGL ES 2.0
    logMsg( "MainWindow: Using GLVideoWidget (OpenGL)\n");
        GLVideoWidget *videoWidget = new GLVideoWidget(this);
    videoWidget->setMediaPlayer(m_player);
    m_videoWidget = videoWidget;
#else
    // Software rendering for webOS
    logMsg( "MainWindow: Using VideoWidget (software)\n");
        VideoWidget *videoWidget = new VideoWidget(this);
    videoWidget->setMediaPlayer(m_player);
    m_videoWidget = videoWidget;
#endif
    m_videoWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    mainLayout->addWidget(m_videoWidget, 1);

    // Controls widget
    m_controlsWidget = new QWidget(this);
    m_controlsWidget->setStyleSheet(
        "QWidget { background-color: #222; }"
        "QPushButton { background-color: #444; color: white; border: none; "
        "              padding: 15px 25px; font-size: 16px; border-radius: 5px; }"
        "QPushButton:hover { background-color: #555; }"
        "QPushButton:pressed { background-color: #666; }"
        "QSlider::groove:horizontal { background: #555; height: 10px; border-radius: 5px; }"
        "QSlider::handle:horizontal { background: #0af; width: 20px; margin: -5px 0; border-radius: 10px; }"
        "QSlider::sub-page:horizontal { background: #0af; border-radius: 5px; }"
        "QLabel { color: white; font-size: 14px; }");

    QVBoxLayout *controlsLayout = new QVBoxLayout(m_controlsWidget);
    controlsLayout->setContentsMargins(10, 10, 10, 10);

    // Seek slider
    QHBoxLayout *seekLayout = new QHBoxLayout();
    m_seekSlider = new QSlider(Qt::Horizontal, this);
    m_seekSlider->setRange(0, 1000);
    m_timeLabel = new QLabel("00:00 / 00:00", this);
    m_timeLabel->setMinimumWidth(120);
    seekLayout->addWidget(m_seekSlider, 1);
    seekLayout->addWidget(m_timeLabel);
    controlsLayout->addLayout(seekLayout);

    // Buttons row
    QHBoxLayout *buttonsLayout = new QHBoxLayout();
    buttonsLayout->setSpacing(10);

    m_openButton = new QPushButton("Open", this);
    m_playButton = new QPushButton("Play", this);
    m_stopButton = new QPushButton("Stop", this);

    QLabel *volumeLabel = new QLabel("Volume:", this);
    m_volumeSlider = new QSlider(Qt::Horizontal, this);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(80);
    m_volumeSlider->setMaximumWidth(150);

    buttonsLayout->addWidget(m_openButton);
    buttonsLayout->addWidget(m_playButton);
    buttonsLayout->addWidget(m_stopButton);
    buttonsLayout->addStretch();
    buttonsLayout->addWidget(volumeLabel);
    buttonsLayout->addWidget(m_volumeSlider);

    controlsLayout->addLayout(buttonsLayout);
    mainLayout->addWidget(m_controlsWidget);
}

void MainWindow::setupConnections()
{
    // Button connections
    connect(m_openButton, &QPushButton::clicked, this, &MainWindow::onOpenFile);
    connect(m_playButton, &QPushButton::clicked, this, &MainWindow::onPlayPause);
    connect(m_stopButton, &QPushButton::clicked, this, &MainWindow::onStop);

    // Slider connections
    connect(m_seekSlider, &QSlider::sliderPressed, [this]() { m_seeking = true; });
    connect(m_seekSlider, &QSlider::sliderReleased, [this]() {
        m_seeking = false;
        onSeek(m_seekSlider->value());
    });
    connect(m_volumeSlider, &QSlider::valueChanged, this, &MainWindow::onVolumeChanged);

    // VLC connections - detailed logging for debugging
    connect(m_player, &VlcMediaPlayer::stateChanged, this, &MainWindow::updateState);
    connect(m_player, &VlcMediaPlayer::error, this, &MainWindow::onVlcError);
    connect(m_player, &VlcMediaPlayer::vout, this, &MainWindow::onVlcVout);
    connect(m_player, &VlcMediaPlayer::opening, this, &MainWindow::onVlcOpening);
    connect(m_player, &VlcMediaPlayer::playing, this, &MainWindow::onVlcPlaying);
    connect(m_player, &VlcMediaPlayer::paused, this, &MainWindow::onVlcPaused);
    connect(m_player, &VlcMediaPlayer::stopped, this, &MainWindow::onVlcStopped);
    connect(m_player, &VlcMediaPlayer::end, this, &MainWindow::onVlcEnd);
    connect(m_player, static_cast<void(VlcMediaPlayer::*)(int)>(&VlcMediaPlayer::buffering),
            this, &MainWindow::onVlcBuffering);

    // FBVideoWidget playback state connections - hide/show UI based on playback
    if (m_fbVideoWidget) {
        connect(m_player, &VlcMediaPlayer::playing, m_fbVideoWidget, &FBVideoWidget::onPlaybackStarted);
        connect(m_player, &VlcMediaPlayer::paused, m_fbVideoWidget, &FBVideoWidget::onPlaybackStopped);
        connect(m_player, &VlcMediaPlayer::stopped, m_fbVideoWidget, &FBVideoWidget::onPlaybackStopped);
        connect(m_player, &VlcMediaPlayer::end, m_fbVideoWidget, &FBVideoWidget::onPlaybackStopped);

        // Hide Qt UI only after first video frame is rendered (avoids black screen)
        connect(m_fbVideoWidget, &FBVideoWidget::firstFrameReady, this, &MainWindow::hideForPlayback);
        // Show Qt UI when stopped/paused
        connect(m_player, &VlcMediaPlayer::paused, this, &MainWindow::showForUI);
        connect(m_player, &VlcMediaPlayer::stopped, this, &MainWindow::showForUI);
        connect(m_player, &VlcMediaPlayer::end, this, &MainWindow::showForUI);

        // When user taps during playback, pause and show UI
        connect(m_fbVideoWidget, &FBVideoWidget::tapped, this, &MainWindow::onVideoTapped);
    }

    // Set initial volume
    onVolumeChanged(m_volumeSlider->value());
}

void MainWindow::openFile(const QString &path)
{
    logMsg( "MainWindow::openFile: %s\n", path.toStdString().c_str());
    
    if (m_media) {
        delete m_media;
    }

    m_media = new VlcMedia(path, true, m_instance);
    logMsg( "MainWindow: VlcMedia created, opening with player\n");
    
    m_player->open(m_media);
    logMsg( "MainWindow: player->open() called, calling play()\n");
    
    m_player->play();
    logMsg( "MainWindow: play() called\n");
    
    // Update title
    QFileInfo fileInfo(path);
    m_titleLabel->setText(fileInfo.fileName());
}

void MainWindow::onOpenFile()
{
    // Default to internal storage on webOS
    QString defaultPath = "/media/internal";
    if (!QDir(defaultPath).exists()) {
        defaultPath = QDir::homePath();
    }

    QString filePath = QFileDialog::getOpenFileName(
        this,
        "Open Media File",
        defaultPath,
        "Media Files (*.mp4 *.mkv *.avi *.mov *.mp3 *.flac *.wav *.ogg);;All Files (*)");

    if (!filePath.isEmpty()) {
        openFile(filePath);
    }
}

void MainWindow::onPlayPause()
{
    if (!m_player) return;

    if (m_player->state() == Vlc::Playing) {
        m_player->pause();
    } else {
        m_player->play();
    }
}

void MainWindow::onStop()
{
    if (m_player) {
        m_player->stop();
    }
}

void MainWindow::onSeek(int position)
{
    if (m_player && m_player->length() > 0) {
        float pos = position / 1000.0f;
        m_player->setPosition(pos);
    }
}

void MainWindow::onVolumeChanged(int volume)
{
    if (m_player) {
        m_player->audio()->setVolume(volume);
    }
}

void MainWindow::updatePosition()
{
    if (!m_player || m_seeking) return;

    int length = m_player->length();
    int time = m_player->time();

    if (length > 0) {
        int pos = static_cast<int>((time * 1000.0) / length);
        m_seekSlider->setValue(pos);
    }

    m_timeLabel->setText(QString("%1 / %2")
                             .arg(formatTime(time))
                             .arg(formatTime(length)));
}

void MainWindow::updateState()
{
    if (!m_player) return;

    Vlc::State state = m_player->state();

    switch (state) {
    case Vlc::Playing:
        m_playButton->setText("Pause");
        break;
    case Vlc::Paused:
    case Vlc::Stopped:
    case Vlc::Ended:
        m_playButton->setText("Play");
        break;
    default:
        break;
    }
}

void MainWindow::onMediaChanged()
{
    m_seekSlider->setValue(0);
    m_timeLabel->setText("00:00 / 00:00");
}

void MainWindow::hideForPlayback()
{
    logMsg( "MainWindow: Hiding UI for video playback\n");
    
    // Hide Qt UI widgets so video can render fullscreen
    // Keep video widget visible to capture touch events
    m_titleLabel->hide();
    m_controlsWidget->hide();
    // m_videoWidget stays visible to receive touch/mouse events
}

void MainWindow::showForUI()
{
    logMsg( "MainWindow: Showing UI\n");
    
    // Show Qt UI widgets
    m_titleLabel->show();
    m_controlsWidget->show();

    // Force repaint
    update();
    repaint();
}

void MainWindow::onVideoTapped()
{
    logMsg( "MainWindow: Video tapped - pausing and showing UI\n");
    
    if (m_player && m_player->state() == Vlc::Playing) {
        m_player->pause();
    }
    // showForUI will be called by the paused signal
}

QString MainWindow::formatTime(int ms) const
{
    int seconds = ms / 1000;
    int minutes = seconds / 60;
    seconds = seconds % 60;

    return QString("%1:%2")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'));
}

void MainWindow::onVlcError()
{
    logMsg("*** VLC ERROR: Playback error occurred ***\n");
}

void MainWindow::onVlcVout(int count)
{
    logMsg("VLC vout: video outputs available = %d\n", count);
}

void MainWindow::onVlcOpening()
{
    logMsg("VLC signal: opening\n");
}

void MainWindow::onVlcPlaying()
{
    logMsg("VLC signal: playing\n");
}

void MainWindow::onVlcPaused()
{
    logMsg("VLC signal: paused\n");
}

void MainWindow::onVlcStopped()
{
    logMsg("VLC signal: stopped\n");
}

void MainWindow::onVlcEnd()
{
    logMsg("VLC signal: end reached\n");
}

void MainWindow::onVlcBuffering(int percent)
{
    if (percent == 0 || percent == 100 || percent % 25 == 0) {
        logMsg("VLC buffering: %d%%\n", percent);
    }
}
