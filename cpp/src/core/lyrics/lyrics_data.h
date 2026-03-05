#pragma once
/*
 * NC-KTV Core — Lyrics Data Structures
 * Port of sync_data.py
 *
 * LyricWord → LyricLine → LyricsData
 * Full JSON serialization + LRC/SRT export
 */

#include <QString>
#include <QVector>
#include <optional>
#include <nlohmann/json.hpp>

namespace ncktv {

// ─── LyricWord ──────────────────────────────────────────────────────────────

struct LyricWord {
    QString word;
    double  startTime  = 0.0;   // seconds
    double  endTime    = 0.0;   // seconds
    double  confidence = 1.0;   // 0–1, for auto-synced words

    [[nodiscard]] double duration() const { return std::max(0.0, endTime - startTime); }
};

void to_json(nlohmann::json& j, const LyricWord& w);
void from_json(const nlohmann::json& j, LyricWord& w);

// ─── LyricLine ──────────────────────────────────────────────────────────────

struct LyricLine {
    QString               text;
    double                startTime = 0.0;
    double                endTime   = 0.0;
    QVector<LyricWord>    words;
    std::optional<QString> romanizedText;

    [[nodiscard]] double duration() const { return std::max(0.0, endTime - startTime); }

    void addWord(const QString& word, double start, double end, double confidence = 1.0);
};

void to_json(nlohmann::json& j, const LyricLine& line);
void from_json(const nlohmann::json& j, LyricLine& line);

// ─── LyricsData ─────────────────────────────────────────────────────────────

class LyricsData {
public:
    QVector<LyricLine>     lines;
    std::optional<QString> title;
    std::optional<QString> artist;
    std::optional<QString> language;

    // ── Mutation ─────────────────────────────────────────────────────────
    void addLine(const LyricLine& line);
    void clear();
    void importFromText(const QString& text);
    void importFromWhisperJson(const QString& jsonString);
    void importFromLrc(const QString& lrcText);
    void importFromSrt(const QString& srtText);

    // ── Queries ──────────────────────────────────────────────────────────
    [[nodiscard]] double     getTotalDuration() const;
    [[nodiscard]] const LyricLine* getLineAtTime(double time) const;

    // ── Serialization ────────────────────────────────────────────────────
    [[nodiscard]] nlohmann::json toJson() const;
    static LyricsData fromJson(const nlohmann::json& j);

    void save(const QString& filePath) const;
    static LyricsData load(const QString& filePath);

    // ── Export ────────────────────────────────────────────────────────────
    [[nodiscard]] QString toLrc() const;
    [[nodiscard]] QString toSrt() const;

private:
    static QString formatSrtTime(double seconds);
};

} // namespace ncktv
