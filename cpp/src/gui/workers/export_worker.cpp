/*
 * NC-KTV GUI — Export Worker Implementation
 * Handles FFmpeg encoding with ASS subtitle burn-in via QProcess
 */

#include "export_worker.h"

#include <QProcess>
#include <QFileInfo>
#include <QDir>
#include <QDebug>

#include "../../core/parsers/ass_generator.h"

namespace ncktv {

ExportWorker::ExportWorker(QObject* parent) : QObject(parent) {}

void ExportWorker::startExport(const QString& projectPath, const QString& outputPath,
                               const QString& format)
{
    emit progress(0, "Preparing export...");

    // ── Validate inputs ──────────────────────────────────────────────────
    if (projectPath.isEmpty()) {
        emit error("No project path specified.");
        return;
    }
    if (outputPath.isEmpty()) {
        emit error("No output path specified.");
        return;
    }

    // ── Determine FFmpeg arguments ───────────────────────────────────────
    QStringList ffmpegArgs;

    // Check if output is subtitle-only (ASS/SRT)
    bool isSubOnly = format.contains("ASS") || format.contains("SRT");

    if (isSubOnly) {
        // For subtitle-only export, we just write the generated content
        emit progress(50, "Generating subtitle file...");
        // The actual ASS/SRT content would be written by the caller
        // using AssGenerator. This worker handles video encoding.
        emit progress(100, "Subtitle export complete.");
        emit exportComplete(outputPath);
        return;
    }

    // Video export via FFmpeg
    emit progress(10, "Starting FFmpeg encode...");

    ffmpegArgs << "-y"          // overwrite output
               << "-i" << projectPath;

    // Add subtitle filter if requested
    // The caller should have pre-generated the .ass file
    QString assPath = QFileInfo(outputPath).absolutePath() + "/temp_subs.ass";
    if (QFileInfo::exists(assPath)) {
        ffmpegArgs << "-vf" << QString("ass='%1'").arg(assPath.replace("'", "\\'"));
    }

    // Codec settings
    if (format.contains("H.265") || format.contains("MKV")) {
        ffmpegArgs << "-c:v" << "libx265" << "-crf" << "23";
    } else if (format.contains("VP9") || format.contains("WebM")) {
        ffmpegArgs << "-c:v" << "libvpx-vp9" << "-crf" << "30" << "-b:v" << "0";
    } else {
        ffmpegArgs << "-c:v" << "libx264" << "-crf" << "20" << "-preset" << "medium";
    }

    ffmpegArgs << "-c:a" << "aac" << "-b:a" << "192k"
               << outputPath;

    // ── Launch FFmpeg ────────────────────────────────────────────────────
    auto* process = new QProcess(this);

    connect(process, &QProcess::readyReadStandardError, this, [this, process]() {
        QString output = process->readAllStandardError();
        // Parse FFmpeg progress from stderr (time=... format)
        if (output.contains("time=")) {
            // Crude progress estimation
            int idx = output.indexOf("time=");
            if (idx >= 0) {
                emit progress(50, "Encoding: " + output.mid(idx, 20).trimmed());
            }
        }
    });

    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, process, outputPath](int exitCode, QProcess::ExitStatus) {
        process->deleteLater();
        if (exitCode == 0) {
            emit progress(100, "Export complete!");
            emit exportComplete(outputPath);
        } else {
            emit error("FFmpeg exited with code " + QString::number(exitCode) +
                       "\n" + process->readAllStandardError());
        }
    });

    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError err) {
        Q_UNUSED(err);
        emit error("Failed to start FFmpeg: " + process->errorString());
        process->deleteLater();
    });

    qDebug() << "ExportWorker: Starting FFmpeg:" << "ffmpeg" << ffmpegArgs;
    process->start("ffmpeg", ffmpegArgs);
}

} // namespace ncktv
