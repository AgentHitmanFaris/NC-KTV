/*
 * NC-KTV GUI — Online Search Worker Implementation
 * Searches LRCLIB for lyrics using LrcLibClient
 */

#include "online_search_worker.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDebug>

#include "net/lrclib_client.h"

namespace ncktv {

OnlineSearchWorker::OnlineSearchWorker(QObject* parent) : QObject(parent) {}

void OnlineSearchWorker::searchLyrics(const QString& title, const QString& artist) {
    if (title.isEmpty()) {
        emit error("Please provide a song title to search.");
        return;
    }

    emit progress(10, "Searching LRCLIB...");

    auto* client = new LrcLibClient(this);

    connect(client, &LrcLibClient::searchComplete, this,
            [this, client](const QVector<LrcLibResult>& results) {
        client->deleteLater();

        if (results.isEmpty()) {
            emit error("No lyrics found. Try different search terms.");
            return;
        }

        emit progress(80, QString("Found %1 result(s)").arg(results.size()));

        // Serialize results to JSON for the caller
        QJsonArray arr;
        for (const auto& r : results) {
            QJsonObject obj;
            obj["title"]        = r.title;
            obj["artist"]       = r.artist;
            obj["syncedLyrics"] = r.syncedLyrics;
            obj["plainLyrics"]  = r.plainLyrics;
            obj["duration"]     = r.duration;
            arr.append(obj);
        }

        QJsonDocument doc(arr);
        emit progress(100, "Search complete.");
        emit searchComplete(doc.toJson(QJsonDocument::Compact));
    });

    connect(client, &LrcLibClient::searchError, this, [this, client](const QString& errMsg) {
        client->deleteLater();
        emit error("LRCLIB search failed: " + errMsg);
    });

    client->searchLyrics(title, artist);
}

} // namespace ncktv
