#pragma once
#include <QObject>
#include <QThread>
#include <vector>
#include "../../core/audio/ncktv_whisper_engine.hpp"

namespace ncktv {

class TranscriptionWorker : public QObject {
    Q_OBJECT

public:
    explicit TranscriptionWorker(QObject* parent = nullptr);

public slots:
    void startTranscription(const QString& audioPath,
                            const QString& model    = "base",
                            const QString& language = "auto");

signals:
    void progressUpdated(const QString& message);
    void transcriptionComplete(const QString& resultJson);
    void error(const QString& errorMessage);

private:
    // Helper used when the native engine path is active (MSVC build)
    QString serializeSegmentsToJson(const std::vector<ai::TranscribedSegment>& segments);
};

} // namespace ncktv
