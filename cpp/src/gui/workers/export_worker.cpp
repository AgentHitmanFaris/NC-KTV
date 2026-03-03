#include "export_worker.h"
namespace ncktv {
ExportWorker::ExportWorker(QObject* parent) : QObject(parent) {}
void ExportWorker::startExport(const QString& projectPath, const QString& outputPath,
                                const QString& format) {
    emit progress(0, "Export starting...");
    Q_UNUSED(projectPath); Q_UNUSED(outputPath); Q_UNUSED(format);
}
} // namespace ncktv
