#include "render_engine.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <iostream>
#include <cstdint>

namespace ncktv {

#pragma pack(push, 1)
struct WavHeader {
    char riff[4] = {'R', 'I', 'F', 'F'};
    uint32_t fileSize = 0;
    char wave[4] = {'W', 'A', 'V', 'E'};
    char fmt[4] = {'f', 'm', 't', ' '};
    uint32_t fmtSize = 16;
    uint16_t audioFormat = 3; // IEEE Float
    uint16_t numChannels = 2; // Stereo
    uint32_t sampleRate = 48000;
    uint32_t byteRate = 48000 * 2 * 4;
    uint16_t blockAlign = 2 * 4;
    uint16_t bitsPerSample = 32;
    char data[4] = {'d', 'a', 't', 'a'};
    uint32_t dataSize = 0;
};
#pragma pack(pop)

static QString findFfmpegPath() {
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

    // 2. Try application relative path
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

    // 3. Common system paths
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
}

RenderEngine::RenderEngine() = default;
RenderEngine::~RenderEngine() = default;

bool RenderEngine::startRender(const QString& outputPath, int width, int height, int fps, int videoBitrate, int audioBitrate, const QString& audioCodecName) {
    (void)audioCodecName;
    m_finalOutputPath = QDir::toNativeSeparators(outputPath);
    m_width = width;
    m_height = height;
    m_fps = fps;
    m_videoBitrate = videoBitrate;
    m_audioBitrate = audioBitrate;

    m_ffmpegPath = findFfmpegPath();
    if (m_ffmpegPath.isEmpty()) {
        m_ffmpegPath = "ffmpeg";
    }

    m_tempVideoPath = m_finalOutputPath + ".temp_video.mp4";
    m_tempAudioPath = m_finalOutputPath + ".temp_audio.wav";

    // Clean up any stale temp files
    QFile::remove(m_tempVideoPath);
    QFile::remove(m_tempAudioPath);

    // Setup video process
    m_videoProcess = new QProcess();
    QStringList videoArgs = {
        "-y",
        "-f", "rawvideo",
        "-pix_fmt", "rgba",
        "-s", QString("%1x%2").arg(m_width).arg(m_height),
        "-r", QString::number(m_fps),
        "-i", "-",
        "-c:v", "libx264",
        "-pix_fmt", "yuv420p",
        "-b:v", QString::number(m_videoBitrate),
        "-preset", "fast",
        m_tempVideoPath
    };

    m_videoProcess->setProgram(m_ffmpegPath);
    m_videoProcess->setArguments(videoArgs);
    m_videoProcess->start();

    if (!m_videoProcess->waitForStarted(5000)) {
        std::cerr << "[RenderEngine] Failed to start FFmpeg video process.\n";
        delete m_videoProcess;
        m_videoProcess = nullptr;
        return false;
    }

    // Setup audio WAV file
    m_audioFile.setFileName(m_tempAudioPath);
    if (!m_audioFile.open(QIODevice::WriteOnly)) {
        std::cerr << "[RenderEngine] Failed to open temp audio file: " << m_tempAudioPath.toStdString() << "\n";
        m_videoProcess->kill();
        m_videoProcess->waitForFinished();
        delete m_videoProcess;
        m_videoProcess = nullptr;
        return false;
    }

    // Write dummy header
    WavHeader dummyHeader;
    m_audioFile.write(reinterpret_cast<const char*>(&dummyHeader), sizeof(dummyHeader));

    m_totalAudioSamples = 0;
    m_chosenVideoCodec = "H.264 (libx264)";
    m_chosenAudioCodec = "AAC";

    std::cout << "[RenderEngine] Video export process started.\n";
    return true;
}

bool RenderEngine::writeVideoFrame(const QImage& frameImage, int64_t frameIndex) {
    (void)frameIndex;
    if (!m_videoProcess || m_videoProcess->state() != QProcess::Running) {
        return false;
    }

    const uchar* pixels = frameImage.constBits();
    int64_t bytesToWrite = static_cast<int64_t>(m_width) * m_height * 4;
    
    int64_t written = 0;
    while (written < bytesToWrite) {
        int64_t chunk = m_videoProcess->write(reinterpret_cast<const char*>(pixels + written), bytesToWrite - written);
        if (chunk < 0) {
            std::cerr << "[RenderEngine] Write error to FFmpeg process stdin.\n";
            return false;
        }
        written += chunk;
    }

    // Yield control to let FFmpeg consume the data, keeping memory footprint low
    m_videoProcess->waitForBytesWritten(50);

    return true;
}

bool RenderEngine::writeAudioFrame(const float* stereoSamples, int numFrames) {
    if (!m_audioFile.isOpen()) {
        return false;
    }

    int64_t numFloats = static_cast<int64_t>(numFrames) * 2;
    int64_t bytesToWrite = numFloats * sizeof(float);
    
    int64_t written = m_audioFile.write(reinterpret_cast<const char*>(stereoSamples), bytesToWrite);
    if (written != bytesToWrite) {
        std::cerr << "[RenderEngine] Failed to write complete audio block to temp file.\n";
        return false;
    }

    m_totalAudioSamples += numFloats;
    return true;
}

bool RenderEngine::finishRender() {
    std::cout << "[RenderEngine] Finalizing render...\n";

    // 1. Close video process input and wait for it to complete
    if (m_videoProcess) {
        if (m_videoProcess->state() == QProcess::Running) {
            m_videoProcess->closeWriteChannel();
            if (!m_videoProcess->waitForFinished(30000)) { // 30 seconds timeout
                std::cerr << "[RenderEngine] FFmpeg video process timed out during completion. Killing...\n";
                m_videoProcess->kill();
                m_videoProcess->waitForFinished();
            }
        }
        delete m_videoProcess;
        m_videoProcess = nullptr;
    }

    // 2. Finalize WAV header and close audio file
    if (m_audioFile.isOpen()) {
        // Seek to start and write finalized header
        if (m_audioFile.seek(0)) {
            WavHeader finalHeader;
            finalHeader.dataSize = m_totalAudioSamples * sizeof(float);
            finalHeader.fileSize = 36 + finalHeader.dataSize;
            m_audioFile.write(reinterpret_cast<const char*>(&finalHeader), sizeof(finalHeader));
        }
        m_audioFile.close();
    }

    // 3. Mux audio and video using FFmpeg
    bool success = false;
    {
        QProcess muxProcess;
        QStringList muxArgs = {
            "-y",
            "-i", m_tempVideoPath,
            "-i", m_tempAudioPath,
            "-c:v", "copy",
            "-c:a", "aac",
            "-b:a", QString::number(m_audioBitrate),
            m_finalOutputPath
        };
        muxProcess.setProgram(m_ffmpegPath);
        muxProcess.setArguments(muxArgs);
        muxProcess.start();
        if (muxProcess.waitForStarted(5000)) {
            if (muxProcess.waitForFinished(60000)) { // 60 seconds timeout
                if (muxProcess.exitCode() == 0) {
                    success = true;
                    std::cout << "[RenderEngine] Muxing completed successfully.\n";
                } else {
                    std::cerr << "[RenderEngine] Muxing process failed with exit code: " 
                              << muxProcess.exitCode() << "\n"
                              << muxProcess.readAllStandardError().toStdString() << "\n";
                }
            } else {
                std::cerr << "[RenderEngine] Muxing process timed out.\n";
                muxProcess.kill();
                muxProcess.waitForFinished();
            }
        } else {
            std::cerr << "[RenderEngine] Failed to start muxing process.\n";
        }
    }

    // 4. Cleanup temp files
    QFile::remove(m_tempVideoPath);
    QFile::remove(m_tempAudioPath);

    return success;
}

} // namespace ncktv
