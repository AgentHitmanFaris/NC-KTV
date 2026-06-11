#include "stem_resampler.h"
#include <iostream>
#include <miniaudio.h>

namespace ncktv {

std::vector<float> StemResampler::resample(const std::vector<float>& inputSamples, int inRate, int outRate, int channels) {
    if (inputSamples.empty()) return {};
    if (inRate == outRate) return inputSamples;

    ma_resampler_config config = ma_resampler_config_init(
        ma_format_f32,
        channels,
        inRate,
        outRate,
        ma_resample_algorithm_linear
    );

    ma_resampler resampler;
    ma_result result = ma_resampler_init(&config, nullptr, &resampler);
    if (result != MA_SUCCESS) {
        std::cerr << "[StemResampler] Error: ma_resampler_init failed with code " << result << "\n";
        return {};
    }

    ma_uint64 framesIn = inputSamples.size() / channels;
    ma_uint64 max_out_frames = (framesIn * outRate + inRate - 1) / inRate + 128;
    std::vector<float> outputSamples(max_out_frames * channels);

    ma_uint64 processedFramesIn = framesIn;
    ma_uint64 processedFramesOut = max_out_frames;

    result = ma_resampler_process_pcm_frames(&resampler, inputSamples.data(), &processedFramesIn, outputSamples.data(), &processedFramesOut);
    if (result != MA_SUCCESS) {
        std::cerr << "[StemResampler] Error: ma_resampler_process_pcm_frames failed with code " << result << "\n";
        ma_resampler_uninit(&resampler, nullptr);
        return {};
    }

    outputSamples.resize(processedFramesOut * channels);
    ma_resampler_uninit(&resampler, nullptr);
    return outputSamples;
}

} // namespace ncktv
