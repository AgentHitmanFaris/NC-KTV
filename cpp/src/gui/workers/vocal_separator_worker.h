#pragma once
#include <QObject>
#include <QString>

namespace ncktv {

class VocalSeparatorWorker : public QObject {
    Q_OBJECT

public:
    explicit VocalSeparatorWorker(QObject* parent = nullptr);

    /**
     * @brief startSeparation
     * @param audioPath   Path to the input audio file
     * @param modelName   UVR model filename (e.g., UVR_MDXNET_KARA_2.onnx)
     * @param outputDir   Where to save separated stems
     */
    void startSeparation(const QString& audioPath, 
                         const QString& modelName = "UVR_MDXNET_KARA_2.onnx",
                         const QString& outputDir = "output");

signals:
    void progress(int percent, const QString& message);
    void separationComplete(const QString& instrumentalPath, const QString& vocalsPath);
    void error(const QString& errorMessage);

private:
};

} // namespace ncktv
