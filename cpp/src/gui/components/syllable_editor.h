#pragma once

#include <QWidget>
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>

#include "../../core/lyrics/lyrics_data.h"

namespace ncktv {

class SyllableEditor : public QWidget {
    Q_OBJECT

public:
    explicit SyllableEditor(QWidget* parent = nullptr);

    void loadLyrics(LyricsData* data);
    void updateCursor(double timeSeconds);
    void setPixelsPerSecond(double pps);

signals:
    void seekRequested(double timeSeconds);
    void wordSelected(int lineIndex, int wordIndex);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    void drawGrid(QPainter& painter);
    void drawWords(QPainter& painter);
    void drawPlayhead(QPainter& painter);

    LyricsData* m_data = nullptr;
    
    // View state
    double m_pixelsPerSecond = 100.0;
    double m_scrollOffsetX = 0.0;
    double m_currentTime = 0.0;
    double m_hoverTime = -1.0;

    // Geometry
    int m_rowHeight = 40;
    int m_rulerHeight = 24;
    
    // Selection state
    int m_selectedLine = -1;
    int m_selectedWord = -1;
};

} // namespace ncktv
