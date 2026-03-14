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
    // 1. Bundled portable (PRIORITY)
    QString p = QDir::cleanPath(appDir + "/python_embed/python.exe");
    if (QFile::exists(p)) return p;
    
    // 2. Local dev path
    p = QDir::cleanPath(QDir::currentPath() + "/python_embed/python.exe");
    if (QFile::exists(p)) return p;

    // 3. User specified D: drive path
    p = "D:/Program Files/Python/python.exe";
    if (QFile::exists(p)) return p;
    
    // 4. Local venv
    p = QDir::cleanPath(QDir::currentPath() + "/venv/Scripts/python.exe");
    if (QFile::exists(p)) return p;
    // 5. System
    return "python";
}

TranscriptionWorker::TranscriptionWorker(QObject* parent) : QObject(parent) {}

void TranscriptionWorker::startTranscription(const QString& audioPath,
                                               const QString& model,
                                               const QString& language)
{
    emit progressUpdated("Initializing Whisper (Python bridge)...");

    QString pythonPath = findPython();
    QString bridgePath = QDir::cleanPath(QCoreApplication::applicationDirPath() + "/python_bridge.py");
    // Fallback to current dir if not in app dir (for dev)
    if (!QFile::exists(bridgePath)) {
        bridgePath = QDir::current().filePath("python_bridge.py");
    }
    QProcess* proc = new QProcess(this);

    // Accumulate stdout/stderr for the final JSON
    QString* fullStdOut = new QString();
    QString* fullStdErr = new QString();

    // Read stderr incrementally to show live progress
    connect(proc, &QProcess::readyReadStandardError, this, [this, proc, fullStdErr]() {
        QByteArray chunk = proc->readAllStandardError();
        fullStdErr->append(QString::fromUtf8(chunk));
        
        QString line = QString::fromUtf8(chunk).trimmed();
        if (!line.isEmpty()) {
            QString lastLine = line.split('\n').last().trimmed();
            emit progressUpdated("Whisper: " + lastLine);
        }
    });

    // Also read stdout incrementally just in case
    connect(proc, &QProcess::readyReadStandardOutput, this, [this, proc, fullStdOut]() {
        QByteArray chunk = proc->readAllStandardOutput();
        fullStdOut->append(QString::fromUtf8(chunk));

        QString line = QString::fromUtf8(chunk).trimmed();
        if (!line.isEmpty()) {
            QString lastLine = line.split('\n').last().trimmed();
            if (!lastLine.startsWith("{") && !lastLine.startsWith("}")) { // Ignore JSON
                emit progressUpdated(lastLine);
            }
        }
    });

    connect(proc, &QProcess::finished, this, [this, proc, fullStdOut, fullStdErr](int exitCode) {
        if (exitCode == 0) {
            emit transcriptionComplete(*fullStdOut);
        } else {
            QString err = *fullStdErr;
            if (err.isEmpty()) err = "Process crashed or 'whisper' not found in Python environment.";
            emit error(QString("Transcription failed (Exit %1):\n%2").arg(exitCode).arg(err));
        }
        delete fullStdOut;
        delete fullStdErr;
        proc->deleteLater();
    });

    QStringList args = {bridgePath, "transcribe", audioPath,
                        "--model", model};

    if (language != "auto" && !language.isEmpty()) {
        args << "--lang" << language;
    }

    qDebug() << "TranscriptionWorker: launching" << pythonPath << args.join(" ");
    
    // Add bundled FFmpeg to PATH so whisper can find it
    auto env = QProcessEnvironment::systemEnvironment();
    QString appDir = QCoreApplication::applicationDirPath();
    QString ffDir = QDir::cleanPath(appDir + "/ffmpeg");
    if (!QFile::exists(ffDir + "/ffmpeg.exe")) {
        ffDir = QDir::cleanPath(appDir + "/../ffmpeg");
    }
    if (QFile::exists(ffDir + "/ffmpeg.exe")) {
        QString path = env.value("PATH");
        env.insert("PATH", QDir::toNativeSeparators(ffDir) + ";" + path);
    }
    proc->setProcessEnvironment(env);

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
