#pragma once
#include <QObject>
namespace ncktv {
class ExportWorker : public QObject {
    Q_OBJECT
public:
    explicit ExportWorker(QObject* parent = nullptr);
    void startExport(const QString& projectPath, const QString& outputPath,
                     const QString& format = "mp4");
signals:
    void progress(int percent, const QString& message);
    void exportComplete(const QString& outputPath);
    void error(const QString& errorMessage);
};
} // namespace ncktv
