#include "lyric_engine.h"
#include "timeline_manager.h"
#include "track.h"
#include "clip.h"

#include <algorithm>
#include <QVariantMap>

namespace ncktv {

// ─────────────────────────────────────────────────────────────────────────────
// Construction
// ─────────────────────────────────────────────────────────────────────────────

LyricEngine::LyricEngine(TimelineManager* manager, QObject* parent)
    : QObject(parent)
    , m_manager(manager)
{
    // Rebuild whenever the track model changes
    if (m_manager && m_manager->trackListModel()) {
        connect(m_manager->trackListModel(), &QAbstractItemModel::rowsInserted,
                this, &LyricEngine::rebuildLineCache);
        connect(m_manager->trackListModel(), &QAbstractItemModel::rowsRemoved,
                this, &LyricEngine::rebuildLineCache);
        connect(m_manager->trackListModel(), &QAbstractItemModel::modelReset,
                this, &LyricEngine::rebuildLineCache);
    }

    // Rebuild when a project is loaded or cleared
    if (m_manager) {
        connect(m_manager, &TimelineManager::projectLoaded,
                this, &LyricEngine::rebuildLineCache);
        connect(m_manager, &TimelineManager::projectCleared,
                this, &LyricEngine::rebuildLineCache);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

void LyricEngine::updatePlaybackPosition(qint64 currentTimeUs)
{
    if (currentTimeUs == m_lastUpdateTimeUs)
        return;

    m_lastUpdateTimeUs = currentTimeUs;
    updateActiveState(currentTimeUs);
}

void LyricEngine::setDisplayMode(int mode)
{
    auto newMode = static_cast<DisplayMode>(qBound(0, mode, 3));
    if (m_displayMode != newMode) {
        m_displayMode = newMode;
        emit displayModeChanged();
    }
}

void LyricEngine::reset()
{
    m_activeLineIndex = -1;
    m_nextLineIndex = -1;
    m_activeWordIndex = -1;
    m_sweepProgress = 0.0;
    m_activeWordProgress = 0.0;
    m_transitionState = Idle;
    m_lastUpdateTimeUs = -1;

    emit activeLineChanged();
    emit sweepProgressChanged();
    emit activeWordChanged();
    emit upcomingLinesChanged();
    emit lineTransitionStateChanged();
}

// ─────────────────────────────────────────────────────────────────────────────
// Property Getters
// ─────────────────────────────────────────────────────────────────────────────

QString LyricEngine::activeLineText() const
{
    if (m_activeLineIndex >= 0 && m_activeLineIndex < m_lineCache.size())
        return m_lineCache[m_activeLineIndex].text;
    return {};
}

QString LyricEngine::nextLineText() const
{
    if (m_nextLineIndex >= 0 && m_nextLineIndex < m_lineCache.size())
        return m_lineCache[m_nextLineIndex].text;
    return {};
}

QVariantList LyricEngine::activeLineSyllables() const
{
    QVariantList result;
    if (m_activeLineIndex >= 0 && m_activeLineIndex < m_lineCache.size()) {
        const auto& line = m_lineCache[m_activeLineIndex];
        for (const auto& word : line.words) {
            QVariantMap m;
            m[QStringLiteral("text")] = word.text;
            m[QStringLiteral("relativeStart")] = word.relativeStartUs;
            m[QStringLiteral("duration")] = word.durationUs;
            m[QStringLiteral("x1")] = word.x1;
            m[QStringLiteral("y1")] = word.y1;
            m[QStringLiteral("x2")] = word.x2;
            m[QStringLiteral("y2")] = word.y2;
            result.append(m);
        }
    }
    return result;
}

QVariantList LyricEngine::upcomingLines() const
{
    QVariantList result;
    if (m_lineCache.isEmpty())
        return result;

    // Find the starting index for upcoming lines
    int startIdx = (m_activeLineIndex >= 0) ? m_activeLineIndex + 1 : m_nextLineIndex;
    if (startIdx < 0)
        startIdx = 0;

    // Return up to 5 upcoming lines
    const int maxUpcoming = 5;
    for (int i = startIdx; i < m_lineCache.size() && result.size() < maxUpcoming; ++i) {
        if (m_activeLineIndex >= 0 && i == m_activeLineIndex)
            continue; // Skip the active line itself

        QVariantMap m;
        m[QStringLiteral("text")] = m_lineCache[i].text;
        m[QStringLiteral("startTime")] = m_lineCache[i].startTimeUs;
        m[QStringLiteral("endTime")] = m_lineCache[i].endTimeUs;
        m[QStringLiteral("clipId")] = m_lineCache[i].clipId;
        result.append(m);
    }
    return result;
}

qint64 LyricEngine::activeLineStartTime() const
{
    if (m_activeLineIndex >= 0 && m_activeLineIndex < m_lineCache.size())
        return m_lineCache[m_activeLineIndex].startTimeUs;
    return 0;
}

qint64 LyricEngine::activeLineEndTime() const
{
    if (m_activeLineIndex >= 0 && m_activeLineIndex < m_lineCache.size())
        return m_lineCache[m_activeLineIndex].endTimeUs;
    return 0;
}

qint64 LyricEngine::nextLineStartTime() const
{
    if (m_nextLineIndex >= 0 && m_nextLineIndex < m_lineCache.size())
        return m_lineCache[m_nextLineIndex].startTimeUs;
    return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Line Cache Rebuild
// ─────────────────────────────────────────────────────────────────────────────

void LyricEngine::rebuildLineCache()
{
    m_lineCache.clear();

    if (!m_manager || !m_manager->trackListModel())
        return;

    // Scan all Lyrics tracks and extract clip data
    const auto tracks = m_manager->trackListModel()->tracks();
    for (auto* track : tracks) {
        if (!track || track->trackType() != 2) // Lyrics track type
            continue;

        const auto clips = track->clips();
        for (auto* clip : clips) {
            if (!clip || clip->clipType() != 2)
                continue;

            LyricLine line;
            line.text = clip->lyricText();
            line.startTimeUs = clip->startTime();
            line.endTimeUs = clip->endTime();
            line.clipId = clip->clipId();

            // Extract syllable/word timings from clip's QVariantList
            const auto syllables = clip->syllables();
            for (const auto& sylVar : syllables) {
                const auto sylMap = sylVar.toMap();
                WordTiming wt;
                wt.text = sylMap.value(QStringLiteral("text")).toString();
                wt.relativeStartUs = sylMap.value(QStringLiteral("relativeStart")).toLongLong();
                wt.durationUs = sylMap.value(QStringLiteral("duration")).toLongLong();
                wt.x1 = sylMap.value(QStringLiteral("x1"), 0.25).toDouble();
                wt.y1 = sylMap.value(QStringLiteral("y1"), 0.25).toDouble();
                wt.x2 = sylMap.value(QStringLiteral("x2"), 0.75).toDouble();
                wt.y2 = sylMap.value(QStringLiteral("y2"), 0.75).toDouble();
                line.words.append(wt);
            }

            m_lineCache.append(line);
        }
    }

    // Sort by start time for binary search
    std::sort(m_lineCache.begin(), m_lineCache.end(),
              [](const LyricLine& a, const LyricLine& b) {
                  return a.startTimeUs < b.startTimeUs;
              });

    // Reset state and re-evaluate if we have a current time
    reset();

    if (m_lastUpdateTimeUs >= 0) {
        qint64 saved = m_lastUpdateTimeUs;
        m_lastUpdateTimeUs = -1;
        updatePlaybackPosition(saved);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Active State Update (core synchronization logic)
// ─────────────────────────────────────────────────────────────────────────────

void LyricEngine::updateActiveState(qint64 currentTimeUs)
{
    if (m_lineCache.isEmpty()) {
        if (m_activeLineIndex != -1) {
            m_activeLineIndex = -1;
            m_nextLineIndex = -1;
            m_sweepProgress = 0.0;
            m_activeWordIndex = -1;
            m_activeWordProgress = 0.0;
            m_transitionState = Idle;
            emit activeLineChanged();
            emit sweepProgressChanged();
            emit activeWordChanged();
            emit upcomingLinesChanged();
            emit lineTransitionStateChanged();
        }
        return;
    }

    // Binary search for the active line (line whose [startTime, endTime) contains currentTimeUs)
    int newActiveIndex = -1;
    int lo = 0, hi = m_lineCache.size() - 1;
    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        const auto& line = m_lineCache[mid];
        if (currentTimeUs >= line.startTimeUs && currentTimeUs < line.endTimeUs) {
            newActiveIndex = mid;
            break;
        } else if (currentTimeUs < line.startTimeUs) {
            hi = mid - 1;
        } else {
            lo = mid + 1;
        }
    }

    // Detect line transition
    bool lineChanged = (newActiveIndex != m_activeLineIndex);
    int prevActiveIndex = m_activeLineIndex;
    m_activeLineIndex = newActiveIndex;

    // Find next line
    int newNextIndex = findNextLineIndex(currentTimeUs);
    bool nextChanged = (newNextIndex != m_nextLineIndex);
    m_nextLineIndex = newNextIndex;

    // Update transition state
    LineTransitionState newTransition = Idle;
    if (m_activeLineIndex >= 0) {
        const auto& activeLine = m_lineCache[m_activeLineIndex];
        qint64 remain = activeLine.endTimeUs - currentTimeUs;
        qint64 elapsed = currentTimeUs - activeLine.startTimeUs;
        const qint64 transitionWindowUs = 400000; // 400ms

        if (elapsed < transitionWindowUs) {
            newTransition = Entering;
        } else if (remain < transitionWindowUs) {
            newTransition = Exiting;
        } else {
            newTransition = Active;
        }
    }

    bool transitionChanged = (newTransition != m_transitionState);
    m_transitionState = newTransition;

    // Calculate sweep progress for active line
    double newSweep = 0.0;
    if (m_activeLineIndex >= 0) {
        const auto& line = m_lineCache[m_activeLineIndex];
        qint64 relativeUs = currentTimeUs - line.startTimeUs;
        newSweep = calculateSweepProgress(line, relativeUs);
    }
    bool sweepChanged = (qAbs(newSweep - m_sweepProgress) > 0.001);
    m_sweepProgress = newSweep;

    // Update word tracking
    int prevWordIndex = m_activeWordIndex;
    double prevWordProgress = m_activeWordProgress;
    if (m_activeLineIndex >= 0) {
        const auto& line = m_lineCache[m_activeLineIndex];
        qint64 relativeUs = currentTimeUs - line.startTimeUs;
        updateWordTracking(line, relativeUs);
    } else {
        m_activeWordIndex = -1;
        m_activeWordProgress = 0.0;
    }
    bool wordChanged = (m_activeWordIndex != prevWordIndex ||
                        qAbs(m_activeWordProgress - prevWordProgress) > 0.001);

    // Emit signals (minimized to only when values actually change)
    if (lineChanged || nextChanged) {
        emit activeLineChanged();
        emit upcomingLinesChanged();
        if (lineChanged) {
            emit lineTransition(prevActiveIndex, m_activeLineIndex);
        }
    }
    if (sweepChanged) {
        emit sweepProgressChanged();
    }
    if (wordChanged) {
        emit activeWordChanged();
    }
    if (transitionChanged) {
        emit lineTransitionStateChanged();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Sweep Progress Calculation (character-weighted, matching existing QML logic)
// ─────────────────────────────────────────────────────────────────────────────

double LyricEngine::calculateSweepProgress(const LyricLine& line, qint64 relativeUs) const
{
    if (relativeUs <= 0) return 0.0;

    qint64 duration = line.endTimeUs - line.startTimeUs;
    if (duration <= 0) return 0.0;
    if (relativeUs >= duration) return 1.0;

    // If no word timings, use linear progress
    if (line.words.isEmpty()) {
        return static_cast<double>(relativeUs) / static_cast<double>(duration);
    }

    // Character-weighted sweep progress (matches calculateClipSweepProgress)
    int totalChars = line.text.length();
    if (totalChars == 0) return 0.0;

    QVector<int> charOffsets;
    charOffsets.reserve(line.words.size());
    int charCount = 0;
    for (const auto& w : line.words) {
        charOffsets.append(charCount);
        charCount += w.text.length();
    }

    for (int i = 0; i < line.words.size(); ++i) {
        const auto& word = line.words[i];
        qint64 wStart = word.relativeStartUs;
        qint64 wEnd = wStart + word.durationUs;

        if (relativeUs >= wStart && relativeUs <= wEnd) {
            double sylProgress = (word.durationUs > 0)
                ? static_cast<double>(relativeUs - wStart) / static_cast<double>(word.durationUs)
                : 1.0;
            double activeChars = charOffsets[i] + (word.text.length() * sylProgress);
            return activeChars / totalChars;
        } else if (relativeUs < wStart) {
            return static_cast<double>(charOffsets[i]) / totalChars;
        }
    }

    return 1.0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Word Tracking (for Word Bounce mode)
// ─────────────────────────────────────────────────────────────────────────────

void LyricEngine::updateWordTracking(const LyricLine& line, qint64 relativeUs)
{
    if (line.words.isEmpty()) {
        m_activeWordIndex = -1;
        m_activeWordProgress = 0.0;
        return;
    }

    for (int i = 0; i < line.words.size(); ++i) {
        const auto& word = line.words[i];
        qint64 wStart = word.relativeStartUs;
        qint64 wEnd = wStart + word.durationUs;

        if (relativeUs >= wStart && relativeUs < wEnd) {
            m_activeWordIndex = i;
            m_activeWordProgress = (word.durationUs > 0)
                ? static_cast<double>(relativeUs - wStart) / static_cast<double>(word.durationUs)
                : 1.0;
            return;
        } else if (relativeUs < wStart) {
            // Before first word or between words
            m_activeWordIndex = i - 1; // -1 if before first word
            m_activeWordProgress = (i > 0) ? 1.0 : 0.0;
            return;
        }
    }

    // Past last word
    m_activeWordIndex = line.words.size() - 1;
    m_activeWordProgress = 1.0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Find Next Line (for lookahead display)
// ─────────────────────────────────────────────────────────────────────────────

int LyricEngine::findNextLineIndex(qint64 currentTimeUs) const
{
    if (m_lineCache.isEmpty())
        return -1;

    // If we have an active line, the next line is the one after it
    if (m_activeLineIndex >= 0 && m_activeLineIndex + 1 < m_lineCache.size()) {
        return m_activeLineIndex + 1;
    }

    // If no active line, find the first line that starts after currentTimeUs
    // (binary search for the lower bound)
    int lo = 0, hi = m_lineCache.size();
    while (lo < hi) {
        int mid = lo + (hi - lo) / 2;
        if (m_lineCache[mid].startTimeUs <= currentTimeUs) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }

    return (lo < m_lineCache.size()) ? lo : -1;
}

} // namespace ncktv
