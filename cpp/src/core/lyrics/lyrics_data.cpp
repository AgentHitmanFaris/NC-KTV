/*
 * NC-KTV Core — Lyrics Data Implementation
 * Port of sync_data.py
 */

#include "lyrics_data.h"

#include <QFile>
#include <QTextStream>
#include <algorithm>
#include <cmath>

namespace ncktv {

// ─── JSON Helpers ────────────────────────────────────────────────────────────

static std::string qs(const QString& s) { return s.toStdString(); }
static QString fromStd(const std::string& s) { return QString::fromStdString(s); }

// ─── LyricWord JSON ─────────────────────────────────────────────────────────

void to_json(nlohmann::json& j, const LyricWord& w)
{
    j = {
        {"word",       qs(w.word)},
        {"start_time", w.startTime},
        {"end_time",   w.endTime},
        {"confidence", w.confidence}
    };
}

void from_json(const nlohmann::json& j, LyricWord& w)
{
    w.word       = fromStd(j.at("word").get<std::string>());
    w.startTime  = j.at("start_time").get<double>();
    w.endTime    = j.at("end_time").get<double>();
    w.confidence = j.value("confidence", 1.0);
}

// ─── LyricLine ──────────────────────────────────────────────────────────────

void LyricLine::addWord(const QString& word, double start, double end, double confidence)
{
    words.append(LyricWord{word, start, end, confidence});
}

void to_json(nlohmann::json& j, const LyricLine& line)
{
    nlohmann::json wordsArr = nlohmann::json::array();
    for (const auto& w : line.words) {
        nlohmann::json wj;
        to_json(wj, w);
        wordsArr.push_back(wj);
    }

    j = {
        {"text",       qs(line.text)},
        {"start_time", line.startTime},
        {"end_time",   line.endTime},
        {"words",      wordsArr}
    };

    if (line.romanizedText.has_value())
        j["romanized_text"] = qs(line.romanizedText.value());
}

void from_json(const nlohmann::json& j, LyricLine& line)
{
    line.text      = fromStd(j.at("text").get<std::string>());
    line.startTime = j.at("start_time").get<double>();
    line.endTime   = j.at("end_time").get<double>();

    line.words.clear();
    if (j.contains("words")) {
        for (const auto& wj : j["words"]) {
            LyricWord w;
            from_json(wj, w);
            line.words.append(w);
        }
    }

    if (j.contains("romanized_text") && !j["romanized_text"].is_null())
        line.romanizedText = fromStd(j["romanized_text"].get<std::string>());
}

// ─── LyricsData ─────────────────────────────────────────────────────────────

void LyricsData::addLine(const LyricLine& line)
{
    lines.append(line);
}

void LyricsData::clear()
{
    lines.clear();
}

void LyricsData::importFromText(const QString& text)
{
    lines.clear();
    const auto rawLines = text.split('\n');
    for (const auto& raw : rawLines) {
        QString trimmed = raw.trimmed();
        if (!trimmed.isEmpty()) {
            LyricLine line;
            line.text      = trimmed;
            line.startTime = 0.0;
            line.endTime   = 0.0;
            lines.append(line);
        }
    }
}

double LyricsData::getTotalDuration() const
{
    if (lines.isEmpty()) return 0.0;
    double maxEnd = 0.0;
    for (const auto& line : lines)
        maxEnd = std::max(maxEnd, line.endTime);
    return maxEnd;
}

const LyricLine* LyricsData::getLineAtTime(double time) const
{
    for (const auto& line : lines) {
        if (line.startTime <= time && time <= line.endTime)
            return &line;
    }
    return nullptr;
}

// ── Serialization ────────────────────────────────────────────────────────────

nlohmann::json LyricsData::toJson() const
{
    nlohmann::json j;

    if (title.has_value())    j["title"]    = qs(title.value());
    if (artist.has_value())   j["artist"]   = qs(artist.value());
    if (language.has_value()) j["language"] = qs(language.value());

    nlohmann::json linesArr = nlohmann::json::array();
    for (const auto& line : lines) {
        nlohmann::json lj;
        to_json(lj, line);
        linesArr.push_back(lj);
    }
    j["lines"] = linesArr;

    return j;
}

LyricsData LyricsData::fromJson(const nlohmann::json& j)
{
    LyricsData data;

    if (j.contains("title") && !j["title"].is_null())
        data.title = fromStd(j["title"].get<std::string>());
    if (j.contains("artist") && !j["artist"].is_null())
        data.artist = fromStd(j["artist"].get<std::string>());
    if (j.contains("language") && !j["language"].is_null())
        data.language = fromStd(j["language"].get<std::string>());

    if (j.contains("lines")) {
        for (const auto& lj : j["lines"]) {
            LyricLine line;
            from_json(lj, line);
            data.lines.append(line);
        }
    }

    return data;
}

void LyricsData::save(const QString& filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return;
    QTextStream out(&file);
    out << QString::fromStdString(toJson().dump(2));
}

LyricsData LyricsData::load(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    auto j = nlohmann::json::parse(file.readAll().toStdString());
    return fromJson(j);
}

// ── Export ────────────────────────────────────────────────────────────────────

QString LyricsData::toLrc() const
{
    QStringList out;

    if (title.has_value())
        out << QStringLiteral("[ti:%1]").arg(title.value());
    if (artist.has_value())
        out << QStringLiteral("[ar:%1]").arg(artist.value());

    for (const auto& line : lines) {
        int minutes = static_cast<int>(line.startTime) / 60;
        double seconds = std::fmod(line.startTime, 60.0);
        out << QStringLiteral("[%1:%2]%3")
                   .arg(minutes, 2, 10, QChar('0'))
                   .arg(seconds, 5, 'f', 2, QChar('0'))
                   .arg(line.text);
    }

    return out.join('\n');
}

QString LyricsData::toSrt() const
{
    QStringList out;

    for (int i = 0; i < lines.size(); ++i) {
        const auto& line = lines[i];
        out << QString::number(i + 1);
        out << QStringLiteral("%1 --> %2")
                   .arg(formatSrtTime(line.startTime),
                        formatSrtTime(line.endTime));
        out << line.text;
        out << QString();   // blank separator
    }

    return out.join('\n');
}

QString LyricsData::formatSrtTime(double seconds)
{
    int h    = static_cast<int>(seconds) / 3600;
    int m    = (static_cast<int>(seconds) % 3600) / 60;
    int s    = static_cast<int>(seconds) % 60;
    int ms   = static_cast<int>(std::fmod(seconds, 1.0) * 1000.0);
    return QStringLiteral("%1:%2:%3,%4")
        .arg(h, 2, 10, QChar('0'))
        .arg(m, 2, 10, QChar('0'))
        .arg(s, 2, 10, QChar('0'))
        .arg(ms, 3, 10, QChar('0'));
}

} // namespace ncktv
