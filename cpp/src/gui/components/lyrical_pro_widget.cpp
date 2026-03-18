#include "lyrical_pro_widget.h"

#include <QPaintEvent>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QFontMetrics>
#include <cmath>
#include <algorithm>

namespace ncktv {

// ─────────────────────────────────────────────────────────────────────────────
// TeleprompterView
// ─────────────────────────────────────────────────────────────────────────────

TeleprompterView::TeleprompterView(QWidget* parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumHeight(300);
}

void TeleprompterView::loadLyrics(const core::LyricsData* lyrics) {
    m_lyrics = lyrics;
    update();
}

void TeleprompterView::updateTime(double timeSecs) {
    m_currentTime = timeSecs;

    int newActive = -1;
    if (m_lyrics) {
        const int n = static_cast<int>(m_lyrics->lines.size());

        // Binary search: find the last line whose start_time <= timeSecs
        int lo = 0, hi = n - 1, candidate = -1;
        while (lo <= hi) {
            int mid = lo + (hi - lo) / 2;
            if (m_lyrics->lines[mid].start_time <= timeSecs) {
                candidate = mid;
                lo = mid + 1;
            } else {
                hi = mid - 1;
            }
        }

        if (candidate >= 0) {
            const auto& ln = m_lyrics->lines[candidate];
            if (timeSecs < ln.end_time) {
                // We're inside this line — it's the active one
                newActive = candidate;
            } else {
                // We've passed this line's end. Check if the next line
                // is within a 2-second lead-in window.
                int next = candidate + 1;
                if (next < n) {
                    double gap = m_lyrics->lines[next].start_time - timeSecs;
                    if (gap <= 0.8) {
                        // Close enough — show the upcoming line (dimmed via paintEvent)
                        newActive = next;
                    } else {
                        // Large gap — hold on the most recently finished line
                        newActive = candidate;
                    }
                } else {
                    // Past the last line — stick on it
                    newActive = candidate;
                }
            }
        } else if (n > 0) {
            // Before the first line: show it only if within 0.8s lead-in
            double gap = m_lyrics->lines[0].start_time - timeSecs;
            if (gap <= 0.8) {
                newActive = 0;
            }
            // else: leave newActive = -1, nothing to show yet
        }
    }

    if (newActive != m_activeIndex) {
        m_activeIndex = newActive;
    }
    // Always update to catch color changes (active vs dimmed)
    update();
}

void TeleprompterView::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    update();
}

void TeleprompterView::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    // ── Background ────────────────────────────────────────────────────────────
    p.fillRect(rect(), m_bg);

    if (!m_lyrics || m_lyrics->lines.empty()) {
        p.setPen(m_nextText);
        QFont ph("Segoe UI", 16, QFont::Normal, true);
        p.setFont(ph);
        p.drawText(rect(), Qt::AlignCenter,
            "No lyrics loaded.\nUse the editor to add subtitles, then open Lyrical Pro Mode.");
        return;
    }

    const int numLines = static_cast<int>(m_lyrics->lines.size());
    const int cx = width() / 2;
    const int cy = height() / 2;
    int activeIdx = (m_activeIndex >= 0) ? m_activeIndex : 0;

    QFont activeFont("Segoe UI", kActiveFontSize, QFont::Bold);
    QFont ctxFont   ("Segoe UI", kContextSize,    QFont::Normal);

    QFontMetrics fmA(activeFont);
    QFontMetrics fmC(ctxFont);

    const int activeH = fmA.height() + 28;          // box height
    const int activeY = cy - activeH / 2;            // top of the active box

    // ── Active line box ───────────────────────────────────────────────────────
    QRectF boxRect(44, activeY - 4, width() - 88, activeH);
    p.setPen(QPen(m_boxBorder, 1.5));
    p.setBrush(QColor(m_boxFill.red(), m_boxFill.green(), m_boxFill.blue(), 210));
    p.drawRoundedRect(boxRect, 12, 12);

    p.setFont(activeFont);
    
    // Dim the text if the line hasn't actually started yet
    const auto& ln = m_lyrics->lines[activeIdx];
    bool isActuallyActive = (m_currentTime >= ln.start_time && m_currentTime < ln.end_time);
    
    QColor textColor = m_activeText;
    if (!isActuallyActive) {
        textColor.setAlpha(120); // Dim it
    }
    
    p.setPen(textColor);
    QString activeText = QString::fromStdString(ln.text);
    p.drawText(boxRect.toRect(), Qt::AlignCenter, activeText);

    // ── Lines above (previous) ────────────────────────────────────────────────
    int drawY = activeY - 12;   // start just above the box
    for (int rel = 1; rel <= 4; ++rel) {
        int idx = activeIdx - rel;
        if (idx < 0) break;

        QString txt = QString::fromStdString(m_lyrics->lines[idx].text);
        float alpha = std::max(0.15f, 1.0f - (rel - 1) * 0.28f);

        QColor col = m_prevText;
        col.setAlphaF(alpha);

        int lineH = fmC.height() + 6;
        drawY -= lineH + (rel == 1 ? 8 : 4);

        p.setFont(ctxFont);
        p.setPen(col);
        p.drawText(QRect(40, drawY, width() - 80, lineH),
                   Qt::AlignHCenter | Qt::AlignVCenter, txt);
    }

    // ── Lines below (next) ────────────────────────────────────────────────────
    int belowY = activeY + activeH + 12;
    for (int rel = 1; rel <= 4; ++rel) {
        int idx = activeIdx + rel;
        if (idx >= numLines) break;

        QString txt = QString::fromStdString(m_lyrics->lines[idx].text);
        float alpha = std::max(0.15f, 1.0f - (rel - 1) * 0.25f);

        QColor col = m_nextText;
        col.setAlphaF(alpha);

        int lineH = fmC.height() + 6;
        p.setFont(ctxFont);
        p.setPen(col);
        p.drawText(QRect(40, belowY, width() - 80, lineH),
                   Qt::AlignHCenter | Qt::AlignVCenter, txt);

        belowY += lineH + (rel == 1 ? 8 : 4);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// LyricalProTopBar
// ─────────────────────────────────────────────────────────────────────────────

LyricalProTopBar::LyricalProTopBar(QWidget* parent)
    : QWidget(parent)
{
    setFixedHeight(52);
    setStyleSheet("background: #2b1e14; border-bottom: 1px solid #4a3020;");

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(16, 0, 16, 0);
    layout->setSpacing(10);

    // Mode badge
    auto* badge = new QLabel("▣  LYRICAL PRO", this);
    badge->setStyleSheet(
        "color: #e8c5aa; font-weight: bold; font-size: 13px;"
        "font-family: 'Segoe UI'; letter-spacing: 2px; background: transparent;");
    layout->addWidget(badge);

    auto* sep = new QLabel("·", this);
    sep->setStyleSheet("color: #5a3e28; font-size: 20px; background: transparent;");
    layout->addWidget(sep);

    // Project title (dynamically set)
    m_titleLabel = new QLabel("Teleprompter Mode", this);
    m_titleLabel->setStyleSheet(
        "color: #c8a882; font-size: 12px; font-family: 'Segoe UI'; background: transparent;");
    layout->addWidget(m_titleLabel, 1);

    // Live timecode pill
    m_timeLabel = new QLabel("00:00.00", this);
    m_timeLabel->setStyleSheet(
        "color: #f0dcc7; font-family: 'Consolas', monospace; font-size: 13px;"
        "background: #3d2b1a; border: 1px solid #6b5540;"
        "border-radius: 10px; padding: 2px 12px;");
    layout->addWidget(m_timeLabel);

    // Exit button
    m_exitBtn = new QPushButton("✕  Exit", this);
    m_exitBtn->setFixedHeight(30);
    m_exitBtn->setStyleSheet(
        "QPushButton { background: #8b4513; color: #ffeedd; border-radius: 6px;"
        "              border: none; font-size: 12px; font-weight: bold; padding: 0 14px; }"
        "QPushButton:hover   { background: #a0522d; }"
        "QPushButton:pressed { background: #6b3010; }");

    connect(m_exitBtn, &QPushButton::clicked, this, &LyricalProTopBar::exitRequested);
    layout->addWidget(m_exitBtn);
}

void LyricalProTopBar::setTime(double timeSecs) {
    if (!m_timeLabel) return;
    int totalMs = static_cast<int>(timeSecs * 1000.0);
    int mins  = totalMs / 60000;
    int secs  = (totalMs % 60000) / 1000;
    int cents = (totalMs % 1000) / 10;
    m_timeLabel->setText(QString("%1:%2.%3")
        .arg(mins,  2, 10, QChar('0'))
        .arg(secs,  2, 10, QChar('0'))
        .arg(cents, 2, 10, QChar('0')));
}

void LyricalProTopBar::setProjectTitle(const QString& title) {
    if (m_titleLabel) m_titleLabel->setText(title);
}

// ─────────────────────────────────────────────────────────────────────────────
// LyricalProBottomBar
// ─────────────────────────────────────────────────────────────────────────────

LyricalProBottomBar::LyricalProBottomBar(QWidget* parent)
    : QWidget(parent)
{
    setFixedHeight(68);
    setStyleSheet("background: #2b1e14; border-top: 1px solid #4a3020;");

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(24, 10, 24, 10);
    layout->setSpacing(8);

    // Helper
    auto mkRoundBtn = [&](const QString& text) -> QPushButton* {
        auto* btn = new QPushButton(text, this);
        btn->setFixedSize(44, 44);
        btn->setStyleSheet(
            "QPushButton { background: #3d2b1a; color: #f0dcc7;"
            "              border: 1px solid #6b5540; border-radius: 22px; font-size: 18px; }"
            "QPushButton:hover   { background: #5a3e28; }"
            "QPushButton:pressed { background: #2b1e14; }");
        return btn;
    };

    layout->addStretch(1);
    m_rewindBtn  = mkRoundBtn("⏮");
    m_playBtn    = mkRoundBtn("▶");
    m_forwardBtn = mkRoundBtn("⏭");
    layout->addWidget(m_rewindBtn);
    layout->addWidget(m_playBtn);
    layout->addWidget(m_forwardBtn);
    layout->addStretch(1);

    // End Session — subtle text link style
    m_endBtn = new QPushButton("END SESSION", this);
    m_endBtn->setFixedHeight(34);
    m_endBtn->setStyleSheet(
        "QPushButton { background: transparent; color: #7a6450;"
        "              border: 1px solid #5a3e28; border-radius: 6px;"
        "              font-size: 10px; font-weight: bold; padding: 0 12px;"
        "              letter-spacing: 1px; }"
        "QPushButton:hover { background: #3d2b1a; color: #e8c5aa; }");
    layout->addWidget(m_endBtn);

    connect(m_rewindBtn,  &QPushButton::clicked, this, &LyricalProBottomBar::seekBackward);
    connect(m_forwardBtn, &QPushButton::clicked, this, &LyricalProBottomBar::seekForward);
    connect(m_endBtn,     &QPushButton::clicked, this, &LyricalProBottomBar::endSessionRequested);
    connect(m_playBtn, &QPushButton::clicked, this, [this]() {
        m_playing = !m_playing;
        setPlaying(m_playing);
        emit playPauseToggled();
    });
}

void LyricalProBottomBar::setPlaying(bool playing) {
    m_playing = playing;
    m_playBtn->setText(playing ? "⏸" : "▶");
}

// ─────────────────────────────────────────────────────────────────────────────
// LyricalProWidget
// ─────────────────────────────────────────────────────────────────────────────

LyricalProWidget::LyricalProWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_topBar    = new LyricalProTopBar(this);
    m_teleView  = new TeleprompterView(this);
    m_bottomBar = new LyricalProBottomBar(this);

    layout->addWidget(m_topBar);
    layout->addWidget(m_teleView, 1);
    layout->addWidget(m_bottomBar);

    connect(m_topBar,    &LyricalProTopBar::exitRequested,
            this, &LyricalProWidget::exitRequested);
    connect(m_bottomBar, &LyricalProBottomBar::endSessionRequested,
            this, &LyricalProWidget::exitRequested);
    connect(m_bottomBar, &LyricalProBottomBar::seekBackward,
            this, &LyricalProWidget::seekBackward);
    connect(m_bottomBar, &LyricalProBottomBar::playPauseToggled,
            this, &LyricalProWidget::playPauseToggled);
    connect(m_bottomBar, &LyricalProBottomBar::seekForward,
            this, &LyricalProWidget::seekForward);
}

void LyricalProWidget::loadLyrics(const core::LyricsData* lyrics) {
    m_teleView->loadLyrics(lyrics);
}

void LyricalProWidget::updateTime(double timeSecs) {
    m_topBar->setTime(timeSecs);
    m_teleView->updateTime(timeSecs);
}

void LyricalProWidget::setPlaying(bool playing) {
    m_bottomBar->setPlaying(playing);
}

void LyricalProWidget::setProjectTitle(const QString& title) {
    m_topBar->setProjectTitle(title);
}

} // namespace ncktv
