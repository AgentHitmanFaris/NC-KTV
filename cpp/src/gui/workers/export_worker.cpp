/*
 * NC-KTV GUI — Export Worker Implementation
 * Handles FFmpeg encoding with ASS subtitle burn-in via QProcess
 */

#include "export_worker.h"

#include <QProcess>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QCoreApplication>

#include "../../core/parsers/ass_generator.h"

namespace ncktv {

ExportWorker::ExportWorker(QObject* parent) : QObject(parent) {}

static QString getFFmpegExecutable() {
    QString appDir = QCoreApplication::applicationDirPath();
    
    // Check bundled ffmpeg in release structure
    QString bundled = appDir + "/ffmpeg/ffmpeg.exe";
    if (QFile::exists(bundled)) return QDir::toNativeSeparators(bundled);
    
    // Check for development environment structure (assuming bin is in build/...)
    bundled = QFileInfo(appDir).absolutePath() + "/ffmpeg/ffmpeg.exe";
    if (QFile::exists(bundled)) return QDir::toNativeSeparators(bundled);

    // Fallback to project root if running from VS/CMake build dir
    QDir projectRoot(appDir);
    while (projectRoot.cdUp()) {
        if (QFile::exists(projectRoot.absoluteFilePath("ffmpeg/ffmpeg.exe"))) {
            return QDir::toNativeSeparators(projectRoot.absoluteFilePath("ffmpeg/ffmpeg.exe"));
        }
        if (projectRoot.isRoot()) break;
    }

    return "ffmpeg"; // Fallback to system PATH
}

void ExportWorker::startExport(const QString& videoPath, const QString& audioPath,
                               const QString& outputPath, const QString& format, bool burnSubs)
{
    emit progress(0, "Preparing export...");

    // ── Handle Subtitle-only export ──────────────────────────────────────
    if (format.contains("Subtitles") || format.contains("ASS") || format.contains("SRT")) {
        emit progress(100, "Subtitle file exported.");
        emit exportComplete(outputPath);
        return;
    }

    if (videoPath.isEmpty() && audioPath.isEmpty()) {
        emit error("No input media specified.");
        return;
    }

    QStringList ffmpegArgs;
    ffmpegArgs << "-y"; // Overwrite

    // ── Inputs ──────────────────────────────────────────────────────────
    if (videoPath == audioPath) {
        ffmpegArgs << "-i" << videoPath;
    } else if (!videoPath.isEmpty() && !audioPath.isEmpty()) {
        ffmpegArgs << "-i" << videoPath << "-i" << audioPath;
        ffmpegArgs << "-map" << "0:v:0" << "-map" << "1:a:0";
    } else if (!videoPath.isEmpty()) {
        ffmpegArgs << "-i" << videoPath;
    } else {
        // Audio only or similar
        ffmpegArgs << "-i" << audioPath;
    }

    // ── Subtitle Filter ──────────────────────────────────────────────────
    if (burnSubs) {
        QString assPath = QFileInfo(outputPath).absolutePath() + "/temp_subs.ass";
        if (QFile::exists(assPath)) {
            // FFmpeg's 'ass' filter on Windows needs careful path escaping.
            // Forward slashes are generally safer, and colons must be escaped.
            QString escapedPath = QDir::fromNativeSeparators(assPath);
            escapedPath.replace(":", "\\:");
            ffmpegArgs << "-vf" << QString("ass='%1'").arg(escapedPath);
        }
    }

    // ── Encoding Parameters ──────────────────────────────────────────────
    if (format.contains("H.265") || format.contains("MKV")) {
        ffmpegArgs << "-c:v" << "libx265" << "-crf" << "23" << "-c:a" << "aac" << "-b:a" << "192k";
    } else if (format.contains("VP9") || format.contains("WebM")) {
        ffmpegArgs << "-c:v" << "libvpx-vp9" << "-crf" << "30" << "-b:v" << "0" << "-c:a" << "libopus" << "-b:a" << "128k";
    } else {
        // Default MP4 (H.264)
        ffmpegArgs << "-c:v" << "libx264" << "-crf" << "20" << "-preset" << "medium" << "-c:a" << "aac" << "-b:a" << "192k";
    }

    ffmpegArgs << outputPath;

    // ── Process Execution ────────────────────────────────────────────────
    auto* process = new QProcess(this);
    QString ffmpegExe = getFFmpegExecutable();

    connect(process, &QProcess::readyReadStandardError, this, [this, process]() {
        QString output = process->readAllStandardError();
        if (output.contains("time=")) {
            int idx = output.indexOf("time=");
            if (idx >= 0) {
                emit progress(50, "Encoding: " + output.mid(idx, 20).trimmed());
            }
        }
    });

    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, process, outputPath](int exitCode, QProcess::ExitStatus) {
        if (exitCode == 0) {
            emit progress(100, "Export complete!");
            emit exportComplete(outputPath);
        } else {
            QString err = process->readAllStandardError();
            if (err.isEmpty()) err = "FFmpeg failed with exit code " + QString::number(exitCode);
            emit error(err);
        }
        process->deleteLater();
    });

    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError err) {
        emit error("Failed to launch FFmpeg: " + process->errorString());
        process->deleteLater();
    });

    qDebug() << "ExportWorker: Executing" << ffmpegExe << ffmpegArgs.join(" ");
    process->start(ffmpegExe, ffmpegArgs);
}

} // namespace ncktv
