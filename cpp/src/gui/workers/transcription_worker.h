#pragma once
#include <QObject>
#include <QThread>
namespace ncktv {
class TranscriptionWorker : public QObject {
    Q_OBJECT

public:
    explicit TranscriptionWorker(QObject* parent = nullptr);
    void startTranscription(const QString& audioPath, const QString& model = "base",
                            const QString& language = "auto");
signals:
    void progress(int percent, const QString& message);
    void transcriptionComplete(const QString& resultJson);
    void error(const QString& errorMessage);
};
} // namespace ncktv
