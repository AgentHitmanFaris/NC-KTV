/*
 * NC-KTV Core — GPU Detector Implementation
 */

#include "gpu_detector.h"
#include <QProcess>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QFile>
#include <QHash>

namespace ncktv {

static QString getFFmpegPath() {
    QString appDir = QCoreApplication::applicationDirPath();
    QString bundled = appDir + "/ffmpeg/ffmpeg.exe";
    if (QFile::exists(bundled)) return QDir::toNativeSeparators(bundled);
    bundled = QFileInfo(appDir).absolutePath() + "/ffmpeg/ffmpeg.exe";
    if (QFile::exists(bundled)) return QDir::toNativeSeparators(bundled);
    QDir projectRoot(appDir);
    while (projectRoot.cdUp()) {
        if (QFile::exists(projectRoot.absoluteFilePath("ffmpeg/ffmpeg.exe"))) {
            return QDir::toNativeSeparators(projectRoot.absoluteFilePath("ffmpeg/ffmpeg.exe"));
        }
        if (projectRoot.isRoot()) break;
    }
    return "ffmpeg";
}

bool GPUDetector::isCudaAvailable() {
    QProcess proc;
    proc.start("nvidia-smi", {});
    proc.waitForFinished(3000);
    return proc.exitCode() == 0;
}

bool GPUDetector::isEncoderAvailable(const QString& encoderName) {
    static QHash<QString, bool> cache;
    if (cache.contains(encoderName)) return cache[encoderName];

    QProcess proc;
    proc.start(getFFmpegPath(), {"-encoders"});
    if (!proc.waitForFinished(5000)) return false;

    QString output = QString::fromUtf8(proc.readAllStandardOutput());
    bool available = output.contains(encoderName, Qt::CaseInsensitive);
    cache[encoderName] = available;
    return available;
}

bool GPUDetector::isNVENCAvailable() {
    return isEncoderAvailable("h264_nvenc") || isEncoderAvailable("hevc_nvenc");
}

bool GPUDetector::isQSVAvailable() {
    return isEncoderAvailable("h264_qsv") || isEncoderAvailable("hevc_qsv");
}

bool GPUDetector::isAMFAvailable() {
    return isEncoderAvailable("h264_amf") || isEncoderAvailable("hevc_amf");
}

QString GPUDetector::getGpuName() {
    QProcess proc;
    proc.start("nvidia-smi", {"--query-gpu=name", "--format=csv,noheader,nounits"});
    proc.waitForFinished(3000);
    if (proc.exitCode() != 0) return {};
    return QString(proc.readAllStandardOutput()).trimmed().split('\n').first();
}

void GPUDetector::clearCache() {
    // No-op in C++ version — GPU memory is managed by ONNX Runtime / driver
}

} // namespace ncktv
