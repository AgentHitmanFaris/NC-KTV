#pragma once

#include <QString>
#include <QImage>

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
};

} // namespace ncktv
