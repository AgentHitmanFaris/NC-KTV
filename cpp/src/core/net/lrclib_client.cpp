/*
 * NC-KTV Core — LrcLib Client Implementation
 */

#include "lrclib_client.h"
#include "parsers/subtitle_parser.h"

#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

namespace ncktv {

LrcLibClient::LrcLibClient(QObject* parent)
    : QObject(parent)
{
}

void LrcLibClient::searchLyrics(const QString& title, const QString& artist,
                                 const QString& album)
{
    QUrl url(QStringLiteral("%1/search").arg(API_BASE));
    QUrlQuery query;
    query.addQueryItem("track_name", title);
    if (!artist.isEmpty()) query.addQueryItem("artist_name", artist);
    if (!album.isEmpty())  query.addQueryItem("album_name", album);
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "NC-KTV/1.0");

    QNetworkReply* reply = m_netManager.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit searchError(reply->errorString());
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (!doc.isArray()) {
            emit searchError("Invalid API response");
            return;
        }

        QVector<LrcLibResult> results;
        for (const auto& val : doc.array()) {
            QJsonObject obj = val.toObject();
            LrcLibResult r;
            r.title        = obj["trackName"].toString();
            r.artist       = obj["artistName"].toString();
            r.syncedLyrics = obj["syncedLyrics"].toString();
            r.plainLyrics  = obj["plainLyrics"].toString();
            r.duration     = obj["duration"].toDouble();
            results.append(r);
        }

        emit searchComplete(results);
    });
}

LyricsData LrcLibClient::parseSyncedLyrics(const QString& lrcContent) {
    return SubtitleParser::parseLrc(lrcContent);
}

} // namespace ncktv
