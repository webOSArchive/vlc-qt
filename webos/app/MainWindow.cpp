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

    // Auto-open test file - use DuckTales episode for longer test
    QString testFile = "/media/internal/movies/DuckTales/1x32-MicroDucksFromOuterSpace.mp4";
    if (QFile::exists(testFile)) {
        fprintf(stderr, "MainWindow: Auto-opening test file: %s\n", testFile.toStdString().c_str());
        fflush(stderr);
        openFile(testFile);
    } else {
        // Fallback to short test
        testFile = "/media/internal/test.mp4";
        if (QFile::exists(testFile)) {
            fprintf(stderr, "MainWindow: Fallback to test.mp4\n");
            fflush(stderr);
            openFile(testFile);
        } else {
            fprintf(stderr, "MainWindow: No test files found\n");
            fflush(stderr);
        }
    }

    // Auto-exit after 30 seconds for testing
    QTimer::singleShot(30000, this, [this]() {
        fprintf(stderr, "\n=== 30 SECOND TEST COMPLETE ===\n");
        fprintf(stderr, "Exiting for log analysis\n");
        fflush(stderr);
        QApplication::quit();
    });
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
    fprintf(stderr, "MainWindow: Using GLESVideoWidget (EGL + OpenGL ES 2.0)\n");
    fflush(stderr);
    GLESVideoWidget *videoWidget = new GLESVideoWidget(this);
    videoWidget->setMediaPlayer(m_player);
    m_videoWidget = videoWidget;
#elif VIDEO_RENDER_MODE == 2
    // Direct framebuffer rendering - bypasses Qt
    fprintf(stderr, "MainWindow: Using FBVideoWidget (framebuffer)\n");
    fflush(stderr);
    FBVideoWidget *videoWidget = new FBVideoWidget(this);
    videoWidget->setMediaPlayer(m_player);
    m_videoWidget = videoWidget;
#elif VIDEO_RENDER_MODE == 1
    // GPU-accelerated rendering via OpenGL ES 2.0
    fprintf(stderr, "MainWindow: Using GLVideoWidget (OpenGL)\n");
    fflush(stderr);
    GLVideoWidget *videoWidget = new GLVideoWidget(this);
    videoWidget->setMediaPlayer(m_player);
    m_videoWidget = videoWidget;
#else
    // Software rendering for webOS
    fprintf(stderr, "MainWindow: Using VideoWidget (software)\n");
    fflush(stderr);
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

    // VLC connections
    connect(m_player, &VlcMediaPlayer::stateChanged, this, &MainWindow::updateState);

    // Set initial volume
    onVolumeChanged(m_volumeSlider->value());
}

void MainWindow::openFile(const QString &path)
{
    fprintf(stderr, "MainWindow::openFile: %s\n", path.toStdString().c_str());
    fflush(stderr);

    if (m_media) {
        delete m_media;
    }

    m_media = new VlcMedia(path, true, m_instance);
    fprintf(stderr, "MainWindow: VlcMedia created, opening with player\n");
    fflush(stderr);

    m_player->open(m_media);
    fprintf(stderr, "MainWindow: player->open() called, calling play()\n");
    fflush(stderr);

    m_player->play();
    fprintf(stderr, "MainWindow: play() called\n");
    fflush(stderr);

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

QString MainWindow::formatTime(int ms) const
{
    int seconds = ms / 1000;
    int minutes = seconds / 60;
    seconds = seconds % 60;

    return QString("%1:%2")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'));
}
