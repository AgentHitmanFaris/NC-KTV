#include "karaoke_preview.h"
#include <QVariant>
#include <QLinearGradient>
#include <cmath>
#include <algorithm>

namespace ncktv {

KaraokePreview::KaraokePreview(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(320, 180); // 16:9 aspect ratio minimum
}

void KaraokePreview::loadLyrics(core::LyricsData* data) {
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

void KaraokePreview::setMediaPlayer(QMediaPlayer* player) {
    if (!player) return;
    // Create sink owned by this widget
    m_videoSink = new QVideoSink(this);
    connect(m_videoSink, &QVideoSink::videoFrameChanged,
            this, &KaraokePreview::onVideoFrameChanged);
    player->setVideoSink(m_videoSink);
}

void KaraokePreview::onVideoFrameChanged(const QVideoFrame& frame) {
    if (!frame.isValid()) return;
    static int frameCount = 0;
    if (frameCount++ % 30 == 0) {
        qDebug() << "KaraokePreview: Received video frame" << frameCount << "Size:" << frame.width() << "x" << frame.height();
    }
    m_backgroundImg = frame.toImage();
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
    if (m_data->lines.empty()) return;

    // Find first line that hasn't finished yet
    int activeIdx = -1;
    for (int i = 0; i < (int)m_data->lines.size(); ++i) {
        const auto& line = m_data->lines[i];
        if (m_currentTime < line.end_time) {
            if (m_currentTime >= line.start_time - 5.0) {
                activeIdx = i;
                break;
            }
        }
    }

    if (activeIdx == -1) return;

    const int linesToDraw = 2;
    int drawnLines = 0;

    // Bottom-third anchoring
    int baseY = static_cast<int>(m_targetHeight * 0.7);
    int lineSpacing = 120;

    QFont font = painter.font();
    font.setFamily("Arial");
    font.setPointSize(60);
    font.setBold(true);
    painter.setFont(font);

    QFontMetrics fm(font);

    // Karaoke wipe colors
    const QColor unsungColor(Qt::white);
    const QColor sungColorStart("#00d4ff");   // Cyan
    const QColor sungColorEnd("#0066ff");     // Deep blue
    const QColor shadowColor(0, 0, 0, 180);

    for (int i = activeIdx; i < (int)m_data->lines.size() && drawnLines < linesToDraw; ++i) {
        const auto& line = m_data->lines[i];

        bool isCurrentLine = (m_currentTime >= line.start_time && m_currentTime < line.end_time);
        bool isPastLine    = (m_currentTime >= line.end_time);

        // Measure total width for centering
        int totalWidth = 0;
        for (const auto& w : line.tokens) {
            totalWidth += fm.horizontalAdvance(QString::fromStdString(w.text)) + 5;
        }

        int currentX = (m_targetWidth - totalWidth) / 2;
        int currentY = baseY + (drawnLines * lineSpacing);
        int textAscent = fm.ascent();

        for (const auto& w : line.tokens) {
            QString wordText = QString::fromStdString(w.text);
            int wWidth = fm.horizontalAdvance(wordText);
            int wordHeight = fm.height();

            // ── Shadow / outline layer ────────────────────────────────
            painter.setPen(QPen(shadowColor, 5));
            painter.drawText(currentX + 3, currentY + 3, wordText);

            // ── Compute wipe progress for this word ──────────────────
            double progress = 0.0; // 0 = unsung, 1 = fully sung
            if (isPastLine || (isCurrentLine && m_currentTime >= w.end_time)) {
                progress = 1.0;
            } else if (isCurrentLine && m_currentTime >= w.start_time) {
                double wordDur = w.end_time - w.start_time;
                if (wordDur > 0.0) {
                    progress = std::clamp((m_currentTime - w.start_time) / wordDur, 0.0, 1.0);
                }
            }

            int wipeX = static_cast<int>(wWidth * progress);

            // ── Layer 1: Draw full word in unsung color ──────────────
            painter.setPen(unsungColor);
            painter.drawText(currentX, currentY, wordText);

            // ── Layer 2: Draw sung portion with gradient via clip rect ─
            if (progress > 0.0) {
                painter.save();

                // Clip to the sung portion of this word
                QRect clipRect(currentX, currentY - textAscent, wipeX, wordHeight + 4);
                painter.setClipRect(clipRect);

                // Gradient fill across the word width
                QLinearGradient grad(currentX, 0, currentX + wWidth, 0);
                grad.setColorAt(0.0, sungColorStart);
                grad.setColorAt(1.0, sungColorEnd);

                QPen gradPen;
                gradPen.setBrush(QBrush(grad));
                gradPen.setWidth(0);
                painter.setPen(gradPen);
                painter.drawText(currentX, currentY, wordText);

                // ── Glow effect for fully-sung words ─────────────────
                if (progress >= 1.0) {
                    painter.setClipping(false);
                    QColor glow = sungColorStart;
                    glow.setAlpha(40);
                    painter.setPen(QPen(glow, 2));
                    painter.drawText(currentX - 1, currentY - 1, wordText);
                    painter.drawText(currentX + 1, currentY + 1, wordText);
                }

                painter.restore();
            }

            currentX += wWidth + 5;
        }

        drawnLines++;
    }
}

} // namespace ncktv
