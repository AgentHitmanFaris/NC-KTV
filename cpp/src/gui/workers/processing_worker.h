#pragma once
#include <QObject>
namespace ncktv {
class ProcessingWorker : public QObject {
    Q_OBJECT
public:
    explicit ProcessingWorker(QObject* parent = nullptr);
    void startProcessing(const QString& inputPath, const QString& outputDir);
signals:
    void progress(int percent, const QString& message);
    void processingComplete(const QString& resultPath);
    void error(const QString& errorMessage);
};
} // namespace ncktv
