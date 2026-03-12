/*
 * NC-KTV Core — Audio Clock Implementation
 */

#include "audio_clock.h"
#include <cmath>
#include <algorithm>

namespace ncktv {

static std::string qs(const QString& s) { return s.toStdString(); }
static QString fromStd(const std::string& s) { return QString::fromStdString(s); }

AudioClock::AudioClock(int sampleRate)
    : m_sampleRate(sampleRate)
{
    // Default latency corrections (seconds)
    m_latencyCorrections["uvr"]           = 0.0;
    m_latencyCorrections["transcription"] = 0.0;
    m_latencyCorrections["playback"]      = 0.023;   // Qt QMediaPlayer typical latency
    m_latencyCorrections["preview"]       = 0.008;   // ~1 frame at 60fps
}

void AudioClock::setMasterOffset(double offset) {
    m_masterOffset = offset;
}

void AudioClock::setSampleRate(int rate) {
    m_sampleRate = rate;
}

double AudioClock::samplesToSeconds(int64_t samples) const {
    return (static_cast<double>(samples) / m_sampleRate) + m_masterOffset;
}

int64_t AudioClock::secondsToSamples(double seconds) const {
    return static_cast<int64_t>((seconds - m_masterOffset) * m_sampleRate);
}

double AudioClock::syncPoint(double timestamp, const QString& source) const {
    double correction = m_latencyCorrections.value(source, 0.0);
    return timestamp + correction + m_masterOffset;
}

void AudioClock::setLatency(const QString& source, double latency) {
    m_latencyCorrections[source] = latency;
}

double AudioClock::getLatency(const QString& source) const {
    return m_latencyCorrections.value(source, 0.0);
}

double AudioClock::getDriftAtTime(double seconds) const {
    // Drift = accumulated timing error due to actual sample rate vs nominal rate.
    // If the actual clock runs at m_sampleRate but the reference is 48000 Hz,
    // after T seconds the drift is:  T * (m_sampleRate - 48000) / 48000
    constexpr int REFERENCE_RATE = 48000;
    if (m_sampleRate == REFERENCE_RATE || seconds <= 0.0) return 0.0;

    double rateRatio = static_cast<double>(m_sampleRate) / REFERENCE_RATE;
    double drift = seconds * (rateRatio - 1.0);

    // Also incorporate the sum of all latency corrections as a constant offset
    double totalLatency = 0.0;
    for (auto it = m_latencyCorrections.begin(); it != m_latencyCorrections.end(); ++it) {
        totalLatency += it.value();
    }

    return drift + totalLatency;
}

// ── Serialization ────────────────────────────────────────────────────────────

nlohmann::json AudioClock::toJson() const {
    nlohmann::json corrections = nlohmann::json::object();
    for (auto it = m_latencyCorrections.begin(); it != m_latencyCorrections.end(); ++it)
        corrections[qs(it.key())] = it.value();

    return {
        {"sample_rate",          m_sampleRate},
        {"master_offset",        m_masterOffset},
        {"latency_corrections",  corrections}
    };
}

AudioClock AudioClock::fromJson(const nlohmann::json& j) {
    AudioClock clock(j.value("sample_rate", 48000));
    clock.m_masterOffset = j.value("master_offset", 0.0);

    if (j.contains("latency_corrections")) {
        for (auto& [key, val] : j["latency_corrections"].items())
            clock.m_latencyCorrections[fromStd(key)] = val.get<double>();
    }

    return clock;
}

} // namespace ncktv
