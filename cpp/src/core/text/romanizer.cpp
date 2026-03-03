/*
 * NC-KTV Core — Romanizer Implementation
 * Uses ICU4C Transliterator when available, fallback to basic tables
 */

#include "romanizer.h"

#ifdef NCKTV_HAS_ICU
#include <unicode/translit.h>
#include <unicode/unistr.h>
#endif

namespace ncktv {

bool Romanizer::needsRomanization(const QString& text) {
    for (const QChar& ch : text) {
        if (ch.script() != QChar::Script_Latin &&
            ch.script() != QChar::Script_Common &&
            ch.script() != QChar::Script_Inherited &&
            !ch.isPunct() && !ch.isSpace() && !ch.isDigit())
        {
            return true;
        }
    }
    return false;
}

QString Romanizer::romanize(const QString& text) {
    if (!needsRomanization(text)) return text;

#ifdef NCKTV_HAS_ICU
    UErrorCode status = U_ZERO_ERROR;
    icu::UnicodeString ustr = icu::UnicodeString::fromUTF8(text.toStdString());

    // Use "Any-Latin; Latin-ASCII" transliteration
    std::unique_ptr<icu::Transliterator> trans(
        icu::Transliterator::createInstance(
            "Any-Latin; Latin-ASCII", UTRANS_FORWARD, status));

    if (U_SUCCESS(status) && trans) {
        trans->transliterate(ustr);
        std::string result;
        ustr.toUTF8String(result);
        return QString::fromStdString(result);
    }
#endif

    // Fallback: return original text
    return text;
}

QString Romanizer::romanizeJapanese(const QString& text) {
#ifdef NCKTV_HAS_ICU
    UErrorCode status = U_ZERO_ERROR;
    icu::UnicodeString ustr = icu::UnicodeString::fromUTF8(text.toStdString());

    std::unique_ptr<icu::Transliterator> trans(
        icu::Transliterator::createInstance(
            "Katakana-Latin; Hiragana-Latin", UTRANS_FORWARD, status));

    if (U_SUCCESS(status) && trans) {
        trans->transliterate(ustr);
        std::string result;
        ustr.toUTF8String(result);
        return QString::fromStdString(result);
    }
#endif
    return text;
}

QString Romanizer::romanizeKorean(const QString& text) {
#ifdef NCKTV_HAS_ICU
    UErrorCode status = U_ZERO_ERROR;
    icu::UnicodeString ustr = icu::UnicodeString::fromUTF8(text.toStdString());

    std::unique_ptr<icu::Transliterator> trans(
        icu::Transliterator::createInstance(
            "Hangul-Latin", UTRANS_FORWARD, status));

    if (U_SUCCESS(status) && trans) {
        trans->transliterate(ustr);
        std::string result;
        ustr.toUTF8String(result);
        return QString::fromStdString(result);
    }
#endif
    return text;
}

QString Romanizer::romanizeIndic(const QString& text) {
#ifdef NCKTV_HAS_ICU
    UErrorCode status = U_ZERO_ERROR;
    icu::UnicodeString ustr = icu::UnicodeString::fromUTF8(text.toStdString());

    std::unique_ptr<icu::Transliterator> trans(
        icu::Transliterator::createInstance(
            "Devanagari-Latin", UTRANS_FORWARD, status));

    if (U_SUCCESS(status) && trans) {
        trans->transliterate(ustr);
        std::string result;
        ustr.toUTF8String(result);
        return QString::fromStdString(result);
    }
#endif
    return text;
}

} // namespace ncktv
