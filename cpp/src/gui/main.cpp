/*
 * NC-KTV — Professional Music Video Karaoke Maker
 * Application Entry Point
 */

#include <QApplication>
#include <QFile>
#include <QIcon>
#include <QPixmap>
#include <QSplashScreen>
#include <QDir>

#include "main_window.h"

int main(int argc, char* argv[])
{
    // Enable high-DPI scaling
    QApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication app(argc, argv);
    app.setApplicationName("NC-KTV");
    app.setOrganizationName("NC-KTV");
    app.setApplicationVersion("1.0.0");

    // ─── Set App Icon & Splash ──────────────────────────────────────────────
    const QString iconPath = QCoreApplication::applicationDirPath() + "/../assets/logo.png";
    QSplashScreen* splash = nullptr;

    if (QFile::exists(iconPath)) {
        QIcon appIcon(iconPath);
        app.setWindowIcon(appIcon);

        QPixmap splashPixmap(iconPath);
        splashPixmap = splashPixmap.scaled(300, 300,
            Qt::KeepAspectRatio, Qt::SmoothTransformation);
        splash = new QSplashScreen(splashPixmap);
        splash->show();
        splash->showMessage("Loading NC-KTV...",
            Qt::AlignBottom | Qt::AlignHCenter, Qt::white);
        app.processEvents();
    }

    // ─── Apply Dark Theme ───────────────────────────────────────────────────
    QFile styleFile(":/styles/dark_theme.qss");
    if (styleFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        app.setStyleSheet(styleFile.readAll());
        styleFile.close();
    }

    // ─── Add FFmpeg to PATH if bundled ──────────────────────────────────────
    const QString ffmpegDir = QCoreApplication::applicationDirPath() + "/../ffmpeg";
    if (QDir(ffmpegDir).exists()) {
        QString path = qEnvironmentVariable("PATH");
        qputenv("PATH", (ffmpegDir + ";" + path).toUtf8());
    }

    // ─── Create Main Window ─────────────────────────────────────────────────
    ncktv::MainWindow window;
    window.show();

    if (splash) {
        splash->finish(&window);
        delete splash;
    }

    return app.exec();
}
