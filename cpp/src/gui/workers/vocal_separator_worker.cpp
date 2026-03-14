/**
 * @file vocal_separator_worker.cpp
 * @brief Vocal Separation worker — native C++ ONNX Runtime + real MDX-Net DSP.
 *
 * Full pipeline (MinGW + MSVC):
 *   1. Decode audio to raw 32-bit float PCM via bundled FFmpeg.
 *   2. STFT (Hann window, n_fft from mdx_model_data.json).
 *   3. ONNX inference (UVR_MDXNET_KARA_2.onnx / UVR-MDX-NET-Inst_HQ_3.onnx).
 *   4. Mask output → vocals mask + instrumental mask.
 *   5. ISTFT → time-domain float audio.
 *   6. Write 16-bit PCM WAV.
 */

#include "vocal_separator_worker.h"
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QProcess>
#include <QCryptographicHash>
#include <memory>
#include <iostream>
#include <fstream>

#if NCKTV_HAS_ONNX
#include <QAudioDecoder>
#include <QAudioFormat>
#include <QEventLoop>
#include "../../core/audio/ncktv_onnx_separator.hpp"
#include "../../core/audio/ncktv_mdx_dsp.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <algorithm>
#include <cstring>
#endif

namespace ncktv {

VocalSeparatorWorker::VocalSeparatorWorker(QObject* parent) : QObject(parent) {}

// ─────────────────────────────────────────────────────────────────────────────
#if NCKTV_HAS_ONNX

// ── Identify the FFmpeg executable bundled next to the app ───────────────────
static QString findFFmpeg() {
    QString appDir = QCoreApplication::applicationDirPath();
    QString ff = QDir::cleanPath(appDir + "/ffmpeg/ffmpeg.exe");
    if (QFile::exists(ff)) return ff;
    ff = QDir::cleanPath(appDir + "/../ffmpeg/ffmpeg.exe");
    if (QFile::exists(ff)) return ff;
    return "ffmpeg";
}

// ── Decode any audio file to interleaved float32 PCM (stereo 44100 Hz) ───────
static std::vector<float> decodeAudioToFloat(const QString& path,
                                              int& outSampleRate,
                                              int& outChannels,
                                              int& outNumSamples,
                                              const std::function<void(const QString&)>& progress)
{
    QString ffmpeg = findFFmpeg();
    outSampleRate = 44100;
    outChannels   = 2;

    // Pipe raw f32le audio from FFmpeg
    QProcess proc;
    proc.setProgram(ffmpeg);
    proc.setArguments({
        "-y", "-i", path,
        "-ar", "44100",
        "-ac", "2",
        "-f",  "f32le",
        "-vn",
        "pipe:1"
    });
    proc.setProcessChannelMode(QProcess::SeparateChannels);
    proc.start();

    if (!proc.waitForStarted(5000)) {
        progress("ERROR: Could not start FFmpeg for audio decode.");
        return {};
    }

    QByteArray raw;
    while (proc.state() != QProcess::NotRunning) {
        proc.waitForReadyRead(200);
        raw += proc.readAllStandardOutput();
    }
    proc.waitForFinished(-1);

    if (raw.isEmpty()) {
        progress("ERROR: FFmpeg produced no audio data.");
        return {};
    }

    size_t nFloats = raw.size() / sizeof(float);
    std::vector<float> audio(nFloats);
    std::memcpy(audio.data(), raw.constData(), nFloats * sizeof(float));

    outNumSamples = (int)(nFloats / outChannels);
    progress(QString("Decoded %1 samples at %2 Hz").arg(outNumSamples).arg(outSampleRate));
    return audio;
}

// ── Load MDX model params from mdx_model_data.json by model MD5 ──────────────
static dsp::MdxParams loadMdxParams(const QString& modelPath) {
    dsp::MdxParams p;  // sensible defaults

    // Compute MD5 of the model file
    QFile f(modelPath);
    if (!f.open(QIODevice::ReadOnly)) return p;
    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(&f);
    QString md5 = hash.result().toHex();
    f.close();

    qDebug() << "VocalSeparator: model MD5 =" << md5;

    // Look for mdx_model_data.json beside the model file
    QString jsonPath = QFileInfo(modelPath).dir().filePath("mdx_model_data.json");
    if (!QFile::exists(jsonPath)) return p;

    std::ifstream ifs(jsonPath.toStdString());
    if (!ifs.is_open()) return p;

    nlohmann::json j;
    try { ifs >> j; } catch (...) { return p; }

    std::string key = md5.toStdString();
    if (j.contains(key)) {
        auto& m = j[key];
        if (m.contains("mdx_n_fft_scale_set")) p.n_fft        = m["mdx_n_fft_scale_set"].get<int>();
        if (m.contains("mdx_dim_f_set"))       p.dim_f        = m["mdx_dim_f_set"].get<int>();
        if (m.contains("mdx_dim_t_set"))       p.dim_t        = m["mdx_dim_t_set"].get<int>();
        if (m.contains("compensate"))          p.compensate   = m["compensate"].get<float>();
        if (m.contains("primary_stem"))        p.primary_stem = m["primary_stem"].get<std::string>();
        qDebug() << "MDX params: n_fft=" << p.n_fft
                 << " dim_f=" << p.dim_f
                 << " dim_t=" << p.dim_t
                 << " primary_stem=" << QString::fromStdString(p.primary_stem);
    } else {
        qDebug() << "VocalSeparator: MD5 not in mdx_model_data.json, using defaults";
    }
    return p;
}

#endif // NCKTV_HAS_ONNX

// ─────────────────────────────────────────────────────────────────────────────

void VocalSeparatorWorker::startSeparation(const QString& audioPath,
                                           const QString& modelName,
                                           const QString& outputDir)
{
#if NCKTV_HAS_ONNX
    // ── 1. Resolve model path ─────────────────────────────────────────────────
    QString appDir = QCoreApplication::applicationDirPath();
    QString localModelsDir   = appDir + "/models/uvr";
    QString cwdModelsDir     = QDir::currentPath() + "/models/uvr";
    QString appDataModelsDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/models/uvr";

    QString actualModelName = modelName;

    // PyTorch .pth files are VR-arch models — they require the Python UVR stack
    // and cannot be loaded by ONNX Runtime. Emit a clear error instead of
    // silently renaming to .onnx (which would just fail with a cryptic message).
    if (actualModelName.endsWith(".pth", Qt::CaseInsensitive)) {
        emit error(
            "\"" + actualModelName + "\" is a PyTorch VR-arch model (.pth).\n"
            "The native ONNX pipeline only supports MDX-Net ONNX models.\n\n"
            "Please select one of the ONNX models instead:\n"
            "  • UVR_MDXNET_KARA_2.onnx\n"
            "  • UVR-MDX-NET-Inst_HQ_3.onnx"
        );
        return;
    }

    if (!actualModelName.endsWith(".onnx", Qt::CaseInsensitive))
        actualModelName += ".onnx";

    // Search in: app dir → cwd → AppData
    QString modelPath = localModelsDir + "/" + actualModelName;
    if (!QFile::exists(modelPath))
        modelPath = cwdModelsDir + "/" + actualModelName;
    if (!QFile::exists(modelPath))
        modelPath = appDataModelsDir + "/" + actualModelName;

    if (!QFile::exists(modelPath)) {
        emit error("ONNX Model not found: " + actualModelName +
                   "\nSearched in:\n  " + localModelsDir +
                   "\n  " + cwdModelsDir +
                   "\n  " + appDataModelsDir +
                   "\n\nPlease place the .onnx UVR model in the models/uvr folder.");
        return;
    }

    // ── 2. Load MDX model parameters ─────────────────────────────────────────
    dsp::MdxParams mdxP = loadMdxParams(modelPath);
    mdxP.channels    = 2;
    mdxP.sample_rate = 44100;

    QDir().mkpath(outputDir);
    QString baseName = QFileInfo(audioPath).completeBaseName();
    QString instrumentalPath = QDir(outputDir).absoluteFilePath(baseName + "_(Instrumental).wav");
    QString vocalsPath       = QDir(outputDir).absoluteFilePath(baseName + "_(Vocals).wav");

    // ── 3. ONNX session ───────────────────────────────────────────────────────
    emit progress(5, "Initializing ONNX Runtime...");
    std::unique_ptr<ncktv::ai::VocalSeparator> separator;
    try {
        separator = std::make_unique<ncktv::ai::VocalSeparator>(modelPath.toStdString());
    } catch (const std::exception& e) {
        emit error(QString("Failed to initialize ONNX model: %1").arg(e.what()));
        return;
    }

    // ── 4. Decode Audio ───────────────────────────────────────────────────────
    emit progress(10, "Decoding audio (FFmpeg)...");
    qDebug() << "VocalSeparator: starting audio decode of" << audioPath;
    int sampleRate = 0, channels = 0, numSamples = 0;
    auto progressFn = [this](const QString& msg){ emit progress(12, msg); };
    std::vector<float> audio = decodeAudioToFloat(audioPath, sampleRate, channels, numSamples, progressFn);

    if (audio.empty()) {
        emit error("Failed to decode audio file.\nMake sure ffmpeg is available.");
        return;
    }

    // ── 5. STFT ───────────────────────────────────────────────────────────────
    emit progress(20, QString("Computing STFT (n_fft=%1)...").arg(mdxP.n_fft));
    qDebug() << "VocalSeparator: computing STFT...";
    dsp::StftResult mixStft = dsp::stft(audio, channels, mdxP);
    qDebug() << "STFT: frames=" << mixStft.n_frames << " freq_bins=" << mixStft.freq_bins;

    // ── 6. Process segments through ONNX ─────────────────────────────────────
    // We build TWO cumulative STFT masks — one for vocals, one for instrumental.
    dsp::StftResult vocStft = mixStft;   // will hold vocals mask result
    dsp::StftResult instStft = mixStft;  // will hold instrumental mask result

    // Zero out the data first — we'll accumulate masks
    std::fill(vocStft.data.begin(),  vocStft.data.end(),  0.f);
    std::fill(instStft.data.begin(), instStft.data.end(), 0.f);

    int seg_size  = mdxP.segment_size();
    int n_segs    = (mixStft.n_frames + seg_size - 1) / seg_size;

    // Get input/output names from ONNX model
    Ort::AllocatorWithDefaultOptions alloc;
    size_t numInputs  = separator->session_->GetInputCount();
    size_t numOutputs = separator->session_->GetOutputCount();

    std::vector<std::string> inputNameStrs(numInputs), outputNameStrs(numOutputs);
    std::vector<const char*> inputNames(numInputs), outputNames(numOutputs);
    for (size_t i = 0; i < numInputs; ++i) {
        inputNameStrs[i] = separator->session_->GetInputNameAllocated(i, alloc).get();
        inputNames[i] = inputNameStrs[i].c_str();
    }
    for (size_t i = 0; i < numOutputs; ++i) {
        outputNameStrs[i] = separator->session_->GetOutputNameAllocated(i, alloc).get();
        outputNames[i] = outputNameStrs[i].c_str();
    }

    // Input shape: [1, ch, dim_f, seg_size, 2] or [1, ch*2, dim_f, seg_size]
    // MDX-Net C++ expects [1, 4, dim_f, seg_size] (ch*2 for real+imag)
    int dim_f    = mdxP.dim_f;
    std::vector<int64_t> inputShape  = {1, (int64_t)channels * 2, (int64_t)dim_f, (int64_t)seg_size};
    std::vector<int64_t> outputShape = {1, (int64_t)channels * 2, (int64_t)dim_f, (int64_t)seg_size};

    for (int seg = 0; seg < n_segs; ++seg) {
        int pct = 20 + (int)((float)seg / n_segs * 60.f);
        emit progress(pct, QString("ONNX inference: segment %1/%2...").arg(seg+1).arg(n_segs));

        int frameStart = seg * seg_size;

        // Build [ch, dim_f, seg_size, 2] => reshape to [1, ch*2, dim_f, seg_size]
        // We build interleaved layout for the model: channels grouped as [R_L, I_L, R_R, I_R, ...]
        std::vector<float> inputData(channels * 2 * dim_f * seg_size, 0.f);

        for (int c = 0; c < channels; ++c) {
            for (int f = 0; f < dim_f; ++f) {
                for (int t = 0; t < seg_size; ++t) {
                    int frame = frameStart + t;
                    float r = 0.f, im = 0.f;
                    if (frame < mixStft.n_frames && f < mixStft.freq_bins) {
                        const float* src = mixStft.data.data() +
                                           ((size_t)c * mixStft.n_frames + frame) * mixStft.freq_bins * 2;
                        r  = src[f * 2];
                        im = src[f * 2 + 1];
                    }
                    // Layout: [1, c*2+0, f, t] = real, [1, c*2+1, f, t] = imag
                    inputData[((size_t)(c * 2 + 0) * dim_f + f) * seg_size + t] = r;
                    inputData[((size_t)(c * 2 + 1) * dim_f + f) * seg_size + t] = im;
                }
            }
        }

        auto memInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
            memInfo, inputData.data(), inputData.size(),
            inputShape.data(), inputShape.size()
        );

        auto outputs = separator->session_->Run(
            Ort::RunOptions{nullptr},
            inputNames.data(), &inputTensor, 1,
            outputNames.data(), (size_t)numOutputs
        );

        // Output: [1, ch*2, dim_f, seg_size]
        // MDX-Net outputs the predicted PRIMARY stem spectrogram directly
        // (real+imag, same layout as input). Secondary stem = mix - primary.
        const float* outPtr = outputs[0].GetTensorMutableData<float>();

        // Determine which StftResult is primary vs secondary based on model metadata
        const bool primaryIsVocals = (mdxP.primary_stem == "Vocals");

        for (int c = 0; c < channels; ++c) {
            for (int f = 0; f < dim_f && f < mixStft.freq_bins; ++f) {
                for (int t = 0; t < seg_size; ++t) {
                    int frame = frameStart + t;
                    if (frame >= mixStft.n_frames) break;

                    const float* mixSrc = mixStft.data.data() +
                                          ((size_t)c * mixStft.n_frames + frame) * mixStft.freq_bins * 2;

                    float mixR = mixSrc[f * 2];
                    float mixI = mixSrc[f * 2 + 1];

                    // Primary stem: model output scaled by compensate
                    float primR = outPtr[((size_t)(c * 2 + 0) * dim_f + f) * seg_size + t] * mdxP.compensate;
                    float primI = outPtr[((size_t)(c * 2 + 1) * dim_f + f) * seg_size + t] * mdxP.compensate;

                    // Secondary stem: residual (mix - primary)
                    float secR = mixR - primR;
                    float secI = mixI - primI;

                    // Assign to correct Stft buffers
                    float* vocDst  = vocStft.data.data()  + ((size_t)c * vocStft.n_frames  + frame) * vocStft.freq_bins  * 2;
                    float* instDst = instStft.data.data() + ((size_t)c * instStft.n_frames + frame) * instStft.freq_bins * 2;

                    if (primaryIsVocals) {
                        vocDst[f * 2]      = primR;  vocDst[f * 2 + 1]  = primI;
                        instDst[f * 2]     = secR;   instDst[f * 2 + 1] = secI;
                    } else {
                        // Model outputs instrumental as primary stem
                        instDst[f * 2]     = primR;  instDst[f * 2 + 1] = primI;
                        vocDst[f * 2]      = secR;   vocDst[f * 2 + 1]  = secI;
                    }
                }
            }
        }
    }

    // ── 7. ISTFT → WAV ────────────────────────────────────────────────────────
    emit progress(82, "Performing ISTFT for vocals...");
    auto vocAudio  = dsp::istft(vocStft,  mdxP, numSamples);

    emit progress(88, "Performing ISTFT for instrumental...");
    auto instAudio = dsp::istft(instStft, mdxP, numSamples);

    emit progress(93, "Writing output WAV files...");
    bool vocOk  = dsp::writeWav(vocalsPath.toStdString(),  vocAudio,  channels, sampleRate);
    bool instOk = dsp::writeWav(instrumentalPath.toStdString(), instAudio, channels, sampleRate);

    if (vocOk && instOk) {
        emit progress(100, "Separation complete!");
        emit separationComplete(instrumentalPath, vocalsPath);
    } else {
        emit error("Failed to write output WAV files.\nCheck disk space and output path.");
    }

#else
    emit error("Vocal Separation requires ONNX Runtime support.\nPlease use a Windows build (MSVC or MinGW) with NCKTV_HAS_ONNX=1.");
#endif
}

} // namespace ncktv
