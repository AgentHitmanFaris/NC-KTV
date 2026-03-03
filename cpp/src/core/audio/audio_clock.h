#pragma once
/*
 * NC-KTV Core — Audio Clock
 * Port of audio_clock.py
 *
 * Sample-accurate timing reference to prevent drift between
 * UVR separation, transcription, and playback.
 */

#include <QString>
#include <QMap>
#include <nlohmann/json.hpp>

namespace ncktv {

class AudioClock {
public:
    explicit AudioClock(int sampleRate = 48000);

    // ── Master offset (user calibration) ─────────────────────────────────
    void   setMasterOffset(double offset);
    double masterOffset() const { return m_masterOffset; }

    // ── Sample rate ──────────────────────────────────────────────────────
    void setSampleRate(int rate);
    int  sampleRate() const { return m_sampleRate; }

    // ── Conversions ──────────────────────────────────────────────────────
    [[nodiscard]] double samplesToSeconds(int64_t samples) const;
    [[nodiscard]] int64_t secondsToSamples(double seconds) const;

    // ── Sync point correction ────────────────────────────────────────────
    /// Apply latency correction for a timestamp from a specific source.
    /// Sources: "uvr", "transcription", "playback", "preview"
    [[nodiscard]] double syncPoint(double timestamp, const QString& source) const;

    void   setLatency(const QString& source, double latency);
    double getLatency(const QString& source) const;

    // ── Diagnostics ──────────────────────────────────────────────────────
    [[nodiscard]] double getDriftAtTime(double seconds) const;

    // ── Serialization ────────────────────────────────────────────────────
    [[nodiscard]] nlohmann::json toJson() const;
    static AudioClock fromJson(const nlohmann::json& j);

private:
    int    m_sampleRate    = 48000;
    double m_masterOffset  = 0.0;
    QMap<QString, double> m_latencyCorrections;
};

} // namespace ncktv
