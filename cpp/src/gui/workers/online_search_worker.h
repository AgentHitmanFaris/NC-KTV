#pragma once
#include <QObject>
namespace ncktv {
class OnlineSearchWorker : public QObject {
    Q_OBJECT
public:
    explicit OnlineSearchWorker(QObject* parent = nullptr);
    void searchLyrics(const QString& title, const QString& artist);
signals:
    void progress(int percent, const QString& message);
    void searchComplete(const QString& resultJson);
    void error(const QString& errorMessage);
};
} // namespace ncktv
