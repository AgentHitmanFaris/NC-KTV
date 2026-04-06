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

#include "../../core/system/gpu_detector.h"

namespace ncktv {

ExportWorker::ExportWorker(QObject* parent) : QObject(parent) {}

static QString getFFmpegExecutable() {
    QString appDir = QCoreApplication::applicationDirPath();
    
    // Bundled structure: app_root/ffmpeg/bin/ffmpeg.exe
    QString bundled = appDir + "/ffmpeg/bin/ffmpeg.exe";
    if (QFile::exists(bundled)) return QDir::toNativeSeparators(bundled);
    
    // Parent directory check: ../ffmpeg/bin/ffmpeg.exe
    bundled = QFileInfo(appDir).absolutePath() + "/ffmpeg/bin/ffmpeg.exe";
    if (QFile::exists(bundled)) return QDir::toNativeSeparators(bundled);

    // Project root check: search ancestors for ffmpeg/bin/ffmpeg.exe
    QDir projectRoot(appDir);
    while (projectRoot.cdUp()) {
        QString testPath = projectRoot.absoluteFilePath("ffmpeg/bin/ffmpeg.exe");
        if (QFile::exists(testPath)) {
            return QDir::toNativeSeparators(testPath);
        }
        if (projectRoot.isRoot()) break;
    }

    return "ffmpeg"; // Fallback to system PATH
}

void ExportWorker::startExport(const QString& videoPath, const QString& audioPath,
                               const QString& outputPath, const QString& format, bool burnSubs,
                               int width, int height)
{
    emit progress(0, "Preparing export...");

    // ── Handle Subtitle-only export ──────────────────────────────────────
    if (format.contains("Subtitles") || format.contains("ASS") || format.contains("SRT")) {
        emit progress(100, "Subtitle file exported.");
        emit exportComplete(outputPath);
        return;
    }

    // ── Handle MP3 audio-only export ──────────────────────────────────────
    if (format.contains("mp3", Qt::CaseInsensitive) || format.contains("MP3")) {
        QString src = audioPath.isEmpty() ? videoPath : audioPath;
        if (src.isEmpty()) {
            emit error("No audio source specified for MP3 export.");
            return;
        }
        QStringList args;
        args << "-y" << "-i" << src
             << "-vn"                     // no video
             << "-c:a" << "libmp3lame"
             << "-q:a" << "2"             // VBR quality 2 (~190 kbps)
             << outputPath;
        auto* process = new QProcess(this);
        QString ffmpegExe = getFFmpegExecutable();
        connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, process, outputPath](int exitCode, QProcess::ExitStatus) {
            if (exitCode == 0) {
                emit progress(100, "MP3 export complete!");
                emit exportComplete(outputPath);
            } else {
                QString err = process->readAllStandardError();
                if (err.isEmpty()) err = "FFmpeg failed with exit code " + QString::number(exitCode);
                emit error(err);
            }
            process->deleteLater();
        });
        connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError) {
            emit error("Failed to launch FFmpeg: " + process->errorString());
            process->deleteLater();
        });
        connect(process, &QProcess::readyReadStandardError, this, [this, process]() {
            QString out = process->readAllStandardError();
            if (out.contains("time=")) emit progress(50, "Encoding MP3...");
        });
        qDebug() << "ExportWorker (MP3): Executing" << ffmpegExe << args.join(" ");
        process->start(ffmpegExe, args);
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

    // ── Subtitle & Scaling Filter ─────────────────────────────────────────
    QStringList filters;
    if (width > 0 && height > 0) {
        filters << QString("scale=%1:%2").arg(width).arg(height);
    }
    if (burnSubs) {
        QString assPath = QFileInfo(outputPath).absolutePath() + "/temp_subs.ass";
        if (QFile::exists(assPath)) {
            // FFmpeg's 'ass' filter on Windows needs careful path escaping.
            QString escapedPath = QDir::fromNativeSeparators(assPath);
            escapedPath.replace(":", "\\:");
            filters << QString("ass='%1'").arg(escapedPath);
        }
    }
    if (!filters.isEmpty()) {
        ffmpegArgs << "-vf" << filters.join(",");
    }

    // ── Encoding Parameters ──────────────────────────────────────────────
    QString videoCodec = "libx264";
    if (format.contains("H.265") || format.contains("MKV")) {
        videoCodec = "libx265";
        if (GPUDetector::isEncoderAvailable("hevc_nvenc")) {
            ffmpegArgs << "-c:v" << "hevc_nvenc" << "-preset" << "p4" << "-rc" << "vbr" << "-cq" << "28";
        } else if (GPUDetector::isEncoderAvailable("hevc_qsv")) {
            ffmpegArgs << "-c:v" << "hevc_qsv" << "-global_quality" << "25" << "-preset" << "fast";
        } else if (GPUDetector::isEncoderAvailable("hevc_amf")) {
            ffmpegArgs << "-c:v" << "hevc_amf" << "-quality" << "balanced";
        } else {
            ffmpegArgs << "-c:v" << "libx265" << "-crf" << "23" << "-preset" << "fast";
        }
        ffmpegArgs << "-c:a" << "aac" << "-b:a" << "192k";
    } else if (format.contains("VP9") || format.contains("WebM")) {
        ffmpegArgs << "-c:v" << "libvpx-vp9" << "-crf" << "30" << "-b:v" << "0" << "-c:a" << "libopus" << "-b:a" << "128k";
    } else {
        // Default MP4 (H.264)
        if (GPUDetector::isEncoderAvailable("h264_nvenc")) {
            ffmpegArgs << "-c:v" << "h264_nvenc" << "-preset" << "p4" << "-rc" << "vbr" << "-cq" << "24";
        } else if (GPUDetector::isEncoderAvailable("h264_qsv")) {
            ffmpegArgs << "-c:v" << "h264_qsv" << "-global_quality" << "25" << "-preset" << "fast";
        } else if (GPUDetector::isEncoderAvailable("h264_amf")) {
            ffmpegArgs << "-c:v" << "h264_amf" << "-quality" << "balanced";
        } else {
            ffmpegArgs << "-c:v" << "libx264" << "-crf" << "20" << "-preset" << "fast";
        }
        ffmpegArgs << "-c:a" << "aac" << "-b:a" << "192k";
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
