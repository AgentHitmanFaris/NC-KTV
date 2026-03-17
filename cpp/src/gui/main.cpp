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
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#if NCKTV_HAS_VULKAN
#include <QVulkanInstance>
#endif
#include <iostream>

/**
 * @brief Custom message handler to redirect Qt logs to a file.
 */
void customMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
    static QFile logFile;
    if (!logFile.isOpen()) {
        QString logPath = QCoreApplication::applicationDirPath() + "/debug.log";
        logFile.setFileName(logPath);
        if (logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            QTextStream stream(&logFile);
            stream << "\n=======================================================\n";
            stream << "NC-KTV Pro Session Started: " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
            stream << "=======================================================\n";
        }
    }

    QString text;
    switch (type) {
        case QtDebugMsg:    text = "DEBUG"; break;
        case QtInfoMsg:     text = "INFO "; break;
        case QtWarningMsg:  text = "WARN "; break;
        case QtCriticalMsg: text = "CRIT "; break;
        case QtFatalMsg:    text = "FATAL"; break;
    }

    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString logLine = QString("[%1] [%2] %3").arg(timestamp, text, msg);

    if (logFile.isOpen()) {
        QTextStream stream(&logFile);
        stream << logLine << Qt::endl;
        stream.flush();
    }

    // Still output to stderr for developer convenience if run from a terminal
    std::cerr << logLine.toLocal8Bit().constData() << std::endl;
}

#include "main_window.h"
#include "../core/config/config_manager.h"

int main(int argc, char *argv[]) {
    // Enable High DPI Scaling
    #if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    #endif

    // Pre-initialization: Graphics API Preference
    {
        ncktv::ConfigManager config("config.ini");
        QString api = config.get<QString>("gui.graphics_api", "Default");
        
        if (api == "Vulkan (High Perf)") {
#if NCKTV_HAS_VULKAN
            QVulkanInstance inst;
            if (inst.create()) {
                qputenv("QSG_RHI_BACKEND", "vulkan");
                qputenv("QT_WIDGETS_RHI_BACKEND", "vulkan");
                qDebug() << "Enabling Vulkan graphics backend (Vulkan Instance verified).";
            } else {
                qWarning() << "Vulkan instance creation failed! Falling back to default graphics backend.";
            }
#else
            qWarning() << "Vulkan support was not compiled into this build. Falling back to default.";
#endif
        } else if (api == "DirectX 11") {
            qputenv("QSG_RHI_BACKEND", "d3d11");
            qputenv("QT_WIDGETS_RHI_BACKEND", "d3d11");
            qDebug() << "Enabling DirectX 11 graphics backend.";
        } else if (api == "OpenGL") {
            qputenv("QSG_RHI_BACKEND", "opengl");
            qputenv("QT_WIDGETS_RHI_BACKEND", "opengl");
            qDebug() << "Enabling OpenGL graphics backend.";
        } else if (api == "Software") {
            qputenv("QSG_RHI_BACKEND", "software");
            qputenv("QT_WIDGETS_RHI_BACKEND", "software");
            qDebug() << "Enabling Software rendering.";
        }
    }

    QApplication app(argc, argv);
    
    // Install message handler to log to file
    qInstallMessageHandler(customMessageHandler);
    app.setApplicationName("NC-KTV Pro");
    app.setOrganizationName("NC-KTV");
    app.setApplicationVersion("1.0.0-pro");

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
        splash->showMessage("Loading NC-KTV Pro...", Qt::AlignBottom | Qt::AlignCenter, QColor("#3b82f6"));
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

    qDebug() << "NC-KTV Pro started successfully.";
    return app.exec();
}

