#pragma once

#include <QObject>
#include <QVariantList>
#include <QVector>
#include <QString>

namespace ncktv {

class TimelineManager; // Forward declaration

// ── Structured lyric timing data for efficient C++ processing ────────────────

struct WordTiming {
    QString text;
    qint64 relativeStartUs = 0;  // Relative to line start
    qint64 durationUs = 0;
    double x1 = 0.25;
    double y1 = 0.25;
    double x2 = 0.75;
    double y2 = 0.75;
};

struct LyricLine {
    QString text;
    qint64 startTimeUs = 0;      // Absolute timeline position (microseconds)
    qint64 endTimeUs = 0;
    QVector<WordTiming> words;
    QString clipId;              // Source clip reference
};

// ── LyricEngine ──────────────────────────────────────────────────────────────
// Encapsulates all lyric synchronization logic: sweep progress, active line
// detection, upcoming line lookahead, and mode-specific calculations.
// Exposes state as Q_PROPERTYs for reactive QML binding.

class LyricEngine : public QObject {
    Q_OBJECT

    // Current display mode
    Q_PROPERTY(int displayMode READ displayMode WRITE setDisplayMode NOTIFY displayModeChanged)

    // Active line state
    Q_PROPERTY(QString activeLineText READ activeLineText NOTIFY activeLineChanged)
    Q_PROPERTY(QString nextLineText READ nextLineText NOTIFY activeLineChanged)
    Q_PROPERTY(bool hasActiveLine READ hasActiveLine NOTIFY activeLineChanged)
    Q_PROPERTY(QVariantList activeLineSyllables READ activeLineSyllables NOTIFY activeLineChanged)

    // Sweep and word tracking
    Q_PROPERTY(double sweepProgress READ sweepProgress NOTIFY sweepProgressChanged)
    Q_PROPERTY(int activeWordIndex READ activeWordIndex NOTIFY activeWordChanged)
    Q_PROPERTY(double activeWordProgress READ activeWordProgress NOTIFY activeWordChanged)

    // Upcoming lines for queue modes
    Q_PROPERTY(QVariantList upcomingLines READ upcomingLines NOTIFY upcomingLinesChanged)

    // Line transition state machine for animation triggers
    Q_PROPERTY(int lineTransitionState READ lineTransitionState NOTIFY lineTransitionStateChanged)

    // Active line timing (for QML scroll offset and opacity calculations)
    Q_PROPERTY(qint64 activeLineStartTime READ activeLineStartTime NOTIFY activeLineChanged)
    Q_PROPERTY(qint64 activeLineEndTime READ activeLineEndTime NOTIFY activeLineChanged)
    Q_PROPERTY(qint64 nextLineStartTime READ nextLineStartTime NOTIFY activeLineChanged)

public:
    enum DisplayMode {
        BottomTwoLine = 0,
        CenterScrollQueue = 1,
        WordBounce = 2,
        CinematicFullScreen = 3
    };
    Q_ENUM(DisplayMode)

    enum LineTransitionState {
        Idle = 0,
        Entering = 1,
        Active = 2,
        Exiting = 3
    };
    Q_ENUM(LineTransitionState)

    explicit LyricEngine(TimelineManager* manager, QObject* parent = nullptr);
    virtual ~LyricEngine() override = default;

    // ── Public API ───────────────────────────────────────────────────────────
    Q_INVOKABLE void updatePlaybackPosition(qint64 currentTimeUs);
    Q_INVOKABLE void setDisplayMode(int mode);
    Q_INVOKABLE void reset();

    // ── Property Getters ─────────────────────────────────────────────────────
    [[nodiscard]] int displayMode() const { return static_cast<int>(m_displayMode); }
    [[nodiscard]] QString activeLineText() const;
    [[nodiscard]] QString nextLineText() const;
    [[nodiscard]] double sweepProgress() const { return m_sweepProgress; }
    [[nodiscard]] int activeWordIndex() const { return m_activeWordIndex; }
    [[nodiscard]] double activeWordProgress() const { return m_activeWordProgress; }
    [[nodiscard]] QVariantList activeLineSyllables() const;
    [[nodiscard]] QVariantList upcomingLines() const;
    [[nodiscard]] bool hasActiveLine() const { return m_activeLineIndex >= 0; }
    [[nodiscard]] int lineTransitionState() const { return static_cast<int>(m_transitionState); }

    [[nodiscard]] qint64 activeLineStartTime() const;
    [[nodiscard]] qint64 activeLineEndTime() const;
    [[nodiscard]] qint64 nextLineStartTime() const;

    [[nodiscard]] const QVector<LyricLine>& lineCache() const { return m_lineCache; }

    // Called by TimelineManager when tracks/clips change
    void rebuildLineCache();

signals:
    void displayModeChanged();
    void activeLineChanged();
    void sweepProgressChanged();
    void activeWordChanged();
    void upcomingLinesChanged();
    void lineTransitionStateChanged();
    void lineTransition(int fromIndex, int toIndex); // For QML animation triggers

private:
    void updateActiveState(qint64 currentTimeUs);
    double calculateSweepProgress(const LyricLine& line, qint64 relativeUs) const;
    void updateWordTracking(const LyricLine& line, qint64 relativeUs);
    int findNextLineIndex(qint64 currentTimeUs) const;

    TimelineManager* m_manager = nullptr;
    DisplayMode m_displayMode = BottomTwoLine;
    QVector<LyricLine> m_lineCache;

    // Cached state
    int m_activeLineIndex = -1;
    int m_nextLineIndex = -1;
    int m_activeWordIndex = -1;
    double m_sweepProgress = 0.0;
    double m_activeWordProgress = 0.0;
    LineTransitionState m_transitionState = Idle;
    qint64 m_lastUpdateTimeUs = -1;
};

} // namespace ncktv
