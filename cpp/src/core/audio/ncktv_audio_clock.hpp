/**
 * @file ncktv_audio_clock.hpp
 * @brief Sample-accurate timing reference and drift prevention (C++17)
 */

#pragma once

#include <map>
#include <string>
#include <vector>
#include <optional>
#include <iostream>

namespace ncktv {
namespace core {

class AudioClock {
private:
    int sample_rate_{48000};
    double master_offset_{0.0};
    std::map<std::string, double> latency_corrections_{
        {"uvr", 0.0},
        {"transcription", 0.0},
        {"playback", 0.023},
        {"preview", 0.008}
    };

public:
    AudioClock() = default;
    explicit AudioClock(int sample_rate) : sample_rate_(sample_rate) {}

    void set_master_offset(double offset) {
        master_offset_ = offset;
    }

    [[nodiscard]] double get_master_offset() const {
        return master_offset_;
    }

    void set_sample_rate(int sample_rate) {
        sample_rate_ = sample_rate;
    }

    [[nodiscard]] int get_sample_rate() const {
        return sample_rate_;
    }

    [[nodiscard]] double samples_to_seconds(int samples) const {
        return (static_cast<double>(samples) / sample_rate_) + master_offset_;
    }

    [[nodiscard]] int seconds_to_samples(double seconds) const {
        return static_cast<int>((seconds - master_offset_) * sample_rate_);
    }

    [[nodiscard]] double sync_point(double timestamp, const std::string& source) const {
        double correction = 0.0;
        auto it = latency_corrections_.find(source);
        if (it != latency_corrections_.end()) {
            correction = it->second;
        }
        return timestamp + correction + master_offset_;
    }

    void set_latency(const std::string& source, double latency) {
        latency_corrections_[source] = latency;
    }

    [[nodiscard]] const std::map<std::string, double>& get_latencies() const {
        return latency_corrections_;
    }
};

} // namespace core
} // namespace ncktv
