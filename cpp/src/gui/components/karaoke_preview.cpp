#include "karaoke_preview.h"
#include <QVariant>
#include <cmath>

namespace ncktv {

KaraokePreview::KaraokePreview(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(320, 180); // 16:9 aspect ratio minimum
}

void KaraokePreview::loadLyrics(LyricsData* data) {
    m_data = data;
    update();
}

void KaraokePreview::updateTime(double timeSeconds) {
    if (m_currentTime != timeSeconds) {
        m_currentTime = timeSeconds;
        update();
    }
}

void KaraokePreview::setBackgroundImage(const QImage& img) {
    m_backgroundImg = img;
    update();
}

void KaraokePreview::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Letterbox scaling math to maintain 16:9 aspect ratio
    double scaleX = static_cast<double>(width()) / m_targetWidth;
    double scaleY = static_cast<double>(height()) / m_targetHeight;
    double scale = std::min(scaleX, scaleY);

    int viewW = static_cast<int>(m_targetWidth * scale);
    int viewH = static_cast<int>(m_targetHeight * scale);
    int offsetX = (width() - viewW) / 2;
    int offsetY = (height() - viewH) / 2;

    // Fill pillarboxes
    painter.fillRect(rect(), Qt::black);

    // Set coordinate system to logical 1920x1080 area
    painter.translate(offsetX, offsetY);
    painter.scale(scale, scale);

    // Draw Background
    if (!m_backgroundImg.isNull()) {
        painter.drawImage(QRectF(0, 0, m_targetWidth, m_targetHeight), m_backgroundImg);
    } else {
        // Fallback dark gradient
        QLinearGradient grad(0, 0, 0, m_targetHeight);
        grad.setColorAt(0, QColor("#141414"));
        grad.setColorAt(1, QColor("#0a0a0d"));
        painter.fillRect(0, 0, m_targetWidth, m_targetHeight, grad);
    }

    if (m_data) {
        drawSubtitles(painter);
    }

    // Border
    painter.setPen(QPen(QColor("#333333"), 2));
    painter.drawRect(0, 0, m_targetWidth, m_targetHeight);
}

void KaraokePreview::drawSubtitles(QPainter& painter) {
    if (m_data->lines.isEmpty()) return;

    // Find active or upcoming line
    int activeIdx = -1;
    for (int i = 0; i < m_data->lines.size(); ++i) {
        const auto& line = m_data->lines[i];
        if (m_currentTime >= line.startTime - 2.0 && m_currentTime <= line.endTime + 1.0) {
            activeIdx = i;
            break;
        }
    }

    if (activeIdx == -1) return;

    // Draw up to 2 lines
    const int linesToDraw = 2;
    int drawnLines = 0;
    
    // Bottom third anchoring
    int baseY = static_cast<int>(m_targetHeight * 0.7);
    int lineSpacing = 120; // Logical pixel spacing
    
    QFont font = painter.font();
    font.setFamily("Arial");
    font.setPointSize(60);
    font.setBold(true);
    painter.setFont(font);

    QFontMetrics fm(font);

    for (int i = activeIdx; i < m_data->lines.size() && drawnLines < linesToDraw; ++i) {
        const auto& line = m_data->lines[i];
        
        bool isCurrentLine = (m_currentTime >= line.startTime && m_currentTime <= line.endTime);
        
        // Calculate total width to center the line
        int totalWidth = 0;
        for (const auto& w : line.words) {
            totalWidth += fm.horizontalAdvance(w.word) + 5; // 5px padding
        }
        
        int currentX = (m_targetWidth - totalWidth) / 2;
        int currentY = baseY + (drawnLines * lineSpacing);
        
        for (const auto& w : line.words) {
            QString wordText = w.word;
            int wWidth = fm.horizontalAdvance(wordText);
            
            // Text Outline/Shadow
            painter.setPen(QPen(QColor(0, 0, 0, 150), 4));
            painter.drawText(currentX + 4, currentY + 4, wordText);
            
            // Fill
            QColor fillColor = Qt::white;
            if (isCurrentLine && m_currentTime >= w.startTime) {
                if (m_currentTime >= w.startTime + w.duration()) {
                    // Fully sung
                    fillColor = QColor("#00a2ff"); // Blue sung color
                } else {
                    // Currently singing (could do partial filling here with clip rects)
                    fillColor = QColor("#00a2ff");
                }
            }
            
            painter.setPen(fillColor);
            painter.drawText(currentX, currentY, wordText);
            
            currentX += wWidth + 5;
        }
        
        drawnLines++;
    }
}

} // namespace ncktv
