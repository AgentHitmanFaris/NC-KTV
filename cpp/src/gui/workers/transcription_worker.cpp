#include "transcription_worker.h"
#include <QProcess>
namespace ncktv {
TranscriptionWorker::TranscriptionWorker(QObject* parent) : QObject(parent) {}
void TranscriptionWorker::startTranscription(const QString& audioPath,
    const QString& model, const QString& language) {
    emit progress(5, "Starting transcription...");
    QProcess* proc = new QProcess(this);
    connect(proc, &QProcess::finished, this, [this, proc](int exitCode) {
        if (exitCode == 0) {
            emit transcriptionComplete(proc->readAllStandardOutput());
        } else {
            emit error("Transcription failed: " + proc->readAllStandardError());
        }
        proc->deleteLater();
    });
    proc->start("python", {"-m", "whisper", audioPath,
        "--model", model, "--language", language, "--output_format", "json"});
}
} // namespace ncktv
