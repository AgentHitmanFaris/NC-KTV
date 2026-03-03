#pragma once

#include <QWidget>
#include <QPainter>
#include <QImage>

#include "../../core/lyrics/lyrics_data.h"

namespace ncktv {

class KaraokePreview : public QWidget {
    Q_OBJECT

public:
    explicit KaraokePreview(QWidget* parent = nullptr);

    void loadLyrics(LyricsData* data);
    void updateTime(double timeSeconds);
    void setBackgroundImage(const QImage& img);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void drawSubtitles(QPainter& painter);

    LyricsData* m_data = nullptr;
    double m_currentTime = 0.0;
    QImage m_backgroundImg;

    // View State
    const int m_targetWidth = 1920;
    const int m_targetHeight = 1080;
};

} // namespace ncktv
