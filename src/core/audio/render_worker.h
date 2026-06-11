#pragma once

#include <QThread>
#include <QString>
#include <QImage>
#include <atomic>
#include "../timeline/timeline_manager.h"
#include "audio_engine.h"
#include "render_engine.h"

namespace ncktv {

class RenderWorker : public QThread {
    Q_OBJECT

public:
    RenderWorker(TimelineManager* timelineManager,
                 AudioEngine* audioEngine,
                 const QString& outputPath,
                 int width,
                 int height,
                 int fps,
                 int videoBitrate,
                 int audioBitrate,
                 const QString& audioCodec,
                 QObject* parent = nullptr);
    
    ~RenderWorker() override;

signals:
    void progressUpdated(double fraction);
    void statusTextChanged(const QString& text);
    void renderCompleted(const QString& outputPath, const QString& videoCodec, const QString& audioCodec);
    void renderFailed(const QString& errorMessage);

protected:
    void run() override;

private:
    void renderFrameAtTime(QImage& image, QImage& lowResImage, qint64 timeUs);
    void renderIntroFrameAtTime(QImage& image, qint64 frameTimeUs, qint64 totalIntroDurationUs);
    void renderEndCreditFrameAtTime(QImage& image, QImage& lowResImage, qint64 frameTimeUs, qint64 totalEndDurationUs);

    TimelineManager* m_timelineManager = nullptr;
    AudioEngine* m_audioEngine = nullptr;
    QString m_outputPath;
    int m_width;
    int m_height;
    int m_fps;
    int m_videoBitrate;
    int m_audioBitrate;
    QString m_audioCodec;

    // Subtitle styles copied from TimelineManager at initialization
    QString m_subtitleFontFamily;
    int m_subtitleFontSize = 36;
    QString m_subtitleFillColor;
    QString m_subtitleActiveColor;
    QString m_subtitleOutlineColor;
    int m_subtitleOutlineWidth = 0;

    bool m_showVideoBackground = true;
    bool m_lyricsOnlyMode = false;
    int m_lyricDisplayMode = 0;
    int m_exportAudioMode = 0;

    // Intro/End Credit metadata
    QString m_songTitle;
    QString m_artistName;
    qint64 m_introSplashDurationUs = 3000000; // microseconds
    QString m_endingVideoPath; // resolved path to end.mp4
    static constexpr qint64 END_CREDIT_DURATION_US = 5000000; // 5 second default end credit
};

} // namespace ncktv
