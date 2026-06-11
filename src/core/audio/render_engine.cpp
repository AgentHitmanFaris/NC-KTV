#include "render_engine.h"
#include <iostream>

namespace ncktv {

RenderEngine::RenderEngine() = default;
RenderEngine::~RenderEngine() = default;

bool RenderEngine::startRender(const QString& outputPath, int width, int height, int fps, int videoBitrate, int audioBitrate, const QString& audioCodecName) {
    (void)outputPath; (void)width; (void)height; (void)fps; (void)videoBitrate; (void)audioBitrate; (void)audioCodecName;
    m_chosenVideoCodec = "libVLC Mock Encoder";
    m_chosenAudioCodec = "libVLC Mock AAC";
    std::cout << "[RenderEngine] Mock render started successfully.\n";
    return true;
}

bool RenderEngine::writeVideoFrame(const QImage& frameImage, int64_t frameIndex) {
    (void)frameImage; (void)frameIndex;
    return true;
}

bool RenderEngine::writeAudioFrame(const float* stereoSamples, int numFrames) {
    (void)stereoSamples; (void)numFrames;
    return true;
}

bool RenderEngine::finishRender() {
    std::cout << "[RenderEngine] Mock render finished successfully.\n";
    return true;
}

} // namespace ncktv
