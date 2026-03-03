/*
 * NC-KTV Core — Vocal Remover Implementation (Subprocess Bridge)
 */

#include "vocal_remover.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QProcess>
#include <stdexcept>

namespace ncktv {

VocalRemover::VocalRemover(const ConfigManager& config)
    : m_config(config)
{
}

QStringList VocalRemover::listAvailableModels() const {
    QString modelsPath = m_config.get<QString>("uvr.models_path", "models/uvr");
    QDir dir(modelsPath);
    if (!dir.exists()) return {};

    QStringList models;
    QStringList filters = {"*.pth", "*.onnx", "*.pt"};
    for (const auto& entry : dir.entryInfoList(filters, QDir::Files)) {
        models << entry.fileName();
    }
    models.sort();
    return models;
}

VocalSeparationResult VocalRemover::separateVocals(
    const QString& audioPath,
    const QString& modelName,
    std::function<void(int, const QString&)> progressCallback)
{
    if (!QFile::exists(audioPath))
        throw std::runtime_error("Audio file not found: " + audioPath.toStdString());

    QString model = modelName.isEmpty()
        ? m_config.get<QString>("uvr.default_model", "UVR_MDXNET_KARA_2.onnx")
        : modelName;

    QString outputDir = m_config.get<QString>("processing.output_dir", "output");
    QDir().mkpath(outputDir);

    if (progressCallback) progressCallback(5, "Starting vocal separation...");

    // Call Python audio-separator via QProcess
    QProcess proc;
    proc.setWorkingDirectory(QDir::currentPath());
    proc.start("python", {
        "-m", "audio_separator.separator",
        "--model_filename", model,
        "--output_dir", outputDir,
        "--output_format", "WAV",
        audioPath
    });
    proc.waitForFinished(-1);

    if (proc.exitCode() != 0)
        throw std::runtime_error("Vocal separation failed: " +
                                 proc.readAllStandardError().toStdString());

    if (progressCallback) progressCallback(100, "Separation complete");

    // Find output files
    VocalSeparationResult result;
    result.modelUsed = model;

    QDir outDir(outputDir);
    QString baseName = QFileInfo(audioPath).completeBaseName();
    for (const auto& entry : outDir.entryInfoList(QDir::Files)) {
        if (entry.fileName().contains(baseName)) {
            if (entry.fileName().contains("Instrumental", Qt::CaseInsensitive))
                result.instrumentalPath = entry.absoluteFilePath();
            else if (entry.fileName().contains("Vocals", Qt::CaseInsensitive))
                result.vocalsPath = entry.absoluteFilePath();
        }
    }

    return result;
}

VocalRemover::ModelInfo VocalRemover::getModelInfo(const QString& modelName) const {
    QString modelsPath = m_config.get<QString>("uvr.models_path", "models/uvr");
    QString fullPath = modelsPath + "/" + modelName;

    ModelInfo info;
    QFileInfo fi(fullPath);
    if (!fi.exists()) return info;

    info.exists = true;
    info.name   = modelName;
    info.sizeMb = fi.size() / (1024.0 * 1024.0);

    if (modelName.contains("MDX", Qt::CaseInsensitive))
        info.type = "MDX-Net";
    else if (modelName.contains("VR", Qt::CaseInsensitive))
        info.type = "VR Architecture";
    else
        info.type = "Unknown";

    return info;
}

} // namespace ncktv
