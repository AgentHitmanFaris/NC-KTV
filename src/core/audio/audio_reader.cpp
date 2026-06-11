#include "audio_reader.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QCryptographicHash>
#include <QStandardPaths>
#include <QDateTime>
#include <QUuid>
#include <QProcess>
#include <QCoreApplication>
#include <iostream>
#include <algorithm>
#include <cstring>

#include <miniaudio.h>

namespace ncktv {

static QString findFfmpegPath() {
    static const QString cachedPath = []() {
        // 1. Try PATH
        {
            QProcess proc;
            proc.setProgram("ffmpeg");
            proc.setArguments({"-version"});
            proc.start();
            if (proc.waitForStarted(1000)) {
                if (proc.waitForFinished(2000)) {
                    if (proc.exitCode() == 0) {
                        return QString("ffmpeg");
                    }
                } else {
                    proc.kill();
                    proc.waitForFinished(500);
                }
            }
        }

        // 2. Try application relative path (e.g. downloaded by download_ffmpeg.py)
        QString appDir = QCoreApplication::applicationDirPath();
        QStringList relativePaths = {
            appDir + "/ffmpeg.exe",
            appDir + "/ffmpeg/bin/ffmpeg.exe",
            appDir + "/../ffmpeg/bin/ffmpeg.exe",
            appDir + "/../../ffmpeg/bin/ffmpeg.exe",
            appDir + "/../../../ffmpeg/bin/ffmpeg.exe",
            appDir + "/../../../../ffmpeg/bin/ffmpeg.exe",
            appDir + "/../../../../../ffmpeg/bin/ffmpeg.exe",
            appDir + "/build/ffmpeg.exe",
            appDir + "/../build/ffmpeg.exe",
            appDir + "/../../build/ffmpeg.exe",
            appDir + "/../../../build/ffmpeg.exe",
            appDir + "/../../../../build/ffmpeg.exe",
            appDir + "/../../../../../build/ffmpeg.exe"
        };
        for (const QString& path : relativePaths) {
            if (QFile::exists(path)) {
                return QDir::cleanPath(path);
            }
        }

        // 3. Try common system paths
        QStringList commonPaths = {
            "D:\\Program Files\\FFmpeg\\Providers\\org.buanzo.ffmpeg8.1\\bin\\ffmpeg.exe",
            "C:\\Program Files\\FFmpeg\\bin\\ffmpeg.exe",
            "D:\\Program Files\\FFmpeg\\bin\\ffmpeg.exe",
            "C:\\Program Files (x86)\\FFmpeg\\bin\\ffmpeg.exe",
            "C:\\msys64\\usr\\bin\\ffmpeg.exe"
        };

        for (const QString& path : commonPaths) {
            if (QFile::exists(path)) {
                return path;
            }
        }

        return QString("");
    }();
    return cachedPath;
}

bool AudioReader::decodeFile(const QString& filePath, int targetSampleRate) {
    m_samples.clear();
    m_peaks256.clear();
    m_peaks4096.clear();

    QString tempWavPath;

    // Setup miniaudio decoder config
    ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 2, targetSampleRate);

    ma_decoder decoder;
    ma_result result;

#if defined(_WIN32)
    // On Windows, use wide-character init for full Unicode file path support
    result = ma_decoder_init_file_w(filePath.toStdWString().c_str(), &config, &decoder);
#else
    result = ma_decoder_init_file(QDir::toNativeSeparators(filePath).toUtf8().constData(), &config, &decoder);
#endif

    if (result != MA_SUCCESS) {
        // Try FFmpeg fallback
        QString ffmpeg = findFfmpegPath();
        if (!ffmpeg.isEmpty()) {
            QString tempDir = QDir::tempPath();
            QString uniqueId = QUuid::createUuid().toString(QUuid::Id128);
            tempWavPath = QDir(tempDir).filePath("ncktv_temp_" + uniqueId + ".wav");

            QStringList args;
            args << "-y" << "-i" << QDir::toNativeSeparators(filePath)
                 << "-vn" << "-acodec" << "pcm_s16le" 
                 << "-ar" << QString::number(targetSampleRate)
                 << "-ac" << "2" << QDir::toNativeSeparators(tempWavPath);

            QProcess proc;
            proc.setProgram(ffmpeg);
            proc.setArguments(args);
            
            if (ffmpeg != "ffmpeg") {
                QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
                QString ffmpegDir = QFileInfo(ffmpeg).absolutePath();
                QString pathVal = env.value("PATH");
                if (!pathVal.isEmpty()) {
                    env.insert("PATH", ffmpegDir + ";" + pathVal);
                } else {
                    env.insert("PATH", ffmpegDir);
                }
                proc.setProcessEnvironment(env);
            }

            proc.start();
            if (proc.waitForStarted(2000) && proc.waitForFinished(30000)) {
                if (proc.exitCode() == 0 && QFile::exists(tempWavPath)) {
#if defined(_WIN32)
                    result = ma_decoder_init_file_w(tempWavPath.toStdWString().c_str(), &config, &decoder);
#else
                    result = ma_decoder_init_file(QDir::toNativeSeparators(tempWavPath).toUtf8().constData(), &config, &decoder);
#endif
                } else {
                    std::cerr << "[AudioReader] FFmpeg fallback failed. Exit code: " << proc.exitCode() << "\n"
                              << "Stdout: " << proc.readAllStandardOutput().constData() << "\n"
                              << "Stderr: " << proc.readAllStandardError().constData() << "\n";
                }
            } else {
                std::cerr << "[AudioReader] FFmpeg process timeout or failed to start.\n"
                          << "Stderr: " << proc.readAllStandardError().constData() << "\n";
            }
        }

        if (result != MA_SUCCESS) {
            if (!tempWavPath.isEmpty() && QFile::exists(tempWavPath)) {
                QFile::remove(tempWavPath);
            }
            std::cerr << "[AudioReader] Error: Could not open/decode file: " 
                      << QDir::toNativeSeparators(filePath).toStdString() << " (ma_result: " << result << ")\n";
            return false;
        }
    }

    // Retrieve total length in frames to allocate memory upfront
    ma_uint64 totalFrames = 0;
    ma_decoder_get_length_in_pcm_frames(&decoder, &totalFrames);

    if (totalFrames > 0) {
        m_samples.resize(totalFrames * 2);
    } else {
        m_samples.reserve(targetSampleRate * 2 * 60 * 5); // 5 mins
    }

    ma_uint64 totalFramesRead = 0;
    const ma_uint64 chunkSize = 4096;
    std::vector<float> tempBuffer(chunkSize * 2);

    while (true) {
        ma_uint64 framesRead = 0;
        ma_result readRes = ma_decoder_read_pcm_frames(&decoder, tempBuffer.data(), chunkSize, &framesRead);
        if (framesRead > 0) {
            if (totalFrames > 0) {
                if (totalFramesRead + framesRead > totalFrames) {
                    m_samples.resize((totalFramesRead + framesRead) * 2);
                }
                std::memcpy(m_samples.data() + totalFramesRead * 2, tempBuffer.data(), framesRead * 2 * sizeof(float));
            } else {
                m_samples.insert(m_samples.end(), tempBuffer.begin(), tempBuffer.begin() + framesRead * 2);
            }
            totalFramesRead += framesRead;
        }
        if (readRes != MA_SUCCESS || framesRead < chunkSize) {
            break;
        }
    }

    m_samples.resize(totalFramesRead * 2);
    ma_decoder_uninit(&decoder);

    if (!tempWavPath.isEmpty() && QFile::exists(tempWavPath)) {
        QFile::remove(tempWavPath);
    }

    m_sampleRate = targetSampleRate;
    m_channels = 2;
    m_durationSeconds = static_cast<double>(totalFramesRead) / targetSampleRate;

    // Precompute Peak Levels for Level of Details Waveform drawing
    precomputePeaks();

    // Save precomputed peaks to cache file
    savePeakCache(filePath);

    std::cout << "[AudioReader] Successfully decoded file: " << QDir::toNativeSeparators(filePath).toStdString() 
              << " (" << m_durationSeconds << "s, " << m_samples.size() << " raw float samples)\n";
    return true;
}


void AudioReader::precomputePeaks() {
    m_peaks256.clear();
    m_peaks4096.clear();

    qint64 frameCount = totalSamples();
    if (frameCount == 0) {
        return;
    }

    m_peaks256.reserve(frameCount / 256 + 1);
    m_peaks4096.reserve(frameCount / 4096 + 1);

    // Compute LOD 256 peaks
    for (qint64 i = 0; i < frameCount; i += 256) {
        float minVal = 0.0f;
        float maxVal = 0.0f;
        qint64 end = (std::min)(i + 256, frameCount);
        
        for (qint64 f = i; f < end; ++f) {
            float left = m_samples[f * 2];
            float right = m_samples[f * 2 + 1];
            if (left < minVal) minVal = left;
            if (right < minVal) minVal = right;
            if (left > maxVal) maxVal = left;
            if (right > maxVal) maxVal = right;
        }
        m_peaks256.push_back({minVal, maxVal});
    }

    // Compute LOD 4096 peaks
    for (qint64 i = 0; i < frameCount; i += 4096) {
        float minVal = 0.0f;
        float maxVal = 0.0f;
        qint64 end = (std::min)(i + 4096, frameCount);
        
        for (qint64 f = i; f < end; ++f) {
            float left = m_samples[f * 2];
            float right = m_samples[f * 2 + 1];
            if (left < minVal) minVal = left;
            if (right < minVal) minVal = right;
            if (left > maxVal) maxVal = left;
            if (right > maxVal) maxVal = right;
        }
        m_peaks4096.push_back({minVal, maxVal});
    }
}

struct PeakCacheHeader {
    char magic[4] = {'N', 'C', 'P', 'K'};
    uint32_t version = 1;
    qint64 sourceFileSize = 0;
    qint64 sourceLastModified = 0;
    uint32_t numPeaks256 = 0;
    uint32_t numPeaks4096 = 0;
    int sampleRate = 48000;
    int channels = 2;
    double durationSeconds = 0.0;
};

QString AudioReader::getCachePath(const QString& filePath) const {
    // Attempt 1: Next to original file
    QString primaryPath = filePath + ".pk";
    QFileInfo fi(primaryPath);
    QDir dir = fi.dir();
    if (dir.exists()) {
        QFile file(primaryPath);
        if (file.open(QIODevice::ReadWrite)) {
            file.close();
            return primaryPath;
        }
    }
    
    // Attempt 2: Fall back to local AppData cache folder
    QString appLocal = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QString cacheDir = QDir(appLocal).filePath("cache");
    QDir().mkpath(cacheDir);
    
    QByteArray hash = QCryptographicHash::hash(filePath.toUtf8(), QCryptographicHash::Md5).toHex();
    return QDir(cacheDir).filePath(QString::fromUtf8(hash) + ".pk");
}

bool AudioReader::loadPeakCache(const QString& filePath) {
    QString cachePath = getCachePath(filePath);
    QFile file(cachePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QFileInfo sourceInfo(filePath);
    if (!sourceInfo.exists()) {
        return false;
    }
    qint64 expectedSize = sourceInfo.size();
    qint64 expectedMod = sourceInfo.lastModified().toMSecsSinceEpoch();

    PeakCacheHeader header;
    if (file.read(reinterpret_cast<char*>(&header), sizeof(header)) != sizeof(header)) {
        return false;
    }

    if (std::strncmp(header.magic, "NCPK", 4) != 0 ||
        header.version != 1 ||
        header.sourceFileSize != expectedSize ||
        header.sourceLastModified != expectedMod) {
        return false;
    }

    m_peaks256.resize(header.numPeaks256);
    if (file.read(reinterpret_cast<char*>(m_peaks256.data()), header.numPeaks256 * sizeof(Peak)) != static_cast<qint64>(header.numPeaks256 * sizeof(Peak))) {
        m_peaks256.clear();
        return false;
    }

    m_peaks4096.resize(header.numPeaks4096);
    if (file.read(reinterpret_cast<char*>(m_peaks4096.data()), header.numPeaks4096 * sizeof(Peak)) != static_cast<qint64>(header.numPeaks4096 * sizeof(Peak))) {
        m_peaks256.clear();
        m_peaks4096.clear();
        return false;
    }

    m_sampleRate = header.sampleRate;
    m_channels = header.channels;
    m_durationSeconds = header.durationSeconds;
    m_samples.clear(); // Loaded in background

    std::cout << "[AudioReader] Peak cache loaded successfully from: " << cachePath.toStdString() << "\n";
    return true;
}

bool AudioReader::savePeakCache(const QString& filePath) {
    if (m_peaks256.empty() || m_peaks4096.empty()) {
        return false;
    }

    QString cachePath = getCachePath(filePath);
    QFile file(cachePath);
    if (!file.open(QIODevice::WriteOnly)) {
        std::cerr << "[AudioReader] Warning: Could not open peak cache for writing at: " << cachePath.toStdString() << "\n";
        return false;
    }

    QFileInfo sourceInfo(filePath);
    if (!sourceInfo.exists()) {
        return false;
    }

    PeakCacheHeader header;
    std::memcpy(header.magic, "NCPK", 4);
    header.version = 1;
    header.sourceFileSize = sourceInfo.size();
    header.sourceLastModified = sourceInfo.lastModified().toMSecsSinceEpoch();
    header.numPeaks256 = static_cast<uint32_t>(m_peaks256.size());
    header.numPeaks4096 = static_cast<uint32_t>(m_peaks4096.size());
    header.sampleRate = m_sampleRate;
    header.channels = m_channels;
    header.durationSeconds = m_durationSeconds;

    if (file.write(reinterpret_cast<const char*>(&header), sizeof(header)) != sizeof(header)) {
        return false;
    }

    if (file.write(reinterpret_cast<const char*>(m_peaks256.data()), m_peaks256.size() * sizeof(Peak)) != static_cast<qint64>(m_peaks256.size() * sizeof(Peak))) {
        return false;
    }

    if (file.write(reinterpret_cast<const char*>(m_peaks4096.data()), m_peaks4096.size() * sizeof(Peak)) != static_cast<qint64>(m_peaks4096.size() * sizeof(Peak))) {
        return false;
    }

    std::cout << "[AudioReader] Peak cache saved successfully to: " << cachePath.toStdString() << "\n";
    return true;
}

} // namespace ncktv
