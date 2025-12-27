/**
 * Transcoder - Runs ffmpeg to re-encode video to lower resolution
 */

#include "Transcoder.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QFile>
#include <QRegularExpression>

#include <stdarg.h>
#include <stdio.h>

// Debug logging to file
static FILE *s_transcoderLogFile = nullptr;
static void logTranscoder(const char *fmt, ...) {
    if (!s_transcoderLogFile) {
        s_transcoderLogFile = fopen("/media/internal/vlcplayer.log", "a");
    }
    if (s_transcoderLogFile) {
        va_list args;
        va_start(args, fmt);
        fprintf(s_transcoderLogFile, "[Transcoder] ");
        vfprintf(s_transcoderLogFile, fmt, args);
        va_end(args);
        fflush(s_transcoderLogFile);
    }
}

Transcoder::Transcoder(QObject *parent)
    : QObject(parent),
      m_process(nullptr),
      m_durationMs(0),
      m_cancelled(false)
{
}

Transcoder::~Transcoder()
{
    cancel();
}

QString Transcoder::ffmpegPath()
{
    QString appDir = QCoreApplication::applicationDirPath();
    return appDir + "/ffmpeg";
}

QString Transcoder::glibcLdPath()
{
    // Path to glibc's dynamic linker from com.nizovn.glibc package
    return "/media/cryptofs/apps/usr/palm/applications/com.nizovn.glibc/lib/ld.so";
}

QString Transcoder::libraryPath()
{
    // Build library path including app libs and glibc
    QString appDir = QCoreApplication::applicationDirPath();
    QString appLib = appDir + "/../lib";
    QString glibcLib = "/media/cryptofs/apps/usr/palm/applications/com.nizovn.glibc/lib";
    return appLib + ":" + glibcLib;
}

void Transcoder::start(const QString &inputPath, const QString &outputPath, int durationMs)
{
    if (m_process) {
        logTranscoder("Already transcoding, ignoring new request\n");
        return;
    }

    m_inputPath = inputPath;
    m_outputPath = outputPath;
    m_durationMs = durationMs;
    m_cancelled = false;

    QString ffmpeg = ffmpegPath();
    QFileInfo ffmpegFile(ffmpeg);
    if (!ffmpegFile.exists() || !ffmpegFile.isExecutable()) {
        logTranscoder("ffmpeg not found at: %s\n", ffmpeg.toStdString().c_str());
        emit error("ffmpeg not found");
        return;
    }

    logTranscoder("Starting transcode:\n");
    logTranscoder("  Input: %s\n", inputPath.toStdString().c_str());
    logTranscoder("  Output: %s\n", outputPath.toStdString().c_str());
    logTranscoder("  Duration: %d ms\n", durationMs);

    // Run ffmpeg via glibc's ld.so to use the newer glibc
    QString ldPath = glibcLdPath();
    QString libPath = libraryPath();

    // Build ffmpeg command - ld.so args first, then ffmpeg path, then ffmpeg args
    QStringList args;
    args << "--library-path" << libPath;
    args << ffmpeg;
    args << "-i" << inputPath;

    // Video: scale to 480p height, auto-calculate width (keep aspect ratio, ensure even)
    args << "-vf" << "scale=-2:480";

    // Video codec: mpeg4 (libx264 not available in this ffmpeg build)
    // Use moderate bitrate for reasonable quality at 480p
    args << "-c:v" << "mpeg4";
    args << "-b:v" << "1500k";    // 1.5 Mbps video bitrate
    args << "-q:v" << "5";        // Quality scale (2-31, lower is better)

    // Audio: AAC at 128kbps (need -strict -2 for experimental encoder)
    args << "-c:a" << "aac";
    args << "-strict" << "-2";
    args << "-b:a" << "128k";

    // Output format hints
    args << "-movflags" << "+faststart";  // Enable streaming

    // Overwrite output file
    args << "-y";

    // Progress output to stdout (machine-readable)
    args << "-progress" << "pipe:1";

    args << outputPath;

    m_process = new QProcess(this);

    connect(m_process, &QProcess::readyReadStandardOutput,
            this, &Transcoder::onReadyReadStandardOutput);
    connect(m_process, &QProcess::readyReadStandardError,
            this, &Transcoder::onReadyReadStandardError);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &Transcoder::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred,
            this, &Transcoder::onProcessError);

    logTranscoder("Running: %s %s\n", ldPath.toStdString().c_str(),
                  args.join(" ").toStdString().c_str());

    m_process->start(ldPath, args);
}

void Transcoder::cancel()
{
    if (m_process) {
        logTranscoder("Cancelling transcode\n");
        m_cancelled = true;
        m_process->terminate();
        if (!m_process->waitForFinished(3000)) {
            m_process->kill();
            m_process->waitForFinished(1000);
        }
        cleanup();

        // Delete partial output file
        if (!m_outputPath.isEmpty() && QFile::exists(m_outputPath)) {
            logTranscoder("Removing partial output: %s\n", m_outputPath.toStdString().c_str());
            QFile::remove(m_outputPath);
        }
    }
}

bool Transcoder::isRunning() const
{
    return m_process != nullptr && m_process->state() != QProcess::NotRunning;
}

void Transcoder::onReadyReadStandardOutput()
{
    while (m_process->canReadLine()) {
        QString line = QString::fromUtf8(m_process->readLine()).trimmed();
        parseProgressLine(line);
    }
}

void Transcoder::onReadyReadStandardError()
{
    QByteArray data = m_process->readAllStandardError();
    // Log stderr but don't parse it (it's human-readable status, not progress)
    if (!data.isEmpty()) {
        logTranscoder("ffmpeg stderr: %s\n", data.constData());
    }
}

void Transcoder::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    logTranscoder("ffmpeg finished: exitCode=%d, exitStatus=%d\n", exitCode, (int)exitStatus);

    if (m_cancelled) {
        cleanup();
        emit error("Transcoding cancelled");
        return;
    }

    if (exitCode == 0 && exitStatus == QProcess::NormalExit) {
        // Verify output file exists
        if (QFile::exists(m_outputPath)) {
            logTranscoder("Transcode completed successfully\n");
            cleanup();
            emit finished(m_outputPath);
        } else {
            logTranscoder("Output file not found after transcode\n");
            cleanup();
            emit error("Output file not created");
        }
    } else {
        cleanup();
        emit error(QString("ffmpeg failed with exit code %1").arg(exitCode));
    }
}

void Transcoder::onProcessError(QProcess::ProcessError processError)
{
    logTranscoder("ffmpeg process error: %d\n", (int)processError);
    QString errorMsg;
    switch (processError) {
    case QProcess::FailedToStart:
        errorMsg = "Failed to start ffmpeg";
        break;
    case QProcess::Crashed:
        errorMsg = "ffmpeg crashed";
        break;
    case QProcess::Timedout:
        errorMsg = "ffmpeg timed out";
        break;
    default:
        errorMsg = "ffmpeg error";
        break;
    }
    cleanup();
    emit error(errorMsg);
}

void Transcoder::parseProgressLine(const QString &line)
{
    // ffmpeg -progress pipe:1 outputs key=value pairs
    // We're looking for: out_time_ms=123456789 or out_time=HH:MM:SS.mmm
    if (line.startsWith("out_time_ms=")) {
        QString value = line.mid(12);
        bool ok;
        qint64 timeUs = value.toLongLong(&ok);  // Actually microseconds
        if (ok && m_durationMs > 0) {
            int timeMs = static_cast<int>(timeUs / 1000);
            int percent = qBound(0, static_cast<int>((timeMs * 100) / m_durationMs), 100);
            emit progressChanged(percent, formatTime(timeMs));
        }
    } else if (line.startsWith("out_time=")) {
        QString timeStr = line.mid(9);
        int timeMs = parseTimeToMs(timeStr);
        if (timeMs >= 0 && m_durationMs > 0) {
            int percent = qBound(0, static_cast<int>((timeMs * 100) / m_durationMs), 100);
            emit progressChanged(percent, formatTime(timeMs));
        }
    } else if (line.startsWith("progress=")) {
        QString status = line.mid(9);
        logTranscoder("Progress status: %s\n", status.toStdString().c_str());
        if (status == "end") {
            emit progressChanged(100, formatTime(m_durationMs));
        }
    }
}

int Transcoder::parseTimeToMs(const QString &timeStr)
{
    // Parse HH:MM:SS.mmm format
    QRegularExpression re("(\\d+):(\\d+):(\\d+)\\.(\\d+)");
    QRegularExpressionMatch match = re.match(timeStr);
    if (match.hasMatch()) {
        int hours = match.captured(1).toInt();
        int mins = match.captured(2).toInt();
        int secs = match.captured(3).toInt();
        int ms = match.captured(4).left(3).toInt();  // Take first 3 digits
        return ((hours * 3600 + mins * 60 + secs) * 1000) + ms;
    }
    return -1;
}

QString Transcoder::formatTime(int ms)
{
    int seconds = ms / 1000;
    int minutes = seconds / 60;
    int hours = minutes / 60;
    seconds = seconds % 60;
    minutes = minutes % 60;

    if (hours > 0) {
        return QString("%1:%2:%3")
            .arg(hours)
            .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0'));
    } else {
        return QString("%1:%2")
            .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0'));
    }
}

void Transcoder::cleanup()
{
    if (m_process) {
        m_process->deleteLater();
        m_process = nullptr;
    }
}
