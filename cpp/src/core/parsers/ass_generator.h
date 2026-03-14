#pragma once
/*
 * NC-KTV Core — ASS Subtitle Generator
 * Port of ass_generator.py — Generates .ass files for karaoke export
 */

#include <QString>
#include <QColor>
#include "lyrics/lyrics_data.h"
#include "../timeline/ncktv_core_data.hpp"

namespace ncktv {

struct AssStyle {
    QString fontFamily = "Arial";
    int     fontSize   = 48;
    QColor  primaryColor   = Qt::white;
    QColor  highlightColor = QColor(255, 215, 0);  // Gold
    QColor  outlineColor   = Qt::black;
    int     outlineWidth   = 2;
    QString position       = "bottom";   // "top", "middle", "bottom"
};

class AssGenerator {
public:
    /// Generate ASS subtitle content from lyrics data
    static QString generate(const LyricsData& lyrics, const AssStyle& style = {},
                            int videoWidth = 1920, int videoHeight = 1080);

    static QString generate(const core::LyricsData& lyrics, const AssStyle& style = {},
                            int videoWidth = 1920, int videoHeight = 1080);

private:
    static QString formatAssTime(double seconds);
    static QString colorToAss(const QColor& color);
};

} // namespace ncktv
