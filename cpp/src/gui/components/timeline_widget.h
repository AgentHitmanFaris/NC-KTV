#pragma once

#include <QWidget>
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QMenu>
#include <QCursor>

#include "../../core/timeline/ncktv_core_data.hpp"

namespace ncktv {

class TimelineWidget : public QWidget {
    Q_OBJECT

public:
    explicit TimelineWidget(QWidget* parent = nullptr);

    void loadTimeline(core::TimelineData* data);
    void loadLyrics(core::LyricsData* data);
    void updateCursor(double timeSeconds);
    void setPixelsPerSecond(double pps);

signals:
    void seekRequested(double timeSeconds);
    void clipSelected(const QString& clipId);
    void subtitleMoved(int lineIndex, double newStartTime, double newEndTime);
    void subtitleDeleted(int lineIndex);
    void subtitleDoubleClicked(int lineIndex);
    // Emitted when a lyrics block is dropped onto the timeline
    void lyricDropped(int lineIndex, double dropTimeSeconds);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    void drawPlayhead(QPainter& painter);
    void drawTracks(QPainter& painter);
    void drawRuler(QPainter& painter);
    bool inSubtitleTrack(int y) const;

    core::TimelineData* m_data = nullptr;
    core::LyricsData*   m_lyricsData = nullptr;

    // View state
    double m_pixelsPerSecond = 100.0;
    double m_scrollOffsetX = 0.0;
    double m_currentTime = 0.0;
    double m_hoverTime = -1.0;
    double m_dropPreviewTime = -1.0; // shows drop position indicator
    
    // Subtitle drag state
    int    m_draggingSubIdx  = -1;  // index of line being dragged (-1 = none)
    double m_dragOffsetSec   = 0.0; // click offset within block
    bool   m_resizingRight   = false;
    bool   m_resizingLeft    = false;
    double m_dragOrigDur     = 0.0; // original duration (for resize)
    
    // Word drag state
    int    m_draggingWordLineIdx = -1;
    int    m_draggingWordIdx = -1;
    bool   m_resizingWordRight = false;
    bool   m_resizingWordLeft = false;
    
    // Geometry
    int m_trackHeight = 60;
    int m_rulerHeight = 24;
};

} // namespace ncktv
