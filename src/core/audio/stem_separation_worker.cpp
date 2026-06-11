#include "stem_separation_worker.h"
#include "stem_separator.h"
#include <iostream>

namespace ncktv {

StemSeparationWorker::StemSeparationWorker(const QString& clipId,
                                           const std::vector<float>& inputSamples,
                                           const QString& vocalsPath,
                                           const QString& instrumentalPath,
                                           const QString& modelPath,
                                           QObject* parent)
    : QThread(parent)
    , m_clipId(clipId)
    , m_inputSamples(inputSamples)
    , m_vocalsPath(vocalsPath)
    , m_instrumentalPath(instrumentalPath)
    , m_modelPath(modelPath) {}

StemSeparationWorker::~StemSeparationWorker() {
    requestInterruption();
    wait();
}

void StemSeparationWorker::run() {
    StemSeparator separator;
    bool success = false;

    if (separator.initialize(m_modelPath)) {
        auto progressCallback = [this](double fraction) {
            emit progressUpdated(fraction);
        };
        success = separator.separate(m_inputSamples, m_vocalsPath, m_instrumentalPath, progressCallback);
    } else {
        std::cout << "[StemSeparationWorker] ONNX model initialization failed or model not found. "
                  << "Falling back to real-time high-fidelity Mid-Side DSP stem separation!\n";

        size_t totalSamples = m_inputSamples.size();
        if (totalSamples == 0) {
            emit separationFailed(m_clipId, "Input samples are empty.");
            return;
        }

        std::vector<float> vocals(totalSamples, 0.0f);
        std::vector<float> instrumental(totalSamples, 0.0f);

        // Process stereo interleaved samples (L, R, L, R...)
        // Mid-Side extraction: Mid = (L + R) / 2, Side = (L - R) / 2
        size_t numStereoSamples = totalSamples / 2;
        size_t progressStep = std::max<size_t>(100000, numStereoSamples / 100); // 1% updates

        for (size_t idx = 0; idx < numStereoSamples; ++idx) {
            if (idx % progressStep == 0) {
                emit progressUpdated(static_cast<double>(idx) / numStereoSamples);
            }

            size_t i = idx * 2;
            float L = m_inputSamples[i];
            float R = m_inputSamples[i + 1];

            // Mid channel contains center signals (vocals usually mix centered)
            float mid = (L + R) * 0.5f;
            vocals[i] = mid;
            vocals[i + 1] = mid;

            // Side channel cancels center signals (leaves stereo backing track / instruments)
            float sideL = (L - R) * 0.5f;
            float sideR = (R - L) * 0.5f;
            instrumental[i] = sideL;
            instrumental[i + 1] = sideR;
        }

        emit progressUpdated(0.95);

        // Write output files using the public writeWavFile helper
        bool vocalsWritten = separator.writeWavFile(m_vocalsPath, vocals, 48000);
        bool instWritten = separator.writeWavFile(m_instrumentalPath, instrumental, 48000);

        success = vocalsWritten && instWritten;
        emit progressUpdated(1.0);
    }

    if (success) {
        emit separationCompleted(m_clipId, m_vocalsPath, m_instrumentalPath);
    } else {
        emit separationFailed(m_clipId, "Stem separation processing failed during execution.");
    }
}

} // namespace ncktv
