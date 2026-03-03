#pragma once
/*
 * NC-KTV Core — Subtitle Parser (Unified)
 * Port of subtitle_parser.py — Parses SRT, LRC, VTT, ASS, TTML, plain text
 */

#include <QString>
#include "lyrics/lyrics_data.h"

namespace ncktv {

class SubtitleParser {
public:
    /// Auto-detect format and parse file into LyricsData
    static LyricsData parseFile(const QString& filePath);

    /// Parse from string with explicit format
    static LyricsData parseSrt(const QString& content);
    static LyricsData parseLrc(const QString& content);
    static LyricsData parseVtt(const QString& content);
    static LyricsData parseAss(const QString& content);
    static LyricsData parseTtml(const QString& content);
    static LyricsData parsePlainText(const QString& content);

    /// Detect subtitle format from file extension
    enum class Format { SRT, LRC, VTT, ASS, TTML, PlainText, Unknown };
    static Format detectFormat(const QString& filePath);

private:
    /// Parse SRT/VTT timestamp "HH:MM:SS,mmm" or "HH:MM:SS.mmm" to seconds
    static double parseSrtTimestamp(const QString& ts);
    /// Parse LRC timestamp "[MM:SS.xx]" to seconds
    static double parseLrcTimestamp(const QString& ts);
    /// Parse ASS timestamp "H:MM:SS.cc" to seconds
    static double parseAssTimestamp(const QString& ts);
};

} // namespace ncktv
