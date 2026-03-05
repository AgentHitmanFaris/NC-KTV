#pragma once

#include <QWidget>
#include <QPainter>
#include <QImage>
#include <QVideoSink>
#include <QVideoFrame>
#include <QMediaPlayer>

#include "../../core/lyrics/lyrics_data.h"

namespace ncktv {

class KaraokePreview : public QWidget {
    Q_OBJECT

public:
    explicit KaraokePreview(QWidget* parent = nullptr);

    void loadLyrics(LyricsData* data);
    void updateTime(double timeSeconds);
    void setBackgroundImage(const QImage& img);

    // Attach the media player so this widget decodes video frames
    void setMediaPlayer(QMediaPlayer* player);

protected:
    void paintEvent(QPaintEvent* event) override;

private slots:
    void onVideoFrameChanged(const QVideoFrame& frame);

private:
    void drawSubtitles(QPainter& painter);

    LyricsData* m_data = nullptr;
    double m_currentTime = 0.0;
    QImage m_backgroundImg;
    QVideoSink* m_videoSink = nullptr;

    // View State
    const int m_targetWidth = 1920;
    const int m_targetHeight = 1080;
};

} // namespace ncktv
