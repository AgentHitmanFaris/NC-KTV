#include "syllable_editor.h"
#include <QVariant>
#include <cmath>

namespace ncktv {

SyllableEditor::SyllableEditor(QWidget* parent)
    : QWidget(parent)
{
    setMinimumHeight(200);
    setMouseTracking(true);
}

void SyllableEditor::loadLyrics(LyricsData* data) {
    m_data = data;
    m_selectedLine = -1;
    m_selectedWord = -1;
    update();
}

void SyllableEditor::updateCursor(double timeSeconds) {
    if (m_currentTime != timeSeconds) {
        m_currentTime = timeSeconds;
        
        // Auto-scroll logic synced with timeline
        double cursorX = (m_currentTime * m_pixelsPerSecond) - m_scrollOffsetX;
        if (cursorX > width() * 0.9) {
            m_scrollOffsetX += width() * 0.5;
        } else if (cursorX < 0 && m_scrollOffsetX > 0) {
            m_scrollOffsetX = std::max(0.0, m_scrollOffsetX - width() * 0.5);
        }
        
        update();
    }
}

void SyllableEditor::setPixelsPerSecond(double pps) {
    m_pixelsPerSecond = std::max(10.0, std::min(pps, 5000.0));
    update();
}

void SyllableEditor::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Background
    painter.fillRect(rect(), QColor("#1e1e1e"));

    drawGrid(painter);
    
    if (m_data) {
        drawWords(painter);
    }
    
    drawPlayhead(painter);
}

void SyllableEditor::drawGrid(QPainter& painter) {
    // Top Ruler
    QRect rulerRect(0, 0, width(), m_rulerHeight);
    painter.fillRect(rulerRect, QColor("#252526"));
    
    // Rows (alternating)
    int yOffset = m_rulerHeight;
    for (int i = 0; i < 10; ++i) { // Draw a few rows based on widget height
        if (yOffset > height()) break;
        QRect rowRect(0, yOffset, width(), m_rowHeight);
        QColor bgColor = (i % 2 == 0) ? QColor("#2d2d30") : QColor("#252526");
        painter.fillRect(rowRect, bgColor);
        
        painter.setPen(QColor("#3f3f46"));
        painter.drawLine(0, yOffset + m_rowHeight, width(), yOffset + m_rowHeight);
        
        yOffset += m_rowHeight;
    }
    
    // Vertical time grid lines
    painter.setPen(QPen(QColor("#3f3f46"), 1, Qt::DotLine));
    double startSec = m_scrollOffsetX / m_pixelsPerSecond;
    double endSec = (m_scrollOffsetX + width()) / m_pixelsPerSecond;
    
    double tickInterval = 1.0;
    if (m_pixelsPerSecond > 200) tickInterval = 0.5;
    if (m_pixelsPerSecond > 500) tickInterval = 0.1;
    
    double firstTick = std::floor(startSec / tickInterval) * tickInterval;
    for (double sec = firstTick; sec <= endSec; sec += tickInterval) {
        int x = static_cast<int>((sec * m_pixelsPerSecond) - m_scrollOffsetX);
        if (x >= 0 && x <= width()) {
            painter.drawLine(x, m_rulerHeight, x, height());
        }
    }
}

void SyllableEditor::drawWords(QPainter& painter) {
    int lineIdx = 0;
    int yOffset = m_rulerHeight;
    
    for (const auto& line : m_data->lines) {
        // Simple overlapping prevention via basic staircasing 
        int rowIndex = lineIdx % 4; // 4 rows
        int visualY = yOffset + (rowIndex * m_rowHeight);
        
        int wordIdx = 0;
        for (const auto& word : line.words) {
            double startX = (word.startTime * m_pixelsPerSecond) - m_scrollOffsetX;
            double wordWidth = word.duration() * m_pixelsPerSecond;
            
            // Culling
            if (startX > width() || startX + wordWidth < 0) {
                wordIdx++;
                continue;
            }

            QRectF wordRect(startX, visualY + 4, wordWidth, m_rowHeight - 8);
            
            bool isSelected = (m_selectedLine == lineIdx) && (m_selectedWord == wordIdx);
            
            // Base Colors
            QColor rectColor = isSelected ? QColor("#4facfe") : QColor("#007acc");
            QColor borderColor = rectColor.lighter(130);
            
            painter.fillRect(wordRect, rectColor.lighter(isSelected ? 100 : 70));
            painter.setPen(QPen(borderColor, isSelected ? 2 : 1));
            painter.drawRect(wordRect);
            
            // Text Drawing
            painter.setPen(Qt::white);
            QFont font = painter.font();
            font.setBold(isSelected);
            painter.setFont(font);
            
            QRectF textRect = wordRect.adjusted(3, 0, -3, 0);
            painter.drawText(textRect, Qt::AlignCenter | Qt::TextSingleLine, word.word);
            
            wordIdx++;
        }
        lineIdx++;
    }
}

void SyllableEditor::drawPlayhead(QPainter& painter) {
    // Draw Hover Line
    if (m_hoverTime >= 0.0) {
        int hx = static_cast<int>((m_hoverTime * m_pixelsPerSecond) - m_scrollOffsetX);
        if (hx >= 0 && hx <= width()) {
            painter.setPen(QPen(QColor(225, 67, 67, 127), 1, Qt::DashLine));
            painter.drawLine(hx, 0, hx, height());
        }
    }

    int x = static_cast<int>((m_currentTime * m_pixelsPerSecond) - m_scrollOffsetX);
    
    if (x >= 0 && x <= width()) {
        painter.setPen(QPen(QColor("#e14343"), 1)); // Adobe-like red
        painter.drawLine(x, 0, x, height());
        
        QPolygon poly;
        poly << QPoint(x - 6, 0) << QPoint(x + 6, 0) << QPoint(x, 6)
             << QPoint(x + 6, 12) << QPoint(x - 6, 12) << QPoint(x, 6);
        painter.setBrush(QColor("#e14343"));
        painter.setPen(Qt::NoPen);
        painter.drawPolygon(poly);
    }
}

void SyllableEditor::mousePressEvent(QMouseEvent* event) {
    double clickedTime = (event->pos().x() + m_scrollOffsetX) / m_pixelsPerSecond;
    
    // Left click seeks on empty space or ruler
    if (event->button() == Qt::LeftButton) {
        emit seekRequested(std::max(0.0, clickedTime));
        
        // TODO: Hit testing for word selection goes here
        // If clicking a word rect, set m_selectedLine / m_selectedWord
        // and emit wordSelected()
    }
}

void SyllableEditor::mouseMoveEvent(QMouseEvent* event) {
    double time = (event->pos().x() + m_scrollOffsetX) / m_pixelsPerSecond;
    if (event->buttons() & Qt::LeftButton) {
        // Dragging seeks or moves word edges depending on hit test
        emit seekRequested(std::max(0.0, time));
    }
    m_hoverTime = std::max(0.0, time);
    update();
}

void SyllableEditor::wheelEvent(QWheelEvent* event) {
    if (event->modifiers() & Qt::ControlModifier) {
        // Zoom functionality identical to timeline
        double zoomFactor = event->angleDelta().y() > 0 ? 1.2 : 0.8;
        double mouseTime = (event->position().x() + m_scrollOffsetX) / m_pixelsPerSecond;
        
        setPixelsPerSecond(m_pixelsPerSecond * zoomFactor);
        
        m_scrollOffsetX = (mouseTime * m_pixelsPerSecond) - event->position().x();
        m_scrollOffsetX = std::max(0.0, m_scrollOffsetX);
        
        update();
    } else {
        // Basic horizontal scrolling
        m_scrollOffsetX -= event->angleDelta().y() * 0.5;
        m_scrollOffsetX -= event->angleDelta().x() * 0.5;
        m_scrollOffsetX = std::max(0.0, m_scrollOffsetX);
        update();
    }
}

void SyllableEditor::resizeEvent(QResizeEvent* /*event*/) {
    update();
}

void SyllableEditor::leaveEvent(QEvent* /*event*/) {
    m_hoverTime = -1.0;
    update();
}

} // namespace ncktv
