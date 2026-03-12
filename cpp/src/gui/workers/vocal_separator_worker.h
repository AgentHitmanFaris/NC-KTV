#pragma once
#include <QObject>
#include <QString>
#include <vector>
#include "../../core/audio/ncktv_onnx_separator.hpp"

namespace ncktv {

class VocalSeparatorWorker : public QObject {
    Q_OBJECT

public:
    explicit VocalSeparatorWorker(QObject* parent = nullptr);

public slots:
    void startSeparation(const QString& audioPath, 
                         const QString& modelName = "UVR_MDXNET_KARA_2.onnx",
                         const QString& outputDir = "output");

signals:
    void progress(int percent, const QString& message);
    void separationComplete(const QString& instrumentalPath, const QString& vocalsPath);
    void error(const QString& errorMessage);

private:
#if NCKTV_HAS_ONNX
    std::vector<float> extractAndPerformSTFT(const QString& audioPath, size_t expectedSize);
    bool performISTFTAndSave(const std::vector<float>& spectrogram, const QString& outputPath);
#endif
};

} // namespace ncktv
