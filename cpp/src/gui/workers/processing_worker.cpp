#include "processing_worker.h"
namespace ncktv {
ProcessingWorker::ProcessingWorker(QObject* parent) : QObject(parent) {}
void ProcessingWorker::startProcessing(const QString& inputPath, const QString& outputDir) {
    emit progress(0, "Processing starting...");
    // TODO: Implement processing pipeline (UVR + transcription)
    Q_UNUSED(inputPath); Q_UNUSED(outputDir);
}
} // namespace ncktv
