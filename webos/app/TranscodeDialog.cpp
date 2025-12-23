/**
 * Transcode Dialog - Offers transcoding and shows progress
 */

#include "TranscodeDialog.h"

#include <QHBoxLayout>
#include <QMessageBox>

TranscodeDialog::TranscodeDialog(QWidget *parent)
    : QDialog(parent),
      m_transcoder(nullptr),
      m_durationMs(0)
{
    setWindowTitle("HD Video Detected");
    setModal(true);
    setMinimumWidth(400);

    // Dark theme to match main app
    setStyleSheet(
        "QDialog { background-color: #222; }"
        "QLabel { color: white; font-size: 14px; }"
        "QPushButton { background-color: #444; color: white; border: none; "
        "              padding: 12px 20px; font-size: 14px; border-radius: 5px; "
        "              min-width: 100px; }"
        "QPushButton:hover { background-color: #555; }"
        "QPushButton:pressed { background-color: #666; }"
        "QPushButton#transcodeBtn { background-color: #0a8; }"
        "QPushButton#transcodeBtn:hover { background-color: #0b9; }"
        "QProgressBar { border: 2px solid #444; border-radius: 5px; "
        "               background-color: #333; height: 20px; }"
        "QProgressBar::chunk { background-color: #0af; border-radius: 3px; }");

    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(15);
    m_mainLayout->setContentsMargins(20, 20, 20, 20);

    setupOfferUI();
    setupProgressUI();

    // Start in offer mode
    m_offerWidget->show();
    m_progressWidget->hide();

    m_transcoder = new Transcoder(this);
    connect(m_transcoder, &Transcoder::progressChanged,
            this, &TranscodeDialog::onProgressChanged);
    connect(m_transcoder, &Transcoder::finished,
            this, &TranscodeDialog::onTranscodeComplete);
    connect(m_transcoder, &Transcoder::error,
            this, &TranscodeDialog::onTranscodeError);
}

TranscodeDialog::~TranscodeDialog()
{
    if (m_transcoder && m_transcoder->isRunning()) {
        m_transcoder->cancel();
    }
}

void TranscodeDialog::setupOfferUI()
{
    m_offerWidget = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(m_offerWidget);
    layout->setSpacing(15);
    layout->setContentsMargins(0, 0, 0, 0);

    m_infoLabel = new QLabel(this);
    m_infoLabel->setWordWrap(true);
    m_infoLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_infoLabel);

    QLabel *warningLabel = new QLabel(
        "The TouchPad's CPU may struggle with HD video.\n"
        "Re-encoding to 480p will enable smooth playback.\n\n"
        "Note: This can take several hours for long videos.",
        this);
    warningLabel->setWordWrap(true);
    warningLabel->setAlignment(Qt::AlignCenter);
    warningLabel->setStyleSheet("color: #aaa; font-size: 12px;");
    layout->addWidget(warningLabel);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(10);

    m_transcodeButton = new QPushButton("Re-encode to 480p", this);
    m_transcodeButton->setObjectName("transcodeBtn");
    connect(m_transcodeButton, &QPushButton::clicked,
            this, &TranscodeDialog::onTranscodeClicked);

    m_playAnywayButton = new QPushButton("Play Anyway", this);
    connect(m_playAnywayButton, &QPushButton::clicked,
            this, &TranscodeDialog::onPlayAnywayClicked);

    m_cancelButton = new QPushButton("Cancel", this);
    connect(m_cancelButton, &QPushButton::clicked,
            this, &TranscodeDialog::onCancelClicked);

    buttonLayout->addWidget(m_transcodeButton);
    buttonLayout->addWidget(m_playAnywayButton);
    buttonLayout->addWidget(m_cancelButton);

    layout->addLayout(buttonLayout);
    m_mainLayout->addWidget(m_offerWidget);
}

void TranscodeDialog::setupProgressUI()
{
    m_progressWidget = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(m_progressWidget);
    layout->setSpacing(15);
    layout->setContentsMargins(0, 0, 0, 0);

    m_progressLabel = new QLabel("Re-encoding video to 480p...", this);
    m_progressLabel->setAlignment(Qt::AlignCenter);
    m_progressLabel->setStyleSheet("font-size: 16px; font-weight: bold;");
    layout->addWidget(m_progressLabel);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    layout->addWidget(m_progressBar);

    m_timeLabel = new QLabel("00:00 / --:--", this);
    m_timeLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_timeLabel);

    QLabel *tipLabel = new QLabel(
        "Keep the device plugged in and awake.\n"
        "You can close this dialog - transcoding will continue.",
        this);
    tipLabel->setWordWrap(true);
    tipLabel->setAlignment(Qt::AlignCenter);
    tipLabel->setStyleSheet("color: #aaa; font-size: 11px;");
    layout->addWidget(tipLabel);

    m_cancelProgressButton = new QPushButton("Cancel", this);
    connect(m_cancelProgressButton, &QPushButton::clicked, this, [this]() {
        if (m_transcoder) {
            m_transcoder->cancel();
        }
        reject();
    });
    layout->addWidget(m_cancelProgressButton, 0, Qt::AlignCenter);

    m_mainLayout->addWidget(m_progressWidget);
}

void TranscodeDialog::showOffer(const VideoProber::VideoInfo &info, const QString &filePath)
{
    m_videoInfo = info;
    m_inputPath = filePath;
    m_outputPath = VideoProber::get480pPath(filePath);
    m_durationMs = info.durationMs;

    QString resolution = VideoProber::resolutionString(info);
    QString infoText = QString(
        "This video is <b>%1</b> (%2x%3).<br><br>"
        "Would you like to re-encode it to 480p for smoother playback?")
        .arg(resolution)
        .arg(info.width)
        .arg(info.height);

    m_infoLabel->setText(infoText);

    m_offerWidget->show();
    m_progressWidget->hide();
    adjustSize();
}

void TranscodeDialog::startTranscode(const QString &inputPath, const QString &outputPath,
                                     int durationMs)
{
    m_inputPath = inputPath;
    m_outputPath = outputPath;
    m_durationMs = durationMs;

    switchToProgressMode();
    m_transcoder->start(inputPath, outputPath, durationMs);
}

void TranscodeDialog::switchToProgressMode()
{
    m_offerWidget->hide();
    m_progressWidget->show();
    m_progressBar->setValue(0);

    // Format duration for display
    int totalSec = m_durationMs / 1000;
    int mins = totalSec / 60;
    int secs = totalSec % 60;
    QString durationStr;
    if (mins >= 60) {
        int hours = mins / 60;
        mins = mins % 60;
        durationStr = QString("%1:%2:%3")
            .arg(hours)
            .arg(mins, 2, 10, QChar('0'))
            .arg(secs, 2, 10, QChar('0'));
    } else {
        durationStr = QString("%1:%2")
            .arg(mins, 2, 10, QChar('0'))
            .arg(secs, 2, 10, QChar('0'));
    }
    m_timeLabel->setText(QString("00:00 / %1").arg(durationStr));

    adjustSize();
}

void TranscodeDialog::onTranscodeClicked()
{
    startTranscode(m_inputPath, m_outputPath, m_durationMs);
}

void TranscodeDialog::onPlayAnywayClicked()
{
    done(PlayAnyway);
}

void TranscodeDialog::onCancelClicked()
{
    done(Cancelled);
}

void TranscodeDialog::onProgressChanged(int percent, const QString &timeStr)
{
    m_progressBar->setValue(percent);

    // Format duration for display
    int totalSec = m_durationMs / 1000;
    int mins = totalSec / 60;
    int secs = totalSec % 60;
    QString durationStr;
    if (mins >= 60) {
        int hours = mins / 60;
        mins = mins % 60;
        durationStr = QString("%1:%2:%3")
            .arg(hours)
            .arg(mins, 2, 10, QChar('0'))
            .arg(secs, 2, 10, QChar('0'));
    } else {
        durationStr = QString("%1:%2")
            .arg(mins, 2, 10, QChar('0'))
            .arg(secs, 2, 10, QChar('0'));
    }
    m_timeLabel->setText(QString("%1 / %2").arg(timeStr).arg(durationStr));
}

void TranscodeDialog::onTranscodeComplete(const QString &outputPath)
{
    m_outputPath = outputPath;
    m_progressLabel->setText("Re-encoding complete!");
    m_progressBar->setValue(100);
    m_cancelProgressButton->setText("Play 480p Version");

    disconnect(m_cancelProgressButton, nullptr, nullptr, nullptr);
    connect(m_cancelProgressButton, &QPushButton::clicked, this, [this]() {
        done(TranscodeComplete);
    });

    emit transcodeFinished(outputPath);
}

void TranscodeDialog::onTranscodeError(const QString &message)
{
    m_progressLabel->setText("Re-encoding failed");
    m_progressLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #f44;");

    QMessageBox::warning(this, "Transcode Failed",
                         QString("Failed to re-encode video:\n%1").arg(message));

    emit transcodeFailed(message);
    reject();
}
