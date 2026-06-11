#pragma once

#include <QString>
#include <QImage>
#include <QProcess>
#include <QFile>

namespace ncktv {

class RenderEngine {
public:
    RenderEngine();
    ~RenderEngine();

    bool startRender(const QString& outputPath, int width, int height, int fps, int videoBitrate, int audioBitrate, const QString& audioCodecName = "");
    bool writeVideoFrame(const QImage& frameImage, int64_t frameIndex);
    bool writeAudioFrame(const float* stereoSamples, int numFrames);
    bool finishRender();

    [[nodiscard]] QString chosenVideoCodec() const { return m_chosenVideoCodec; }
    [[nodiscard]] QString chosenAudioCodec() const { return m_chosenAudioCodec; }

private:
    QString m_chosenVideoCodec;
    QString m_chosenAudioCodec;

    QProcess* m_videoProcess = nullptr;
    QFile m_audioFile;
    QString m_tempVideoPath;
    QString m_tempAudioPath;
    QString m_finalOutputPath;
    QString m_ffmpegPath;

    int m_width = 1920;
    int m_height = 1080;
    int m_fps = 30;
    int m_videoBitrate = 4000000;
    int m_audioBitrate = 192000;
    int64_t m_totalAudioSamples = 0;
};

} // namespace ncktv

