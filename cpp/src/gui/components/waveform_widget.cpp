#include "waveform_widget.h"
#include <QVariant>
#include <cmath>

namespace ncktv {

WaveformWidget::WaveformWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumHeight(120);
    setMouseTracking(true);
}

void WaveformWidget::loadWaveformData(const QVector<float>& minData, const QVector<float>& maxData, double sampleRate, int samplesPerPixel) {
    m_minData = minData;
    m_maxData = maxData;
    // Base 100 pixels per second calculation based on samples
    if (sampleRate > 0 && samplesPerPixel > 0) {
        m_audioDuration = (m_minData.size() * samplesPerPixel) / sampleRate;
    }
    update();
}

void WaveformWidget::updateCursor(double timeSeconds) {
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

void WaveformWidget::setPixelsPerSecond(double pps) {
    m_pixelsPerSecond = std::max(10.0, std::min(pps, 5000.0));
    update();
}

void WaveformWidget::addMarker(double timeSeconds, const QColor& color, const QString& label) {
    m_markers.append({timeSeconds, color, label});
    update();
}

void WaveformWidget::clearMarkers() {
    m_markers.clear();
    update();
}

void WaveformWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Background
    painter.fillRect(rect(), QColor("#1e1e1e"));

    if (!m_minData.isEmpty()) {
        drawWaveform(painter);
    }
    
    drawMarkers(painter);
    drawPlayhead(painter);
}

void WaveformWidget::drawWaveform(QPainter& painter) {
    int h = height();
    int centerY = h / 2;
    
    painter.setPen(QPen(QColor("#007acc"), 1));
    
    double startSec = std::max(0.0, m_scrollOffsetX / m_pixelsPerSecond);
    double endSec = (m_scrollOffsetX + width()) / m_pixelsPerSecond;

    // This is a simplified drawing routine. In a production app, 
    // the min/max data would be pre-calculated to exactly match the zoom level.
    // For now, we project the available data points to screen coordinates.
    
    int startIndex = static_cast<int>((startSec / m_audioDuration) * m_minData.size());
    int endIndex = static_cast<int>((endSec / m_audioDuration) * m_minData.size());
    
    startIndex = std::max(0, std::min(startIndex, static_cast<int>(m_minData.size() - 1)));
    endIndex = std::max(0, std::min(endIndex, static_cast<int>(m_minData.size() - 1)));

    for (int i = startIndex; i <= endIndex; ++i) {
        double timeSec = (static_cast<double>(i) / m_minData.size()) * m_audioDuration;
        int x = static_cast<int>((timeSec * m_pixelsPerSecond) - m_scrollOffsetX);
        
        if (x >= 0 && x <= width()) {
            int yMin = centerY - static_cast<int>(m_minData[i] * centerY);
            int yMax = centerY - static_cast<int>(m_maxData[i] * centerY);
            painter.drawLine(x, yMin, x, yMax);
        }
    }
    
    // Center line
    painter.setPen(QColor("#ffffff"));
    painter.setOpacity(0.2);
    painter.drawLine(0, centerY, width(), centerY);
    painter.setOpacity(1.0);
}

void WaveformWidget::drawMarkers(QPainter& painter) {
    for (const auto& marker : m_markers) {
        int x = static_cast<int>((marker.time * m_pixelsPerSecond) - m_scrollOffsetX);
        if (x >= 0 && x <= width()) {
            painter.setPen(QPen(marker.color, 2));
            painter.drawLine(x, 0, x, height());
            
            if (!marker.label.isEmpty()) {
                painter.setPen(Qt::white);
                painter.drawText(x + 5, 15, marker.label);
            }
        }
    }
}

void WaveformWidget::drawPlayhead(QPainter& painter) {
    int x = static_cast<int>((m_currentTime * m_pixelsPerSecond) - m_scrollOffsetX);
    
    if (x >= 0 && x <= width()) {
        painter.setPen(QPen(QColor("#dc143c"), 1));
        painter.drawLine(x, 0, x, height());
        
        QPolygon poly;
        poly << QPoint(x, height()) << QPoint(x - 5, height() - 5) << QPoint(x - 5, height() - 12) 
             << QPoint(x + 5, height() - 12) << QPoint(x + 5, height() - 5);
        painter.setBrush(QColor("#dc143c"));
        painter.setPen(Qt::NoPen);
        painter.drawPolygon(poly);
    }
}

void WaveformWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        double clickedTime = (event->pos().x() + m_scrollOffsetX) / m_pixelsPerSecond;
        emit seekRequested(std::max(0.0, clickedTime));
    }
}

void WaveformWidget::mouseMoveEvent(QMouseEvent* event) {
    if (event->buttons() & Qt::LeftButton) {
        double draggedTime = (event->pos().x() + m_scrollOffsetX) / m_pixelsPerSecond;
        emit seekRequested(std::max(0.0, draggedTime));
    }
}

void WaveformWidget::wheelEvent(QWheelEvent* event) {
    if (event->modifiers() & Qt::ControlModifier) {
        double zoomFactor = event->angleDelta().y() > 0 ? 1.2 : 0.8;
        double mouseTime = (event->position().x() + m_scrollOffsetX) / m_pixelsPerSecond;
        setPixelsPerSecond(m_pixelsPerSecond * zoomFactor);
        m_scrollOffsetX = (mouseTime * m_pixelsPerSecond) - event->position().x();
        m_scrollOffsetX = std::max(0.0, m_scrollOffsetX);
        update();
    } else {
        m_scrollOffsetX -= event->angleDelta().y() * 0.5;
        m_scrollOffsetX -= event->angleDelta().x() * 0.5;
        m_scrollOffsetX = std::max(0.0, m_scrollOffsetX);
        update();
    }
}

void WaveformWidget::resizeEvent(QResizeEvent* /*event*/) {
    update();
}

} // namespace ncktv
