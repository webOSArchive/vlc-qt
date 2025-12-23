/**
 * VLC Player for webOS - Main Window
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSlider>
#include <QPushButton>
#include <QLabel>
#include <QTimer>

// Forward declarations
class VlcInstance;
class VlcMedia;
class VlcMediaPlayer;
class FBVideoWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void openFile(const QString &path);

public slots:
    void onOpenFile();
    void onPlayPause();
    void onStop();
    void onSeek(int position);
    void onVolumeChanged(int volume);

private slots:
    void updatePosition();
    void updateState();
    void onMediaChanged();
    void hideForPlayback();
    void showForUI();
    void onVideoTapped();

private:
    void setupUI();
    void setupVLC();
    void setupConnections();
    QString formatTime(int ms) const;

    // VLC components
    VlcInstance *m_instance;
    VlcMedia *m_media;
    VlcMediaPlayer *m_player;

    // UI components
    QWidget *m_videoWidget;
    FBVideoWidget *m_fbVideoWidget;  // For FB mode state connections
    QWidget *m_controlsWidget;
    QPushButton *m_playButton;
    QPushButton *m_stopButton;
    QPushButton *m_openButton;
    QSlider *m_seekSlider;
    QSlider *m_volumeSlider;
    QLabel *m_timeLabel;
    QLabel *m_titleLabel;

    // Timer for position updates
    QTimer *m_positionTimer;

    // State
    bool m_seeking;
};

#endif // MAINWINDOW_H
