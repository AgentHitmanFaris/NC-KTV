#include "render_worker.h"
#include <future>
#include <thread>
#include <QPainter>
#include <QPainterPath>
#include <QFont>
#include <QFontMetrics>
#include <QColor>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QRect>
#include <QDebug>
#include <cmath>
#include <vector>
#include <algorithm>
#include <QElapsedTimer>
#include <QDir>

namespace ncktv {

class VideoFrameReader {
public:
    VideoFrameReader() = default;
    ~VideoFrameReader() = default;

    QString filePath() const { return m_filePath; }
    void cleanup() {}

    bool open(const QString& filePath) {
        m_filePath = filePath;
        return true;
    }

    bool readFrameAt(qint64 timeUs, QImage& image) {
        (void)timeUs; (void)image;
        return false; // Return false so background drawing falls back to solid/gradient fills
    }

private:
    QString m_filePath;
};

namespace {
thread_local VideoFrameReader t_videoReader;
}


RenderWorker::RenderWorker(TimelineManager* timelineManager,
                           AudioEngine* audioEngine,
                           const QString& outputPath,
                           int width,
                           int height,
                           int fps,
                           int videoBitrate,
                           int audioBitrate,
                           const QString& audioCodec,
                           QObject* parent)
    : QThread(parent),
      m_timelineManager(timelineManager),
      m_audioEngine(audioEngine),
      m_outputPath(outputPath),
      m_width(width),
      m_height(height),
      m_fps(fps),
      m_videoBitrate(videoBitrate),
      m_audioBitrate(audioBitrate),
      m_audioCodec(audioCodec) {
    if (m_timelineManager) {
        m_subtitleFontFamily = m_timelineManager->subtitleFontFamily();
        m_subtitleFontSize = m_timelineManager->subtitleFontSize();
        m_subtitleFillColor = m_timelineManager->subtitleFillColor();
        m_subtitleActiveColor = m_timelineManager->subtitleActiveColor();
        m_subtitleOutlineColor = m_timelineManager->subtitleOutlineColor();
        m_subtitleOutlineWidth = m_timelineManager->subtitleOutlineWidth();
        m_showVideoBackground = m_timelineManager->showVideoBackground();
        m_lyricsOnlyMode = m_timelineManager->lyricsOnlyMode();
        m_lyricDisplayMode = m_timelineManager->lyricDisplayMode();
        m_exportAudioMode = m_timelineManager->exportAudioMode();

        // Intro/End Credit metadata
        m_songTitle = m_timelineManager->songTitle();
        m_artistName = m_timelineManager->artistName();
        m_introSplashDurationUs = static_cast<qint64>(m_timelineManager->introSplashDuration()) * 1000; // ms -> us
        m_endingVideoPath = m_timelineManager->resolveEndingVideoPath();
    }
}

RenderWorker::~RenderWorker() {
    if (isRunning()) {
        requestInterruption();
        wait();
    }
}

void RenderWorker::run() {
    emit statusTextChanged("Preparing render engine...");
    emit progressUpdated(0.0);

    qint64 totalDuration = m_timelineManager->totalDuration();
    if (totalDuration <= 0) {
        emit renderFailed("Timeline is empty. Cannot export a project with zero duration.");
        return;
    }

    // Calculate frame counts for each phase
    int64_t introFrames = (m_introSplashDurationUs * m_fps) / 1000000;
    int64_t timelineFrames = (totalDuration * m_fps) / 1000000;
    if (timelineFrames <= 0) timelineFrames = 1;

    // Determine end credit duration: try to probe end.mp4 length, else use default
    qint64 endCreditDurationUs = END_CREDIT_DURATION_US;
    // If ending video is unavailable, still render a styled end screen
    int64_t endCreditFrames = (endCreditDurationUs * m_fps) / 1000000;

    int64_t totalFrames = introFrames + timelineFrames + endCreditFrames;

    RenderEngine engine;
    if (!engine.startRender(m_outputPath, m_width, m_height, m_fps, m_videoBitrate, m_audioBitrate, m_audioCodec)) {
        emit renderFailed("Could not initialize FFmpeg render engine. Check file path or codec permissions.");
        return;
    }

    QElapsedTimer renderTimer;
    renderTimer.start();

    emit statusTextChanged(QString("Rendering with codec: %1...").arg(engine.chosenVideoCodec()));

    double audioSamplePlayhead = 0.0;
    bool success = true;

    int numThreads = std::max(2, static_cast<int>(std::thread::hardware_concurrency()));
    int chunkSize = numThreads * 2;

    std::vector<QImage> chunkImages(chunkSize);
    for (int i = 0; i < chunkSize; ++i) {
        chunkImages[i] = QImage(m_width, m_height, QImage::Format_RGBA8888);
    }

    int64_t globalFrameIdx = 0;

    // ═══════════════════════════════════════════════════════════════════
    // PHASE 1: Intro Splash Screen Frames
    // ═══════════════════════════════════════════════════════════════════
    if (introFrames > 0) {
        emit statusTextChanged("Rendering intro splash screen...");
        int64_t introIdx = 0;
        while (introIdx < introFrames) {
            if (isInterruptionRequested()) { success = false; break; }

            int currentChunkSize = std::min(static_cast<int>(chunkSize), static_cast<int>(introFrames - introIdx));

            std::vector<std::future<void>> futures;
            futures.reserve(currentChunkSize);

            for (int i = 0; i < currentChunkSize; ++i) {
                int64_t frameIdx = introIdx + i;
                qint64 frameTimeUs = (frameIdx * 1000000) / m_fps;

                futures.push_back(std::async(std::launch::async, [this, i, frameTimeUs, &chunkImages]() {
                    this->renderIntroFrameAtTime(chunkImages[i], frameTimeUs, m_introSplashDurationUs);
                }));
            }

            for (auto& fut : futures) { fut.wait(); }

            for (int i = 0; i < currentChunkSize; ++i) {
                if (!engine.writeVideoFrame(chunkImages[i], globalFrameIdx)) {
                    emit renderFailed("Failed to encode intro video frame.");
                    return;
                }

                // Write silent audio during intro
                double nextAudioSamplePlayhead = (static_cast<double>(globalFrameIdx + 1) * 48000.0) / m_fps;
                int samplesToMix = static_cast<int>(std::round(nextAudioSamplePlayhead)) - static_cast<int>(std::round(audioSamplePlayhead));
                if (samplesToMix > 0) {
                    std::vector<float> silenceBuffer(samplesToMix * 2, 0.0f);
                    if (!engine.writeAudioFrame(silenceBuffer.data(), samplesToMix)) {
                        emit renderFailed("Failed to encode intro audio frame.");
                        return;
                    }
                }
                audioSamplePlayhead = nextAudioSamplePlayhead;
                globalFrameIdx++;
            }
            introIdx += currentChunkSize;

            double progressFraction = static_cast<double>(globalFrameIdx) / static_cast<double>(totalFrames);
            emit progressUpdated(progressFraction);
            emit statusTextChanged(QString("Rendering intro frame %1 of %2 (%3%)").arg(introIdx).arg(introFrames).arg(static_cast<int>(progressFraction * 100)));
        }
    }

    // ═══════════════════════════════════════════════════════════════════
    // PHASE 2: Timeline Content Frames (existing rendering)
    // ═══════════════════════════════════════════════════════════════════
    if (success) {
        int64_t timelineIdx = 0;
        while (timelineIdx < timelineFrames) {
            if (isInterruptionRequested()) { success = false; break; }

            int currentChunkSize = std::min(static_cast<int>(chunkSize), static_cast<int>(timelineFrames - timelineIdx));

            std::vector<std::future<void>> futures;
            futures.reserve(currentChunkSize);

            for (int i = 0; i < currentChunkSize; ++i) {
                int64_t frameIdx = timelineIdx + i;
                qint64 frameTimeUs = (frameIdx * 1000000) / m_fps;

                futures.push_back(std::async(std::launch::async, [this, i, frameTimeUs, &chunkImages]() {
                    int lowW = std::max(60, m_width / 8);
                    int lowH = std::max(34, m_height / 8);
                    QImage lowResImage(lowW, lowH, QImage::Format_RGBA8888);
                    this->renderFrameAtTime(chunkImages[i], lowResImage, frameTimeUs);
                }));
            }

            for (auto& fut : futures) { fut.wait(); }

            for (int i = 0; i < currentChunkSize; ++i) {
                if (!engine.writeVideoFrame(chunkImages[i], globalFrameIdx)) {
                    emit renderFailed("Failed to encode video frame. Export aborted.");
                    return;
                }

                double nextAudioSamplePlayhead = (static_cast<double>(globalFrameIdx + 1) * 48000.0) / m_fps;
                int samplesToMix = static_cast<int>(std::round(nextAudioSamplePlayhead)) - static_cast<int>(std::round(audioSamplePlayhead));

                if (samplesToMix > 0) {
                    std::vector<float> mixBuffer(samplesToMix * 2, 0.0f);
                    // Compute the actual timeline audio position (subtract intro offset)
                    qint64 audioPos = static_cast<qint64>(std::round(audioSamplePlayhead)) - static_cast<qint64>((static_cast<double>(introFrames) * 48000.0) / m_fps);
                    if (audioPos >= 0) {
                        m_audioEngine->mixOffline(mixBuffer.data(), samplesToMix, audioPos, m_exportAudioMode == 1);
                    }
                    if (!engine.writeAudioFrame(mixBuffer.data(), samplesToMix)) {
                        emit renderFailed("Failed to encode audio stream. Export aborted.");
                        return;
                    }
                }
                audioSamplePlayhead = nextAudioSamplePlayhead;
                globalFrameIdx++;
            }

            timelineIdx += currentChunkSize;

            double progressFraction = static_cast<double>(globalFrameIdx) / static_cast<double>(totalFrames);
            emit progressUpdated(progressFraction);

            QString etaStr;
            if (progressFraction > 0.01) {
                qint64 elapsedMs = renderTimer.elapsed();
                double totalEstimatedMs = static_cast<double>(elapsedMs) / progressFraction;
                qint64 remainingMs = static_cast<qint64>(totalEstimatedMs - elapsedMs);

                int totalSecs = static_cast<int>(remainingMs / 1000.0);
                int hours = totalSecs / 3600;
                int minutes = (totalSecs % 3600) / 60;
                int seconds = totalSecs % 60;

                if (hours > 0) {
                    etaStr = QString(" (ETA: %1h %2m)").arg(hours).arg(minutes);
                } else if (minutes > 0) {
                    etaStr = QString(" (ETA: %1m %2s)").arg(minutes).arg(seconds);
                } else {
                    etaStr = QString(" (ETA: %1s)").arg(seconds);
                }
            } else {
                etaStr = " (ETA: Calculating...)";
            }

            emit statusTextChanged(QString("Encoding frame %1 of %2 (%3%)%4")
                                   .arg(globalFrameIdx)
                                   .arg(totalFrames)
                                   .arg(static_cast<int>(progressFraction * 100))
                                   .arg(etaStr));
        }
    }

    // ═══════════════════════════════════════════════════════════════════
    // PHASE 3: End Credit Frames
    // ═══════════════════════════════════════════════════════════════════
    if (success && endCreditFrames > 0) {
        emit statusTextChanged("Rendering end credits...");
        int64_t endIdx = 0;
        while (endIdx < endCreditFrames) {
            if (isInterruptionRequested()) { success = false; break; }

            int currentChunkSize = std::min(static_cast<int>(chunkSize), static_cast<int>(endCreditFrames - endIdx));

            std::vector<std::future<void>> futures;
            futures.reserve(currentChunkSize);

            for (int i = 0; i < currentChunkSize; ++i) {
                int64_t frameIdx = endIdx + i;
                qint64 frameTimeUs = (frameIdx * 1000000) / m_fps;

                futures.push_back(std::async(std::launch::async, [this, i, frameTimeUs, endCreditDurationUs, &chunkImages]() {
                    int lowW = std::max(60, m_width / 8);
                    int lowH = std::max(34, m_height / 8);
                    QImage lowResImage(lowW, lowH, QImage::Format_RGBA8888);
                    this->renderEndCreditFrameAtTime(chunkImages[i], lowResImage, frameTimeUs, endCreditDurationUs);
                }));
            }

            for (auto& fut : futures) { fut.wait(); }

            for (int i = 0; i < currentChunkSize; ++i) {
                if (!engine.writeVideoFrame(chunkImages[i], globalFrameIdx)) {
                    emit renderFailed("Failed to encode end credit video frame.");
                    return;
                }

                // Write silent audio during end credits
                double nextAudioSamplePlayhead = (static_cast<double>(globalFrameIdx + 1) * 48000.0) / m_fps;
                int samplesToMix = static_cast<int>(std::round(nextAudioSamplePlayhead)) - static_cast<int>(std::round(audioSamplePlayhead));
                if (samplesToMix > 0) {
                    std::vector<float> silenceBuffer(samplesToMix * 2, 0.0f);
                    if (!engine.writeAudioFrame(silenceBuffer.data(), samplesToMix)) {
                        emit renderFailed("Failed to encode end credit audio frame.");
                        return;
                    }
                }
                audioSamplePlayhead = nextAudioSamplePlayhead;
                globalFrameIdx++;
            }
            endIdx += currentChunkSize;

            double progressFraction = static_cast<double>(globalFrameIdx) / static_cast<double>(totalFrames);
            emit progressUpdated(progressFraction);
            emit statusTextChanged(QString("Rendering end credit frame %1 of %2 (%3%)").arg(endIdx).arg(endCreditFrames).arg(static_cast<int>(progressFraction * 100)));
        }
    }

    if (success) {
        emit statusTextChanged("Flushing buffers and closing container...");
        if (engine.finishRender()) {
            emit progressUpdated(1.0);
            emit statusTextChanged("Export completed successfully!");
            emit renderCompleted(m_outputPath, engine.chosenVideoCodec(), engine.chosenAudioCodec());
        } else {
            emit renderFailed("Failed to flush FFmpeg buffers. Output file might be corrupted.");
        }
    } else {
        emit renderFailed("Render cancelled by user.");
    }
}

void RenderWorker::renderFrameAtTime(QImage& image, QImage& lowResImage, qint64 timeUs) {
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    // 1. Draw Background (Video clip decoding or gradient fallback)
    bool drewVideo = false;
    if (m_showVideoBackground) {
        Clip* activeVideoClip = nullptr;
        for (Track* track : m_timelineManager->trackListModel()->tracks()) {
            if (track->trackType() == Track::Video) {
                for (Clip* clip : track->clips()) {
                    if (timeUs >= clip->startTime() && timeUs < clip->endTime()) {
                        activeVideoClip = clip;
                        break;
                    }
                }
            }
            if (activeVideoClip) break;
        }

        if (activeVideoClip) {
            qint64 sourceTimeUs = (timeUs - activeVideoClip->startTime()) + activeVideoClip->sourceStart();
            if (t_videoReader.filePath() != activeVideoClip->sourceFile()) {
                t_videoReader.open(activeVideoClip->sourceFile());
            }
            drewVideo = t_videoReader.readFrameAt(sourceTimeUs, image);
        }
    }

    if (!drewVideo) {
        if (m_lyricDisplayMode == 2) {
            painter.fillRect(image.rect(), QColor(0, 0, 0));
        } else if (m_lyricDisplayMode == 3) {
            QRectF rect = image.rect();
            // Create dark gradient background
            QLinearGradient bgGrad(rect.topLeft(), rect.bottomRight());
            bgGrad.setColorAt(0.0, QColor(10, 10, 15));
            bgGrad.setColorAt(0.5, QColor(20, 20, 30));
            bgGrad.setColorAt(1.0, QColor(10, 10, 15));
            painter.fillRect(rect, bgGrad);
        } else {
            // Render slate background gradient and ambient glows on the persistent low-res canvas
            int lowW = lowResImage.width();
            int lowH = lowResImage.height();
            {
                QPainter lowPainter(&lowResImage);
                lowPainter.setRenderHint(QPainter::Antialiasing);

                QLinearGradient bgGradient(0, 0, lowW, lowH);
                bgGradient.setColorAt(0.0, QColor("#080711"));
                bgGradient.setColorAt(0.5, QColor("#120d24"));
                bgGradient.setColorAt(1.0, QColor("#040308"));
                lowPainter.fillRect(lowResImage.rect(), bgGradient);

                double phase = static_cast<double>(timeUs) / 1000000.0;
                
                // Slow moving glow 1
                int cx1 = static_cast<int>(lowW * (0.3 + 0.1 * std::sin(phase * 0.5)));
                int cy1 = static_cast<int>(lowH * (0.4 + 0.1 * std::cos(phase * 0.7)));
                QRadialGradient glow1(cx1, cy1, lowW * 0.4);
                glow1.setColorAt(0.0, QColor(99, 102, 241, 20)); // Indigo soft glow
                glow1.setColorAt(1.0, QColor(0, 0, 0, 0));
                lowPainter.fillRect(lowResImage.rect(), glow1);

                // Slow moving glow 2
                int cx2 = static_cast<int>(lowW * (0.7 + 0.15 * std::cos(phase * 0.6)));
                int cy2 = static_cast<int>(lowH * (0.6 + 0.08 * std::sin(phase * 0.4)));
                QRadialGradient glow2(cx2, cy2, lowW * 0.35);
                glow2.setColorAt(0.0, QColor(236, 72, 153, 16)); // Pink soft glow
                glow2.setColorAt(1.0, QColor(0, 0, 0, 0));
                lowPainter.fillRect(lowResImage.rect(), glow2);
            }

            // Upscale the soft background using smooth bilinear scaling
            painter.drawImage(image.rect(), lowResImage);
        }
    }

    // Apply Radial Vignette Overlay for Cinematic mode (mode 3) over video or fallback background
    if (m_lyricDisplayMode == 3) {
        QRectF rect = image.rect();
        QRadialGradient vignette(rect.center(), std::max(rect.width(), rect.height()) * 0.7);
        vignette.setColorAt(0.0, QColor(0, 0, 0, 0));
        vignette.setColorAt(0.7, QColor(0, 0, 0, 50));
        vignette.setColorAt(1.0, QColor(0, 0, 0, 220)); // Soft cinematic shadow frame
        painter.fillRect(rect, vignette);
    }

    // 2. Draw NC-KTV watermark (crisp full-resolution text)
    painter.setFont(QFont("Arial", 16, QFont::Medium));
    painter.setPen(QColor(255, 255, 255, 60)); // Subtle watermark
    painter.drawText(QRect(50, 50, 300, 50), Qt::AlignLeft | Qt::AlignVCenter, "NC-KTV Studio");

    // 3. Find active lyric clip, sorted upcoming lyric clips, and all lyric clips
    Clip* activeClip = nullptr;
    std::vector<Clip*> upcomingClips;
    std::vector<Clip*> lyricClips;
    for (Track* track : m_timelineManager->trackListModel()->tracks()) {
        if (track->trackType() == Track::Lyrics) {
            for (Clip* clip : track->clips()) {
                lyricClips.push_back(clip);
                if (timeUs >= clip->startTime() && timeUs < clip->endTime()) {
                    activeClip = clip;
                } else if (clip->startTime() > timeUs) {
                    upcomingClips.push_back(clip);
                }
            }
        }
    }
    std::sort(upcomingClips.begin(), upcomingClips.end(), [](Clip* a, Clip* b) {
        return a->startTime() < b->startTime();
    });
    std::sort(lyricClips.begin(), lyricClips.end(), [](Clip* a, Clip* b) {
        return a->startTime() < b->startTime();
    });

    Clip* nextClip = upcomingClips.empty() ? nullptr : upcomingClips.front();

    // Helper lambda to draw styled text with outline/fill and centering/alignment
    auto drawStyledText = [&](const QString& text, const QPointF& pos, const QFont& font, const QColor& fill, const QColor& outline, int outlineWidth, int alignFlags = Qt::AlignCenter) {
        painter.save();
        painter.setFont(font);

        QFontMetricsF fm(font);
        qreal textWidth = fm.horizontalAdvance(text);
        qreal textHeight = fm.height();
        qreal ascent = fm.ascent();

        QPointF textPos = pos;
        if (alignFlags & Qt::AlignHCenter) {
            textPos.setX(pos.x() - textWidth / 2.0);
        } else if (alignFlags & Qt::AlignRight) {
            textPos.setX(pos.x() - textWidth);
        }

        if (alignFlags & Qt::AlignVCenter) {
            textPos.setY(pos.y() - textHeight / 2.0 + ascent);
        } else if (alignFlags & Qt::AlignBottom) {
            textPos.setY(pos.y() - textHeight + ascent);
        } else {
            // Default relative to baseline
            textPos.setY(pos.y() + ascent - textHeight / 2.0);
        }

        if (outlineWidth > 0) {
            QPainterPath path;
            path.addText(textPos, font, text);

            QPen pen(outline, outlineWidth * 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            painter.strokePath(path, pen);
            painter.fillPath(path, fill);
        } else {
            painter.setPen(fill);
            painter.drawText(textPos, text);
        }

        painter.restore();
    };

    // Helper lambda to draw an active swept lyric line
    auto drawActiveClip = [&](Clip* clip, int textY, double scaleFactor, double opacity, double customFontScale = 1.0) {
        QVariantList syllables = clip->syllables();
        if (syllables.isEmpty()) return;

        double opacityVal = opacity;
        
        int fontSize = static_cast<int>((m_subtitleFontSize > 0 ? m_subtitleFontSize : 36) * scaleFactor * customFontScale);
        QString fontName = m_subtitleFontFamily.isEmpty() ? "Arial" : m_subtitleFontFamily;
        
        QFont lyricFont(fontName, fontSize, QFont::Bold);
        painter.setFont(lyricFont);
        QFontMetrics fm(lyricFont);

        // Lay out syllables to compute metrics
        struct SylLayout {
            QString text;
            qint64 relativeStart;
            qint64 duration;
            int width;
            int xStart;
        };
        std::vector<SylLayout> layouts;
        int accumulatedWidth = 0;
        for (const QVariant& var : syllables) {
            QVariantMap map = var.toMap();
            SylLayout item;
            item.text = map["text"].toString();
            item.relativeStart = map["relativeStart"].toLongLong();
            item.duration = map["duration"].toLongLong();
            item.width = fm.horizontalAdvance(item.text);
            item.xStart = accumulatedWidth;
            accumulatedWidth += item.width;
            layouts.push_back(item);
        }

        int totalWidth = accumulatedWidth;
        int startX = (m_width - totalWidth) / 2;

        // Draw translucent backing rounded card to protect legibility (only if display mode is 0 or 1, matching preview overlays)
        if (m_lyricDisplayMode == 0 || m_lyricDisplayMode == 1) {
            int paddingX = static_cast<int>(40 * scaleFactor);
            int paddingY = static_cast<int>(20 * scaleFactor);
            QRect backdropRect(startX - paddingX, textY - fm.ascent() - paddingY, totalWidth + paddingX * 2, fm.height() + paddingY * 2);
            QColor backdropColor(15, 23, 42, static_cast<int>(160 * opacityVal));
            painter.setBrush(backdropColor);
            painter.setPen(Qt::NoPen);
            painter.drawRoundedRect(backdropRect, 16, 16);
        }

        // Create path representing the full sentence
        QPainterPath phrasePath;
        int currX = startX;
        for (const auto& item : layouts) {
            phrasePath.addText(currX, textY, lyricFont, item.text);
            currX += item.width;
        }

        // Draw standard inactive text outline
        QColor outlineColor = m_subtitleOutlineColor.isEmpty() ? QColor("#020617") : QColor(m_subtitleOutlineColor);
        int outlineWidth = static_cast<int>((m_subtitleOutlineWidth >= 0 ? m_subtitleOutlineWidth : 6) * scaleFactor * customFontScale);
        
        if (outlineWidth > 0) {
            QColor oCol = outlineColor;
            oCol.setAlphaF(opacityVal * outlineColor.alphaF());
            QPen outlinePen(oCol);
            outlinePen.setWidth(outlineWidth);
            outlinePen.setCapStyle(Qt::RoundCap);
            outlinePen.setJoinStyle(Qt::RoundJoin);
            painter.strokePath(phrasePath, outlinePen);
        }
        
        QColor fillColor = m_subtitleFillColor.isEmpty() ? QColor("#E2E8F0") : QColor(m_subtitleFillColor);
        QColor fCol = fillColor;
        fCol.setAlphaF(opacityVal * fillColor.alphaF());
        painter.fillPath(phrasePath, fCol);

        // Draw swept active neon sweeps
        qint64 relativeClipTime = timeUs - clip->startTime();
        QColor activeColor = m_subtitleActiveColor.isEmpty() ? QColor("#22C55E") : QColor(m_subtitleActiveColor);
        
        for (const auto& item : layouts) {
            double sweepFraction = 0.0;
            if (relativeClipTime >= item.relativeStart + item.duration) {
                sweepFraction = 1.0;
            } else if (relativeClipTime <= item.relativeStart) {
                sweepFraction = 0.0;
            } else {
                sweepFraction = static_cast<double>(relativeClipTime - item.relativeStart) / item.duration;
            }

            if (sweepFraction > 0.0) {
                int sylStartX = startX + item.xStart;
                int sweepWidth = static_cast<int>(item.width * sweepFraction);

                painter.save();
                painter.setClipRect(sylStartX, textY - fm.ascent() - 20, sweepWidth, fm.height() + 40);

                QPainterPath sylPath;
                sylPath.addText(sylStartX, textY, lyricFont, item.text);

                // Underlay glow drop shadow using active color with transparency
                if (outlineWidth > 0) {
                    QColor glowColor = activeColor;
                    glowColor.setAlpha(static_cast<int>(120 * opacityVal));
                    QPen glowPen(glowColor);
                    glowPen.setWidth(outlineWidth + 2);
                    glowPen.setCapStyle(Qt::RoundCap);
                    glowPen.setJoinStyle(Qt::RoundJoin);
                    painter.strokePath(sylPath, glowPen);

                    // Middle separation outline
                    QColor sepCol = outlineColor;
                    sepCol.setAlphaF(opacityVal * outlineColor.alphaF());
                    QPen separatorPen(sepCol);
                    separatorPen.setWidth(outlineWidth / 2 + 1);
                    separatorPen.setCapStyle(Qt::RoundCap);
                    separatorPen.setJoinStyle(Qt::RoundJoin);
                    painter.strokePath(sylPath, separatorPen);
                }

                // Fill active text
                QColor actCol = activeColor;
                actCol.setAlphaF(opacityVal * activeColor.alphaF());
                painter.fillPath(sylPath, actCol);
                painter.restore();
            }
        }
    };

    // Helper lambda to draw an inactive (upcoming/previous) lyric line
    auto drawInactiveClip = [&](Clip* clip, int textY, double scaleFactor, double opacity, double customFontScale = 1.0) {
        int fontSize = static_cast<int>((m_subtitleFontSize > 0 ? m_subtitleFontSize : 36) * scaleFactor * customFontScale);
        QString fontName = m_subtitleFontFamily.isEmpty() ? "Arial" : m_subtitleFontFamily;
        QFont lyricFont(fontName, fontSize, QFont::Bold);
        painter.setFont(lyricFont);
        QFontMetrics fm(lyricFont);

        int textWidth = fm.horizontalAdvance(clip->lyricText());
        int x = (m_width - textWidth) / 2;

        QPainterPath path;
        path.addText(x, textY, lyricFont, clip->lyricText());

        QColor outlineColor = m_subtitleOutlineColor.isEmpty() ? QColor("#020617") : QColor(m_subtitleOutlineColor);
        int outlineWidth = static_cast<int>((m_subtitleOutlineWidth >= 0 ? m_subtitleOutlineWidth : 6) * scaleFactor * customFontScale);
        QColor fillColor = m_subtitleFillColor.isEmpty() ? QColor("#E2E8F0") : QColor(m_subtitleFillColor);

        QColor oCol = outlineColor;
        oCol.setAlphaF(opacity * outlineColor.alphaF());
        QColor fCol = fillColor;
        fCol.setAlphaF(opacity * fillColor.alphaF());

        if (outlineWidth > 0) {
            QPen pen(oCol);
            pen.setWidth(outlineWidth);
            pen.setCapStyle(Qt::RoundCap);
            pen.setJoinStyle(Qt::RoundJoin);
            painter.strokePath(path, pen);
        }
        painter.fillPath(path, fCol);
    };

    double scaleFactor = static_cast<double>(m_height) / 800.0;
    int outlineWidth = static_cast<int>((m_subtitleOutlineWidth >= 0 ? m_subtitleOutlineWidth : 6) * scaleFactor);
    QColor outlineColor = m_subtitleOutlineColor.isEmpty() ? QColor("#020617") : QColor(m_subtitleOutlineColor);
    QColor fillColor = m_subtitleFillColor.isEmpty() ? QColor("#E2E8F0") : QColor(m_subtitleFillColor);

    if (m_lyricDisplayMode == 0) {
        // Mode 0: Bottom Two-Line (Timing sweep, bottom aligned)
        if (activeClip) {
            int textY = static_cast<int>(m_height * 0.75);
            drawActiveClip(activeClip, textY, scaleFactor, 1.0);
        }

        if (nextClip) {
            int nextFontSize = static_cast<int>(18 * scaleFactor);
            QFont nextFont("Outfit", nextFontSize, QFont::Bold);
            painter.setFont(nextFont);
            QFontMetrics fmNext(nextFont);

            int textWidth = fmNext.horizontalAdvance(nextClip->lyricText());
            int x = (m_width - textWidth) / 2;
            int y = static_cast<int>(m_height * 0.75 + 50 * scaleFactor);

            QPainterPath path;
            path.addText(x, y, nextFont, nextClip->lyricText());

            QColor oCol = outlineColor;
            oCol.setAlphaF(0.4 * outlineColor.alphaF());
            QColor fCol = fillColor;
            fCol.setAlphaF(0.4 * fillColor.alphaF());

            if (outlineWidth > 0) {
                QPen pen(oCol);
                pen.setWidth(outlineWidth / 2 + 1);
                pen.setCapStyle(Qt::RoundCap);
                pen.setJoinStyle(Qt::RoundJoin);
                painter.strokePath(path, pen);
            }
            painter.fillPath(path, fCol);
        }
    } else if (m_lyricDisplayMode == 1) {
        // Mode 1: Center Scroll Queue (Classic Cinematic Block style)
        double centerY = m_height * 0.45;
        double lineSpacing = 70.0 * scaleFactor;

        double scrollOffset = 0.0;
        double activeOpacity = 1.0;
        double nextOpacityFactor = 1.0;

        if (activeClip) {
            qint64 remain = activeClip->endTime() - timeUs;
            if (remain < 400000) {
                double t = (400000.0 - remain) / 400000.0;
                scrollOffset = t * lineSpacing;
                activeOpacity = 1.0 - t;
                nextOpacityFactor = 0.6 + 0.4 * t;
            }
        } else if (nextClip) {
            qint64 dt = nextClip->startTime() - timeUs;
            if (dt < 400000) {
                double t = (400000.0 - dt) / 400000.0;
                scrollOffset = (1.0 - t) * lineSpacing;
                nextOpacityFactor = 0.6 + 0.4 * t;
            } else {
                scrollOffset = lineSpacing;
                nextOpacityFactor = 0.6;
            }
        } else {
            scrollOffset = lineSpacing;
            nextOpacityFactor = 0.6;
        }

        // Draw Active Clip in the center (scrolling up)
        if (activeClip) {
            drawActiveClip(activeClip, centerY - scrollOffset, scaleFactor, activeOpacity);
        }

        // Draw Upcoming Clips in a queue below the center
        int fontSize = static_cast<int>((m_subtitleFontSize > 0 ? m_subtitleFontSize : 36) * scaleFactor);
        QString fontName = m_subtitleFontFamily.isEmpty() ? "Arial" : m_subtitleFontFamily;
        QFont lyricFont(fontName, fontSize, QFont::Bold);
        QFont metricsFont = lyricFont;
        metricsFont.setPixelSize(static_cast<int>(fontSize * 0.95));
        QFontMetrics fm(metricsFont);

        for (size_t i = 0; i < upcomingClips.size() && i < 4; ++i) {
            Clip* clip = upcomingClips[i];
            double baseOpacity = 0.6;
            if (i == 0) {
                baseOpacity = 0.6 * nextOpacityFactor;
                if (!activeClip) {
                    baseOpacity = nextOpacityFactor; // transitioning to active
                }
            } else if (i == 1) {
                baseOpacity = 0.4;
            } else if (i == 2) {
                baseOpacity = 0.2;
            } else {
                baseOpacity = 0.05;
            }

            int textWidth = fm.horizontalAdvance(clip->lyricText());
            int x = (m_width - textWidth) / 2;
            double y = centerY + (i + 1) * lineSpacing - scrollOffset;

            QPainterPath path;
            path.addText(x, y, metricsFont, clip->lyricText());

            QColor oCol = outlineColor;
            oCol.setAlphaF(baseOpacity * outlineColor.alphaF());
            QColor fCol = fillColor;
            fCol.setAlphaF(baseOpacity * fillColor.alphaF());

            if (outlineWidth > 0) {
                QPen pen(oCol);
                pen.setWidth(outlineWidth);
                pen.setCapStyle(Qt::RoundCap);
                pen.setJoinStyle(Qt::RoundJoin);
                painter.strokePath(path, pen);
            }
            painter.fillPath(path, fCol);
        }
    } else if (m_lyricDisplayMode == 2) {
        // Mode 2: Word Bounce (Centered Progressive timing sweep style)
        double midY = m_height / 2.0;

        if (activeClip) {
            drawActiveClip(activeClip, midY, scaleFactor, 1.0, 1.2);
            if (nextClip) {
                drawInactiveClip(nextClip, midY + m_subtitleFontSize * 2.0 * scaleFactor, scaleFactor * 0.8, 0.4, 1.2);
            }
        } else {
            if (nextClip) {
                drawInactiveClip(nextClip, midY, scaleFactor, 0.6, 1.2);
            } else {
                QString fontName = m_subtitleFontFamily.isEmpty() ? "Arial" : m_subtitleFontFamily;
                int fontSize = static_cast<int>((m_subtitleFontSize > 0 ? m_subtitleFontSize : 36) * scaleFactor * 1.2);
                QFont waitingFont(fontName, fontSize, QFont::Bold);
                drawStyledText("(Waiting for lyric cues)", QPointF(m_width / 2.0, midY), waitingFont, QColor(128, 128, 128, 128), outlineColor, outlineWidth);
            }
        }
    } else if (m_lyricDisplayMode == 3) {
        // Mode 3: Cinematic Full-Screen Focus (Transitions + scaling)
        double midY = m_height / 2.0;
        double midX = m_width / 2.0;
        double spacing = m_subtitleFontSize * 2.2 * scaleFactor;

        int fontSize = static_cast<int>((m_subtitleFontSize > 0 ? m_subtitleFontSize : 36) * scaleFactor);
        QString fontName = m_subtitleFontFamily.isEmpty() ? "Arial" : m_subtitleFontFamily;
        QFont lyricFont(fontName, fontSize, QFont::Bold);

        if (lyricClips.empty()) {
            drawStyledText("(Waiting for lyric cues)", QPointF(midX, midY), lyricFont, QColor(128, 128, 128, 128), outlineColor, outlineWidth);
        } else {
            // Find focusIdx in lyricClips
            int focusIdx = -1;
            for (int i = 0; i < (int)lyricClips.size(); ++i) {
                if (timeUs >= lyricClips[i]->startTime() && timeUs < lyricClips[i]->endTime()) {
                    focusIdx = i;
                    break;
                }
            }
            if (focusIdx < 0) {
                for (int i = 0; i < (int)lyricClips.size(); ++i) {
                    if (lyricClips[i]->startTime() > timeUs) {
                        focusIdx = i;
                        break;
                    }
                }
            }
            if (focusIdx < 0) {
                focusIdx = (int)lyricClips.size() - 1;
            }

            // Calculate transition progress (interpolator)
            double t = 1.0;
            const qint64 transitionDurationUs = 300000; // Smooth 300ms transition (in microseconds)

            if (focusIdx >= 0) {
                qint64 lineStart = lyricClips[focusIdx]->startTime();
                qint64 diff = timeUs - lineStart;
                if (diff >= 0 && diff < transitionDurationUs) {
                    t = static_cast<double>(diff) / transitionDurationUs;
                }
            }

            double tSmooth = t * t * (3.0 - 2.0 * t);

            // Dynamic scale pulse factor for focus lyric (gentle breathing pulse)
            double pulseFactor = 1.0;
            bool hasActive = false;
            for (Clip* clip : lyricClips) {
                if (timeUs >= clip->startTime() && timeUs < clip->endTime()) {
                    hasActive = true;
                    break;
                }
            }
            if (hasActive) {
                pulseFactor = 1.0 + 0.025 * std::sin(2.0 * M_PI * ((timeUs / 1000) % 2500) / 2500.0);
            }

            // 1. Previous clip
            int prevIdx = focusIdx - 1;
            if (prevIdx >= 0) {
                Clip* clip = lyricClips[prevIdx];
                qreal fromY = midY - spacing;
                qreal toY = midY - spacing * 1.5;
                qreal y = fromY + (toY - fromY) * (1.0 - tSmooth);

                double fromScale = 0.8;
                double toScale = 0.6;
                double scale = fromScale + (toScale - fromScale) * (1.0 - tSmooth);

                double fromOpacity = 0.35;
                double toOpacity = 0.0;
                double opacity = fromOpacity + (toOpacity - fromOpacity) * (1.0 - tSmooth);

                painter.save();
                painter.translate(midX, y);
                painter.scale(scale, scale);

                QColor fill = QColor(200, 200, 200, static_cast<int>(opacity * 255));
                drawStyledText(clip->lyricText(), QPointF(0, 0), lyricFont, fill, outlineColor, outlineWidth);
                painter.restore();
            }

            // 2. Focus clip
            if (focusIdx >= 0 && focusIdx < (int)lyricClips.size()) {
                Clip* clip = lyricClips[focusIdx];
                qreal fromY = midY + spacing;
                qreal toY = midY;
                qreal y = fromY + (toY - fromY) * tSmooth;

                double fromScale = 0.8;
                double toScale = 1.15 * pulseFactor;
                double scale = fromScale + (toScale - fromScale) * tSmooth;

                double fromOpacity = 0.35;
                double toOpacity = 1.0;
                double opacity = fromOpacity + (toOpacity - fromOpacity) * tSmooth;

                painter.save();
                painter.translate(midX, y);
                painter.scale(scale, scale);

                QColor fill = QColor(255, 255, 255, static_cast<int>(opacity * 255));
                drawStyledText(clip->lyricText(), QPointF(0, 0), lyricFont, fill, outlineColor, outlineWidth);
                painter.restore();
            }

            // 3. Next clip
            int nextIdx = focusIdx + 1;
            if (nextIdx < (int)lyricClips.size()) {
                Clip* clip = lyricClips[nextIdx];
                qreal fromY = midY + spacing * 2.0;
                qreal toY = midY + spacing;
                qreal y = fromY + (toY - fromY) * tSmooth;

                double fromScale = 0.0;
                double toScale = 0.8;
                double scale = fromScale + (toScale - fromScale) * tSmooth;

                double fromOpacity = 0.0;
                double toOpacity = 0.35;
                double opacity = fromOpacity + (toOpacity - fromOpacity) * tSmooth;

                painter.save();
                painter.translate(midX, y);
                painter.scale(scale, scale);

                QColor fill = QColor(180, 180, 180, static_cast<int>(opacity * 255));
                drawStyledText(clip->lyricText(), QPointF(0, 0), lyricFont, fill, outlineColor, outlineWidth);
                painter.restore();
            }
        }
    }
}

void RenderWorker::renderIntroFrameAtTime(QImage& image, qint64 frameTimeUs, qint64 totalIntroDurationUs) {
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    // Dark gradient background (matching QML intro splash)
    QLinearGradient bgGrad(0, 0, image.width(), image.height());
    bgGrad.setColorAt(0.0, QColor("#050508"));
    bgGrad.setColorAt(0.5, QColor("#0F0B1E"));
    bgGrad.setColorAt(1.0, QColor("#050508"));
    painter.fillRect(image.rect(), bgGrad);

    // Breathing glow circle in center
    double phase = static_cast<double>(frameTimeUs) / 1000000.0;
    double glowOpacity = 0.03 + 0.05 * (0.5 + 0.5 * std::sin(phase * 2.5));
    int glowRadius = static_cast<int>(std::min(m_width, m_height) * 0.4);
    QRadialGradient glowGrad(m_width / 2.0, m_height / 2.0, glowRadius);
    glowGrad.setColorAt(0.0, QColor(124, 77, 255, static_cast<int>(glowOpacity * 255)));
    glowGrad.setColorAt(1.0, QColor(0, 0, 0, 0));
    painter.fillRect(image.rect(), glowGrad);

    // Animation timing (microseconds) - matching QML animation sequence
    const qint64 fadeInStartUs   = 0;
    const qint64 fadeInEndUs     = 500000;   // 500ms overlay fade-in
    const qint64 brandStartUs    = 150000;   // 150ms - "NOW PLAYING" appears
    const qint64 brandEndUs      = 500000;
    const qint64 titleStartUs    = 300000;   // 300ms - title appears
    const qint64 titleEndUs      = 900000;
    const qint64 dividerStartUs  = 450000;   // 450ms - divider line
    const qint64 dividerEndUs    = 950000;
    const qint64 artistStartUs   = 600000;   // 600ms - artist appears
    const qint64 artistEndUs     = 1100000;
    const qint64 fadeOutStartUs  = totalIntroDurationUs - 500000; // fade out in last 500ms
    const qint64 fadeOutEndUs    = totalIntroDurationUs;

    // Calculate master overlay opacity (fade in + fade out)
    double masterOpacity = 1.0;
    if (frameTimeUs < fadeInEndUs) {
        masterOpacity = std::clamp(static_cast<double>(frameTimeUs - fadeInStartUs) / (fadeInEndUs - fadeInStartUs), 0.0, 1.0);
    }
    if (frameTimeUs > fadeOutStartUs) {
        double fadeOutT = std::clamp(static_cast<double>(frameTimeUs - fadeOutStartUs) / (fadeOutEndUs - fadeOutStartUs), 0.0, 1.0);
        masterOpacity *= (1.0 - fadeOutT);
    }

    double scaleFactor = static_cast<double>(m_height) / 1080.0;
    double centerX = m_width / 2.0;
    double centerY = m_height / 2.0;

    // Helper: smooth step
    auto smoothStep = [](double t) -> double {
        t = std::clamp(t, 0.0, 1.0);
        return t * t * (3.0 - 2.0 * t);
    };

    // 1. "NOW PLAYING" branding label
    if (frameTimeUs >= brandStartUs) {
        double t = smoothStep(std::clamp(static_cast<double>(frameTimeUs - brandStartUs) / (brandEndUs - brandStartUs), 0.0, 1.0));
        double brandOpacity = 0.8 * t * masterOpacity;
        double brandScale = 0.8 + 0.2 * t;

        int fontSize = static_cast<int>(11 * scaleFactor * brandScale);
        QFont brandFont("Outfit", fontSize, QFont::Bold);
        brandFont.setLetterSpacing(QFont::AbsoluteSpacing, 4 * scaleFactor);
        painter.setFont(brandFont);
        QFontMetrics fm(brandFont);

        QString text = "NOW PLAYING";
        int textWidth = fm.horizontalAdvance(text);
        QColor color(0, 230, 118, static_cast<int>(brandOpacity * 255)); // #00E676
        painter.setPen(color);
        painter.drawText(static_cast<int>(centerX - textWidth / 2.0), static_cast<int>(centerY - 80 * scaleFactor), text);
    }

    // 2. Song Title
    if (frameTimeUs >= titleStartUs) {
        double t = smoothStep(std::clamp(static_cast<double>(frameTimeUs - titleStartUs) / (titleEndUs - titleStartUs), 0.0, 1.0));
        double titleOpacity = t * masterOpacity;
        double yOffset = 30.0 * scaleFactor * (1.0 - t); // slides up

        int fontSize = static_cast<int>(std::clamp(m_width / 14.0, 26.0, 44.0) * scaleFactor);
        QFont titleFont("Outfit", fontSize, QFont::Bold);
        painter.setFont(titleFont);
        QFontMetrics fm(titleFont);

        QString title = m_songTitle.isEmpty() ? "Untitled Song" : m_songTitle;
        int textWidth = fm.horizontalAdvance(title);

        // Outline glow
        QPainterPath titlePath;
        titlePath.addText(centerX - textWidth / 2.0, centerY - 20 * scaleFactor + yOffset, titleFont, title);

        QColor outlineColor(124, 77, 255, static_cast<int>(titleOpacity * 64)); // violet outline
        QPen outlinePen(outlineColor);
        outlinePen.setWidth(static_cast<int>(3 * scaleFactor));
        outlinePen.setCapStyle(Qt::RoundCap);
        outlinePen.setJoinStyle(Qt::RoundJoin);
        painter.strokePath(titlePath, outlinePen);

        QColor fillColor(240, 240, 245, static_cast<int>(titleOpacity * 255)); // #F0F0F5
        painter.fillPath(titlePath, fillColor);
    }

    // 3. Divider Line
    if (frameTimeUs >= dividerStartUs) {
        double t = smoothStep(std::clamp(static_cast<double>(frameTimeUs - dividerStartUs) / (dividerEndUs - dividerStartUs), 0.0, 1.0));
        double divOpacity = 0.7 * t * masterOpacity;
        double divWidth = 140.0 * scaleFactor * t;

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(124, 77, 255, static_cast<int>(divOpacity * 255))); // #7C4DFF
        painter.drawRect(QRectF(centerX - divWidth / 2.0, centerY + 15 * scaleFactor, divWidth, 1 * scaleFactor));
    }

    // 4. Artist Name
    if (frameTimeUs >= artistStartUs) {
        double t = smoothStep(std::clamp(static_cast<double>(frameTimeUs - artistStartUs) / (artistEndUs - artistStartUs), 0.0, 1.0));
        double artistOpacity = t * masterOpacity;
        double yOffset = 20.0 * scaleFactor * (1.0 - t);

        int fontSize = static_cast<int>(std::clamp(m_width / 22.0, 15.0, 22.0) * scaleFactor);
        QFont artistFont("Outfit", fontSize);
        painter.setFont(artistFont);
        QFontMetrics fm(artistFont);

        QString artist = m_artistName.isEmpty() ? "Unknown Artist" : m_artistName;
        int textWidth = fm.horizontalAdvance(artist);

        QColor color(138, 138, 158, static_cast<int>(artistOpacity * 255)); // #8A8A9E
        painter.setPen(color);
        painter.drawText(static_cast<int>(centerX - textWidth / 2.0), static_cast<int>(centerY + 55 * scaleFactor + yOffset), artist);
    }

    // 5. NC-KTV watermark (subtle)
    painter.setFont(QFont("Arial", static_cast<int>(10 * scaleFactor)));
    painter.setPen(QColor(255, 255, 255, static_cast<int>(30 * masterOpacity)));
    painter.drawText(QRect(0, m_height - static_cast<int>(40 * scaleFactor), m_width, static_cast<int>(30 * scaleFactor)),
                     Qt::AlignCenter, "NC-KTV Studio");
}

void RenderWorker::renderEndCreditFrameAtTime(QImage& image, QImage& lowResImage, qint64 frameTimeUs, qint64 totalEndDurationUs) {
    Q_UNUSED(lowResImage);

    // Try to decode end.mp4 video frame
    bool drewVideo = false;
    if (!m_endingVideoPath.isEmpty() && !m_endingVideoPath.startsWith("qrc:")) {
        // For filesystem paths, try VideoFrameReader
        // Note: qrc paths can't be decoded by FFmpeg directly
        // We'll use styled fallback for now
    }

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    if (!drewVideo) {
        // Styled "Thank You" end credit screen
        double scaleFactor = static_cast<double>(m_height) / 1080.0;
        double centerX = m_width / 2.0;
        double centerY = m_height / 2.0;

        // Fade in/out timing
        const qint64 fadeInDurationUs = 800000;  // 800ms fade in
        const qint64 fadeOutDurationUs = 1000000; // 1s fade out
        const qint64 fadeOutStartUs = totalEndDurationUs - fadeOutDurationUs;

        double masterOpacity = 1.0;
        if (frameTimeUs < fadeInDurationUs) {
            masterOpacity = std::clamp(static_cast<double>(frameTimeUs) / fadeInDurationUs, 0.0, 1.0);
        }
        if (frameTimeUs > fadeOutStartUs && fadeOutStartUs > 0) {
            double fadeOutT = std::clamp(static_cast<double>(frameTimeUs - fadeOutStartUs) / fadeOutDurationUs, 0.0, 1.0);
            masterOpacity *= (1.0 - fadeOutT);
        }

        // Elegant dark gradient background
        QLinearGradient bgGrad(0, 0, m_width, m_height);
        bgGrad.setColorAt(0.0, QColor(5, 5, 10));
        bgGrad.setColorAt(0.5, QColor(15, 12, 30));
        bgGrad.setColorAt(1.0, QColor(5, 5, 10));
        painter.fillRect(image.rect(), bgGrad);

        // Subtle breathing radial glow
        double phase = static_cast<double>(frameTimeUs) / 1000000.0;
        double glowAlpha = 0.04 + 0.03 * std::sin(phase * 1.5);
        QRadialGradient glow(centerX, centerY, std::min(m_width, m_height) * 0.5);
        glow.setColorAt(0.0, QColor(124, 77, 255, static_cast<int>(glowAlpha * 255 * masterOpacity)));
        glow.setColorAt(1.0, QColor(0, 0, 0, 0));
        painter.fillRect(image.rect(), glow);

        // "Thank You For Watching" title
        {
            int fontSize = static_cast<int>(36 * scaleFactor);
            QFont font("Outfit", fontSize, QFont::Bold);

            QPainterPath path;
            QString text = "Thank You For Watching";
            QFontMetrics fm(font);
            int textWidth = fm.horizontalAdvance(text);
            path.addText(centerX - textWidth / 2.0, centerY - 30 * scaleFactor, font, text);

            // Violet glow outline
            QColor outlineColor(124, 77, 255, static_cast<int>(80 * masterOpacity));
            QPen outlinePen(outlineColor);
            outlinePen.setWidth(static_cast<int>(4 * scaleFactor));
            outlinePen.setCapStyle(Qt::RoundCap);
            outlinePen.setJoinStyle(Qt::RoundJoin);
            painter.strokePath(path, outlinePen);

            QColor fillColor(240, 240, 245, static_cast<int>(255 * masterOpacity));
            painter.fillPath(path, fillColor);
        }

        // Song title (smaller, below)
        if (!m_songTitle.isEmpty()) {
            int fontSize = static_cast<int>(20 * scaleFactor);
            QFont font("Outfit", fontSize);
            QFontMetrics fm(font);
            int textWidth = fm.horizontalAdvance(m_songTitle);
            painter.setPen(QColor(138, 138, 158, static_cast<int>(200 * masterOpacity)));
            painter.setFont(font);
            painter.drawText(static_cast<int>(centerX - textWidth / 2.0), static_cast<int>(centerY + 30 * scaleFactor), m_songTitle);
        }

        // Artist name
        if (!m_artistName.isEmpty()) {
            int fontSize = static_cast<int>(16 * scaleFactor);
            QFont font("Outfit", fontSize);
            QFontMetrics fm(font);
            int textWidth = fm.horizontalAdvance(m_artistName);
            painter.setPen(QColor(100, 100, 120, static_cast<int>(180 * masterOpacity)));
            painter.setFont(font);
            painter.drawText(static_cast<int>(centerX - textWidth / 2.0), static_cast<int>(centerY + 65 * scaleFactor), m_artistName);
        }

        // Divider line
        {
            double divWidth = 100 * scaleFactor * masterOpacity;
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(124, 77, 255, static_cast<int>(120 * masterOpacity)));
            painter.drawRect(QRectF(centerX - divWidth / 2.0, centerY + 85 * scaleFactor, divWidth, 1.5 * scaleFactor));
        }

        // NC-KTV Studio branding
        {
            int fontSize = static_cast<int>(13 * scaleFactor);
            QFont font("Outfit", fontSize, QFont::Bold);
            font.setLetterSpacing(QFont::AbsoluteSpacing, 3 * scaleFactor);
            QFontMetrics fm(font);
            QString text = "NC-KTV STUDIO";
            int textWidth = fm.horizontalAdvance(text);
            painter.setPen(QColor(0, 230, 118, static_cast<int>(150 * masterOpacity))); // green accent
            painter.setFont(font);
            painter.drawText(static_cast<int>(centerX - textWidth / 2.0), static_cast<int>(centerY + 120 * scaleFactor), text);
        }
    }
}

} // namespace ncktv
