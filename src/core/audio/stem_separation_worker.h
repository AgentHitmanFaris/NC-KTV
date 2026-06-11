#pragma once

#include <QThread>
#include <QString>
#include <vector>
#include <memory>

namespace ncktv {

class StemSeparationWorker : public QThread {
    Q_OBJECT
public:
    StemSeparationWorker(const QString& clipId,
                         const std::vector<float>& inputSamples,
                         const QString& vocalsPath,
                         const QString& instrumentalPath,
                         const QString& modelPath = QString(),
                         QObject* parent = nullptr);
    ~StemSeparationWorker() override;

protected:
    void run() override;

signals:
    void progressUpdated(double fraction);
    void separationCompleted(const QString& clipId, const QString& vocalsPath, const QString& instrumentalPath);
    void separationFailed(const QString& clipId, const QString& errorMessage);

private:
    QString m_clipId;
    std::vector<float> m_inputSamples;
    QString m_vocalsPath;
    QString m_instrumentalPath;
    QString m_modelPath;
};

} // namespace ncktv
