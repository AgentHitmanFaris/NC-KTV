#pragma once

#include <QWidget>
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>

#include "../../core/timeline/timeline_data.h"

namespace ncktv {

class TimelineWidget : public QWidget {
    Q_OBJECT

public:
    explicit TimelineWidget(QWidget* parent = nullptr);

    void loadTimeline(TimelineData* data);
    void updateCursor(double timeSeconds);
    void setPixelsPerSecond(double pps);

signals:
    void seekRequested(double timeSeconds);
    void clipSelected(const QString& clipId);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void drawPlayhead(QPainter& painter);
    void drawTracks(QPainter& painter);
    void drawRuler(QPainter& painter);

    TimelineData* m_data = nullptr;
    
    // View state
    double m_pixelsPerSecond = 100.0;
    double m_scrollOffsetX = 0.0;
    double m_currentTime = 0.0;
    
    // Geometry
    int m_trackHeight = 60;
    int m_rulerHeight = 24;
};

} // namespace ncktv
