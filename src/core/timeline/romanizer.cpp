#include "romanizer.h"
#include <QChar>
#include <QStringList>

namespace ncktv {

// Korean Revised Romanization tables
static const QString choseongTable[] = {
    "g", "kk", "n", "d", "tt", "r", "m", "b", "pp", "s",
    "ss", "", "j", "jj", "ch", "k", "t", "p", "h"
};

static const QString jungseongTable[] = {
    "a", "ae", "ya", "yae", "eo", "e", "yeo", "ye", "o", "wa",
    "wae", "oe", "yo", "u", "wo", "we", "wi", "yu", "eu", "ui", "i"
};

static const QString jongseongTable[] = {
    "", "g", "kk", "gs", "n", "nj", "nh", "d", "l", "lg",
    "lm", "lb", "ls", "lt", "lp", "lh", "m", "b", "bs", "s",
    "ss", "ng", "j", "ch", "k", "t", "p", "h"
};

// Japanese Hiragana Romaji table (Unicode 0x3041 to 0x3096)
static const QString hiraganaTable[] = {
    "a", "a", "i", "i", "u", "u", "e", "e", "o", "o", // 3041 - 304A
    "ka", "ga", "ki", "gi", "ku", "gu", "ke", "ge", "ko", "go", // 304B - 3054
    "sa", "za", "shi", "ji", "su", "zu", "se", "ze", "so", "zo", // 3055 - 0x305E
    "ta", "da", "chi", "ji", "tsu", "tsu", "zu", "te", "de", "to", "do", // 305F - 3069
    "na", "ni", "nu", "ne", "no", // 306A - 306E
    "ha", "ba", "pa", "hi", "bi", "pi", "fu", "bu", "pu", "he", "be", "pe", "ho", "bo", "po", // 306F - 307D
    "ma", "mi", "mu", "me", "mo", // 307E - 3082
    "ya", "ya", "yu", "yu", "yo", "yo", // 3083 - 3088
    "ra", "ri", "ru", "re", "ro", // 3089 - 308D
    "wa", "wa", "wi", "we", "wo", // 0x308E - 0x3092
    "n", // 0x3093
    "vu", "ka", "ke" // 0x3094 - 0x3096
};

char32_t Romanizer::toHiragana(char32_t cp) {
    if (cp >= 0x30A1 && cp <= 0x30F6) {
        return cp - 0x60;
    }
    return cp;
}

QString Romanizer::romanize(const QString& text) {
    QString result;
    int i = 0;
    int len = text.length();

    while (i < len) {
        char32_t cp = text[i].unicode();

        // 1. Check Korean Hangul [0xAC00, 0xD7A3]
        if (cp >= 0xAC00 && cp <= 0xD7A3) {
            int index = cp - 0xAC00;
            int choseongIndex = index / (21 * 28);
            int jungseongIndex = (index % (21 * 28)) / 28;
            int jongseongIndex = index % 28;

            result.append(choseongTable[choseongIndex]);
            result.append(jungseongTable[jungseongIndex]);
            result.append(jongseongTable[jongseongIndex]);

            i++;
            continue;
        }

        // 2. Check Japanese small tsu / double consonant (っ / ッ)
        if (cp == 0x3063 || cp == 0x30C3) {
            if (i + 1 < len) {
                char32_t nextCp = text[i + 1].unicode();
                char32_t nextMapped = toHiragana(nextCp);
                if (nextMapped >= 0x3041 && nextMapped <= 0x3096) {
                    QString nextRomaji = hiraganaTable[nextMapped - 0x3041];
                    if (!nextRomaji.isEmpty()) {
                        QChar firstChar = nextRomaji[0];
                        if (firstChar != 'a' && firstChar != 'e' && firstChar != 'i' &&
                            firstChar != 'o' && firstChar != 'u') {
                            result.append(firstChar);
                        }
                    }
                }
            }
            i++;
            continue;
        }

        // 3. Check Japanese Hiragana / Katakana
        char32_t mappedCp = toHiragana(cp);
        if (mappedCp >= 0x3041 && mappedCp <= 0x3096) {
            QString romaji = hiraganaTable[mappedCp - 0x3041];

            // Lookahead for Japanese small blends (ゃ, ゅ, ょ or Katakana equivalents)
            if (i + 1 < len) {
                char32_t nextCp = toHiragana(text[i + 1].unicode());
                if (nextCp == 0x3083 || nextCp == 0x3085 || nextCp == 0x3087) {
                    if (romaji.length() > 1 && romaji.endsWith('i')) {
                        QString prefix = romaji.left(romaji.length() - 1);
                        QString blendChar;
                        if (nextCp == 0x3083) blendChar = "a";
                        else if (nextCp == 0x3085) blendChar = "u";
                        else if (nextCp == 0x3087) blendChar = "o";

                        if (prefix.endsWith("sh") || prefix.endsWith("ch") || prefix == "j") {
                            romaji = prefix + blendChar;
                        } else {
                            romaji = prefix + "y" + blendChar;
                        }
                        i += 2; // consume both base and small kana
                        result.append(romaji);
                        continue;
                    }
                }
            }

            result.append(romaji);
            i++;
            continue;
        }

        // 4. Handle Japanese long vowel marker
        if (cp == 0x30FC || cp == 0xFF0D) {
            i++;
            continue;
        }

        // 5. Default: copy character
        result.append(text[i]);
        i++;
    }

    return result;
}

} // namespace ncktv
