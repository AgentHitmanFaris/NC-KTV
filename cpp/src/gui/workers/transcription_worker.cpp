/**
 * @file transcription_worker.cpp
 * @brief Routes AI transcription through the Python whisper subprocess.
 *
 * On MinGW builds, whisper.cpp cannot be compiled natively (ggml.c crash).
 * This worker launches the embedded Python interpreter with the whisper CLI
 * and emits the resulting JSON back to the UI.
 */

#include "transcription_worker.h"
#include <QProcess>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QDebug>
#include <nlohmann/json.hpp>

namespace ncktv {

// Helper to find Python interpreter
static QString findPython() {
    QString appDir = QCoreApplication::applicationDirPath();
    // 1. Bundled portable
    QString p = QDir::cleanPath(appDir + "/python_embed/python.exe");
    if (QFile::exists(p)) return p;
    // 2. Local venv
    p = QDir::cleanPath(QDir::currentPath() + "/venv/Scripts/python.exe");
    if (QFile::exists(p)) return p;
    // 3. System
    return "python";
}

TranscriptionWorker::TranscriptionWorker(QObject* parent) : QObject(parent) {}

void TranscriptionWorker::startTranscription(const QString& audioPath,
                                               const QString& model,
                                               const QString& language)
{
    emit progressUpdated("Initializing Whisper (Python bridge)...");

    QString pythonPath = findPython();
    QProcess* proc = new QProcess(this);

    connect(proc, &QProcess::finished, this, [this, proc](int exitCode) {
        if (exitCode == 0) {
            emit transcriptionComplete(QString::fromUtf8(proc->readAllStandardOutput()));
        } else {
            QString err = QString::fromUtf8(proc->readAllStandardError());
            if (err.isEmpty()) err = "Process crashed or 'whisper' not found in Python environment.";
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

    qDebug() << "TranscriptionWorker: launching" << pythonPath << args.join(" ");
    proc->start(pythonPath, args);
}

QString TranscriptionWorker::serializeSegmentsToJson(
    const std::vector<ai::TranscribedSegment>& segments)
{
    nlohmann::json j;
    j["segments"] = nlohmann::json::array();

    for (const auto& seg : segments) {
        nlohmann::json s;
        s["text"]  = seg.text;
        s["start"] = seg.start_time;
        s["end"]   = seg.end_time;

        s["words"] = nlohmann::json::array();
        for (const auto& word : seg.words) {
            nlohmann::json w;
            w["word"]        = word.text;
            w["start"]       = word.start_time;
            w["end"]         = word.end_time;
            w["probability"] = word.probability;
            s["words"].push_back(w);
        }
        j["segments"].push_back(s);
    }
    return QString::fromStdString(j.dump());
}

} // namespace ncktv
