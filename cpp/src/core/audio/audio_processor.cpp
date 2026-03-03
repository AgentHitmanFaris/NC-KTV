/*
 * NC-KTV Core — Audio Processor Implementation
 */

#include "audio_processor.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <stdexcept>

namespace ncktv {

const QStringList AudioProcessor::AUDIO_EXTENSIONS =
    {".mp3", ".wav", ".flac", ".m4a", ".aac", ".ogg", ".wma"};
const QStringList AudioProcessor::VIDEO_EXTENSIONS =
    {".mp4", ".avi", ".mkv", ".mov", ".wmv", ".flv", ".webm"};

AudioProcessor::AudioProcessor(const QString& tempDir)
    : m_tempDir(tempDir)
{
    QDir().mkpath(m_tempDir);
}

QString AudioProcessor::extractAudioFromVideo(const QString& videoPath,
                                               const QString& outputPath)
{
    QString output = outputPath;
    if (output.isEmpty()) {
        output = m_tempDir + "/" + QFileInfo(videoPath).completeBaseName() + "_audio.wav";
    }
    QDir().mkpath(QFileInfo(output).absolutePath());

    QProcess proc;
    proc.start("ffmpeg", {
        "-i", videoPath,
        "-vn",
        "-acodec", "pcm_s16le",
        "-ar", "44100",
        "-ac", "2",
        "-y",
        output
    });
    proc.waitForFinished(-1);

    if (proc.exitCode() != 0)
        throw std::runtime_error("FFmpeg audio extraction failed: " +
                                 proc.readAllStandardError().toStdString());
    return output;
}

QString AudioProcessor::convertAudio(const QString& inputPath, const QString& outputPath,
                                      int sampleRate, int channels)
{
    QDir().mkpath(QFileInfo(outputPath).absolutePath());

    QProcess proc;
    proc.start("ffmpeg", {
        "-i", inputPath,
        "-acodec", "pcm_s16le",
        "-ar", QString::number(sampleRate),
        "-ac", QString::number(channels),
        "-y",
        "-loglevel", "error",
        outputPath
    });
    proc.waitForFinished(-1);

    if (proc.exitCode() != 0)
        throw std::runtime_error("FFmpeg conversion failed: " +
                                 proc.readAllStandardError().toStdString());
    return outputPath;
}

AudioInfo AudioProcessor::getAudioInfo(const QString& audioPath)
{
    QProcess proc;
    proc.start("ffprobe", {
        "-v", "quiet",
        "-print_format", "json",
        "-show_format",
        "-show_streams",
        audioPath
    });
    proc.waitForFinished(-1);

    AudioInfo info;

    QJsonDocument doc = QJsonDocument::fromJson(proc.readAllStandardOutput());
    if (doc.isNull()) return info;

    QJsonObject root = doc.object();
    QJsonObject format = root["format"].toObject();

    info.duration  = format["duration"].toString().toDouble();
    info.sizeBytes = format["size"].toString().toLongLong();
    info.bitRate   = format["bit_rate"].toString().toInt();

    QJsonArray streams = root["streams"].toArray();
    for (const auto& streamVal : streams) {
        QJsonObject stream = streamVal.toObject();
        if (stream["codec_type"].toString() == "audio") {
            info.sampleRate = stream["sample_rate"].toString().toInt();
            info.channels   = stream["channels"].toInt();
            info.codec      = stream["codec_name"].toString();
            break;
        }
    }

    return info;
}

double AudioProcessor::getDuration(const QString& audioPath)
{
    return getAudioInfo(audioPath).duration;
}

QPair<bool, QString> AudioProcessor::validateAudioFile(const QString& filePath)
{
    if (!QFile::exists(filePath))
        return {false, "File does not exist"};

    QString ext = QFileInfo(filePath).suffix().toLower().prepend(".");

    if (AUDIO_EXTENSIONS.contains(ext))
        return {true, "Valid audio file"};
    if (VIDEO_EXTENSIONS.contains(ext))
        return {true, "Valid video file (audio will be extracted)"};

    return {false, "Unsupported format: " + ext};
}

} // namespace ncktv
