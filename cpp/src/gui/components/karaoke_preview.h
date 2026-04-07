#pragma once

#include <QWidget>
#include <QPainter>
#include <QImage>
#include <QVideoSink>
#include <QVideoFrame>
#include <QMediaPlayer>
#include "../../core/timeline/ncktv_core_data.hpp"

namespace ncktv {

class KaraokePreview : public QWidget {
    Q_OBJECT

public:
    // Display style: VideoOverlay = lyrics over video (bottom-third),
    //                CenteredBlack = lyrics centered on black bg (karaoke box style)
    enum class DisplayMode { VideoOverlay, CenteredBlack };

    explicit KaraokePreview(QWidget* parent = nullptr);

    // Data Loading
    void loadLyrics(core::LyricsData* data);
    void updateTime(double timeSeconds);
    void setBackgroundImage(const QImage& img);
    void setDisplayMode(DisplayMode mode);
    DisplayMode displayMode() const { return m_displayMode; }

    // Attach the media player so this widget decodes video frames
    void setMediaPlayer(QMediaPlayer* player);

protected:
    void paintEvent(QPaintEvent* event) override;

private slots:
    void onVideoFrameChanged(const QVideoFrame& frame);

private:
    void drawSubtitles(QPainter& painter);

    core::LyricsData* m_data = nullptr;
    double m_currentTime = 0.0;
    QImage m_backgroundImg;
    QVideoSink* m_videoSink = nullptr;
    DisplayMode m_displayMode = DisplayMode::VideoOverlay;

    // View State
    const int m_targetWidth = 1920;
    const int m_targetHeight = 1080;
};

} // namespace ncktv
