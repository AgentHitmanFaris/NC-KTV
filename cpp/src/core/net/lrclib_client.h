#pragma once
/*
 * NC-KTV Core — LrcLib API Client
 * Port of lrclib_client.py — Async lyrics search via LRCLIB
 */

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <functional>
#include "lyrics/lyrics_data.h"

namespace ncktv {

struct LrcLibResult {
    QString title;
    QString artist;
    QString syncedLyrics;    // LRC format
    QString plainLyrics;
    double  duration = 0.0;
};

class LrcLibClient : public QObject {
    Q_OBJECT
public:
    explicit LrcLibClient(QObject* parent = nullptr);

    /// Search lyrics by title and artist (async)
    void searchLyrics(const QString& title, const QString& artist,
                      const QString& album = {});

    /// Parse synced lyrics from LrcLib response into LyricsData
    static LyricsData parseSyncedLyrics(const QString& lrcContent);

signals:
    void searchComplete(const QVector<LrcLibResult>& results);
    void searchError(const QString& errorMessage);

private:
    QNetworkAccessManager m_netManager;
    static constexpr const char* API_BASE = "https://lrclib.net/api";
};

} // namespace ncktv
