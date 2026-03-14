/*
 * NC-KTV Core — ASS Subtitle Generator Implementation
 * Port of ass_generator.py
 */

#include "ass_generator.h"
#include <cmath>

namespace ncktv {

QString AssGenerator::formatAssTime(double seconds) {
    int h   = static_cast<int>(seconds) / 3600;
    int m   = (static_cast<int>(seconds) % 3600) / 60;
    int s   = static_cast<int>(seconds) % 60;
    int cs  = static_cast<int>(std::fmod(seconds, 1.0) * 100.0);
    return QStringLiteral("%1:%2:%3.%4")
        .arg(h)
        .arg(m, 2, 10, QChar('0'))
        .arg(s, 2, 10, QChar('0'))
        .arg(cs, 2, 10, QChar('0'));
}

QString AssGenerator::colorToAss(const QColor& color) {
    // ASS uses &HBBGGRR& format (BGR, not RGB)
    return QStringLiteral("&H%1%2%3&")
        .arg(color.blue(), 2, 16, QChar('0'))
        .arg(color.green(), 2, 16, QChar('0'))
        .arg(color.red(), 2, 16, QChar('0'))
        .toUpper();
}

QString AssGenerator::generate(const LyricsData& lyrics, const AssStyle& style,
                                int videoWidth, int videoHeight)
{
    QStringList output;

    // Header
    output << "[Script Info]";
    output << "ScriptType: v4.00+";
    output << "Title: " + lyrics.title.value_or("NC-KTV Karaoke");
    output << QStringLiteral("PlayResX: %1").arg(videoWidth);
    output << QStringLiteral("PlayResY: %1").arg(videoHeight);
    output << "WrapStyle: 0";
    output << "";

    // Styles
    output << "[V4+ Styles]";
    output << "Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, "
              "OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, "
              "ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, "
              "Alignment, MarginL, MarginR, MarginV, Encoding";

    // Calculate alignment based on position
    int alignment = 2;   // Bottom center (default)
    if (style.position == "top")    alignment = 8;
    if (style.position == "middle") alignment = 5;

    output << QStringLiteral(
        "Style: Default,%1,%2,%3,%4,%5,&H00000000,0,0,0,0,"
        "100,100,0,0,1,%6,0,%7,20,20,30,1")
        .arg(style.fontFamily)
        .arg(style.fontSize)
        .arg(colorToAss(style.primaryColor))
        .arg(colorToAss(style.highlightColor))
        .arg(colorToAss(style.outlineColor))
        .arg(style.outlineWidth)
        .arg(alignment);
    output << "";

    // Events
    output << "[Events]";
    output << "Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text";

    for (const auto& line : lyrics.lines) {
        QString start = formatAssTime(line.startTime);
        QString end   = formatAssTime(line.endTime);

        // Build karaoke-timed text
        QString text;
        if (!line.words.isEmpty()) {
            for (const auto& word : line.words) {
                int durationCs = static_cast<int>(word.duration() * 100.0);
                text += QStringLiteral("{\\kf%1}%2").arg(durationCs).arg(word.word);
            }
        } else {
            // No word-level timing — use whole line
            int durationCs = static_cast<int>(line.duration() * 100.0);
            text = QStringLiteral("{\\kf%1}%2").arg(durationCs).arg(line.text);
        }

        output << QStringLiteral("Dialogue: 0,%1,%2,Default,,0,0,0,,%3")
                      .arg(start, end, text);
    }

    return output.join('\n');
}

QString AssGenerator::generate(const core::LyricsData& lyrics, const AssStyle& style,
                                int videoWidth, int videoHeight)
{
    QStringList output;

    // Header
    output << "[Script Info]";
    output << "ScriptType: v4.00+";
    
    // Look for title in metadata or default
    QString title = "NC-KTV Karaoke";
    if (lyrics.metadata.find("title") != lyrics.metadata.end()) {
        try {
            title = QString::fromStdString(std::any_cast<std::string>(lyrics.metadata.at("title")));
        } catch(...) {}
    }
    
    output << "Title: " + title;
    output << QStringLiteral("PlayResX: %1").arg(videoWidth);
    output << QStringLiteral("PlayResY: %1").arg(videoHeight);
    output << "WrapStyle: 0";
    output << "";

    // Styles
    output << "[V4+ Styles]";
    output << "Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, "
              "OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, "
              "ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, "
              "Alignment, MarginL, MarginR, MarginV, Encoding";

    int alignment = 2;
    if (style.position == "top")    alignment = 8;
    if (style.position == "middle") alignment = 5;

    output << QStringLiteral(
        "Style: Default,%1,%2,%3,%4,%5,&H00000000,0,0,0,0,"
        "100,100,0,0,1,%6,0,%7,20,20,30,1")
        .arg(style.fontFamily)
        .arg(style.fontSize)
        .arg(colorToAss(style.primaryColor))
        .arg(colorToAss(style.highlightColor))
        .arg(colorToAss(style.outlineColor))
        .arg(style.outlineWidth)
        .arg(alignment);
    output << "";

    // Events
    output << "[Events]";
    output << "Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text";

    for (const auto& line : lyrics.lines) {
        QString start = formatAssTime(line.start_time);
        QString end   = formatAssTime(line.end_time);

        QString text;
        if (!line.tokens.empty()) {
            for (const auto& token : line.tokens) {
                double duration = std::max(0.0f, token.end_time - token.start_time);
                int durationCs = static_cast<int>(duration * 100.0);
                text += QStringLiteral("{\\kf%1}%2").arg(durationCs).arg(QString::fromStdString(token.text));
            }
        } else {
            int durationCs = static_cast<int>(line.duration() * 100.0);
            text = QStringLiteral("{\\kf%1}%2").arg(durationCs).arg(QString::fromStdString(line.text));
        }

        output << QStringLiteral("Dialogue: 0,%1,%2,Default,,0,0,0,,%3")
                      .arg(start, end, text);
    }

    return output.join('\n');
}

} // namespace ncktv
