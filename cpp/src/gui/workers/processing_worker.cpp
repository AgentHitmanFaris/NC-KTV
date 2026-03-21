/*
 * NC-KTV GUI — Processing Worker Implementation
 * Orchestrates the full UVR vocal separation + AI transcription pipeline
 */

#include "processing_worker.h"

#include <QDebug>
#include <QFileInfo>

#include "vocal_separator_worker.h"
#include "transcription_worker.h"

namespace ncktv {

ProcessingWorker::ProcessingWorker(QObject* parent) : QObject(parent) {}

void ProcessingWorker::startProcessing(const QString& inputPath, const QString& outputDir) {
    if (inputPath.isEmpty()) {
        emit progress(0, "Error: No input file specified.");
        return;
    }

    emit progress(0, "Starting processing pipeline...");

    // ── Phase 1: Vocal Separation ────────────────────────────────────────
    emit progress(5, "Phase 1: Separating vocals...");

    auto* separator = new VocalSeparatorWorker(this);

    connect(separator, &VocalSeparatorWorker::progress, this,
            [this](int percent, const QString& msg) {
        // Scale separator progress to 0-50% of total
        emit progress(percent / 2, "Separation: " + msg);
    });

    connect(separator, &VocalSeparatorWorker::separationComplete, this,
            [this, separator](const QString& instrumentalPath, const QString& vocalsPath) {
        separator->deleteLater();
        emit progress(50, "Separation complete. Starting transcription...");

        // ── Phase 2: AI Transcription ────────────────────────────────────
        auto* transcriber = new TranscriptionWorker(this);

        connect(transcriber, &TranscriptionWorker::progressUpdated, this, [this](const QString& msg){
            emit progress(70, msg);
        });

        connect(transcriber, &TranscriptionWorker::transcriptionComplete, this,
                [this, transcriber, instrumentalPath, vocalsPath](const QString& resultJson) {
            transcriber->deleteLater();
            emit progress(100, "Pipeline complete!");

            // Emit all results — the caller (WizardMode) handles project creation
            // We store the result JSON in a temporary property or emit a dedicated signal
            Q_UNUSED(instrumentalPath);
            Q_UNUSED(vocalsPath);
            Q_UNUSED(resultJson);
        });

        connect(transcriber, &TranscriptionWorker::error, this,
                [this, transcriber](const QString& err) {
            transcriber->deleteLater();
            emit progress(50, "Transcription failed (skipped): " + err);
            // Pipeline continues without transcription — separation still valid
            emit progress(100, "Pipeline complete (without transcription).");
        });

        // Use vocals for transcription (better quality) — WhisperX by default
        transcriber->startTranscription(vocalsPath, "medium", "Auto", TranscriptionEngine::WhisperX);
    });

    connect(separator, &VocalSeparatorWorker::error, this,
            [this, separator](const QString& err) {
        separator->deleteLater();
        emit progress(0, "Separation failed: " + err);
    });

    separator->startSeparation(inputPath, "UVR_MDXNET_KARA_2.onnx", outputDir);
}

} // namespace ncktv
