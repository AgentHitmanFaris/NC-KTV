/**
 * @file ncktv_mdx_dsp.hpp
 * @brief MDX-Net DSP pipeline: Audio decode → STFT → ONNX → ISTFT → WAV write.
 *
 * Uses FFmpeg (via QProcess) for audio decoding to raw float PCM.
 * Implements the standard MDX-Net preprocessing/postprocessing used by
 * the audio-separator library, fully in C++17.
 *
 * References:
 *   - audio-separator (python): MDXModel class in mdx.py
 *   - Ultimate Vocal Remover: uvr5_pack/uvr_mdxnet.py
 */

#pragma once

#include <vector>
#include <string>
#include <cmath>
#include <complex>
#include <stdexcept>
#include <algorithm>
#include <numeric>
#include <cstring>
#include <cstdint>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace ncktv {
namespace dsp {

// ─── Model Parameters ────────────────────────────────────────────────────────
struct MdxParams {
    int    n_fft     = 6144;  // FFT size (mdx_n_fft_scale_set)
    int    dim_f     = 2048;  // frequency bins used (mdx_dim_f_set)
    int    dim_t     = 8;     // time-axis power (segment = 2^dim_t)
    float  compensate = 1.035f;
    int    sample_rate = 44100;
    int    channels    = 2;
    std::string primary_stem = "Vocals"; // Which stem the model outputs directly

    // Derived
    int hop_size()     const { return n_fft / 2; }
    int segment_size() const { return (1 << dim_t);  }  // 2^dim_t chunks
};

// ─── Hann Window ─────────────────────────────────────────────────────────────
static std::vector<float> makeHannWindow(int n) {
    std::vector<float> w(n);
    if (n <= 1) { if (n == 1) w[0] = 1.f; return w; }
    for (int i = 0; i < n; ++i)
        w[i] = 0.5f * (1.f - std::cos(2.f * (float)M_PI * i / (n - 1)));
    return w;
}

// ─── Mixed-Radix FFT Implementation ─────────────────────────────────────────
// This implementation handles any N, efficiently processing factors of 2, 3, and 5.
// Necessary for MDX-Net models which use window sizes like 6144 or 7680.

static void dft_small(const std::complex<float>* in, std::complex<float>* out, int n, int stride, bool inv) {
    for (int k = 0; k < n; ++k) {
        std::complex<float> sum(0, 0);
        for (int j = 0; j < n; ++j) {
            float angle = (inv ? 2.0f : -2.0f) * (float)M_PI * k * j / n;
            sum += in[j * stride] * std::complex<float>(std::cos(angle), std::sin(angle));
        }
        out[k] = sum;
    }
}

static void fft_work(std::complex<float>* data, std::complex<float>* buffer, int n, bool inv) {
    if (n <= 1) return;

    // Small prime factors we handle efficiently
    int p = 0;
    if (n % 2 == 0) p = 2;
    else if (n % 3 == 0) p = 3;
    else if (n % 5 == 0) p = 5;

    if (p == 0) { // Prime N > 5, fallback to DFT
        std::vector<std::complex<float>> tmp(n);
        dft_small(data, tmp.data(), n, 1, inv);
        std::copy(tmp.begin(), tmp.end(), data);
        return;
    }

    int m = n / p;
    // Decimation in time
    for (int i = 0; i < p; ++i) {
        // We use the buffer as a temporary workspace
        for (int j = 0; j < m; ++j) buffer[i * m + j] = data[j * p + i];
    }
    for (int i = 0; i < p; ++i) fft_work(buffer + i * m, data, m, inv);

    // Recombine with twiddle factors
    for (int j = 0; j < m; ++j) {
        std::vector<std::complex<float>> items(p);
        for (int i = 0; i < p; ++i) {
            float angle = (inv ? 2.0f : -2.0f) * (float)M_PI * i * j / n;
            std::complex<float> twiddle(std::cos(angle), std::sin(angle));
            items[i] = buffer[i * m + j] * twiddle;
        }

        // p-point DFT for the current set of items
        for (int k = 0; k < p; ++k) {
            std::complex<float> sum(0, 0);
            for (int i = 0; i < p; ++i) {
                float angle = (inv ? 2.0f : -2.0f) * (float)M_PI * k * i / p;
                sum += items[i] * std::complex<float>(std::cos(angle), std::sin(angle));
            }
            data[k * m + j] = sum;
        }
    }
}

static void fft_main(std::vector<std::complex<float>>& data, bool inv) {
    int n = (int)data.size();
    if (n <= 1) return;
    std::vector<std::complex<float>> buffer(n);
    fft_work(data.data(), buffer.data(), n, inv);
}

// ─── Real STFT (single frame, Cooley-Tukey with power-of-2 zero-padding) ──────
// Input x may be ANY length (e.g. 6144). We zero-pad to nextPow2(N) for the
// FFT, then return only the first (N/2 + 1) bins matching the original N.
static std::vector<std::complex<float>> rfft(const std::vector<float>& x) {
    int N    = (int)x.size();         // window size (e.g. 6144)
    int Nout = N / 2 + 1;            // bins = N/2 + 1

    std::vector<std::complex<float>> buf(N);
    for (int i = 0; i < N; ++i) buf[i] = {x[i], 0.f};

    fft_main(buf, false);

    // Return only Nout bins
    buf.resize(Nout);
    return buf;
}

// ─── Inverse FFT (full complex in → full complex out, power-of-2 padded) ─────
// Input buf is the full Hermitian spectrum of size n_fft (e.g. 6144).
// We zero-pad to nextPow2(N) for the FFT, then truncate to N output samples.
static std::vector<std::complex<float>> ifft_full(std::vector<std::complex<float>> buf) {
    int N = (int)buf.size(); // original n_fft

    fft_main(buf, true);

    // Scale by N
    float scale = 1.f / N;
    for (auto& v : buf) v *= scale;
    return buf;
}

// ─── STFT ────────────────────────────────────────────────────────────────────
// Returns [channels][freq_bins][time_frames] as interleaved real+imag floats.
// Output shape: channels x (n_fft/2+1) x n_frames  (complex)
struct StftResult {
    int channels, freq_bins, n_frames;
    // [ch][frame][bin]  — complex stored as (real,imag) pairs
    std::vector<float> data;  // size = channels * n_frames * freq_bins * 2
};

static StftResult stft(const std::vector<float>& audio,
                        int n_channels,
                        const MdxParams& p)
{
    int n_fft     = p.n_fft;
    int hop_size  = p.hop_size();
    int freq_bins = n_fft / 2 + 1;
    int n_samples = (int)audio.size() / n_channels;

    auto window   = makeHannWindow(n_fft);

    // Pad signal by n_fft/2 on each side (same as librosa center=True)
    int half = n_fft / 2;
    int padded_len = n_samples + 2 * half;

    // Number of frames
    int n_frames = 1 + (padded_len - n_fft) / hop_size;

    StftResult res;
    res.channels  = n_channels;
    res.freq_bins = freq_bins;
    res.n_frames  = n_frames;
    res.data.resize((size_t)n_channels * n_frames * freq_bins * 2, 0.f);

    std::vector<float> frame(n_fft);

    for (int ch = 0; ch < n_channels; ++ch) {
        // build padded channel signal (reflect padding)
        std::vector<float> sig(padded_len, 0.f);
        for (int i = 0; i < n_samples; ++i)
            sig[half + i] = audio[(size_t)i * n_channels + ch];
        // reflect at edges
        for (int i = 0; i < half; ++i)
            sig[half - 1 - i] = sig[half + 1 + i];
        for (int i = 0; i < half; ++i)
            sig[half + n_samples + i] = sig[half + n_samples - 2 - i];

        for (int t = 0; t < n_frames; ++t) {
            int start = t * hop_size;
            for (int k = 0; k < n_fft; ++k)
                frame[k] = (start + k < padded_len ? sig[start + k] : 0.f) * window[k];

            auto spec = rfft(frame);

            float* dst = res.data.data() +
                         ((size_t)ch * n_frames + t) * freq_bins * 2;
            for (int f = 0; f < freq_bins; ++f) {
                dst[f * 2]     = spec[f].real();
                dst[f * 2 + 1] = spec[f].imag();
            }
        }
    }
    return res;
}

// ─── ISTFT ───────────────────────────────────────────────────────────────────
static std::vector<float> istft(const StftResult& res, const MdxParams& p, int n_samples_original) {
    int n_fft     = p.n_fft;
    int hop_size  = p.hop_size();
    int freq_bins = res.freq_bins;
    int n_frames  = res.n_frames;
    int n_channels = res.channels;

    auto window   = makeHannWindow(n_fft);
    int half = n_fft / 2;
    int padded_len = n_samples_original + 2 * half;

    std::vector<float> output(n_samples_original * n_channels, 0.f);

    for (int ch = 0; ch < n_channels; ++ch) {
        std::vector<float> buf(padded_len, 0.f);
        std::vector<float> win_sum(padded_len, 0.f);

        for (int t = 0; t < n_frames; ++t) {
            // Reconstruct full spectrum (Hermitian symmetry)
            std::vector<std::complex<float>> spec(n_fft);
            const float* src = res.data.data() +
                               ((size_t)ch * n_frames + t) * freq_bins * 2;
            for (int f = 0; f < freq_bins; ++f)
                spec[f] = {src[f*2], src[f*2+1]};
            for (int f = freq_bins; f < n_fft; ++f)
                spec[f] = std::conj(spec[n_fft - f]);

            auto td = ifft_full(spec);

            int start = t * hop_size;
            for (int k = 0; k < n_fft && start + k < padded_len; ++k) {
                buf[start + k]     += td[k].real() * window[k];
                win_sum[start + k] += window[k] * window[k];
            }
        }

        // Normalize by window sum and strip padding
        for (int i = 0; i < n_samples_original; ++i) {
            float w = win_sum[half + i];
            float val = (w > 1e-8f) ? buf[half + i] / w : 0.f;
            output[(size_t)i * n_channels + ch] = std::clamp(val, -1.f, 1.f);
        }
    }
    return output;
}

// ─── WAV writer (16-bit PCM) ─────────────────────────────────────────────────
struct WavHdr {
    char     riff[4]   = {'R','I','F','F'};
    uint32_t fileSize;
    char     wave[4]   = {'W','A','V','E'};
    char     fmt[4]    = {'f','m','t',' '};
    uint32_t fmtSize   = 16;
    uint16_t audioFmt  = 1;
    uint16_t numCh;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample = 16;
    char     data[4]   = {'d','a','t','a'};
    uint32_t dataSize;
};

static bool writeWav(const std::string& path,
                     const std::vector<float>& samples,
                     int channels, int sampleRate)
{
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;

    uint32_t nSamples = (uint32_t)(samples.size() / channels);
    WavHdr h;
    h.numCh      = (uint16_t)channels;
    h.sampleRate = (uint32_t)sampleRate;
    h.bitsPerSample = 16;
    h.blockAlign = (uint16_t)(channels * 2);
    h.byteRate   = sampleRate * channels * 2;
    h.dataSize   = nSamples * channels * 2;
    h.fileSize   = 36 + h.dataSize;

    fwrite(&h, sizeof(h), 1, f);
    for (float v : samples) {
        int16_t s = (int16_t)std::clamp(v * 32767.f, -32768.f, 32767.f);
        fwrite(&s, sizeof(s), 1, f);
    }
    fclose(f);
    return true;
}

} // namespace dsp
} // namespace ncktv
