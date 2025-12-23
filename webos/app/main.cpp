/**
 * VLC Player for webOS
 * A simple Qt5-based media player using VLC-Qt
 */

#include <QApplication>
#include <QDir>
#include <QStandardPaths>

#include "MainWindow.h"

int main(int argc, char *argv[])
{
    // webOS environment setup
    qputenv("QT_QPA_FONTDIR", "/usr/share/fonts");

    // VLC_PLUGIN_PATH and VLC_VERBOSE are set by the launcher script (vlcplayer.sh)
    // Don't override them here - applicationDirPath() requires QApplication first

    QApplication app(argc, argv);
    app.setApplicationName("VLC Player");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("webOS");

    // Create and show main window
    MainWindow window;

    // Check for file argument
    if (argc > 1) {
        window.openFile(QString::fromUtf8(argv[1]));
    }

    // Show fullscreen on webOS device
#ifdef WEBOS
    window.showFullScreen();
#else
    window.show();
#endif

    return app.exec();
}
