#pragma once
/*
 * NC-KTV Core — Text Romanizer
 * Port of romanizer.py — Transliterates non-Latin scripts
 */

#include <QString>

namespace ncktv {

class Romanizer {
public:
    /// Romanize text (auto-detect language)
    static QString romanize(const QString& text);

    /// Check if text contains non-Latin characters
    static bool needsRomanization(const QString& text);

    /// Language-specific romanization
    static QString romanizeJapanese(const QString& text);
    static QString romanizeKorean(const QString& text);
    static QString romanizeIndic(const QString& text);
};

} // namespace ncktv
