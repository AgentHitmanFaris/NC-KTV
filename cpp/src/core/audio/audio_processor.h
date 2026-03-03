#pragma once
/*
 * NC-KTV Core — Audio Processor
 * Port of audio_processor.py — FFmpeg-based audio extraction, conversion, metadata
 */

#include <QString>
#include <QStringList>
#include <QPair>
#include <nlohmann/json.hpp>

namespace ncktv {

struct AudioInfo {
    double  duration   = 0.0;
    int64_t sizeBytes  = 0;
    int     bitRate    = 0;
    int     sampleRate = 0;
    int     channels   = 0;
    QString codec;
};

class AudioProcessor {
public:
    explicit AudioProcessor(const QString& tempDir = "temp");

    /// Extract audio from video file using FFmpeg
    QString extractAudioFromVideo(const QString& videoPath,
                                  const QString& outputPath = {});

    /// Convert audio to specified format
    QString convertAudio(const QString& inputPath, const QString& outputPath,
                         int sampleRate = 44100, int channels = 2);

    /// Get audio file metadata via FFprobe
    AudioInfo getAudioInfo(const QString& audioPath);

    /// Get duration in seconds
    double getDuration(const QString& audioPath);

    /// Validate if file is a supported audio/video format
    QPair<bool, QString> validateAudioFile(const QString& filePath);

private:
    QString m_tempDir;

    static const QStringList AUDIO_EXTENSIONS;
    static const QStringList VIDEO_EXTENSIONS;
};

} // namespace ncktv
