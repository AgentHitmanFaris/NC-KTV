#pragma once
#include <QObject>
#include <QThread>
#include <vector>
#include "../../core/audio/ncktv_whisper_engine.hpp"

namespace ncktv {

/**
 * Transcription engine selection.
 * Controls which Python bridge backend is used for AI transcription.
 */
enum class TranscriptionEngine {
    Whisper,    ///< Legacy OpenAI Whisper (attention-based word timestamps, ±500ms)
    WhisperX,   ///< WhisperX with Wav2Vec2 forced alignment (±30ms precision)
};

class TranscriptionWorker : public QObject {
    Q_OBJECT

public:
    explicit TranscriptionWorker(QObject* parent = nullptr);

public slots:
    /**
     * Start transcription using the selected engine.
     * @param audioPath     Path to audio file (vocals preferred)
     * @param model         Whisper model name (e.g. "base", "medium", "large-v3", "turbo")
     * @param language      Language code (e.g. "en", "ja") or "auto" for detection
     * @param engine        Transcription engine to use (Whisper or WhisperX)
     */
    void startTranscription(const QString& audioPath,
                            const QString& model    = "base",
                            const QString& language = "auto",
                            TranscriptionEngine engine = TranscriptionEngine::Whisper);

    /**
     * Force-align known lyrics text to audio using WhisperX.
     * Skips transcription entirely — only runs Wav2Vec2 alignment.
     * This is ideal when lyrics are already available (LRC import, Gemini output, etc.)
     * @param audioPath     Path to audio file (vocals preferred)
     * @param lyricsText    Full lyrics text (lines separated by newlines)
     * @param language      Language code for alignment model (must be specific, not "auto")
     */
    void startAlignment(const QString& audioPath,
                        const QString& lyricsText,
                        const QString& language = "en");

signals:
    void progressUpdated(const QString& message);
    void transcriptionComplete(const QString& resultJson);
    void error(const QString& errorMessage);

private:
    /// Shared process launcher used by both transcription and alignment
    void launchPythonBridge(const QString& executable, const QStringList& args, const QString& engineLabel);

    /// Helper used when the native engine path is active (MSVC build)
    QString serializeSegmentsToJson(const std::vector<ai::TranscribedSegment>& segments);
};

} // namespace ncktv

Q_DECLARE_METATYPE(ncktv::TranscriptionEngine)
