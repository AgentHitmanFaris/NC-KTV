/**
 * @file transcription_worker.cpp
 * @brief Routes AI transcription through the Python bridge subprocess.
 *
 * Supports two engines:
 *   - Whisper  (legacy): attention-based word timestamps, ±500ms accuracy
 *   - WhisperX (new):    Wav2Vec2 forced alignment, ±30ms accuracy
 *
 * Also provides startAlignment() for force-aligning known lyrics to audio,
 * which skips transcription entirely and produces precise word timing from
 * pre-existing lyrics text.
 */

#include "transcription_worker.h"
#include <QProcess>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QDebug>
#include <QTemporaryFile>
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

// Returns {executable, args_prefix} for the bridge.
// In portable builds the PyInstaller bundle is used directly (no python interpreter needed).
// In dev builds the .py script is run via the python interpreter.
static std::pair<QString, QStringList> findBridge() {
    QString appDir = QCoreApplication::applicationDirPath();

    // 1. PyInstaller portable bundle (highest priority)
    QString bundleExe = QDir::cleanPath(appDir + "/python_bridge/python_bridge.exe");
    if (QFile::exists(bundleExe))
        return {bundleExe, {}};

    // 2. .py script next to the executable (dev / loose layout)
    QString pyScript = QDir::cleanPath(appDir + "/python_bridge.py");
    if (QFile::exists(pyScript))
        return {findPython(), {pyScript}};

    // 3. .py script in the working directory
    pyScript = QDir::current().filePath("python_bridge.py");
    return {findPython(), {pyScript}};
}

// Helper to set up process environment with FFmpeg in PATH
static QProcessEnvironment buildEnv() {
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
    return env;
}

TranscriptionWorker::TranscriptionWorker(QObject* parent) : QObject(parent) {
    qRegisterMetaType<TranscriptionEngine>("TranscriptionEngine");
    qRegisterMetaType<TranscriptionEngine>("ncktv::TranscriptionEngine");
}

void TranscriptionWorker::startTranscription(const QString& audioPath,
                                               const QString& model,
                                               const QString& language,
                                               TranscriptionEngine engine)
{
    QString engineLabel;
    auto [bridgeExe, bridgePrefix] = findBridge();
    QStringList args = bridgePrefix;

    if (engine == TranscriptionEngine::WhisperX) {
        engineLabel = "WhisperX";
        args << "transcribe-x" << audioPath << "--model" << model;
    } else {
        engineLabel = "Whisper";
        args << "transcribe" << audioPath << "--model" << model;
    }

    if (language != "auto" && !language.isEmpty()) {
        args << "--lang" << language;
    }

    launchPythonBridge(bridgeExe, args, engineLabel);
}

void TranscriptionWorker::startAlignment(const QString& audioPath,
                                           const QString& lyricsText,
                                           const QString& language)
{
    // Write lyrics to a temporary file for the Python bridge
    QTemporaryFile* tmpFile = new QTemporaryFile(QDir::tempPath() + "/ncktv_lyrics_XXXXXX.txt", this);
    if (!tmpFile->open()) {
        emit error("Failed to create temporary lyrics file");
        return;
    }
    tmpFile->write(lyricsText.toUtf8());
    tmpFile->flush();
    QString tmpPath = tmpFile->fileName();

    auto [bridgeExe2, bridgePrefix2] = findBridge();
    QStringList args = bridgePrefix2;
    args << "align" << audioPath << "--lyrics" << tmpPath << "--lang" << language;

    launchPythonBridge(bridgeExe2, args, "WhisperX Alignment");

    // tmpFile will be cleaned up when this worker is deleted
}

void TranscriptionWorker::launchPythonBridge(const QString& executable, const QStringList& args, const QString& engineLabel)
{
    emit progressUpdated(QString("Initializing %1 (Python bridge)...").arg(engineLabel));

    QProcess* proc = new QProcess(this);

    // Accumulate stdout/stderr for the final JSON
    QString* fullStdOut = new QString();
    QString* fullStdErr = new QString();

    // Read stderr incrementally to show live progress
    connect(proc, &QProcess::readyReadStandardError, this, [this, proc, fullStdErr, engineLabel]() {
        QByteArray chunk = proc->readAllStandardError();
        fullStdErr->append(QString::fromUtf8(chunk));
        
        QString line = QString::fromUtf8(chunk).trimmed();
        if (!line.isEmpty()) {
            QString lastLine = line.split('\n').last().trimmed();
            emit progressUpdated(engineLabel + ": " + lastLine);
        }
    });

    // Read stdout incrementally
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

    connect(proc, &QProcess::finished, this, [this, proc, fullStdOut, fullStdErr, engineLabel](int exitCode) {
        if (exitCode == 0) {
            emit transcriptionComplete(*fullStdOut);
        } else {
            QString err = *fullStdErr;
            if (err.isEmpty()) err = engineLabel + " process crashed or dependencies not found.";
            emit error(QString("%1 failed (Exit %2):\n%3").arg(engineLabel).arg(exitCode).arg(err));
        }
        delete fullStdOut;
        delete fullStdErr;
        emit finished();
        proc->deleteLater();
    });

    qDebug() << "TranscriptionWorker: launching" << engineLabel << executable << args.join(" ");
    
    proc->setProcessEnvironment(buildEnv());
    proc->start(executable, args);
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
