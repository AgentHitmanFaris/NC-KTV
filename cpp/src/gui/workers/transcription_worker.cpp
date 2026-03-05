#include "transcription_worker.h"
#include <QProcess>
#include <QFile>
#include <QFileInfo>
#include <QDir>
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
    
    // Use the same environment as the current process but force Python into UTF-8 mode
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("PYTHONIOENCODING", "utf-8");
    env.insert("PYTHONUTF8", "1");
    proc->setProcessEnvironment(env);

    // Output dir mapping
    QFileInfo fi(audioPath);
    QString outDir = "temp/transcription";
    QDir().mkpath(outDir);
    QString outJsonPath = outDir + "/" + fi.baseName() + ".json";
    QFile::remove(outJsonPath);

    connect(proc, &QProcess::finished, this, [this, proc, outJsonPath](int exitCode) {
        if (exitCode == 0 && QFile::exists(outJsonPath)) {
            QFile jFile(outJsonPath);
            if (jFile.open(QIODevice::ReadOnly)) {
                QString result = QString::fromUtf8(jFile.readAll());
                emit transcriptionComplete(result);
            } else {
                emit error("Could not open transcription JSON file");
            }
        } else {
            QString err = QString::fromUtf8(proc->readAllStandardError());
            emit error(QString("Transcription failed (Exit %1): %2").arg(exitCode).arg(err));
        }
        proc->deleteLater();
    });

    QStringList args = {"-m", "whisper", audioPath,
                        "--model", model,
                        "--output_format", "json",
                        "--output_dir", outDir,
                        "--word_timestamps", "True"};

    if (language != "auto" && language != "Auto" && !language.isEmpty()) {
        args << "--language" << language;
    }

    proc->start(pythonPath, args);
}
} // namespace ncktv
