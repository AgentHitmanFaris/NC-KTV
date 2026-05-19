/**
 * @file transcription_worker.cpp
 * @brief Routes AI transcription through the Python JSON-RPC server.
 *
 * This implementation acts as a TCP client to `python_bridge.py --serve`.
 * It manages the lifecycle of the Python server centrally, ensuring that heavy 
 * AI models remain loaded in memory between transcription requests.
 */

#include "transcription_worker.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QDebug>
#include <QThread>
#include <QJsonDocument>
#include <QJsonParseError>
#include <nlohmann/json.hpp>

namespace ncktv {

// Initialize static server process
QProcess* TranscriptionWorker::s_serverProcess = nullptr;

// Helper to find Python interpreter
static QString findPython() {
    QString appDir = QCoreApplication::applicationDirPath();
    QString p = QDir::cleanPath(appDir + "/python_embed/python.exe");
    if (QFile::exists(p)) return p;
    
    p = QDir::cleanPath(QDir::currentPath() + "/python_embed/python.exe");
    if (QFile::exists(p)) return p;

    p = "D:/Program Files/Python/python.exe";
    if (QFile::exists(p)) return p;
    
    p = QDir::cleanPath(QDir::currentPath() + "/venv/Scripts/python.exe");
    if (QFile::exists(p)) return p;
    
    return "python";
}

// Returns {executable, args_prefix} for the bridge
static std::pair<QString, QStringList> findBridge() {
    QString appDir = QCoreApplication::applicationDirPath();

    QString bundleExe = QDir::cleanPath(appDir + "/python_bridge/python_bridge.exe");
    if (QFile::exists(bundleExe)) return {bundleExe, {}};

    QString pyScript = QDir::cleanPath(appDir + "/python_bridge.py");
    if (QFile::exists(pyScript)) return {findPython(), {pyScript}};

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

    m_socket = new QTcpSocket(this);
    connect(m_socket, &QTcpSocket::readyRead, this, &TranscriptionWorker::onSocketReadyRead);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &TranscriptionWorker::onSocketError);
}

TranscriptionWorker::~TranscriptionWorker() {
    if (s_serverProcess) {
        // Disconnect logging to prevent writing to a deleted worker
        disconnect(s_serverProcess, &QProcess::readyReadStandardError, this, &TranscriptionWorker::onProcessStandardError);
    }
}

bool TranscriptionWorker::ensureServerRunning() {
    if (s_serverProcess && s_serverProcess->state() == QProcess::Running) {
        return true;
    }

    emit progressUpdated("Spinning up persistent AI backend server...");

    if (!s_serverProcess) {
        // Parent to qApp so it outlives this worker but dies when the main app closes
        s_serverProcess = new QProcess(qApp);
        s_serverProcess->setProcessEnvironment(buildEnv());
    }

    auto [bridgeExe, bridgePrefix] = findBridge();
    QStringList args = bridgePrefix;
    args << "--serve" << "--port" << "50051";

    s_serverProcess->start(bridgeExe, args);
    if (!s_serverProcess->waitForStarted(3000)) {
        emit error("Failed to launch Python JSON-RPC server.");
        return false;
    }

    // Give Python time to load libraries and bind to the TCP port
    QThread::msleep(1500);
    return true;
}

void TranscriptionWorker::sendRpcRequest(const QString& method, const QJsonObject& params) {
    if (!ensureServerRunning()) {
        emit finished();
        return;
    }

    // Connect stderr of the server to THIS worker to capture live WhisperX logs (progress)
    connect(s_serverProcess, &QProcess::readyReadStandardError, this, &TranscriptionWorker::onProcessStandardError, Qt::UniqueConnection);

    m_socket->connectToHost("127.0.0.1", 50051);
    if (!m_socket->waitForConnected(2000)) {
        emit error("Failed to connect to AI server on port 50051.");
        emit finished();
        return;
    }

    QJsonObject request;
    request["jsonrpc"] = "2.0";
    request["method"] = method;
    request["params"] = params;
    request["id"] = 1;

    QJsonDocument doc(request);
    m_socket->write(doc.toJson(QJsonDocument::Compact) + "\n");
    m_socket->flush();
}

void TranscriptionWorker::startTranscription(const QString& audioPath,
                                             const QString& model,
                                             const QString& language,
                                             TranscriptionEngine engine)
{
    QJsonObject params;
    params["audio_file"] = audioPath;
    params["model_name"] = model;
    
    if (language != "auto" && !language.isEmpty()) {
        params["language"] = language;
    }

    QString method = (engine == TranscriptionEngine::WhisperX) ? "transcribe-x" : "transcribe";
    emit progressUpdated(QString("Sending %1 request to AI server...").arg(method));
    
    sendRpcRequest(method, params);
}

void TranscriptionWorker::startAlignment(const QString& audioPath,
                                         const QString& lyricsText,
                                         const QString& language)
{
    QJsonObject params;
    params["audio_file"] = audioPath;
    params["lyrics_text"] = lyricsText; // JSON-RPC lets us send text directly. No temp files needed!
    params["language"] = language;

    emit progressUpdated("Sending forced-alignment request to AI server...");
    sendRpcRequest("align", params);
}

void TranscriptionWorker::onSocketReadyRead() {
    m_responseBuffer.append(m_socket->readAll());
    
    // The Python bridge delimitates JSON-RPC responses with a newline
    if (m_responseBuffer.contains('\n')) {
        QJsonParseError parseErr;
        QJsonDocument doc = QJsonDocument::fromJson(m_responseBuffer, &parseErr);
        m_responseBuffer.clear();

        if (parseErr.error != QJsonParseError::NoError) {
            emit error("Failed to parse JSON-RPC response: " + parseErr.errorString());
        } else {
            QJsonObject root = doc.object();
            if (root.contains("error") && !root["error"].isNull()) {
                QString errMsg = root["error"].toObject()["message"].toString();
                emit error("AI Server Error: " + errMsg);
            } else if (root.contains("result")) {
                QJsonDocument resDoc(root["result"].toObject());
                emit transcriptionComplete(resDoc.toJson(QJsonDocument::Compact));
            } else {
                emit error("Invalid JSON-RPC response format received from server.");
            }
        }
        
        // Clean up connections so the next worker can cleanly attach
        disconnect(s_serverProcess, &QProcess::readyReadStandardError, this, &TranscriptionWorker::onProcessStandardError);
        m_socket->disconnectFromHost();
        emit finished();
    }
}

void TranscriptionWorker::onSocketError(QTcpSocket::SocketError /*socketError*/) {
    emit error("Socket connection error: " + m_socket->errorString());
    if (s_serverProcess) {
        disconnect(s_serverProcess, &QProcess::readyReadStandardError, this, &TranscriptionWorker::onProcessStandardError);
    }
    emit finished();
}

void TranscriptionWorker::onProcessStandardError() {
    if (!s_serverProcess) return;
    
    QByteArray chunk = s_serverProcess->readAllStandardError();
    QString line = QString::fromUtf8(chunk).trimmed();
    
    if (!line.isEmpty()) {
        QString lastLine = line.split('\n').last().trimmed();
        emit progressUpdated(lastLine);
    }
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