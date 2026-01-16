/**
 * Video Prober - Uses ffprobe to get video metadata
 */

#ifndef VIDEOPROBER_H
#define VIDEOPROBER_H

#include <QString>
#include <QObject>

class VideoProber
{
public:
    struct VideoInfo {
        int width;
        int height;
        int durationMs;
        QString codec;
        bool valid;

        VideoInfo() : width(0), height(0), durationMs(0), valid(false) {}
    };

    // Probe video file and return metadata
    static VideoInfo probe(const QString &filePath);

    // Check if video is HD (720p or higher)
    static bool isHD(const VideoInfo &info);

    // Get the 480p version path for a video file
    // e.g., /media/internal/movies/Firefly.mp4 -> /media/internal/movies/Firefly_480p.mp4
    static QString get480pPath(const QString &originalPath);

    // Check if 480p version already exists
    static bool has480pVersion(const QString &originalPath);

    // Get human-readable resolution string (e.g., "1080p", "720p", "480p")
    static QString resolutionString(const VideoInfo &info);

private:
    // Path to ffprobe binary (within app bundle)
    static QString ffprobePath();
    // Path to glibc's ld.so from com.nizovn.glibc
    static QString glibcLdPath();
    // Library path for ld.so --library-path
    static QString libraryPath();
};

#endif // VIDEOPROBER_H
