#include "timeline_widget.h"
#include <QStyleOption>
#include <QFileInfo>
#include <cmath>

namespace ncktv {

TimelineWidget::TimelineWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumHeight(150);
    setMouseTracking(true); // For hover effects if needed
}

void TimelineWidget::loadTimeline(TimelineData* data) {
    m_data = data;
    update();
}

void TimelineWidget::updateCursor(double timeSeconds) {
    if (m_currentTime != timeSeconds) {
        m_currentTime = timeSeconds;
        
        // Auto-scroll if cursor goes off screen
        double cursorX = (m_currentTime * m_pixelsPerSecond) - m_scrollOffsetX;
        if (cursorX > width() * 0.9) {
            m_scrollOffsetX += width() * 0.5;
        } else if (cursorX < 0 && m_scrollOffsetX > 0) {
            m_scrollOffsetX = std::max(0.0, m_scrollOffsetX - width() * 0.5);
        }
        
        update();
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
            double startX = (clip.startTime * m_pixelsPerSecond) - m_scrollOffsetX;
            double clipWidth = clip.duration * m_pixelsPerSecond;
            
            // Culling
            if (startX > width() || startX + clipWidth < 0) continue;

            QRectF clipRect(startX, yOffset + 5, clipWidth, m_trackHeight - 10);
            
            // Clip background
            QColor clipColor = (track.trackType == TrackType::Audio) ? QColor("#007acc").darker(120) : QColor("#c22026").darker(120);
            painter.fillRect(clipRect, clipColor);
            
            // Clip border
            painter.setPen(QPen(clipColor.lighter(120), 1));
            painter.drawRect(clipRect);

            // Clip label
            painter.setPen(Qt::white);
            QString sourcePath = clip.sourceFile.value_or("");
            QString label = sourcePath.isEmpty() ? "Clip" : QFileInfo(sourcePath).fileName();
            painter.drawText(clipRect.adjusted(5, 0, -5, 0), Qt::AlignLeft | Qt::AlignVCenter, label);
        }

        yOffset += m_trackHeight;
        trackIndex++;
    }
}

void TimelineWidget::drawPlayhead(QPainter& painter) {
    int x = static_cast<int>((m_currentTime * m_pixelsPerSecond) - m_scrollOffsetX);
    
    if (x >= 0 && x <= width()) {
        painter.setPen(QPen(QColor("#dc143c"), 1)); // Crimson red
        painter.drawLine(x, 0, x, height());
        
        // Playhead triangle handle
        QPolygon poly;
        poly << QPoint(x, 0) << QPoint(x - 5, 5) << QPoint(x - 5, 12) 
             << QPoint(x + 5, 12) << QPoint(x + 5, 5);
        painter.setBrush(QColor("#dc143c"));
        painter.setPen(Qt::NoPen);
        painter.drawPolygon(poly);
    }
}

void TimelineWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        // Clicking ruler or track seeks
        double clickedTime = (event->pos().x() + m_scrollOffsetX) / m_pixelsPerSecond;
        emit seekRequested(std::max(0.0, clickedTime));
    }
}

void TimelineWidget::mouseMoveEvent(QMouseEvent* event) {
    if (event->buttons() & Qt::LeftButton) {
        // Dragging playhead
        double draggedTime = (event->pos().x() + m_scrollOffsetX) / m_pixelsPerSecond;
        emit seekRequested(std::max(0.0, draggedTime));
    }
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

} // namespace ncktv

