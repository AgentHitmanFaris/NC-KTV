#pragma once

#include <vector>

namespace ncktv {

class StemResampler {
public:
    // Resamples interleaved float samples between sample rates.
    // Handles arbitrary number of channels (default: 2 for Stereo).
    static std::vector<float> resample(const std::vector<float>& inputSamples, int inRate, int outRate, int channels = 2);
};

} // namespace ncktv
