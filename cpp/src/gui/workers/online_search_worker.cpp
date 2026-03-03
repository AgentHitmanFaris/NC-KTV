#include "online_search_worker.h"
namespace ncktv {
OnlineSearchWorker::OnlineSearchWorker(QObject* parent) : QObject(parent) {}
void OnlineSearchWorker::searchLyrics(const QString& title, const QString& artist) {
    emit progress(0, "Searching lyrics...");
    Q_UNUSED(title); Q_UNUSED(artist);
}
} // namespace ncktv
