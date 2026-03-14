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

// Helper to find Python interpreter
static QString findPython() {
    QString appDir = QCoreApplication::applicationDirPath();
    // 1. Bundled portable (PRIORITY)
    QString p = QDir::cleanPath(appDir + "/python_embed/python.exe");
    if (QFile::exists(p)) return p;
    
    // 2. Local dev path
    p = QDir::cleanPath(QDir::currentPath() + "/python_embed/python.exe");
    if (QFile::exists(p)) return p;

    // 3. User specified D: drive path
    p = "D:/Program Files/Python/python.exe";
    if (QFile::exists(p)) return p;
    
    // 4. Local venv
    p = QDir::cleanPath(QDir::currentPath() + "/venv/Scripts/python.exe");
    if (QFile::exists(p)) return p;
    // 5. System
    return "python";
}

void VocalSeparatorWorker::startSeparation(const QString& audioPath,
                                           const QString& modelName,
                                           const QString& outputDir)
{
    emit progress(5, "Initializing Python bridge for separation...");

    QString pythonPath = findPython();
    QString bridgePath = QDir::cleanPath(QCoreApplication::applicationDirPath() + "/python_bridge.py");
    // Fallback to current dir if not in app dir (for dev)
    if (!QFile::exists(bridgePath)) {
        bridgePath = QDir::current().filePath("python_bridge.py");
    }
    QProcess* proc = new QProcess(this);

    // Ensure output directory exists
    QDir().mkpath(outputDir);

    // Accumulate stdout/stderr for the final checks
    QString* fullStdOut = new QString();
    QString* fullStdErr = new QString();

    // Read stderr incrementally to show live progress (e.g. downloading models, GPU status)
    connect(proc, &QProcess::readyReadStandardError, this, [this, proc, fullStdErr]() {
        QByteArray chunk = proc->readAllStandardError();
        fullStdErr->append(QString::fromUtf8(chunk));
        
        QString line = QString::fromUtf8(chunk).trimmed();
        if (!line.isEmpty()) {
            QString lastLine = line.split('\n').last().trimmed();
            emit progress(50, "Python: " + lastLine);
        }
    });

    // Also read stdout incrementally just in case
    connect(proc, &QProcess::readyReadStandardOutput, this, [this, proc, fullStdOut]() {
        QByteArray chunk = proc->readAllStandardOutput();
        fullStdOut->append(QString::fromUtf8(chunk));

        QString line = QString::fromUtf8(chunk).trimmed();
        if (!line.isEmpty()) {
            QString lastLine = line.split('\n').last().trimmed();
            if (!lastLine.startsWith("{") && !lastLine.startsWith("}")) { // Ignore JSON
                emit progress(50, lastLine);
            }
        }
    });

    connect(proc, &QProcess::finished, this, [this, proc, audioPath, outputDir, fullStdOut, fullStdErr](int exitCode) {
        if (exitCode == 0) {
            QString out = *fullStdOut;
            qDebug() << "VocalSeparatorWorker: Python output:" << out;
            
            QString instrumentalPath;
            QString vocalsPath;

            try {
                // Parse the JSON output: {"files": ["file1.wav", "file2.wav"]}
                auto j = nlohmann::json::parse(out.toStdString());
                if (j.contains("files") && j["files"].is_array()) {
                    for (const auto& f : j["files"]) {
                        QString fileName = QString::fromStdString(f.get<std::string>());
                        QString fullPath = QDir(outputDir).absoluteFilePath(fileName);
                        
                        // audio-separator outputs something like OriginalName_(Vocals)_ModelName.wav
                        if (fileName.contains("(Vocals)", Qt::CaseInsensitive) || fileName.contains("vocals", Qt::CaseInsensitive)) {
                            vocalsPath = fullPath;
                        } else if (fileName.contains("(Instrumental)", Qt::CaseInsensitive) || fileName.contains("instrumental", Qt::CaseInsensitive)) {
                            instrumentalPath = fullPath;
                        }
                    }
                }
            } catch (const std::exception& e) {
                qDebug() << "VocalSeparatorWorker: Failed to parse Python JSON output:" << e.what();
            }

            // Verify files exist
            if (!instrumentalPath.isEmpty() && !vocalsPath.isEmpty() && 
                QFile::exists(instrumentalPath) && QFile::exists(vocalsPath)) {
                emit progress(100, "Separation complete!");
                emit separationComplete(instrumentalPath, vocalsPath);
            } else {
                emit error("Separation seemed to succeed but output files were not found.\nCheck " + outputDir);
            }
        } else {
            QString err = *fullStdErr;
            if (err.isEmpty()) err = "Process crashed or 'audio-separator' not found in Python environment.";
            emit error(QString("Separation failed (Exit %1):\n%2").arg(exitCode).arg(err));
        }
        delete fullStdOut;
        delete fullStdErr;
        proc->deleteLater();
    });

    // Arguments: python_bridge.py separate <file> <model> <outdir>
    QStringList args = {bridgePath, "separate", audioPath, modelName, outputDir};

    qDebug() << "VocalSeparatorWorker: launching" << pythonPath << args.join(" ");
    
    // Add bundled FFmpeg to PATH so audio-separator can find it
    auto env = QProcessEnvironment::systemEnvironment();
    QString ff = findFFmpeg();
    if (QFile::exists(ff)) {
        QString ffDir = QFileInfo(ff).absolutePath();
        QString path = env.value("PATH");
        env.insert("PATH", QDir::toNativeSeparators(ffDir) + ";" + path);
    }
    proc->setProcessEnvironment(env);
    
    proc->start(pythonPath, args);
}

} // namespace ncktv
