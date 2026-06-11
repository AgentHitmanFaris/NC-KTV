#pragma once

#include <QString>

namespace ncktv {

class Romanizer {
public:
    // Main entry point for romanization.
    // Handles Korean Hangul (to Revised Romanization) and Japanese Hiragana/Katakana (to Hepburn Romaji).
    // Non-Korean and non-Japanese characters are left unchanged.
    static QString romanize(const QString& text);

private:
    static char32_t toHiragana(char32_t cp);
};

} // namespace ncktv
