#include "timeline_widget.h"
#include <QStyleOption>
#include <QFileInfo>
#include <cmath>

namespace ncktv {

TimelineWidget::TimelineWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumHeight(150);
    setMouseTracking(true);
    setAcceptDrops(true); // accept lyric block drops
}

void TimelineWidget::loadTimeline(core::TimelineData* data) {
    m_data = data;
    update();
}

void TimelineWidget::loadLyrics(core::LyricsData* data) {
    m_lyricsData = data;
    update();
}

void TimelineWidget::updateCursor(double timeSeconds) {
    if (m_currentTime != timeSeconds) {
        double oldCursorX = (m_currentTime * m_pixelsPerSecond) - m_scrollOffsetX;
        double newCursorX = (timeSeconds * m_pixelsPerSecond) - m_scrollOffsetX;
        
        m_currentTime = timeSeconds;
        
        // Auto-scroll if cursor goes off screen
        bool needsScroll = false;
        if (newCursorX > width() * 0.9) {
            m_scrollOffsetX += width() * 0.5;
            needsScroll = true;
        } else if (newCursorX < 0 && m_scrollOffsetX > 0) {
            m_scrollOffsetX = std::max(0.0, m_scrollOffsetX - width() * 0.5);
            needsScroll = true;
        }
        
        // Debounce: Only trigger intensive UI repaint if the cursor has actually moved by at least 1 pixel
        if (needsScroll || std::abs(newCursorX - oldCursorX) >= 1.0) {
            update();
        }
    }
}

void TimelineWidget::setPixelsPerSecond(double pps) {
    m_pixelsPerSecond = std::max(10.0, std::min(pps, 5000.0));
    update();
}

void TimelineWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Background
    painter.fillRect(rect(), QColor("#1e1e1e"));

    drawRuler(painter);
    
    if (m_data) {
        drawTracks(painter);
    }
    
    drawPlayhead(painter);

    // Drop preview indicator
    if (m_dropPreviewTime >= 0.0) {
        double x = (m_dropPreviewTime * m_pixelsPerSecond) - m_scrollOffsetX;
        painter.setPen(QPen(QColor("#00ff88"), 2, Qt::DashLine));
        painter.drawLine(static_cast<int>(x), 0, static_cast<int>(x), height());
        // Label
        painter.setPen(QColor("#00ff88"));
        painter.setFont(QFont("Segoe UI", 8));
        painter.drawText(static_cast<int>(x) + 4, m_rulerHeight + 14, "Drop here");
    }
}

void TimelineWidget::drawRuler(QPainter& painter) {
    QRect rulerRect(0, 0, width(), m_rulerHeight);
    painter.fillRect(rulerRect, QColor("#252526"));
    painter.setPen(QPen(QColor("#808080"), 1));

    double startSec = m_scrollOffsetX / m_pixelsPerSecond;
    double endSec = (m_scrollOffsetX + width()) / m_pixelsPerSecond;

    // Determine tick interval based on zoom
    double tickInterval = 1.0;
    if (m_pixelsPerSecond > 200) tickInterval = 0.1;
    else if (m_pixelsPerSecond < 50) tickInterval = 5.0;

    double firstTick = std::floor(startSec / tickInterval) * tickInterval;
    
    QFont font = painter.font();
    font.setPointSize(8);
    painter.setFont(font);

    for (double sec = firstTick; sec <= endSec; sec += tickInterval) {
        int x = static_cast<int>((sec * m_pixelsPerSecond) - m_scrollOffsetX);
        if (x < 0 || x > width()) continue;

        // Draw tick mark
        bool isMajor = std::fmod(sec, std::max(1.0, tickInterval * 5)) < 0.001;
        int tickHeight = isMajor ? m_rulerHeight - 4 : m_rulerHeight / 2;
        painter.drawLine(x, m_rulerHeight - tickHeight, x, m_rulerHeight);

        // Draw text for major ticks
        if (isMajor) {
            QString timeStr;
            int mins = static_cast<int>(sec) / 60;
            double secs = std::fmod(sec, 60.0);
            if (tickInterval < 1.0) {
                timeStr = QString("%1:%2").arg(mins, 2, 10, QChar('0')).arg(secs, 5, 'f', 1, QChar('0'));
            } else {
                timeStr = QString("%1:%2").arg(mins, 2, 10, QChar('0')).arg(static_cast<int>(secs), 2, 10, QChar('0'));
            }
            painter.drawText(x + 2, m_rulerHeight - 2, timeStr);
        }
    }
}

void TimelineWidget::drawTracks(QPainter& painter) {
    int yOffset = m_rulerHeight;
    int trackIndex = 0;

    // ── Subtitle Track (always first, above audio) ──────────────────────────
    if (m_lyricsData && !m_lyricsData->lines.empty()) {
        QRect subTrackRect(0, yOffset, width(), m_trackHeight);
        painter.fillRect(subTrackRect, QColor("#1a2535"));
        painter.setPen(QColor("#3f3f46"));
        painter.drawLine(0, yOffset + m_trackHeight, width(), yOffset + m_trackHeight);

        // Label
        painter.setPen(QColor("#aaaaaa"));
        painter.setFont(QFont("Segoe UI", 8));
        painter.drawText(4, yOffset + 14, "SUB");

        for (const auto& line : m_lyricsData->lines) {
            double startX = (line.start_time * m_pixelsPerSecond) - m_scrollOffsetX;
            double endX   = (line.end_time   * m_pixelsPerSecond) - m_scrollOffsetX;
            double blockW = endX - startX;
            if (endX < 0 || startX > width()) continue;

            bool isActive = (m_currentTime >= line.start_time && m_currentTime <= line.end_time);
            QColor blockColor = isActive ? QColor("#e6a817") : QColor("#7a5c00");

            QRectF blockRect(startX, yOffset + 4, std::max(blockW, 2.0), m_trackHeight - 8);
            painter.fillRect(blockRect, blockColor);
            painter.setPen(QPen(blockColor.lighter(130), 1));
            painter.drawRect(blockRect);
            
            // Draw individual words if available
            if (!line.tokens.empty()) {
                for (const auto& w : line.tokens) {
                    double wStartX = (w.start_time * m_pixelsPerSecond) - m_scrollOffsetX;
                    double wEndX   = (w.end_time * m_pixelsPerSecond) - m_scrollOffsetX;
                    double wBlockW = std::max(2.0, wEndX - wStartX);
                    
                    if (wStartX >= startX && wEndX <= startX + blockW) {
                        QRectF wRect(wStartX, yOffset + m_trackHeight / 2, wBlockW, m_trackHeight / 2 - 4);
                        painter.fillRect(wRect, QColor("#997a00"));
                        painter.setPen(QPen(QColor("#ccaa00"), 1));
                        painter.drawRect(wRect);
                        
                        painter.setPen(Qt::white);
                        painter.setFont(QFont("Segoe UI", 7));
                        painter.drawText(wRect.adjusted(2, 0, -2, 0), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, QString::fromStdString(w.text));
                    }
                }
            } else if (blockW > 30) {
                painter.setPen(Qt::white);
                painter.setFont(QFont("Segoe UI", 8));
                painter.drawText(blockRect.adjusted(4, 0, -4, 0), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, QString::fromStdString(line.text));
            }
        }

        yOffset += m_trackHeight;
    }

    // ── Audio / Video Tracks ─────────────────────────────────────────────────
    if (!m_data) return;
    for (const auto& track : m_data->tracks) {
        QRect trackRect(0, yOffset, width(), m_trackHeight);
        
        // Track background (alternating colors)
        QColor bgColor = (trackIndex % 2 == 0) ? QColor("#2d2d30") : QColor("#252526");
        painter.fillRect(trackRect, bgColor);

        // Track border
        painter.setPen(QColor("#3f3f46"));
        painter.drawLine(0, yOffset + m_trackHeight, width(), yOffset + m_trackHeight);

        // Draw Clips
        for (const auto& clip : track.clips) {
            double startX = (clip.start_time * m_pixelsPerSecond) - m_scrollOffsetX;
            double clipWidth = clip.duration * m_pixelsPerSecond;
            
            // Culling
            if (startX > width() || startX + clipWidth < 0) continue;

            QRectF clipRect(startX, yOffset + 5, clipWidth, m_trackHeight - 10);
            
            // Clip background
            QColor clipColor = (track.track_type == core::TrackType::AUDIO) ? QColor("#007acc").darker(120) : QColor("#c22026").darker(120);
            painter.fillRect(clipRect, clipColor);
            
            // Clip border
            painter.setPen(QPen(clipColor.lighter(120), 1));
            painter.drawRect(clipRect);

            // Clip label
            painter.setPen(Qt::white);
            QString sourcePath = QString::fromStdString(clip.source_file.value_or(""));
            QString label = sourcePath.isEmpty() ? "Clip" : QFileInfo(sourcePath).fileName();
            painter.drawText(clipRect.adjusted(5, 0, -5, 0), Qt::AlignLeft | Qt::AlignVCenter, label);
        }

        yOffset += m_trackHeight;
        trackIndex++;
    }
}

void TimelineWidget::drawPlayhead(QPainter& painter) {
    if (m_hoverTime >= 0.0) {
        int hx = static_cast<int>((m_hoverTime * m_pixelsPerSecond) - m_scrollOffsetX);
        if (hx >= 0 && hx <= width()) {
            // Subtle alignment guide (was red)
            painter.setPen(QPen(QColor(255, 255, 255, 60), 1, Qt::DashLine));
            painter.drawLine(hx, 0, hx, height());
        }
    }

    int x = static_cast<int>((m_currentTime * m_pixelsPerSecond) - m_scrollOffsetX);
    
    if (x >= 0 && x <= width()) {
        painter.setPen(QPen(QColor("#e14343"), 1)); // Adobe red
        painter.drawLine(x, 0, x, height());
        
        // Playhead triangle handle
        QPolygon poly;
        poly << QPoint(x - 6, 0) << QPoint(x + 6, 0) << QPoint(x, 6)
             << QPoint(x + 6, 12) << QPoint(x - 6, 12) << QPoint(x, 6);
        painter.setBrush(QColor("#e14343"));
        painter.setPen(Qt::NoPen);
        painter.drawPolygon(poly);
    }
}

// ─── Helper: find subtitle block index at screen x within subtitle track y ────
static int hitTestSubtitle(core::LyricsData* lyricsData, double clickedTime) {
    if (!lyricsData) return -1;
    for (int i = 0; i < lyricsData->lines.size(); ++i) {
        const auto& ln = lyricsData->lines[i];
        if (clickedTime >= ln.start_time && clickedTime <= ln.end_time)
            return i;
    }
    return -1;
}

static int hitTestWord(const core::LyricsLine& line, double clickedTime) {
    for (int i = 0; i < line.tokens.size(); ++i) {
        const auto& w = line.tokens[i];
        // add a tiny bit of padding for easier clicking
        if (clickedTime >= w.start_time - 0.05 && clickedTime <= w.end_time + 0.05)
            return i;
    }
    return -1;
}

bool TimelineWidget::inSubtitleTrack(int y) const {
    return m_lyricsData && !m_lyricsData->lines.empty()
        && y >= m_rulerHeight && y < m_rulerHeight + m_trackHeight;
}

void TimelineWidget::mousePressEvent(QMouseEvent* event) {
    const int y = event->pos().y();
    const double clickedTime = (event->pos().x() + m_scrollOffsetX) / m_pixelsPerSecond;

    // ── Subtitle track interaction ─────────────────────────────────────────
    if (inSubtitleTrack(y) && m_lyricsData) {
        int idx = hitTestSubtitle(m_lyricsData, clickedTime);
        if (idx >= 0 && event->button() == Qt::LeftButton) {
            const auto& ln = m_lyricsData->lines[idx];
            
            // Check if user clicked in the lower half (words section)
            bool inWordsY = y > m_rulerHeight + m_trackHeight / 2 && !ln.tokens.empty();
            
            if (inWordsY) {
                int wIdx = hitTestWord(ln, clickedTime);
                if (wIdx >= 0) {
                    const auto& w = ln.tokens[wIdx];
                    double wStartX = (w.start_time * m_pixelsPerSecond) - m_scrollOffsetX;
                    double wEndX   = (w.end_time * m_pixelsPerSecond) - m_scrollOffsetX;
                    m_draggingWordLineIdx = idx;
                    m_draggingWordIdx = wIdx;
                    m_resizingWordLeft = false;
                    m_resizingWordRight = false;
                    
                    if (event->pos().x() - wStartX < 6) {
                        m_resizingWordLeft = true;
                    } else if (wEndX - event->pos().x() < 6) {
                        m_resizingWordRight = true;
                    }
                    if (m_resizingWordLeft || m_resizingWordRight) return;
                }
            }
            
            double startX = (ln.start_time * m_pixelsPerSecond) - m_scrollOffsetX;
            double endX   = (ln.end_time   * m_pixelsPerSecond) - m_scrollOffsetX;
            m_draggingSubIdx = idx;
            m_dragOrigDur    = ln.end_time - ln.start_time;
            m_resizingLeft   = false;
            m_resizingRight  = false;
            // Edge hotzone: 8 px
            if (event->pos().x() - startX < 8) {
                m_resizingLeft  = true;
                m_dragOffsetSec = clickedTime - ln.start_time;
            } else if (endX - event->pos().x() < 8) {
                m_resizingRight = true;
                m_dragOffsetSec = ln.end_time - clickedTime;
            } else {
                m_dragOffsetSec = clickedTime - ln.start_time;
            }
            return;
        }
        return; // clicked subtitle track but no block — do nothing (don't seek)
    }

    // ── Normal seek ───────────────────────────────────────────────────────
    if (event->button() == Qt::LeftButton)
        emit seekRequested(std::max(0.0, clickedTime));
}

void TimelineWidget::mouseDoubleClickEvent(QMouseEvent* event) {
    const int y = event->pos().y();
    const double clickedTime = (event->pos().x() + m_scrollOffsetX) / m_pixelsPerSecond;

    if (inSubtitleTrack(y) && m_lyricsData && event->button() == Qt::LeftButton) {
        int idx = hitTestSubtitle(m_lyricsData, clickedTime);
        if (idx >= 0) {
            emit subtitleDoubleClicked(idx);
        }
    }
}

void TimelineWidget::mouseMoveEvent(QMouseEvent* event) {
    const double time = (event->pos().x() + m_scrollOffsetX) / m_pixelsPerSecond;
    const int    y    = event->pos().y();

    // ── Active drag/resize ─────────────────────────────────────────────────
    if (m_draggingWordIdx >= 0 && m_draggingWordLineIdx >= 0 && m_lyricsData) {
        auto& w = m_lyricsData->lines[m_draggingWordLineIdx].tokens[m_draggingWordIdx];
        if (m_resizingWordLeft) {
            double newStart = std::max(0.0, time);
            if (newStart < w.end_time - 0.05) w.start_time = newStart;
        } else if (m_resizingWordRight) {
            w.end_time = std::max(w.start_time + 0.05, time);
        }
        update();
        return;
    }
    
    if (m_draggingSubIdx >= 0 && m_lyricsData &&
        m_draggingSubIdx < m_lyricsData->lines.size()) {
        auto& ln = m_lyricsData->lines[m_draggingSubIdx];
        if (m_resizingLeft) {
            double newStart = std::max(0.0, time - m_dragOffsetSec);
            if (newStart < ln.end_time - 0.1) ln.start_time = newStart;
        } else if (m_resizingRight) {
            double newEnd = std::max(ln.start_time + 0.1, time + m_dragOffsetSec);
            ln.end_time = newEnd;
        } else {
            double newStart = std::max(0.0, time - m_dragOffsetSec);
            double delta = newStart - ln.start_time;
            ln.start_time = newStart;
            ln.end_time   = newStart + m_dragOrigDur;
            // Shift all words with the line
            for(auto& w : ln.tokens) {
                w.start_time += delta;
                w.end_time += delta;
            }
        }
        update();
        return;
    }

    // ── Cursor hint on hover ───────────────────────────────────────────────
    if (inSubtitleTrack(y) && m_lyricsData) {
        int idx = hitTestSubtitle(m_lyricsData, time);
        if (idx >= 0) {
            const auto& ln = m_lyricsData->lines[idx];
            bool inWordsY = y > m_rulerHeight + m_trackHeight / 2 && !ln.tokens.empty();
            
            if (inWordsY) {
                int wIdx = hitTestWord(ln, time);
                if (wIdx >= 0) {
                    const auto& w = ln.tokens[wIdx];
                    double wStartX = (w.start_time * m_pixelsPerSecond) - m_scrollOffsetX;
                    double wEndX   = (w.end_time * m_pixelsPerSecond) - m_scrollOffsetX;
                    bool nearWEdge = (event->pos().x() - wStartX < 6) || (wEndX - event->pos().x() < 6);
                    setCursor(nearWEdge ? Qt::SizeHorCursor : Qt::ArrowCursor);
                } else {
                    setCursor(Qt::ArrowCursor);
                }
            } else {
                double startX = (ln.start_time * m_pixelsPerSecond) - m_scrollOffsetX;
                double endX   = (ln.end_time   * m_pixelsPerSecond) - m_scrollOffsetX;
                bool nearEdge = (event->pos().x() - startX < 8) || (endX - event->pos().x() < 8);
                setCursor(nearEdge ? Qt::SizeHorCursor : Qt::SizeAllCursor);
            }
        } else {
            setCursor(Qt::ArrowCursor);
        }
    } else {
        setCursor(Qt::ArrowCursor);
    }

    // ── Hover + seek drag ─────────────────────────────────────────────────
    if (event->buttons() & Qt::LeftButton && !inSubtitleTrack(y))
        emit seekRequested(std::max(0.0, time));
    m_hoverTime = std::max(0.0, time);
    update();
}

void TimelineWidget::wheelEvent(QWheelEvent* event) {
    if (event->modifiers() & Qt::ControlModifier) {
        // Zoom
        double zoomFactor = event->angleDelta().y() > 0 ? 1.2 : 0.8;
        
        // Keep the point under mouse fixed while zooming
        double mouseTime = (event->position().x() + m_scrollOffsetX) / m_pixelsPerSecond;
        
        setPixelsPerSecond(m_pixelsPerSecond * zoomFactor);
        
        m_scrollOffsetX = (mouseTime * m_pixelsPerSecond) - event->position().x();
        m_scrollOffsetX = std::max(0.0, m_scrollOffsetX);
        
        update();
    } else {
        // Scroll horizontally
        m_scrollOffsetX -= event->angleDelta().y() * 0.5;
        m_scrollOffsetX -= event->angleDelta().x() * 0.5;
        m_scrollOffsetX = std::max(0.0, m_scrollOffsetX);
        update();
    }
}

void TimelineWidget::resizeEvent(QResizeEvent* /*event*/) {
    update();
}

void TimelineWidget::leaveEvent(QEvent* /*event*/) {
    m_hoverTime = -1.0;
    update();
}

void TimelineWidget::mouseReleaseEvent(QMouseEvent* /*event*/) {
    if (m_draggingWordLineIdx >= 0) {
        // Trigger subtitle update
        if (m_lyricsData && m_draggingWordLineIdx < m_lyricsData->lines.size()) {
            const auto& ln = m_lyricsData->lines[m_draggingWordLineIdx];
            emit subtitleMoved(m_draggingWordLineIdx, ln.start_time, ln.end_time);
        }
        m_draggingWordIdx = -1;
        m_draggingWordLineIdx = -1;
        m_resizingWordLeft = false;
        m_resizingWordRight = false;
        setCursor(Qt::ArrowCursor);
        return;
    }

    if (m_draggingSubIdx >= 0 && m_lyricsData &&
        m_draggingSubIdx < m_lyricsData->lines.size()) {
        const auto& ln = m_lyricsData->lines[m_draggingSubIdx];
        emit subtitleMoved(m_draggingSubIdx, ln.start_time, ln.end_time);
    }
    m_draggingSubIdx = -1;
    m_resizingLeft   = false;
    m_resizingRight  = false;
    setCursor(Qt::ArrowCursor);
}

void TimelineWidget::contextMenuEvent(QContextMenuEvent* event) {
    if (!m_lyricsData || m_lyricsData->lines.empty()) return;

    const int    y    = event->pos().y();
    const double time = (event->pos().x() + m_scrollOffsetX) / m_pixelsPerSecond;

    if (!inSubtitleTrack(y)) return;

    int idx = hitTestSubtitle(m_lyricsData, time);
    if (idx < 0) return;

    QMenu menu(this);
    QAction* moveAct = menu.addAction(QStringLiteral("⏎  Move here (seek to start)"));
    menu.addSeparator();
    QAction* delAct  = menu.addAction(QStringLiteral("✕  Delete Subtitle"));

    QAction* chosen = menu.exec(event->globalPos());
    if (chosen == delAct) {
        emit subtitleDeleted(idx);
    } else if (chosen == moveAct) {
        const auto& ln = m_lyricsData->lines[idx];
        emit seekRequested(ln.start_time);
    }
}

} // namespace ncktv


void TimelineWidget::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasFormat("application/x-ncktv-lyric-index")) {
        event->acceptProposedAction();
        m_dropPreviewTime = (event->position().x() + m_scrollOffsetX) / m_pixelsPerSecond;
        update();
    }
}

void TimelineWidget::dragMoveEvent(QDragMoveEvent* event) {
    if (event->mimeData()->hasFormat("application/x-ncktv-lyric-index")) {
        event->acceptProposedAction();
        m_dropPreviewTime = (event->position().x() + m_scrollOffsetX) / m_pixelsPerSecond;
        update();
    }
}

void TimelineWidget::dropEvent(QDropEvent* event) {
    if (!event->mimeData()->hasFormat("application/x-ncktv-lyric-index")) return;
    bool ok = false;
    int lineIdx = event->mimeData()->data("application/x-ncktv-lyric-index").toInt(&ok);
    if (!ok) return;

    double dropTime = (event->position().x() + m_scrollOffsetX) / m_pixelsPerSecond;
    dropTime = std::max(0.0, dropTime);

    m_dropPreviewTime = -1.0;
    event->acceptProposedAction();
    update();

    emit lyricDropped(lineIdx, dropTime);
}

} // namespace ncktv
