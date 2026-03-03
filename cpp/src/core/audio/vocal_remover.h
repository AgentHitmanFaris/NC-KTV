#pragma once
/*
 * NC-KTV Core — Vocal Remover (Subprocess Bridge)
 * Phase 1: Calls Python audio-separator via QProcess
 * Future: Native ONNX Runtime integration
 */

#include <QString>
#include <QStringList>
#include <functional>
#include "config/config_manager.h"

namespace ncktv {

struct VocalSeparationResult {
    QString instrumentalPath;
    QString vocalsPath;
    QString modelUsed;
};

class VocalRemover {
public:
    explicit VocalRemover(const ConfigManager& config);

    /// List available UVR models (.pth, .onnx, .pt)
    QStringList listAvailableModels() const;

    /// Separate vocals from audio (calls Python audio-separator via QProcess)
    VocalSeparationResult separateVocals(
        const QString& audioPath,
        const QString& modelName = {},
        std::function<void(int, const QString&)> progressCallback = nullptr);

    /// Get model info
    struct ModelInfo {
        bool    exists  = false;
        QString name;
        QString type;    // "MDX-Net", "VR Architecture", "Unknown"
        double  sizeMb  = 0.0;
    };
    ModelInfo getModelInfo(const QString& modelName) const;

private:
    const ConfigManager& m_config;
};

} // namespace ncktv
