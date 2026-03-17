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
    if (content.trimmed().isEmpty()) return {};

    Format fmt = detectFormat(filePath);
    LyricsData data;

    switch (fmt) {
    case Format::SRT:       data = parseSrt(content); break;
    case Format::LRC:       data = parseLrc(content); break;
    case Format::VTT:       data = parseVtt(content); break;
    case Format::ASS:       data = parseAss(content); break;
    case Format::TTML:      data = parseTtml(content); break;
    case Format::PlainText: data = parsePlainText(content); break;
    default:                data = parsePlainText(content); break;
    }

    // Fallback: If specialized parser failed but we have text, import as plain text
    if (data.lines.isEmpty() && !content.trimmed().isEmpty()) {
        data.importFromText(content);
    }

    return data;
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
    QString clean = ts.trimmed();
    if (clean.startsWith('[')) clean.remove(0, 1);
    if (clean.endsWith(']')) clean.remove(clean.length() - 1, 1);
    
    QStringList parts = clean.split(':');
    if (parts.size() == 3) { // HH:MM:SS.xx
        return parts[0].toInt() * 3600.0 + parts[1].toInt() * 60.0 + parts[2].replace(',', '.').toDouble();
    } else if (parts.size() == 2) { // MM:SS.xx
        return parts[0].toInt() * 60.0 + parts[1].replace(',', '.').toDouble();
    } else if (parts.size() == 1) { // SS.xx
        return parts[0].replace(',', '.').toDouble();
    }
    return 0.0;
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
    static QRegularExpression timingRe(R"((\d+:\d+:\d+[,.]\d+)\s*-->\s*(\d+:\d+:\d+[,.]\d+))");
    
    QStringList lines = content.split('\n');
    QString currentText;
    double currentStart = -1.0;
    double currentEnd = -1.0;

    for (const auto& line : lines) {
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) {
            if (currentStart >= 0.0 && !currentText.isEmpty()) {
                LyricLine ll;
                ll.text = currentText.trimmed();
                ll.startTime = currentStart;
                ll.endTime = currentEnd;
                data.addLine(ll);
                currentStart = -1.0;
                currentText = "";
            }
            continue;
        }

        auto m = timingRe.match(trimmed);
        if (m.hasMatch()) {
            // If we found a timing line but had a pending block, save it
            if (currentStart >= 0.0 && !currentText.isEmpty()) {
                LyricLine ll;
                ll.text = currentText.trimmed();
                ll.startTime = currentStart;
                ll.endTime = currentEnd;
                data.addLine(ll);
            }
            currentStart = parseSrtTimestamp(m.captured(1));
            currentEnd = parseSrtTimestamp(m.captured(2));
            currentText = "";
        } else if (currentStart >= 0.0) {
            // Check if it's just the numeric sequence line
            bool isIndex;
            trimmed.toInt(&isIndex);
            if (!isIndex) {
                if (!currentText.isEmpty()) currentText += " ";
                currentText += trimmed;
            }
        }
    }

    // Final block
    if (currentStart >= 0.0 && !currentText.isEmpty()) {
        LyricLine ll;
        ll.text = currentText.trimmed();
        ll.startTime = currentStart;
        ll.endTime = currentEnd;
        data.addLine(ll);
    }

    return data;
}

// ─── LRC Parser ──────────────────────────────────────────────────────────────

LyricsData SubtitleParser::parseLrc(const QString& content) {
    LyricsData data;
    static QRegularExpression metaRe(R"(\[\s*(\w+)\s*:\s*(.*)\s*\])");
    static QRegularExpression rangeRe(R"(\[\s*(\d+:[\d\.\:]+)\s*[-\u2013\u2014]\s*(\d+:[\d\.\:]+)\s*\]\s*(.*))");
    static QRegularExpression lineRe(R"(\[\s*(\d+:[\d\.\:]+)\s*\]\s*(.*))");

    for (const auto& rawLine : content.split('\n')) {
        QString trimmed = rawLine.trimmed();
        if (trimmed.isEmpty()) continue;

        // Check metadata
        auto metaMatch = metaRe.match(trimmed);
        if (metaMatch.hasMatch() && !lineRe.match(trimmed).hasMatch() && !rangeRe.match(trimmed).hasMatch()) {
            QString key = metaMatch.captured(1).toLower();
            QString val = metaMatch.captured(2).trimmed();
            if (key == "ti") data.title  = val;
            if (key == "ar") data.artist = val;
            continue;
        }

        // Try range format first: [MM:SS.xx - MM:SS.xx]
        auto rangeMatch = rangeRe.match(trimmed);
        if (rangeMatch.hasMatch()) {
            double start = parseLrcTimestamp(rangeMatch.captured(1));
            double end = parseLrcTimestamp(rangeMatch.captured(2));
            QString text = rangeMatch.captured(3).trimmed();

            if (!text.isEmpty()) {
                LyricLine line;
                line.text      = text;
                line.startTime = start;
                line.endTime   = end;
                line.addWord(text, start, end, 1.0);
                data.addLine(line);
            }
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

    // Post-process: set end times to next line's start time if they weren't explicitly set by range format
    for (int i = 0; i < data.lines.size() - 1; ++i) {
        if (data.lines[i].endTime <= data.lines[i].startTime) {
            data.lines[i].endTime = data.lines[i + 1].startTime;
        }
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

LyricsData SubtitleParser::parsePlainText(const QString& content) {
    // If it contains timestamp brackets like [00:00.00, treat it as timed format
    if (content.contains(QRegularExpression(R"(\[\s*(?:\d+:)?\d+:[\d\.\:]+\s*\])"))) {
        return parseLrc(content);
    }
    
    // Check for Range pattern directly [00:00 - 00:05]
    if (content.contains(QRegularExpression(R"(\[\s*(?:\d+:)?\d+:[\d\.\:]+\s*[-\u2013\u2014])"))) {
        return parseLrc(content);
    }

    LyricsData data;
    data.importFromText(content);
    return data;
}

} // namespace ncktv
