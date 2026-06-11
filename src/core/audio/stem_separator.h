#pragma once

#include <QString>
#include <vector>
#include <memory>
#include <string>
#include <functional>
#include <onnxruntime_cxx_api.h>

namespace ncktv {

class StemSeparator {
public:
    StemSeparator();
    ~StemSeparator();

    // Initializes the ONNX Session and environment.
    // Tries to resolve a model from local AppData if modelPath is empty.
    // Cascade-registers DirectML -> CUDA -> CPU execution providers.
    bool initialize(const QString& modelPath = QString());

    // Performs stem separation on input interleaved stereo 48kHz audio.
    // Progress callback is invoked with value from 0.0 to 1.0.
    // Writes the resulting audio to vocalsPath and instrumentalPath as 48kHz stereo WAV files.
    bool separate(const std::vector<float>& input48kStereo,
                  const QString& vocalsPath,
                  const QString& instrumentalPath,
                  std::function<void(double)> progressCallback = nullptr);

    [[nodiscard]] bool isInitialized() const { return m_initialized; }
    [[nodiscard]] QString modelName() const { return m_modelName; }
    [[nodiscard]] QString executionProvider() const { return m_executionProvider; }

    // Helper for WAV file writing
    bool writeWavFile(const QString& filePath, const std::vector<float>& samples, int sampleRate);

private:
    bool m_initialized = false;
    QString m_modelName;
    QString m_executionProvider;

    std::unique_ptr<Ort::Env> m_env;
    std::unique_ptr<Ort::Session> m_session;
    Ort::SessionOptions m_sessionOptions;
};

} // namespace ncktv
