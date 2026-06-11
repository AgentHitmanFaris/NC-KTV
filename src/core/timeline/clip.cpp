#include "clip.h"
#include "romanizer.h"
#include <QUuid>
#include <algorithm>
#include <QStringList>
#include <QRegularExpression>

namespace ncktv {

Clip::Clip(QObject* parent)
    : QObject(parent),
      m_clipId(QUuid::createUuid().toString(QUuid::Id128)) {
}

Clip::Clip(const QString& id, Type type, qint64 start, qint64 dur, QObject* parent)
    : QObject(parent),
      m_clipId(id.isEmpty() ? QUuid::createUuid().toString(QUuid::Id128) : id),
      m_clipType(type),
      m_startTime(start),
      m_duration(dur) {
    if (type == Lyrics) {
        autoGenerateSyllables();
    }
}

void Clip::setTrackId(const QString& id) {
    if (m_trackId != id) {
        m_trackId = id;
        emit trackIdChanged();
    }
}

void Clip::setClipType(int type) {
    auto castedType = static_cast<Type>(type);
    if (m_clipType != castedType) {
        m_clipType = castedType;
        emit clipTypeChanged();
        if (castedType == Lyrics) {
            autoGenerateSyllables();
        }
    }
}

void Clip::setStartTime(qint64 start) {
    if (m_startTime != start) {
        m_startTime = start;
        emit startTimeChanged();
    }
}

void Clip::setDuration(qint64 dur) {
    if (m_duration != dur) {
        m_duration = dur;
        emit durationChanged();
        if (m_clipType == Lyrics) {
            autoGenerateSyllables();
        }
    }
}

void Clip::setSourceFile(const QString& path) {
    if (m_sourceFile != path) {
        m_sourceFile = path;
        emit sourceFileChanged();
    }
}

void Clip::setSourceStart(qint64 start) {
    if (m_sourceStart != start) {
        m_sourceStart = start;
        emit sourceStartChanged();
    }
}

void Clip::setSourceDuration(qint64 dur) {
    if (m_sourceDuration != dur) {
        m_sourceDuration = dur;
        emit sourceDurationChanged();
    }
}

void Clip::setLyricText(const QString& text) {
    if (m_lyricText != text) {
        if (text.contains('<') && text.contains('>')) {
            parseLrcSyllables(text);
        } else {
            m_lyricText = text;
            emit lyricTextChanged();
            autoGenerateSyllables();
        }
    }
}

void Clip::setSyllables(const QVariantList& list) {
    m_syllables = list;
    emit syllablesChanged();
}

void Clip::moveTo(qint64 newStart) {
    setStartTime(newStart);
}

void Clip::resizeClip(qint64 newDuration, bool fromStart) {
    if (newDuration < 0) newDuration = 0;
    
    if (fromStart) {
        qint64 diff = newDuration - m_duration;
        m_startTime -= diff;
        m_duration = newDuration;
        emit startTimeChanged();
        emit durationChanged();
        if (m_clipType == Lyrics) {
            autoGenerateSyllables();
        }
    } else {
        setDuration(newDuration);
    }
}

void Clip::parseLrcSyllables(const QString& rawLrc) {
    m_syllables.clear();
    
    QString cleanText;
    qint64 lastTime = 0; // Relative to clip start in microseconds
    QString currentWord;
    
    int i = 0;
    int len = rawLrc.length();
    QVariantList parsedList;
    
    while (i < len) {
        if (rawLrc[i] == '<') {
            int closingIdx = rawLrc.indexOf('>', i);
            if (closingIdx != -1) {
                QString tagContent = rawLrc.mid(i + 1, closingIdx - i - 1).trimmed();
                
                double seconds = 0.0;
                QStringList parts = tagContent.split(':');
                if (parts.size() == 2) {
                    int mins = parts[0].toInt();
                    double secs = parts[1].toDouble();
                    seconds = mins * 60.0 + secs;
                } else {
                    seconds = tagContent.toDouble();
                }
                
                qint64 absoluteMicroseconds = static_cast<qint64>(seconds * 1000000.0);
                qint64 relativeMicroseconds = (std::max)(0LL, absoluteMicroseconds - m_startTime);
                
                if (!currentWord.isEmpty()) {
                    QVariantMap syl;
                    syl["text"] = currentWord;
                    syl["relativeStart"] = lastTime;
                    syl["duration"] = (std::max)(100000LL, relativeMicroseconds - lastTime);
                    syl["x1"] = 0.25;
                    syl["y1"] = 0.25;
                    syl["x2"] = 0.75;
                    syl["y2"] = 0.75;
                    parsedList.append(syl);
                    cleanText += currentWord;
                }
                
                lastTime = relativeMicroseconds;
                currentWord.clear();
                i = closingIdx + 1;
                continue;
            }
        }
        
        currentWord.append(rawLrc[i]);
        ++i;
    }
    
    if (!currentWord.isEmpty()) {
        QVariantMap syl;
        syl["text"] = currentWord;
        syl["relativeStart"] = lastTime;
        syl["duration"] = (std::max)(100000LL, m_duration - lastTime);
        syl["x1"] = 0.25;
        syl["y1"] = 0.25;
        syl["x2"] = 0.75;
        syl["y2"] = 0.75;
        parsedList.append(syl);
        cleanText += currentWord;
    }
    
    m_syllables = parsedList;
    m_lyricText = cleanText.isEmpty() ? rawLrc : cleanText;
    
    emit lyricTextChanged();
    emit syllablesChanged();
}

void Clip::updateSyllable(int index, qint64 relativeStart, qint64 duration) {
    if (index >= 0 && index < m_syllables.size()) {
        QVariantMap syl = m_syllables[index].toMap();
        syl["relativeStart"] = relativeStart;
        syl["duration"] = duration;
        m_syllables[index] = syl;
        emit syllablesChanged();
    }
}

void Clip::updateSyllableCurve(int index, double x1, double y1, double x2, double y2) {
    if (index >= 0 && index < m_syllables.size()) {
        QVariantMap syl = m_syllables[index].toMap();
        syl["x1"] = x1;
        syl["y1"] = y1;
        syl["x2"] = x2;
        syl["y2"] = y2;
        m_syllables[index] = syl;
        emit syllablesChanged();
    }
}

void Clip::romanize() {
    if (m_clipType != Lyrics) return;
    
    QVariantList newSylList;
    QString cleanText;
    for (const QVariant& s : m_syllables) {
        QVariantMap map = s.toMap();
        QString oldText = map["text"].toString();
        QString newText = Romanizer::romanize(oldText);
        map["text"] = newText;
        newSylList.append(map);
        cleanText.append(newText);
    }
    
    m_syllables = newSylList;
    m_lyricText = cleanText;
    
    emit lyricTextChanged();
    emit syllablesChanged();
}

void Clip::autoGenerateSyllables() {
    m_syllables.clear();
    if (m_lyricText.isEmpty() || m_duration <= 0) {
        return;
    }

    // Split text by whitespace into words
    QStringList rawWords = m_lyricText.split(' ', Qt::SkipEmptyParts);
    if (rawWords.isEmpty()) {
        return;
    }

    // Heuristics: Helper function to split a word into syllables
    auto splitWordIntoSyllables = [](const QString& word) -> QStringList {
        QString clean = word.trimmed();
        QString punctuation = "";
        while (!clean.isEmpty() && (clean.endsWith(',') || clean.endsWith('.') || clean.endsWith('!') || clean.endsWith('?'))) {
            punctuation = clean.right(1) + punctuation;
            clean.chop(1);
        }
        
        if (clean.length() <= 3) {
            return { clean + punctuation };
        }
        
        QString lower = clean.toLower();
        QList<int> vowelIndices;
        bool inVowel = false;
        for (int i = 0; i < lower.length(); ++i) {
            QChar ch = lower[i];
            bool isV = (ch == 'a' || ch == 'e' || ch == 'i' || ch == 'o' || ch == 'u' || ch == 'y');
            if (isV) {
                if (!inVowel) {
                    vowelIndices.append(i);
                    inVowel = true;
                }
            } else {
                inVowel = false;
            }
        }
        
        // Handle silent 'e'
        if (lower.endsWith('e') && vowelIndices.size() > 1) {
            bool endsWithLe = false;
            if (lower.endsWith("le") && lower.length() >= 3) {
                QChar prev = lower[lower.length() - 3];
                if (prev != 'a' && prev != 'e' && prev != 'i' && prev != 'o' && prev != 'u' && prev != 'y') {
                    endsWithLe = true;
                }
            }
            if (!endsWithLe) {
                if (!vowelIndices.isEmpty() && vowelIndices.last() == lower.length() - 1) {
                    vowelIndices.removeLast();
                }
            }
        }
        
        // Handle silent 'ed' unless preceded by 't' or 'd'
        if (lower.endsWith("ed") && lower.length() >= 3 && vowelIndices.size() > 1) {
            QChar prev = lower[lower.length() - 3];
            if (prev != 't' && prev != 'd') {
                if (!vowelIndices.isEmpty() && vowelIndices.last() == lower.length() - 2) {
                    vowelIndices.removeLast();
                }
            }
        }
        
        if (vowelIndices.size() <= 1) {
            return { clean + punctuation };
        }
        
        QStringList syllables;
        int lastSplit = 0;
        
        for (int s = 0; s < vowelIndices.size() - 1; ++s) {
            int v1 = vowelIndices[s];
            int v2 = vowelIndices[s + 1];
            
            int consonantsStart = -1;
            int consonantsCount = 0;
            
            for (int i = v1 + 1; i < v2; ++i) {
                QChar ch = lower[i];
                bool isC = !(ch == 'a' || ch == 'e' || ch == 'i' || ch == 'o' || ch == 'u' || ch == 'y');
                if (isC) {
                    if (consonantsStart == -1) {
                        consonantsStart = i;
                    }
                    consonantsCount++;
                }
            }
            
            int splitPt = v1 + 1;
            if (consonantsCount == 1) {
                if (consonantsStart - lastSplit == 1) {
                    splitPt = consonantsStart + 1;
                } else {
                    splitPt = consonantsStart;
                }
            } else if (consonantsCount >= 2) {
                splitPt = consonantsStart + 1; // Split between consonants
            }
            
            QString syl = clean.mid(lastSplit, splitPt - lastSplit) + "-";
            syllables.append(syl);
            lastSplit = splitPt;
        }
        
        QString lastSyl = clean.mid(lastSplit);
        if (!lastSyl.isEmpty()) {
            if (!syllables.isEmpty()) {
                lastSyl = "-" + lastSyl;
            }
            syllables.append(lastSyl + punctuation);
        }
        
        return syllables;
    };

    // First, split every word into syllables and build a list of all tokens (syllables)
    struct TokenInfo {
        QString text;
        double weight = 1.0;
    };
    QList<TokenInfo> tokens;

    for (int w = 0; w < rawWords.size(); ++w) {
        const QString& word = rawWords[w];
        QStringList syllables = splitWordIntoSyllables(word);
        
        // Append space to the very last syllable of this word (unless it's the last word of the line)
        for (int s = 0; s < syllables.size(); ++s) {
            QString tokText = syllables[s];
            if (s == syllables.size() - 1 && w < rawWords.size() - 1) {
                tokText += " ";
            }
            
            // Weight calculation for this syllable
            QString cleanSyl = syllables[s].trimmed();
            cleanSyl.remove(QRegularExpression("[.,?!\"'\\-]"));
            double weight = cleanSyl.length();
            
            // Count vowels in syllable
            int vowelCount = 0;
            QString lowerSyl = cleanSyl.toLower();
            for (int i = 0; i < lowerSyl.length(); ++i) {
                QChar ch = lowerSyl[i];
                if (ch == 'a' || ch == 'e' || ch == 'i' || ch == 'o' || ch == 'u' || ch == 'y') {
                    vowelCount++;
                }
            }
            weight += vowelCount * 1.5;
            
            // Punctuation weight bump
            if (syllables[s].contains(',') || syllables[s].contains('.') || syllables[s].contains('!') || syllables[s].contains('?')) {
                weight += 2.0;
            }
            
            tokens.append({tokText, qMax(1.0, weight)});
        }
    }

    if (tokens.isEmpty()) {
        return;
    }

    // Distribute duration proportionally based on weights
    double totalWeight = 0.0;
    for (const auto& tok : tokens) {
        totalWeight += tok.weight;
    }

    QVariantList list;
    qint64 accumulatedTime = 0;
    
    for (int i = 0; i < tokens.size(); ++i) {
        QVariantMap syl;
        syl["text"] = tokens[i].text;
        syl["relativeStart"] = accumulatedTime;
        
        qint64 duration = 0;
        if (i == tokens.size() - 1) {
            duration = m_duration - accumulatedTime;
        } else {
            duration = static_cast<qint64>((tokens[i].weight / totalWeight) * m_duration);
            // Ensure no syllable is less than 50ms
            duration = qMax(50000LL, duration);
        }
        
        syl["duration"] = duration;
        syl["x1"] = 0.25;
        syl["y1"] = 0.25;
        syl["x2"] = 0.75;
        syl["y2"] = 0.75;
        accumulatedTime += duration;
        list.append(syl);
    }
    
    m_syllables = list;
    emit syllablesChanged();
}

nlohmann::json Clip::toJson() const {
    nlohmann::json j = nlohmann::json::object();
    j["clipId"] = m_clipId.toStdString();
    j["trackId"] = m_trackId.toStdString();
    j["clipType"] = static_cast<int>(m_clipType);
    j["startTime"] = m_startTime;
    j["duration"] = m_duration;
    j["sourceFile"] = m_sourceFile.toStdString();
    j["sourceStart"] = m_sourceStart;
    j["sourceDuration"] = m_sourceDuration;
    j["lyricText"] = m_lyricText.toStdString();

    nlohmann::json syllablesJson = nlohmann::json::array();
    for (const QVariant& s : m_syllables) {
        QVariantMap map = s.toMap();
        nlohmann::json sObj = nlohmann::json::object();
        sObj["text"] = map["text"].toString().toStdString();
        sObj["relativeStart"] = map["relativeStart"].toLongLong();
        sObj["duration"] = map["duration"].toLongLong();
        sObj["x1"] = map.contains("x1") ? map["x1"].toDouble() : 0.25;
        sObj["y1"] = map.contains("y1") ? map["y1"].toDouble() : 0.25;
        sObj["x2"] = map.contains("x2") ? map["x2"].toDouble() : 0.75;
        sObj["y2"] = map.contains("y2") ? map["y2"].toDouble() : 0.75;
        syllablesJson.push_back(sObj);
    }
    j["syllables"] = syllablesJson;

    return j;
}

Clip* Clip::fromJson(const nlohmann::json& j, QObject* parent) {
    QString id = QString::fromStdString(j.value("clipId", ""));
    int typeVal = j.value("clipType", 0);
    qint64 start = j.value("startTime", 0LL);
    qint64 dur = j.value("duration", 0LL);

    Clip* clip = new Clip(id, static_cast<Type>(typeVal), start, dur, parent);
    clip->setTrackId(QString::fromStdString(j.value("trackId", "")));
    clip->setSourceFile(QString::fromStdString(j.value("sourceFile", "")));
    clip->setSourceStart(j.value("sourceStart", 0LL));
    clip->setSourceDuration(j.value("sourceDuration", 0LL));
    clip->setLyricText(QString::fromStdString(j.value("lyricText", "")));

    if (j.contains("syllables") && j["syllables"].is_array()) {
        QVariantList list;
        for (const auto& sObj : j["syllables"]) {
            QVariantMap map;
            map["text"] = QString::fromStdString(sObj.value("text", ""));
            map["relativeStart"] = sObj.value("relativeStart", 0LL);
            map["duration"] = sObj.value("duration", 0LL);
            map["x1"] = sObj.value("x1", 0.25);
            map["y1"] = sObj.value("y1", 0.25);
            map["x2"] = sObj.value("x2", 0.75);
            map["y2"] = sObj.value("y2", 0.75);
            list.append(map);
        }
        clip->setSyllables(list);
    } else if (static_cast<Type>(typeVal) == Lyrics) {
        clip->autoGenerateSyllables();
    }

    return clip;
}

} // namespace ncktv
