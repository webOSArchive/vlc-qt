/**
 * Transcode Dialog - Offers transcoding and shows progress
 */

#ifndef TRANSCODEDIALOG_H
#define TRANSCODEDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QVBoxLayout>

#include "VideoProber.h"
#include "Transcoder.h"

class TranscodeDialog : public QDialog
{
    Q_OBJECT

public:
    enum Result {
        Cancelled = 0,
        Transcode = 1,
        PlayAnyway = 2,
        TranscodeComplete = 3
    };

    explicit TranscodeDialog(QWidget *parent = nullptr);
    ~TranscodeDialog();

    // Show the offer dialog ("This video is 1080p, transcode to 480p?")
    void showOffer(const VideoProber::VideoInfo &info, const QString &filePath);

    // Start transcoding and show progress
    void startTranscode(const QString &inputPath, const QString &outputPath,
                        int durationMs);

    // Get the output path after successful transcode
    QString outputPath() const { return m_outputPath; }

signals:
    void transcodeFinished(const QString &outputPath);
    void transcodeFailed(const QString &error);

private slots:
    void onTranscodeClicked();
    void onPlayAnywayClicked();
    void onCancelClicked();
    void onProgressChanged(int percent, const QString &timeStr);
    void onTranscodeComplete(const QString &outputPath);
    void onTranscodeError(const QString &message);

private:
    void setupOfferUI();
    void setupProgressUI();
    void switchToProgressMode();

    // Offer mode widgets
    QLabel *m_infoLabel;
    QPushButton *m_transcodeButton;
    QPushButton *m_playAnywayButton;
    QPushButton *m_cancelButton;

    // Progress mode widgets
    QLabel *m_progressLabel;
    QProgressBar *m_progressBar;
    QLabel *m_timeLabel;
    QPushButton *m_cancelProgressButton;

    // Layout containers
    QWidget *m_offerWidget;
    QWidget *m_progressWidget;
    QVBoxLayout *m_mainLayout;

    // Transcoder
    Transcoder *m_transcoder;

    // State
    QString m_inputPath;
    QString m_outputPath;
    int m_durationMs;
    VideoProber::VideoInfo m_videoInfo;
};

#endif // TRANSCODEDIALOG_H
