#pragma once
#include <QObject>
namespace ncktv {
class ProgressReporter : public QObject {
    Q_OBJECT
public:
    explicit ProgressReporter(QObject* parent = nullptr);
    void reportProgress(int percent, const QString& message);
    void reportStage(const QString& stage);
signals:
    void progress(int percent, const QString& message);
    void stageChanged(const QString& stage);
};
} // namespace ncktv
