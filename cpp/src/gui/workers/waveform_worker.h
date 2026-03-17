#pragma once
#include <QObject>
#include <QString>
#include <QVector>

namespace ncktv {

class WaveformWorker : public QObject {
    Q_OBJECT

public:
    explicit WaveformWorker(QObject* parent = nullptr);

public slots:
    void generate(const QString& filePath);

signals:
    void waveformReady(const QVector<float>& minData, const QVector<float>& maxData, double sampleRate, int samplesPerPixel);
    void progress(int percent);
    void error(const QString& message);
};

} // namespace ncktv
