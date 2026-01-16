/**
 * Transcoder - Runs ffmpeg to re-encode video to lower resolution
 */

#ifndef TRANSCODER_H
#define TRANSCODER_H

#include <QObject>
#include <QProcess>
#include <QString>

class Transcoder : public QObject
{
    Q_OBJECT

public:
    explicit Transcoder(QObject *parent = nullptr);
    ~Transcoder();

    // Start transcoding from input to output (480p)
    void start(const QString &inputPath, const QString &outputPath, int durationMs);

    // Cancel ongoing transcode
    void cancel();

    // Check if transcoding is in progress
    bool isRunning() const;

signals:
    // Progress update (0-100), with current time string
    void progressChanged(int percent, const QString &timeStr);

    // Transcoding completed successfully
    void finished(const QString &outputPath);

    // Transcoding failed or was cancelled
    void error(const QString &message);

private slots:
    void onReadyReadStandardOutput();
    void onReadyReadStandardError();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);

private:
    QString ffmpegPath();
    QString glibcLdPath();
    QString libraryPath();
    void parseProgressLine(const QString &line);
    int parseTimeToMs(const QString &timeStr);
    QString formatTime(int ms);
    void cleanup();

    QProcess *m_process;
    QString m_inputPath;
    QString m_outputPath;
    int m_durationMs;
    bool m_cancelled;
};

#endif // TRANSCODER_H
