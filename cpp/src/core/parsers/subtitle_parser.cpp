/*
 * NC-KTV Core — Subtitle Parser Implementation
 * Port of subtitle_parser.py, lrc_parser.py, srt_parser.py
 */

#include "subtitle_parser.h"

#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextStream>
#include <cmath>

namespace ncktv {

// ─── Format Detection ────────────────────────────────────────────────────────

SubtitleParser::Format SubtitleParser::detectFormat(const QString& filePath) {
    QString ext = QFileInfo(filePath).suffix().toLower();
    if (ext == "srt")                         return Format::SRT;
    if (ext == "lrc")                         return Format::LRC;
    if (ext == "vtt")                         return Format::VTT;
    if (ext == "ass" || ext == "ssa")          return Format::ASS;
    if (ext == "ttml" || ext == "dfxp" || ext == "xml") return Format::TTML;
    if (ext == "txt")                         return Format::PlainText;
    return Format::Unknown;
}

LyricsData SubtitleParser::parseFile(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    QString content = QTextStream(&file).readAll();
    Format fmt = detectFormat(filePath);

    switch (fmt) {
    case Format::SRT:       return parseSrt(content);
    case Format::LRC:       return parseLrc(content);
    case Format::VTT:       return parseVtt(content);
    case Format::ASS:       return parseAss(content);
    case Format::TTML:      return parseTtml(content);
    case Format::PlainText: return parsePlainText(content);
    default:                return parsePlainText(content);
    }
}

// ─── Timestamp Parsers ───────────────────────────────────────────────────────

double SubtitleParser::parseSrtTimestamp(const QString& ts) {
    // "HH:MM:SS,mmm" or "HH:MM:SS.mmm"
    static QRegularExpression re(R"((\d+):(\d+):(\d+)[,.](\d+))");
    auto m = re.match(ts.trimmed());
    if (!m.hasMatch()) return 0.0;

    int h   = m.captured(1).toInt();
    int min = m.captured(2).toInt();
    int s   = m.captured(3).toInt();
    int ms  = m.captured(4).toInt();

    return h * 3600.0 + min * 60.0 + s + ms / 1000.0;
}

double SubtitleParser::parseLrcTimestamp(const QString& ts) {
    // "[MM:SS.xx]"
    static QRegularExpression re(R"(\[?(\d+):(\d+(?:\.\d+)?)\]?)");
    auto m = re.match(ts.trimmed());
    if (!m.hasMatch()) return 0.0;

    int min    = m.captured(1).toInt();
    double sec = m.captured(2).toDouble();

    return min * 60.0 + sec;
}

double SubtitleParser::parseAssTimestamp(const QString& ts) {
    // "H:MM:SS.cc"
    static QRegularExpression re(R"((\d+):(\d+):(\d+)\.(\d+))");
    auto m = re.match(ts.trimmed());
    if (!m.hasMatch()) return 0.0;

    int h   = m.captured(1).toInt();
    int min = m.captured(2).toInt();
    int s   = m.captured(3).toInt();
    int cs  = m.captured(4).toInt();

    return h * 3600.0 + min * 60.0 + s + cs / 100.0;
}

// ─── SRT Parser ──────────────────────────────────────────────────────────────

LyricsData SubtitleParser::parseSrt(const QString& content) {
    LyricsData data;
    static QRegularExpression timingRe(
        R"((\d+:\d+:\d+[,.]\d+)\s*-->\s*(\d+:\d+:\d+[,.]\d+))");

    QStringList blocks = content.split(QRegularExpression(R"(\n\s*\n)"),
                                        Qt::SkipEmptyParts);

    for (const auto& block : blocks) {
        QStringList lines = block.trimmed().split('\n');
        if (lines.size() < 2) continue;

        // Find the timing line
        for (int i = 0; i < lines.size() - 1; ++i) {
            auto m = timingRe.match(lines[i]);
            if (m.hasMatch()) {
                double start = parseSrtTimestamp(m.captured(1));
                double end   = parseSrtTimestamp(m.captured(2));

                // Join remaining lines as text
                QStringList textLines;
                for (int j = i + 1; j < lines.size(); ++j)
                    textLines << lines[j].trimmed();
                QString text = textLines.join(" ");

                // Strip HTML tags
                text.remove(QRegularExpression("<[^>]*>"));

                if (!text.isEmpty()) {
                    LyricLine line;
                    line.text      = text;
                    line.startTime = start;
                    line.endTime   = end;
                    data.addLine(line);
                }
                break;
            }
        }
    }

    return data;
}

// ─── LRC Parser ──────────────────────────────────────────────────────────────

LyricsData SubtitleParser::parseLrc(const QString& content) {
    LyricsData data;
    static QRegularExpression lineRe(R"(\[(\d+:\d+(?:\.\d+)?)\](.*))");
    static QRegularExpression metaRe(R"(\[(\w+):(.*)\])");

    for (const auto& rawLine : content.split('\n')) {
        QString trimmed = rawLine.trimmed();
        if (trimmed.isEmpty()) continue;

        // Check metadata
        auto metaMatch = metaRe.match(trimmed);
        if (metaMatch.hasMatch() && !lineRe.match(trimmed).hasMatch()) {
            QString key = metaMatch.captured(1).toLower();
            QString val = metaMatch.captured(2).trimmed();
            if (key == "ti") data.title  = val;
            if (key == "ar") data.artist = val;
            continue;
        }

        // Parse timed line
        auto m = lineRe.match(trimmed);
        if (m.hasMatch()) {
            double start = parseLrcTimestamp(m.captured(1));
            QString text = m.captured(2).trimmed();

            if (!text.isEmpty()) {
                LyricLine line;
                line.text      = text;
                line.startTime = start;
                line.endTime   = start;   // Will be corrected in post-processing
                data.addLine(line);
            }
        }
    }

    // Post-process: set end times to next line's start time
    for (int i = 0; i < data.lines.size() - 1; ++i) {
        data.lines[i].endTime = data.lines[i + 1].startTime;
    }
    if (!data.lines.isEmpty()) {
        auto& last = data.lines.last();
        if (last.endTime <= last.startTime)
            last.endTime = last.startTime + 5.0;   // Default 5s for last line
    }

    return data;
}

// ─── VTT Parser ──────────────────────────────────────────────────────────────

LyricsData SubtitleParser::parseVtt(const QString& content) {
    // VTT is very similar to SRT, just uses "." instead of ","
    QString normalized = content;
    // Remove WEBVTT header
    normalized.remove(QRegularExpression(R"(^WEBVTT[^\n]*\n)", QRegularExpression::MultilineOption));
    return parseSrt(normalized);
}

// ─── ASS Parser ──────────────────────────────────────────────────────────────

LyricsData SubtitleParser::parseAss(const QString& content) {
    LyricsData data;
    static QRegularExpression dialogueRe(
        R"(Dialogue:\s*\d+,(\d+:\d+:\d+\.\d+),(\d+:\d+:\d+\.\d+),[^,]*,[^,]*,\d+,\d+,\d+,[^,]*,(.*))");

    for (const auto& rawLine : content.split('\n')) {
        auto m = dialogueRe.match(rawLine.trimmed());
        if (m.hasMatch()) {
            double start = parseAssTimestamp(m.captured(1));
            double end   = parseAssTimestamp(m.captured(2));
            QString text = m.captured(3).trimmed();

            // Strip ASS override tags like {\pos(x,y)}
            text.remove(QRegularExpression(R"(\{[^}]*\})"));
            // Replace \N with space
            text.replace("\\N", " ");

            if (!text.isEmpty()) {
                LyricLine line;
                line.text      = text;
                line.startTime = start;
                line.endTime   = end;
                data.addLine(line);
            }
        }
    }

    return data;
}

// ─── TTML Parser ─────────────────────────────────────────────────────────────

LyricsData SubtitleParser::parseTtml(const QString& content) {
    LyricsData data;
    // Simplified TTML parser using regex (full XML parsing could use QXmlStreamReader)
    static auto pRe = QRegularExpression(
        R"re(<p[^>]*begin="([^"]+)"[^>]*end="([^"]+)"[^>]*>(.*?)</p>)re",
        QRegularExpression::DotMatchesEverythingOption);

    auto it = pRe.globalMatch(content);
    while (it.hasNext()) {
        auto m = it.next();
        double start = parseSrtTimestamp(m.captured(1));
        double end   = parseSrtTimestamp(m.captured(2));
        QString text = m.captured(3);

        // Strip XML tags
        text.remove(QRegularExpression("<[^>]*>"));
        text = text.trimmed();

        if (!text.isEmpty()) {
            LyricLine line;
            line.text      = text;
            line.startTime = start;
            line.endTime   = end;
            data.addLine(line);
        }
    }

    return data;
}

// ─── Plain Text ──────────────────────────────────────────────────────────────

LyricsData SubtitleParser::parsePlainText(const QString& content) {
    LyricsData data;
    data.importFromText(content);
    return data;
}

} // namespace ncktv
