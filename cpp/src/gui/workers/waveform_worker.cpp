#include "waveform_worker.h"
#include <QProcess>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <cmath>
#include <algorithm>
#include <QDebug>

namespace ncktv {

WaveformWorker::WaveformWorker(QObject* parent) : QObject(parent) {}

void WaveformWorker::generate(const QString& filePath) {
    if (filePath.isEmpty() || !QFile::exists(filePath)) {
        emit error("File not found: " + filePath);
        return;
    }

    qDebug() << "WaveformWorker: Generating for" << filePath;

    QString appDir = QCoreApplication::applicationDirPath();
    QString ffmpeg = QDir::cleanPath(appDir + "/ffmpeg/bin/ffmpeg.exe");
    if (!QFile::exists(ffmpeg)) {
        ffmpeg = QDir::cleanPath(QDir::currentPath() + "/ffmpeg/bin/ffmpeg.exe");
        if (!QFile::exists(ffmpeg)) ffmpeg = "ffmpeg";
    }

    QProcess proc;
    proc.setProgram(ffmpeg);
    proc.setArguments({
        "-i", filePath,
        "-f", "f32le",
        "-ac", "1",
        "-ar", "44100",
        "-vn",
        "pipe:1"
    });

    proc.start();
    if (!proc.waitForStarted()) {
        emit error("Failed to start FFmpeg for waveform generation");
        return;
    }

    QVector<float> allSamples;
    allSamples.reserve(44100 * 240); // Pre-allocate for 4 mins

    while (proc.state() == QProcess::Running || proc.bytesAvailable() > 0) {
        if (!proc.waitForReadyRead(100) && proc.state() == QProcess::NotRunning && proc.bytesAvailable() == 0) break;
        
        QByteArray data = proc.readAllStandardOutput();
        if (data.isEmpty()) continue;
        
        int nEntries = data.size() / sizeof(float);
        const float* ptr = reinterpret_cast<const float*>(data.constData());
        for (int i = 0; i < nEntries; ++i) {
            allSamples.append(ptr[i]);
        }
    }
    proc.waitForFinished();
    // Final check for any remaining data
    QByteArray finalData = proc.readAllStandardOutput();
    if (!finalData.isEmpty()) {
        int nEntries = finalData.size() / sizeof(float);
        const float* ptr = reinterpret_cast<const float*>(finalData.constData());
        for (int i = 0; i < nEntries; ++i) allSamples.append(ptr[i]);
    }

    if (allSamples.isEmpty()) {
        emit error("FFmpeg produced no audio data for waveform");
        return;
    }

    qDebug() << "WaveformWorker: Decoded" << allSamples.size() << "samples";

    // Bucket samples into min/max for the waveform widget
    int samplesPerPixel = 512; 
    QVector<float> minData;
    QVector<float> maxData;
    minData.reserve(allSamples.size() / samplesPerPixel + 1);
    maxData.reserve(allSamples.size() / samplesPerPixel + 1);

    for (int i = 0; i < allSamples.size(); i += samplesPerPixel) {
        float minVal = 0;
        float maxVal = 0;
        int end = std::min(i + samplesPerPixel, (int)allSamples.size());
        for (int j = i; j < end; ++j) {
            float s = allSamples[j];
            if (s < minVal) minVal = s;
            if (s > maxVal) maxVal = s;
        }
        minData.append(minVal);
        maxData.append(maxVal);
    }

    qDebug() << "WaveformWorker: Generation complete," << minData.size() << "buckets";
    emit waveformReady(minData, maxData, 44100.0, samplesPerPixel);
}

} // namespace ncktv
