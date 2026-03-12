/**
 * @file main.cpp
 * @brief Main application entry point for NC-KTV C++ port
 */

#include <QApplication>
#include <QIcon>
#include <QSplashScreen>
#include <QPixmap>
#include <QTimer>
#include <QLocale>
#include <QTranslator>
#include <QDebug>
#include <QDir>
#include <QSurfaceFormat>

#include "main_window.h"

int main(int argc, char *argv[]) {
    // Enable High DPI Scaling
    #if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    #endif

    QApplication app(argc, argv);
    app.setApplicationName("NC-KTV");
    app.setOrganizationName("NC-KTV");
    app.setApplicationVersion("0.11.0-cpp-port");

    // Set hardware acceleration workaround for Qt Multimedia (Windows)
    qputenv("QT_MEDIA_BACKEND", "windows");

    // Load App Icon & Splash Screen
    QString iconPath = QCoreApplication::applicationDirPath() + "/assets/logo.png";
    QSplashScreen* splash = nullptr;
    
    if (QFile::exists(iconPath)) {
        app.setWindowIcon(QIcon(iconPath));
        QPixmap pixmap(iconPath);
        splash = new QSplashScreen(pixmap.scaled(300, 300, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        splash->show();
        splash->showMessage("Loading NC-KTV C++ Engine...", Qt::AlignBottom | Qt::AlignCenter);
        app.processEvents();
    }

    // Initialize Config
    // Config config;
    
    // Load Modern UI Stylesheet
    // app.setStyleSheet(get_modern_stylesheet());

    // Create Main Window
    ncktv::MainWindow window;
    window.show();

    if (splash) {
        splash->finish(&window);
        // Do not delete splash here, finish() handles it or the app handles it. 
        // Actually splash->deleteLater() or just let it be.
    }

    qDebug() << "NC-KTV started successfully in C++ mode.";
    return app.exec();
}

