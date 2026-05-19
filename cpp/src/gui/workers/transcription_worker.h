#pragma once
#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QTcpSocket>
#include <QProcess>
#include <vector>
#include "../../core/audio/ncktv_whisper_engine.hpp"

namespace ncktv {

/**
 * Transcription engine selection.
 * Controls which Python bridge backend is used for AI transcription.
 */
enum class TranscriptionEngine {
    Whisper,    ///< Legacy OpenAI Whisper (attention-based word timestamps)
    WhisperX,   ///< WhisperX with Wav2Vec2 forced alignment (±30ms precision)
};

class TranscriptionWorker : public QObject {
    Q_OBJECT

public:
    explicit TranscriptionWorker(QObject* parent = nullptr);
    ~TranscriptionWorker();

public slots:
    /**
     * Start transcription using the selected engine via JSON-RPC.
     */
    void startTranscription(const QString& audioPath,
                            const QString& model    = "base",
                            const QString& language = "auto",
                            TranscriptionEngine engine = TranscriptionEngine::Whisper);

    /**
     * Force-align known lyrics text to audio using WhisperX via JSON-RPC.
     * Skips the need for temporary files by sending lyrics directly in the payload.
     */
    void startAlignment(const QString& audioPath,
                        const QString& lyricsText,
                        const QString& language = "en");

signals:
    void progressUpdated(const QString& message);
    void transcriptionComplete(const QString& resultJson);
    void error(const QString& errorMessage);
    void finished();

private slots:
    void onSocketReadyRead();
    void onSocketError(QTcpSocket::SocketError socketError);
    void onProcessStandardError();

private:
    void sendRpcRequest(const QString& method, const QJsonObject& params);
    bool ensureServerRunning();

    QString serializeSegmentsToJson(const std::vector<ai::TranscribedSegment>& segments);

    QTcpSocket* m_socket;
    QByteArray m_responseBuffer;

    // Static process to ensure the Python server stays alive across multiple 
    // worker calls. This avoids reloading PyTorch/Whisper models into VRAM every time.
    static QProcess* s_serverProcess;
};

} // namespace ncktv

Q_DECLARE_METATYPE(ncktv::TranscriptionEngine)