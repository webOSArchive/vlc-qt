/**
 * Video Prober - Uses ffprobe to get video metadata
 */

#include "VideoProber.h"

#include <QProcess>
#include <QFileInfo>
#include <QDir>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include <stdarg.h>
#include <stdio.h>

// Debug logging to file
static FILE *s_proberLogFile = nullptr;
static void logProber(const char *fmt, ...) {
    if (!s_proberLogFile) {
        s_proberLogFile = fopen("/media/internal/vlcplayer.log", "a");
    }
    if (s_proberLogFile) {
        va_list args;
        va_start(args, fmt);
        fprintf(s_proberLogFile, "[VideoProber] ");
        vfprintf(s_proberLogFile, fmt, args);
        va_end(args);
        fflush(s_proberLogFile);
    }
}

QString VideoProber::ffprobePath()
{
    // ffprobe is bundled in the app's bin directory
    QString appDir = QCoreApplication::applicationDirPath();
    return appDir + "/ffprobe";
}

QString VideoProber::glibcLdPath()
{
    // Path to glibc's dynamic linker from com.nizovn.glibc package
    return "/media/cryptofs/apps/usr/palm/applications/com.nizovn.glibc/lib/ld.so";
}

QString VideoProber::libraryPath()
{
    // Build library path including app libs and glibc
    QString appDir = QCoreApplication::applicationDirPath();
    QString appLib = appDir + "/../lib";
    QString glibcLib = "/media/cryptofs/apps/usr/palm/applications/com.nizovn.glibc/lib";
    return appLib + ":" + glibcLib;
}

VideoProber::VideoInfo VideoProber::probe(const QString &filePath)
{
    VideoInfo info;
    info.valid = false;

    QString probePath = ffprobePath();
    QFileInfo probeFile(probePath);
    if (!probeFile.exists() || !probeFile.isExecutable()) {
        logProber("ffprobe not found at: %s\n", probePath.toStdString().c_str());
        return info;
    }

    logProber("Probing: %s\n", filePath.toStdString().c_str());

    // Run ffprobe via glibc's ld.so to use the newer glibc
    QString ldPath = glibcLdPath();
    QString libPath = libraryPath();

    QProcess process;
    QStringList args;
    args << "--library-path" << libPath;
    args << probePath;
    args << "-v" << "quiet";
    args << "-print_format" << "json";
    args << "-show_format";
    args << "-show_streams";
    args << "-select_streams" << "v:0";  // First video stream only
    args << filePath;

    logProber("Running: %s %s\n", ldPath.toStdString().c_str(),
              args.join(" ").toStdString().c_str());

    process.start(ldPath, args);
    if (!process.waitForFinished(10000)) {  // 10 second timeout
        logProber("ffprobe timed out\n");
        return info;
    }

    if (process.exitCode() != 0) {
        logProber("ffprobe failed with exit code %d\n", process.exitCode());
        QByteArray errOutput = process.readAllStandardError();
        if (!errOutput.isEmpty()) {
            logProber("ffprobe stderr: %s\n", errOutput.constData());
        }
        return info;
    }

    QByteArray output = process.readAllStandardOutput();
    QJsonDocument doc = QJsonDocument::fromJson(output);
    if (doc.isNull()) {
        logProber("Failed to parse ffprobe JSON output\n");
        return info;
    }

    QJsonObject root = doc.object();

    // Get video stream info
    QJsonArray streams = root["streams"].toArray();
    if (!streams.isEmpty()) {
        QJsonObject videoStream = streams[0].toObject();
        info.width = videoStream["width"].toInt();
        info.height = videoStream["height"].toInt();
        info.codec = videoStream["codec_name"].toString();
        logProber("Video stream: %dx%d, codec=%s\n",
                  info.width, info.height, info.codec.toStdString().c_str());
    }

    // Get duration from format
    QJsonObject format = root["format"].toObject();
    if (format.contains("duration")) {
        double durationSec = format["duration"].toString().toDouble();
        info.durationMs = static_cast<int>(durationSec * 1000.0);
        logProber("Duration: %.2f seconds\n", durationSec);
    }

    info.valid = (info.width > 0 && info.height > 0);
    return info;
}

bool VideoProber::isHD(const VideoInfo &info)
{
    // 720p is 1280x720, so check if height >= 720
    return info.valid && info.height >= 720;
}

QString VideoProber::get480pPath(const QString &originalPath)
{
    QFileInfo fileInfo(originalPath);
    QString dir = fileInfo.absolutePath();
    QString baseName = fileInfo.completeBaseName();
    QString extension = fileInfo.suffix();

    // Create path like: /media/internal/movies/Firefly_480p.mp4
    return dir + "/" + baseName + "_480p." + extension;
}

bool VideoProber::has480pVersion(const QString &originalPath)
{
    QString path480p = get480pPath(originalPath);
    return QFileInfo::exists(path480p);
}

QString VideoProber::resolutionString(const VideoInfo &info)
{
    if (!info.valid) {
        return "Unknown";
    }

    if (info.height >= 2160) {
        return "4K";
    } else if (info.height >= 1080) {
        return "1080p";
    } else if (info.height >= 720) {
        return "720p";
    } else if (info.height >= 480) {
        return "480p";
    } else if (info.height >= 360) {
        return "360p";
    } else {
        return QString("%1p").arg(info.height);
    }
}
