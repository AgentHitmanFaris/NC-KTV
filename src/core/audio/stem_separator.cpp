#include "stem_separator.h"
#include "stem_resampler.h"
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <iostream>
#include <thread>
#include <algorithm>
#include <cmath>

#include <complex>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Custom AVComplexFloat structure matching the legacy FFmpeg struct signature
struct AVComplexFloat {
    float re, im;
};

namespace {

// Cooley-Tukey Radix-2 FFT (requires n to be a power of 2)
void fft(std::vector<std::complex<float>>& a, bool invert) {
    int n = a.size();
    for (int i = 1, j = 0; i < n; i++) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
            std::swap(a[i], a[j]);
    }
    for (int len = 2; len <= n; len <<= 1) {
        double angle = 2 * M_PI / len * (invert ? -1 : 1);
        std::complex<float> wlen(std::cos(angle), std::sin(angle));
        for (int i = 0; i < n; i += len) {
            std::complex<float> w(1);
            for (int j = 0; j < len / 2; j++) {
                std::complex<float> u = a[i+j], v = a[i+j+len/2] * w;
                a[i+j] = u + v;
                a[i+j+len/2] = u - v;
                w *= wlen;
            }
        }
    }
    if (invert) {
        for (auto& x : a)
            x /= n;
    }
}

// General FFT wrapper supporting powers of 2 and size 6144 via mixed-radix Cooley-Tukey
void fft_general(std::vector<std::complex<float>>& a, bool invert) {
    int n = a.size();
    if (n == 0) return;
    
    // Check if power of 2
    if ((n & (n - 1)) == 0) {
        fft(a, invert);
        return;
    }
    
    // Fast path: Mixed-radix 3 x 2048 for size 6144 (since 6144 = 3 * 2048)
    if (n == 6144) {
        int N1 = 3;
        int N2 = 2048;
        
        // 1. Reshape into 3 x 2048 2D array
        std::vector<std::vector<std::complex<float>>> A(N1, std::vector<std::complex<float>>(N2));
        for (int n1 = 0; n1 < N1; ++n1) {
            for (int n2 = 0; n2 < N2; ++n2) {
                A[n1][n2] = a[n1 * N2 + n2];
            }
        }
        
        // 2. Perform 2048-point FFTs on the rows
        for (int n1 = 0; n1 < N1; ++n1) {
            fft(A[n1], invert);
        }
        
        double sign = invert ? 1.0 : -1.0;
        std::complex<float> w3 = std::polar(1.0f, static_cast<float>(sign * 2.0 * M_PI / 3.0));
        std::complex<float> w3_2 = w3 * w3;
        
        // 3. Multiply by twiddle factors and apply 3-point DFT along the columns
        for (int k2 = 0; k2 < N2; ++k2) {
            std::complex<float> z0 = A[0][k2];
            std::complex<float> z1 = A[1][k2] * std::polar(1.0f, static_cast<float>(sign * 2.0 * M_PI * k2 * 1 / 6144.0));
            std::complex<float> z2 = A[2][k2] * std::polar(1.0f, static_cast<float>(sign * 2.0 * M_PI * k2 * 2 / 6144.0));
            
            a[k2 * 3 + 0] = z0 + z1 + z2;
            a[k2 * 3 + 1] = z0 + z1 * w3 + z2 * w3_2;
            a[k2 * 3 + 2] = z0 + z1 * w3_2 + z2 * w3;
            
            if (invert) {
                a[k2 * 3 + 0] /= 3.0f;
                a[k2 * 3 + 1] /= 3.0f;
                a[k2 * 3 + 2] /= 3.0f;
            }
        }
        return;
    }
    
    // Slow fallback: O(N^2) Discrete Fourier Transform (for general non-power-of-2 sizes)
    std::cout << "[StemSeparator] Warning: Non-power-of-two size " << n << " detected. Falling back to slow O(N^2) DFT." << std::endl;
    std::vector<std::complex<float>> out(n, 0.0f);
    double sign = invert ? 1.0 : -1.0;
    for (int k = 0; k < n; ++k) {
        std::complex<float> sum(0.0f, 0.0f);
        for (int j = 0; j < n; ++j) {
            double angle = sign * 2.0 * M_PI * k * j / n;
            sum += a[j] * std::complex<float>(static_cast<float>(std::cos(angle)), static_cast<float>(std::sin(angle)));
        }
        if (invert) {
            sum /= static_cast<float>(n);
        }
        out[k] = sum;
    }
    a = out;
}

// Real DFT: real input of size n -> complex output of size n / 2 + 1
void rdft_forward(const float* input, AVComplexFloat* output, int n) {
    std::vector<std::complex<float>> a(n);
    for (int i = 0; i < n; i++) {
        a[i] = std::complex<float>(input[i], 0.0f);
    }
    fft_general(a, false);
    for (int i = 0; i <= n / 2; i++) {
        output[i].re = a[i].real();
        output[i].im = a[i].imag();
    }
}

// Inverse Real DFT: complex input of size n / 2 + 1 -> real output of size n
void rdft_inverse(float* output, const AVComplexFloat* input, int n) {
    std::vector<std::complex<float>> a(n);
    for (int i = 0; i <= n / 2; i++) {
        a[i] = std::complex<float>(input[i].re, input[i].im);
    }
    for (int i = n / 2 + 1; i < n; i++) {
        a[i] = std::conj(a[n - i]);
    }
    fft_general(a, true);
    for (int i = 0; i < n; i++) {
        output[i] = a[i].real();
    }
}

} // namespace


namespace ncktv {

StemSeparator::StemSeparator() = default;
StemSeparator::~StemSeparator() = default;

bool StemSeparator::initialize(const QString& modelPath) {
    std::cout << "[StemSeparator] initialize() entered. modelPath: " << modelPath.toStdString() << std::endl;
    m_initialized = false;
    m_modelName = "";
    m_executionProvider = "CPU";

    QString resolvedPath = modelPath;
    if (resolvedPath.isEmpty()) {
        // Find by default in Local AppData: Local/NC-KTV/models
        QString appLocal = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        QString modelDir = QDir(appLocal).filePath("models");
        
        QDir dir(modelDir);
        if (dir.exists()) {
            QStringList filters;
            filters << "*.onnx";
            QStringList files = dir.entryList(filters, QDir::Files);
            if (!files.isEmpty()) {
                resolvedPath = dir.filePath(files.first());
            }
        }
    }

    std::cout << "[StemSeparator] resolvedPath: " << resolvedPath.toStdString() << std::endl;

    if (resolvedPath.isEmpty() || !QFile::exists(resolvedPath)) {
        std::cerr << "[StemSeparator] Error: Model not found at path: " << resolvedPath.toStdString() << std::endl;
        return false;
    }

    m_modelName = QFileInfo(resolvedPath).baseName();

    try {
        std::cout << "[StemSeparator] Creating Ort::Env..." << std::endl;
        m_env = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "NC-KTV-StemSeparation");
        std::cout << "[StemSeparator] Ort::Env created successfully." << std::endl;

        m_sessionOptions = Ort::SessionOptions();

        // 1. Thread configuration (N - 2)
        int num_cores = std::max(1, static_cast<int>(std::thread::hardware_concurrency()));
        int intra_threads = std::max(1, num_cores - 2);
        m_sessionOptions.SetIntraOpNumThreads(intra_threads);
        m_sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

        // 2. Hardware Cascade: DirectML -> CUDA -> CPU
        bool forceCpu = false;
#ifdef _WIN32
        char* forceCpuEnv = nullptr;
        size_t len = 0;
        if (_dupenv_s(&forceCpuEnv, &len, "NCKTV_FORCE_CPU") == 0 && forceCpuEnv != nullptr) {
            if (std::string(forceCpuEnv) == "1") {
                forceCpu = true;
            }
            free(forceCpuEnv);
        }
#else
        char* forceCpuEnv = std::getenv("NCKTV_FORCE_CPU");
        if (forceCpuEnv && std::string(forceCpuEnv) == "1") {
            forceCpu = true;
        }
#endif

#if defined(_WIN32)
        bool ep_registered = forceCpu;
        if (forceCpu) {
            m_executionProvider = "CPU";
            std::cout << "[StemSeparator] CPU execution forced by environment variable NCKTV_FORCE_CPU." << std::endl;
        }
        if (!ep_registered) {
            try {
                // Try registering DirectML execution provider
                std::unordered_map<std::string, std::string> dml_options;
                dml_options["device_id"] = "0";
                m_sessionOptions.AppendExecutionProvider("DML", dml_options);
                m_executionProvider = "DirectML";
                ep_registered = true;
                std::cout << "[StemSeparator] DirectML hardware acceleration registered successfully." << std::endl;
            } catch (const Ort::Exception& e) {
                std::cout << "[StemSeparator] DirectML not available, trying CUDA... Ort::Exception: " << e.what() << std::endl;
            } catch (const std::exception& e) {
                std::cout << "[StemSeparator] DirectML not available, trying CUDA... std::exception: " << e.what() << std::endl;
            }
        }

        if (!ep_registered) {
            try {
                // Try registering CUDA execution provider
                OrtCUDAProviderOptions cuda_options;
                cuda_options.device_id = 0;
                m_sessionOptions.AppendExecutionProvider_CUDA(cuda_options);
                m_executionProvider = "CUDA";
                ep_registered = true;
                std::cout << "[StemSeparator] CUDA hardware acceleration registered successfully." << std::endl;
            } catch (const Ort::Exception& e) {
                std::cout << "[StemSeparator] CUDA not available, falling back to CPU. Ort::Exception: " << e.what() << std::endl;
            } catch (const std::exception& e) {
                std::cout << "[StemSeparator] CUDA not available, falling back to CPU. std::exception: " << e.what() << std::endl;
            }
        }
#endif

        // Create the session
        std::cout << "[StemSeparator] Creating Ort::Session..." << std::endl;
#if defined(_WIN32)
        std::wstring modelPathW = resolvedPath.toStdWString();
        m_session = std::make_unique<Ort::Session>(*m_env, modelPathW.c_str(), m_sessionOptions);
#else
        std::string modelPathStr = resolvedPath.toStdString();
        m_session = std::make_unique<Ort::Session>(*m_env, modelPathStr.c_str(), m_sessionOptions);
#endif
        std::cout << "[StemSeparator] Ort::Session created successfully." << std::endl;

        m_initialized = true;
        std::cout << "[StemSeparator] Successfully loaded model: " << m_modelName.toStdString()
                  << " with Execution Provider: " << m_executionProvider.toStdString() << std::endl;
        return true;
    } catch (const Ort::Exception& e) {
        std::cerr << "[StemSeparator] ONNX Runtime Error: " << e.what() << std::endl;
        return false;
    } catch (const std::exception& e) {
        std::cerr << "[StemSeparator] Standard Error: " << e.what() << std::endl;
        return false;
    }
}

bool StemSeparator::separate(const std::vector<float>& input48kStereo,
                             const QString& vocalsPath,
                             const QString& instrumentalPath,
                             std::function<void(double)> progressCallback) {
    if (!m_initialized || !m_session) {
        std::cerr << "[StemSeparator] Error: Separator not initialized." << std::endl;
        return false;
    }

    if (input48kStereo.empty()) {
        std::cerr << "[StemSeparator] Error: Input samples vector is empty." << std::endl;
        return false;
    }

    try {
        const int modelRate = 44100;
        const int audioChannels = 2;

        std::cout << "[StemSeparator] Querying model inputs..." << std::endl;
        size_t num_inputs = m_session->GetInputCount();
        std::cout << "[StemSeparator] Number of inputs: " << num_inputs << std::endl;
        if (num_inputs == 0) {
            std::cerr << "[StemSeparator] Error: Model has 0 inputs." << std::endl;
            return false;
        }

        // Slide across audio in spectrogram frames rather than PCM samples
        Ort::TypeInfo input_type_info = m_session->GetInputTypeInfo(0);
        auto input_tensor_info = input_type_info.GetTensorTypeAndShapeInfo();
        std::vector<int64_t> input_shape = input_tensor_info.GetShape();

        // Detect model expected window size (fixed vs dynamic) on frame level
        // Expected shape: [1, 4, num_bins, W_frame]
        int64_t W_frame = 256; 
        if (input_shape.size() >= 4 && input_shape.back() > 0) {
            W_frame = input_shape.back();
        }

        int num_bins = 2048;
        if (input_shape.size() >= 3 && input_shape[2] > 0) {
            num_bins = static_cast<int>(input_shape[2]);
        }

        std::cout << "[StemSeparator] MDX-Net Model Detected. Shape: [";
        for (size_t i = 0; i < input_shape.size(); ++i) {
            std::cout << input_shape[i] << (i + 1 < input_shape.size() ? ", " : "");
        }
        std::cout << "], Frame Window: " << W_frame << ", Bins: " << num_bins << std::endl;

    // Downsample input from 48kHz stereo to model target sample rate (stereo audio)
    std::vector<float> inputModelRate = StemResampler::resample(input48kStereo, 48000, modelRate, audioChannels);
    if (inputModelRate.empty()) {
        std::cerr << "[StemSeparator] Error: Downsampling failed.\n";
        return false;
    }

    // ─────────────────────────────────────────────────────────────────────────────
    // STFT SETUP
    // ─────────────────────────────────────────────────────────────────────────────
    const int n_fft = num_bins * 2;
    const int hop_size = 1024;
    const int pad_size = n_fft / 2;
    const size_t num_samples = inputModelRate.size() / audioChannels;

    // Calculate exact number of frames so that padding perfectly covers all samples
    const size_t num_frames = static_cast<size_t>(std::ceil(static_cast<double>(num_samples) / hop_size));
    const size_t padded_length = (num_frames - 1) * hop_size + n_fft;

    // De-interleave input samples
    std::vector<float> input_left(padded_length, 0.0f);
    std::vector<float> input_right(padded_length, 0.0f);
    for (size_t i = 0; i < num_samples; ++i) {
        input_left[i + pad_size] = inputModelRate[i * 2 + 0];
        input_right[i + pad_size] = inputModelRate[i * 2 + 1];
    }

    // Pre-calculate Hann Window
    const float PI = 3.141592653589793f;
    std::vector<float> hann(n_fft);
    for (int i = 0; i < n_fft; ++i) {
        hann[i] = 0.5f * (1.0f - std::cos(2.0f * PI * i / n_fft));
    }

    // Create 4-channel complex spectrogram: flat layout of size [4 * num_bins * num_frames]
    // Channel mapping: 0 = L_re, 1 = L_im, 2 = R_re, 3 = R_im
    std::vector<float> spectrogram(4 * num_bins * num_frames, 0.0f);

    // Pre-allocate STFT loop temporary buffers to avoid 40,000+ heap allocations
    std::vector<float> windowed_left(n_fft);
    std::vector<AVComplexFloat> output_left(n_fft / 2 + 1);
    std::vector<float> windowed_right(n_fft);
    std::vector<AVComplexFloat> output_right(n_fft / 2 + 1);

    // Execute forward STFT on Left and Right channels
    for (size_t f = 0; f < num_frames; ++f) {
        size_t start_idx = f * hop_size;

        // 1. Process Left Channel
        for (int i = 0; i < n_fft; ++i) {
            windowed_left[i] = input_left[start_idx + i] * hann[i];
        }
        rdft_forward(windowed_left.data(), output_left.data(), n_fft);

        // 2. Process Right Channel
        for (int i = 0; i < n_fft; ++i) {
            windowed_right[i] = input_right[start_idx + i] * hann[i];
        }
        rdft_forward(windowed_right.data(), output_right.data(), n_fft);

        // Store first num_bins bins into flat complex spectrogram
        for (int b = 0; b < num_bins; ++b) {
            spectrogram[(0 * num_bins + b) * num_frames + f] = output_left[b].re;
            spectrogram[(1 * num_bins + b) * num_frames + f] = output_left[b].im;
            spectrogram[(2 * num_bins + b) * num_frames + f] = output_right[b].re;
            spectrogram[(3 * num_bins + b) * num_frames + f] = output_right[b].im;
        }
    }

    // ─────────────────────────────────────────────────────────────────────────────
    // ONNX INFERENCE OVERLAP-ADD PIPELINE
    // ─────────────────────────────────────────────────────────────────────────────
    Ort::AllocatorWithDefaultOptions allocator;
    Ort::AllocatedStringPtr input_name = m_session->GetInputNameAllocated(0, allocator);
    Ort::AllocatedStringPtr output_name = m_session->GetOutputNameAllocated(0, allocator);

    std::vector<const char*> input_names = { input_name.get() };
    std::vector<const char*> output_names = { output_name.get() };

    Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

    std::vector<float> model_output_spectrogram(4 * num_bins * num_frames, 0.0f);
    std::vector<float> model_output_weights(num_frames, 0.0f);

    const size_t O_frame = 32; // Overlap size on frame level
    const size_t S_frame = W_frame - O_frame;

    for (size_t offset_frame = 0; offset_frame < num_frames; offset_frame += S_frame) {
        size_t chunk_len_frame = std::min<size_t>(W_frame, num_frames - offset_frame);
        if (chunk_len_frame == 0) break;

        // Build rank-4 tensor of shape [1, 4, num_bins, W_frame] with optimized memcpy copies
        std::vector<float> planarInput(1 * 4 * num_bins * W_frame, 0.0f);
        for (int c = 0; c < 4; ++c) {
            for (int b = 0; b < num_bins; ++b) {
                size_t src_start = (c * num_bins + b) * num_frames + offset_frame;
                size_t dest_start = (c * num_bins + b) * W_frame;
                std::memcpy(&planarInput[dest_start], &spectrogram[src_start], chunk_len_frame * sizeof(float));
            }
        }

        std::vector<int64_t> input_dims = {1, 4, static_cast<int64_t>(num_bins), static_cast<int64_t>(W_frame)};
        Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
            memory_info, planarInput.data(), planarInput.size(), input_dims.data(), input_dims.size());

        try {
            auto output_tensors = m_session->Run(Ort::RunOptions{nullptr}, input_names.data(), &input_tensor, 1, output_names.data(), 1);
            if (!output_tensors.empty()) {
                float* out_data = output_tensors[0].GetTensorMutableData<float>();

                // Overlap-add model output spectrogram using a cosine OLA window on the frame/time axis
                for (size_t t = 0; t < chunk_len_frame; ++t) {
                    float weight = 1.0f;
                    if (offset_frame > 0 && t < O_frame) {
                        float theta = (PI * t) / (2.0f * O_frame);
                        weight = std::sin(theta) * std::sin(theta);
                    }
                    else if (offset_frame + W_frame < num_frames && t >= W_frame - O_frame) {
                        size_t t_prime = t - (W_frame - O_frame);
                        float theta = (PI * t_prime) / (2.0f * O_frame);
                        weight = std::cos(theta) * std::cos(theta);
                    }

                    size_t global_frame = offset_frame + t;
                    if (global_frame < num_frames) {
                        model_output_weights[global_frame] += weight;
                        for (int c = 0; c < 4; ++c) {
                            for (int b = 0; b < num_bins; ++b) {
                                size_t idx = c * (num_bins * W_frame) + b * W_frame + t;
                                size_t dest_idx = (c * num_bins + b) * num_frames + global_frame;
                                model_output_spectrogram[dest_idx] += out_data[idx] * weight;
                            }
                        }
                    }
                }
            }
        }
        catch (const Ort::Exception& e) {
            std::cerr << "[StemSeparator] Inference session run failed: " << e.what() << "\n";
            return false;
        }

        if (progressCallback) {
            double prog = static_cast<double>(offset_frame + chunk_len_frame) / num_frames;
            progressCallback(std::min(1.0, prog));
        }

        if (offset_frame + W_frame >= num_frames) {
            break;
        }
    }

    // Normalize model output spectrogram by overlap weights (with cache-friendly loop order)
    for (int c = 0; c < 4; ++c) {
        for (int b = 0; b < num_bins; ++b) {
            size_t base_idx = (c * num_bins + b) * num_frames;
            for (size_t f = 0; f < num_frames; ++f) {
                float w = model_output_weights[f];
                if (w > 1e-5f) {
                    model_output_spectrogram[base_idx + f] /= w;
                }
            }
        }
    }

    // Determine target stems based on model type (Vocals vs Instrumental/Backing)
    bool isInstrumentalModel = m_modelName.toLower().contains("inst") || m_modelName.toLower().contains("backing") || m_modelName.toLower().contains("kara");

    std::vector<float> vocals_spectrogram(4 * num_bins * num_frames);
    std::vector<float> instrumental_spectrogram(4 * num_bins * num_frames);

    if (isInstrumentalModel) {
        // Model output is instrumental; vocals = input - instrumental
        std::memcpy(instrumental_spectrogram.data(), model_output_spectrogram.data(), model_output_spectrogram.size() * sizeof(float));
        for (size_t i = 0; i < spectrogram.size(); ++i) {
            vocals_spectrogram[i] = spectrogram[i] - instrumental_spectrogram[i];
        }
    } else {
        // Model output is vocals; instrumental = input - vocals
        std::memcpy(vocals_spectrogram.data(), model_output_spectrogram.data(), model_output_spectrogram.size() * sizeof(float));
        for (size_t i = 0; i < spectrogram.size(); ++i) {
            instrumental_spectrogram[i] = spectrogram[i] - vocals_spectrogram[i];
        }
    }

    // ─────────────────────────────────────────────────────────────────────────────
    // ISTFT SETUP AND WAVEFORM RECONSTRUCTION
    // ─────────────────────────────────────────────────────────────────────────────

    // Pre-calculate synthesis window normalization weights (WOLA)
    std::vector<float> window_sum(padded_length, 0.0f);
    for (size_t f = 0; f < num_frames; ++f) {
        size_t start_idx = f * hop_size;
        for (int i = 0; i < n_fft; ++i) {
            float w = hann[i];
            window_sum[start_idx + i] += w * w;
        }
    }

    // Pre-allocate synthesis loop temporary buffers to avoid another 80,000+ heap allocations
    std::vector<AVComplexFloat> v_left_complex(n_fft / 2 + 1);
    std::vector<float> v_left_frame(n_fft);
    std::vector<AVComplexFloat> v_right_complex(n_fft / 2 + 1);
    std::vector<float> v_right_frame(n_fft);

    // 1. Reconstruct Vocals Waveform
    std::vector<float> vocals_left_padded(padded_length, 0.0f);
    std::vector<float> vocals_right_padded(padded_length, 0.0f);
    for (size_t f = 0; f < num_frames; ++f) {
        size_t start_idx = f * hop_size;

        // Reconstruct Left Vocals Frame
        for (int b = 0; b < num_bins; ++b) {
            v_left_complex[b].re = vocals_spectrogram[(0 * num_bins + b) * num_frames + f];
            v_left_complex[b].im = vocals_spectrogram[(1 * num_bins + b) * num_frames + f];
        }
        v_left_complex[num_bins].re = 0.0f;
        v_left_complex[num_bins].im = 0.0f;
        rdft_inverse(v_left_frame.data(), v_left_complex.data(), n_fft);

        // Reconstruct Right Vocals Frame
        for (int b = 0; b < num_bins; ++b) {
            v_right_complex[b].re = vocals_spectrogram[(2 * num_bins + b) * num_frames + f];
            v_right_complex[b].im = vocals_spectrogram[(3 * num_bins + b) * num_frames + f];
        }
        v_right_complex[num_bins].re = 0.0f;
        v_right_complex[num_bins].im = 0.0f;
        rdft_inverse(v_right_frame.data(), v_right_complex.data(), n_fft);

        // Accumulate overlap-add
        for (int i = 0; i < n_fft; ++i) {
            vocals_left_padded[start_idx + i] += v_left_frame[i] * hann[i];
            vocals_right_padded[start_idx + i] += v_right_frame[i] * hann[i];
        }
    }

    // Reuse pre-allocated complex buffers for instrumental reconstruction to avoid reallocation
    std::vector<AVComplexFloat> i_left_complex(n_fft / 2 + 1);
    std::vector<float> i_left_frame(n_fft);
    std::vector<AVComplexFloat> i_right_complex(n_fft / 2 + 1);
    std::vector<float> i_right_frame(n_fft);

    // 2. Reconstruct Instrumental Waveform
    std::vector<float> inst_left_padded(padded_length, 0.0f);
    std::vector<float> inst_right_padded(padded_length, 0.0f);
    for (size_t f = 0; f < num_frames; ++f) {
        size_t start_idx = f * hop_size;

        // Reconstruct Left Instrumental Frame
        for (int b = 0; b < num_bins; ++b) {
            i_left_complex[b].re = instrumental_spectrogram[(0 * num_bins + b) * num_frames + f];
            i_left_complex[b].im = instrumental_spectrogram[(1 * num_bins + b) * num_frames + f];
        }
        i_left_complex[num_bins].re = 0.0f;
        i_left_complex[num_bins].im = 0.0f;
        rdft_inverse(i_left_frame.data(), i_left_complex.data(), n_fft);

        // Reconstruct Right Instrumental Frame
        for (int b = 0; b < num_bins; ++b) {
            i_right_complex[b].re = instrumental_spectrogram[(2 * num_bins + b) * num_frames + f];
            i_right_complex[b].im = instrumental_spectrogram[(3 * num_bins + b) * num_frames + f];
        }
        i_right_complex[num_bins].re = 0.0f;
        i_right_complex[num_bins].im = 0.0f;
        rdft_inverse(i_right_frame.data(), i_right_complex.data(), n_fft);

        // Accumulate overlap-add
        for (int i = 0; i < n_fft; ++i) {
            inst_left_padded[start_idx + i] += i_left_frame[i] * hann[i];
            inst_right_padded[start_idx + i] += i_right_frame[i] * hann[i];
        }
    }

    // Interleave, normalize and trim padding
    std::vector<float> vocalsAccumulated(num_samples * audioChannels, 0.0f);
    std::vector<float> instAccumulated(num_samples * audioChannels, 0.0f);
    for (size_t i = 0; i < num_samples; ++i) {
        size_t idx = i + pad_size;
        float w = window_sum[idx];
        float scale = (w > 1e-5f) ? (1.0f / w) : 1.0f;

        vocalsAccumulated[i * 2 + 0] = vocals_left_padded[idx] * scale;
        vocalsAccumulated[i * 2 + 1] = vocals_right_padded[idx] * scale;

        instAccumulated[i * 2 + 0] = inst_left_padded[idx] * scale;
        instAccumulated[i * 2 + 1] = inst_right_padded[idx] * scale;
    }

    // Upsample results from model target rate back to standard 48kHz (stereo)
    std::vector<float> vocals48k = StemResampler::resample(vocalsAccumulated, modelRate, 48000, audioChannels);
    std::vector<float> inst48k = StemResampler::resample(instAccumulated, modelRate, 48000, audioChannels);

    if (vocals48k.empty() || inst48k.empty()) {
        std::cerr << "[StemSeparator] Error: Upsampling failed.\n";
        return false;
    }

    // Write WAV files with correct stem mappings
    if (!writeWavFile(vocalsPath, vocals48k, 48000)) return false;
    if (!writeWavFile(instrumentalPath, inst48k, 48000)) return false;

    std::cout << "[StemSeparator] Completed separating stems. Outputs written to:" << std::endl
              << "Vocals: " << vocalsPath.toStdString() << std::endl
              << "Instrumental: " << instrumentalPath.toStdString() << std::endl;
        return true;
    } catch (const Ort::Exception& e) {
        std::cerr << "[StemSeparator] ONNX Runtime Error during separation: " << e.what() << std::endl;
        return false;
    } catch (const std::exception& e) {
        std::cerr << "[StemSeparator] Standard Error during separation: " << e.what() << std::endl;
        return false;
    }
}

bool StemSeparator::writeWavFile(const QString& filePath, const std::vector<float>& samples, int sampleRate) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        std::cerr << "[StemSeparator] Error: Could not open WAV file for writing: " << filePath.toStdString() << "\n";
        return false;
    }

    // Write WAV header
    // RIFF header
    file.write("RIFF", 4);
    quint32 fileSize = 36 + samples.size() * sizeof(int16_t);
    file.write(reinterpret_cast<const char*>(&fileSize), 4);
    file.write("WAVE", 4);

    // fmt subchunk
    file.write("fmt ", 4);
    quint32 subchunk1Size = 16;
    file.write(reinterpret_cast<const char*>(&subchunk1Size), 4);
    quint16 audioFormat = 1; // PCM
    file.write(reinterpret_cast<const char*>(&audioFormat), 2);
    quint16 numChannels = 2; // Stereo
    file.write(reinterpret_cast<const char*>(&numChannels), 2);
    quint32 sRate = sampleRate;
    file.write(reinterpret_cast<const char*>(&sRate), 4);
    quint32 byteRate = sampleRate * 2 * sizeof(int16_t);
    file.write(reinterpret_cast<const char*>(&byteRate), 4);
    quint16 blockAlign = 2 * sizeof(int16_t);
    file.write(reinterpret_cast<const char*>(&blockAlign), 2);
    quint16 bitsPerSample = 16;
    file.write(reinterpret_cast<const char*>(&bitsPerSample), 2);

    // data subchunk
    file.write("data", 4);
    quint32 subchunk2Size = samples.size() * sizeof(int16_t);
    file.write(reinterpret_cast<const char*>(&subchunk2Size), 4);

    // Write samples converted to 16-bit PCM
    std::vector<int16_t> pcmSamples(samples.size());
    for (size_t i = 0; i < samples.size(); ++i) {
        float s = std::clamp(samples[i], -1.0f, 1.0f);
        pcmSamples[i] = static_cast<int16_t>(s * 32767.0f);
    }
    file.write(reinterpret_cast<const char*>(pcmSamples.data()), pcmSamples.size() * sizeof(int16_t));
    return true;
}

} // namespace ncktv
