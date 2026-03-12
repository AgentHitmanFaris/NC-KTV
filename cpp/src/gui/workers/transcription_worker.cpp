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

TranscriptionWorker::TranscriptionWorker(QObject* parent) : QObject(parent) {}

void TranscriptionWorker::startTranscription(const QString& audioPath,
                                             const QString& model,
                                             const QString& language)
{
    // ── Locate the embedded Python interpreter ────────────────────────────────
    QString appDir   = QCoreApplication::applicationDirPath();
    QString pyExe    = appDir + "/../python_embed/python.exe";
    // Normalise path separators
    pyExe = QDir::toNativeSeparators(QDir::cleanPath(pyExe));

    if (!QFile::exists(pyExe)) {
        // Fallback: system Python
        pyExe = "python";
    }

    // ── Build the whisper command ─────────────────────────────────────────────
    // Usage: python -m whisper <audio> --model <model> --language <lang>
    //        --output_format json --output_dir <tmpdir>
    QString tmpDir   = QDir::tempPath() + "/ncktv_whisper";
    QDir().mkpath(tmpDir);

    QString modelArg = model.isEmpty() ? "base" : model;
    QString langArg  = (language.isEmpty() || language == "Auto") ? "auto" : language;

    QStringList args = {
        "-m", "whisper",
        audioPath,
        "--model",         modelArg,
        "--language",      langArg,
        "--output_format", "json",
        "--output_dir",    tmpDir
    };

    qDebug() << "TranscriptionWorker: Launching Python whisper:" << pyExe << args;
    emit progressUpdated("Starting AI transcription (Python whisper)...");

    // ── Run process synchronously (worker lives on a QThread) ─────────────────
    QProcess proc;
    proc.setProgram(pyExe);
    proc.setArguments(args);
    proc.setProcessChannelMode(QProcess::MergedChannels);
    proc.start();

    if (!proc.waitForStarted(5000)) {
        emit error("Could not start Python process.\nMake sure Python + whisper are installed.");
        return;
    }

    // Stream progress lines to UI
    while (proc.state() != QProcess::NotRunning) {
        proc.waitForReadyRead(500);
        QString out = QString::fromUtf8(proc.readAll()).trimmed();
        if (!out.isEmpty()) {
            emit progressUpdated(out.left(120));   // truncate very long lines
        }
    }
    proc.waitForFinished(10 * 60 * 1000);  // 10 min max

    if (proc.exitCode() != 0) {
        QString err = QString::fromUtf8(proc.readAllStandardError()).trimmed();
        emit error("Whisper process failed (exit " + QString::number(proc.exitCode()) + "):\n" + err);
        return;
    }

    // ── Read the output JSON ──────────────────────────────────────────────────
    // whisper names the file after the audio basename
    QFileInfo fi(audioPath);
    QString jsonPath = tmpDir + "/" + fi.completeBaseName() + ".json";

    if (!QFile::exists(jsonPath)) {
        // Scan tmpDir for any .json file
        QDir dir(tmpDir);
        QStringList jsonFiles = dir.entryList({"*.json"}, QDir::Files);
        if (!jsonFiles.isEmpty())
            jsonPath = tmpDir + "/" + jsonFiles.last();
    }

    if (!QFile::exists(jsonPath)) {
        emit error("Whisper finished but produced no JSON output.\nExpected: " + jsonPath);
        return;
    }

    QFile f(jsonPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit error("Could not read whisper JSON output: " + jsonPath);
        return;
    }
    QString resultJson = QString::fromUtf8(f.readAll());
    f.close();

    emit progressUpdated("Transcription complete!");
    emit transcriptionComplete(resultJson);
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
