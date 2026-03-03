#include "progress_reporter.h"
namespace ncktv {
ProgressReporter::ProgressReporter(QObject* parent) : QObject(parent) {}
void ProgressReporter::reportProgress(int percent, const QString& message) {
    emit progress(percent, message);
}
void ProgressReporter::reportStage(const QString& stage) {
    emit stageChanged(stage);
}
} // namespace ncktv
