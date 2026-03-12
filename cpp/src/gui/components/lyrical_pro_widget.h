#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QPainter>
#include <QPainterPath>
#include <vector>
#include <string>
#include "../../core/timeline/ncktv_core_data.hpp"

namespace ncktv {

// ─────────────────────────────────────────────────────────────────────────────
// TeleprompterView — Custom paint widget: cream background + scrolling lyrics
// ─────────────────────────────────────────────────────────────────────────────
class TeleprompterView : public QWidget {
    Q_OBJECT
public:
    explicit TeleprompterView(QWidget* parent = nullptr);

    void loadLyrics(const core::LyricsData* lyrics);
    void updateTime(double timeSecs);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    const core::LyricsData* m_lyrics      = nullptr;
    double                   m_currentTime = 0.0;
    int                      m_activeIndex = -1;

    // Visual constants
    static constexpr int kActiveFontSize = 40;
    static constexpr int kContextSize    = 24;
    static constexpr int kLineGap        = 56;   // vertical gap between context lines

    // Palette (warm cream theme)
    QColor m_bg        { 0xf5, 0xed, 0xe0 };
    QColor m_activeText{ 0xc0, 0x52, 0x2a };
    QColor m_prevText  { 0xc8, 0xbf, 0xb5 };
    QColor m_nextText  { 0x8a, 0x7e, 0x74 };
    QColor m_boxBorder { 0xe8, 0xc5, 0xaa };
    QColor m_boxFill   { 0xfa, 0xf5, 0xee };
};

// ─────────────────────────────────────────────────────────────────────────────
// LyricalProTopBar — dark header: title label + time pill + ✕ Exit button
// ─────────────────────────────────────────────────────────────────────────────
class LyricalProTopBar : public QWidget {
    Q_OBJECT
public:
    explicit LyricalProTopBar(QWidget* parent = nullptr);

    void setTime(double timeSecs);
    void setProjectTitle(const QString& title);

signals:
    void exitRequested();

private:
    QLabel*      m_timeLabel  = nullptr;
    QLabel*      m_titleLabel = nullptr;
    QPushButton* m_exitBtn    = nullptr;
};

// ─────────────────────────────────────────────────────────────────────────────
// LyricalProBottomBar — dark footer: ⏮ ▶ ⏭ controls + END SESSION link
// ─────────────────────────────────────────────────────────────────────────────
class LyricalProBottomBar : public QWidget {
    Q_OBJECT
public:
    explicit LyricalProBottomBar(QWidget* parent = nullptr);

    void setPlaying(bool playing);

signals:
    void seekBackward();
    void playPauseToggled();
    void seekForward();
    void endSessionRequested();

private:
    QPushButton* m_rewindBtn  = nullptr;
    QPushButton* m_playBtn    = nullptr;
    QPushButton* m_forwardBtn = nullptr;
    QPushButton* m_endBtn     = nullptr;
    bool         m_playing    = false;
};

// ─────────────────────────────────────────────────────────────────────────────
// LyricalProWidget — Orchestrating widget that stacks the three sub-widgets
// ─────────────────────────────────────────────────────────────────────────────
class LyricalProWidget : public QWidget {
    Q_OBJECT
public:
    explicit LyricalProWidget(QWidget* parent = nullptr);

    void loadLyrics(const core::LyricsData* lyrics);
    void updateTime(double timeSecs);
    void setPlaying(bool playing);
    void setProjectTitle(const QString& title);

signals:
    void exitRequested();
    void seekBackward();
    void playPauseToggled();
    void seekForward();

private:
    LyricalProTopBar*    m_topBar    = nullptr;
    TeleprompterView*    m_teleView  = nullptr;
    LyricalProBottomBar* m_bottomBar = nullptr;
};

} // namespace ncktv
