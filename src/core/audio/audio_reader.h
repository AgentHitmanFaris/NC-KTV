#pragma once

#include <QString>
#include <vector>

namespace ncktv {

class AudioReader {
public:
    struct Peak {
        float minVal;
        float maxVal;
    };

    AudioReader() = default;
    ~AudioReader() = default;

    // Decodes audio using FFmpeg and resamples to target rate (Default: 48000 Hz), Stereo, Float format.
    // Precomputes Peak Caches for dynamic timeline rendering.
    // Returns true on success, false on failure.
    bool decodeFile(const QString& filePath, int targetSampleRate = 48000);

    [[nodiscard]] const std::vector<float>& samples() const { return m_samples; }
    [[nodiscard]] const std::vector<Peak>& peaks256() const { return m_peaks256; }
    [[nodiscard]] const std::vector<Peak>& peaks4096() const { return m_peaks4096; }

    void setSamples(const std::vector<float>& samples) {
        m_samples = samples;
        m_durationSeconds = static_cast<double>(m_samples.size() / 2.0) / m_sampleRate;
    }

    bool loadPeakCache(const QString& filePath);
    bool savePeakCache(const QString& filePath);
    QString getCachePath(const QString& filePath) const;

    [[nodiscard]] int sampleRate() const { return m_sampleRate; }
    [[nodiscard]] int channels() const { return m_channels; }
    [[nodiscard]] double durationSeconds() const { return m_durationSeconds; }
    [[nodiscard]] qint64 totalSamples() const { return static_cast<qint64>(m_samples.size() / 2); }

    // For unit testing peak precomputations
    void setSamplesForTesting(const std::vector<float>& mockSamples, int rate = 48000) {
        m_samples = mockSamples;
        m_sampleRate = rate;
        m_channels = 2;
        m_durationSeconds = static_cast<double>(m_samples.size() / 2.0) / m_sampleRate;
        precomputePeaks();
    }

private:
    void precomputePeaks();

    std::vector<float> m_samples; // Interleaved stereo samples
    std::vector<Peak> m_peaks256;  // 256 frame level of detail peaks
    std::vector<Peak> m_peaks4096; // 4096 frame level of detail peaks

    int m_sampleRate = 48000;
    int m_channels = 2;
    double m_durationSeconds = 0.0;
};

} // namespace ncktv
