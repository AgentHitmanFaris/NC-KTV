#pragma once
#include <QObject>
namespace ncktv {
class ExportWorker : public QObject {
    Q_OBJECT

public:
    explicit ExportWorker(QObject* parent = nullptr);
    void startExport(const QString& videoPath, const QString& audioPath, const QString& outputPath,
                     const QString& format = "mp4", bool burnSubs = true, int width = 0, int height = 0);
signals:
    void progress(int percent, const QString& message);
    void exportComplete(const QString& outputPath);
    void error(const QString& errorMessage);
};
} // namespace ncktv
