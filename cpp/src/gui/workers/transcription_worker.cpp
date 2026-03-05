#include "transcription_worker.h"
#include <QProcess>
#include <QFile>
#include <QDebug>
namespace ncktv {
TranscriptionWorker::TranscriptionWorker(QObject* parent) : QObject(parent) {}
void TranscriptionWorker::startTranscription(const QString& audioPath,
    const QString& model, const QString& language) {
    QString pythonPath = "D:/Document/NC-KTV/python_embed/python.exe";
    if (!QFile::exists(pythonPath)) {
        emit error("Python environment not found at " + pythonPath);
        return;
    }

    QProcess* proc = new QProcess(this);
    
    // Use the same environment as the current process but potentially modify PATH
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    proc->setProcessEnvironment(env);

    connect(proc, &QProcess::finished, this, [this, proc](int exitCode) {
        if (exitCode == 0) {
            emit transcriptionComplete(QString::fromUtf8(proc->readAllStandardOutput()));
        } else {
            QString err = QString::fromUtf8(proc->readAllStandardError());
            emit error(QString("Transcription failed (Exit %1): %2").arg(exitCode).arg(err));
        }
        proc->deleteLater();
    });

    QStringList args = {"-m", "whisper", audioPath,
                        "--model", model,
                        "--output_format", "json"};

    if (language != "auto" && !language.isEmpty()) {
        args << "--language" << language;
    }

    proc->start(pythonPath, args);
}
} // namespace ncktv
