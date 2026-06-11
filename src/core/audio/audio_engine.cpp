#define NOMINMAX
#define MINIAUDIO_IMPLEMENTATION
#include "audio_engine.h"
#include <iostream>
#include <algorithm>
#include <QThreadPool>
#include <QRunnable>

namespace ncktv {

AudioEngine* AudioEngine::s_instance = nullptr;

AudioEngine* AudioEngine::instance() {
    return s_instance;
}

// Static C-compatible callback passed to miniaudio device configuration
static void maAudioCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    (void)pInput; // Unused input
    auto* engine = static_cast<AudioEngine*>(pDevice->pUserData);
    if (engine) {
        engine->mixAudio(static_cast<float*>(pOutput), frameCount);
    }
}

AudioEngine::AudioEngine(TimelineManager* timelineManager, QObject* parent)
    : QObject(parent),
      m_timelineManager(timelineManager),
      m_syncTimer(new QTimer(this)),
      m_playbackSampleAccumulator(0.0) {
    
    s_instance = this;

    // Configure miniaudio playback device
    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format   = ma_format_f32; // Interleaved standard floats
    config.playback.channels = 2;              // Stereo L/R channels
    config.sampleRate        = 48000;          // Studio reference rate
    config.dataCallback      = maAudioCallback;
    config.pUserData         = this;

    if (ma_device_init(nullptr, &config, &m_device) == MA_SUCCESS) {
        m_deviceInitialized = true;
    } else {
        std::cerr << "[AudioEngine] Error: Could not initialize miniaudio device.\n";
    }

    // Set up high-resolution sync timer (60fps update interval)
    m_syncTimer->setInterval(16); 
    connect(m_syncTimer, &QTimer::timeout, this, &AudioEngine::updatePlayheadFromAudio);

    // Track playhead moves done by user scrubbing to dynamically update playback samples
    connect(m_timelineManager, &TimelineManager::currentPlayheadTimeChanged, this, [this]() {
        if (!m_isUpdatingPlayheadFromAudio) {
            qint64 currentMicroseconds = m_timelineManager->currentPlayheadTime();
            m_playbackSample.store((currentMicroseconds * 48000) / 1000000);
        }
    });

    // Automatically cache audio files loaded/added to the timeline tracks
    connect(m_timelineManager->trackListModel(), &TrackListModel::trackAdded, this, [this](Track* track) {
        auto wireClips = [this, track]() {
            for (Clip* clip : track->clips()) {
                if (clip->type() == Clip::Audio && !clip->sourceFile().isEmpty()) {
                    preloadFile(clip->sourceFile());
                }
            }
        };
        connect(track, &Track::clipsChanged, this, wireClips);
        wireClips();
    });
}

AudioEngine::~AudioEngine() {
    // Wait for any active background preloader tasks to finish first to prevent them from calling
    // onFileDecoded on a partially or fully destroyed AudioEngine instance.
    QThreadPool::globalInstance()->waitForDone();

    stop();
    if (m_deviceInitialized) {
        ma_device_uninit(&m_device);
    }
    clearCache();
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

void AudioEngine::setMasterVolume(float vol) {
    if (m_masterVolume != vol) {
        m_masterVolume = vol;
        emit masterVolumeChanged();
    }
}

void AudioEngine::setPlaybackRate(float rate) {
    if (m_playbackRate.load() != rate) {
        m_playbackRate.store(rate);
        emit playbackRateChanged();
    }
}

void AudioEngine::setIsPlaying(bool playing) {
    if (playing) {
        play();
    } else {
        pause();
    }
}

void AudioEngine::play() {
    if (!m_deviceInitialized || m_isPlaying.load()) {
        return;
    }

    // Synchronize playhead starting sample
    qint64 currentMicroseconds = m_timelineManager->currentPlayheadTime();
    m_playbackSample.store((currentMicroseconds * 48000) / 1000000);
    m_playbackSampleAccumulator = static_cast<double>(m_playbackSample.load());

    m_isPlaying.store(true);
    
    // Start hardware playback device
    if (ma_device_start(&m_device) != MA_SUCCESS) {
        std::cerr << "[AudioEngine] Error: Failed to start playback device.\n";
        m_isPlaying.store(false);
        return;
    }

    m_syncTimer->start();
    emit isPlayingChanged();
}

void AudioEngine::pause() {
    if (!m_isPlaying.load()) {
        return;
    }

    m_isPlaying.store(false);
    m_syncTimer->stop();

    // Pause hardware device thread
    if (m_deviceInitialized) {
        ma_device_stop(&m_device);
    }

    emit isPlayingChanged();
}

void AudioEngine::stop() {
    pause();
    m_timelineManager->setCurrentPlayheadTime(0);
    m_playbackSample.store(0);
}

class PreloadRunnable : public QRunnable {
public:
    PreloadRunnable(AudioEngine* engine, const QString& filePath)
        : m_engine(engine), m_filePath(filePath) {
        setAutoDelete(true);
    }

    void run() override {
        AudioReader* reader = new AudioReader();
        bool success = reader->decodeFile(m_filePath, 48000);
        // Safely notify the engine on the main thread
        QMetaObject::invokeMethod(m_engine, "onFileDecoded",
                                  Qt::QueuedConnection,
                                  Q_ARG(QString, m_filePath),
                                  Q_ARG(void*, reader),
                                  Q_ARG(bool, success));
    }
private:
    AudioEngine* m_engine;
    QString m_filePath;
};

void AudioEngine::preloadFile(const QString& filePath) {
    if (filePath.isEmpty()) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_cacheMutex);
    if (m_audioCache.contains(filePath) || m_loadingFiles.contains(filePath)) {
        return;
    }

    // Try to load peak cache synchronously for immediate UI feedback
    AudioReader* cacheReader = new AudioReader();
    if (cacheReader->loadPeakCache(filePath)) {
        m_audioCache[filePath] = cacheReader;
        std::cout << "[AudioEngine] Loaded peak cache synchronously for: " << filePath.toStdString() << "\n";
    } else {
        delete cacheReader;
    }

    m_loadingFiles.insert(filePath);

    PreloadRunnable* task = new PreloadRunnable(this, filePath);
    QThreadPool::globalInstance()->start(task);
}

void AudioEngine::onFileDecoded(const QString& filePath, void* readerPtr, bool success) {
    AudioReader* reader = static_cast<AudioReader*>(readerPtr);

    std::lock_guard<std::mutex> lock(m_cacheMutex);
    m_loadingFiles.remove(filePath);

    if (success && reader) {
        if (m_audioCache.contains(filePath)) {
            AudioReader* cachedReader = m_audioCache[filePath];
            if (cachedReader) {
                // Merge decoded samples into the existing reader loaded from cache
                cachedReader->setSamples(reader->samples());
                delete reader;
                std::cout << "[AudioEngine] Merged background decoded samples for: " << filePath.toStdString() << "\n";
            } else {
                m_audioCache[filePath] = reader;
            }
        } else {
            m_audioCache[filePath] = reader;
            std::cout << "[AudioEngine] Async decode finished successfully for file: " << filePath.toStdString() << "\n";
        }
    } else {
        delete reader;
        std::cerr << "[AudioEngine] Async decode failed for file: " << filePath.toStdString() << "\n";
    }
}

void AudioEngine::clearCache() {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    qDeleteAll(m_audioCache);
    m_audioCache.clear();
}

AudioReader* AudioEngine::getReader(const QString& filePath) const {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    return m_audioCache.value(filePath, nullptr);
}

void AudioEngine::mixAudio(float* pOutput, unsigned int frameCount) {
    // 1. Zero out raw speaker frame buffer
    std::fill(pOutput, pOutput + frameCount * 2, 0.0f);

    if (!m_isPlaying.load()) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_cacheMutex);
    
    qint64 currentSampleInt = m_playbackSample.load();
    if (std::abs(m_playbackSampleAccumulator - currentSampleInt) > 480) { // > 10ms diff
        m_playbackSampleAccumulator = static_cast<double>(currentSampleInt);
    }
    
    double startSample = m_playbackSampleAccumulator;
    float speed = m_playbackRate.load();
    double endSample = startSample + frameCount * speed;

    // Loop through tracks in parallel
    for (Track* track : m_timelineManager->trackListModel()->tracks()) {
        if (track->trackType() != Track::Audio) {
            continue;
        }

        // Determine target volume
        float targetVol = (track->isMuted() ? 0.0f : track->volume()) * m_masterVolume;
        float prevVol = m_prevVolumes.value(track->trackId(), targetVol);
        m_prevVolumes[track->trackId()] = targetVol; // Keep track of current for next buffer pass

        for (Clip* clip : track->clips()) {
            // Translate clip timings to sample references
            qint64 clipStartSample = (clip->startTime() * 48000) / 1000000;
            qint64 clipEndSample = (clip->endTime() * 48000) / 1000000;

            QString srcFile = clip->sourceFile();
            if (!m_audioCache.contains(srcFile)) {
                continue; // Dynamic pre-load fail fallback
            }

            AudioReader* reader = m_audioCache[srcFile];
            const auto& samples = reader->samples();
            if (samples.empty()) {
                continue;
            }

            qint64 srcStartSample = (clip->sourceStart() * 48000) / 1000000;
            unsigned int rampLen = (std::min)(frameCount, 256u);
            float gainStep = (targetVol - prevVol) / static_cast<float>(rampLen);

            for (unsigned int outFrameIdx = 0; outFrameIdx < frameCount; ++outFrameIdx) {
                double s = startSample + outFrameIdx * speed;
                if (s >= clipStartSample && s < clipEndSample) {
                    double sampleOffsetInClip = s - clipStartSample;
                    qint64 srcFrameIdx = srcStartSample + static_cast<qint64>(sampleOffsetInClip);

                    if (srcFrameIdx >= 0 && srcFrameIdx < reader->totalSamples()) {
                        float gain = outFrameIdx < rampLen ? (prevVol + gainStep * outFrameIdx) : targetVol;

                        // Mix left/right channels additively with smoothed gain factor
                        pOutput[outFrameIdx * 2]     += samples[srcFrameIdx * 2] * gain;
                        pOutput[outFrameIdx * 2 + 1] += samples[srcFrameIdx * 2 + 1] * gain;
                    }
                }
            }
        }
    }

    m_playbackSampleAccumulator = endSample;
    m_playbackSample.store(static_cast<qint64>(endSample));
}

void AudioEngine::mixOffline(float* pOutput, unsigned int frameCount, qint64 startSample, bool excludeVocals) {
    std::fill(pOutput, pOutput + frameCount * 2, 0.0f);

    std::lock_guard<std::mutex> lock(m_cacheMutex);
    qint64 endSample = startSample + frameCount;

    for (Track* track : m_timelineManager->trackListModel()->tracks()) {
        if (track->trackType() != Track::Audio) {
            continue;
        }

        if (excludeVocals) {
            QString nameLower = track->name().toLower();
            if (nameLower.contains("vocals") || nameLower.contains("vocal")) {
                continue; // Skip vocal track
            }
        }

        float targetVol = (track->isMuted() ? 0.0f : track->volume()) * m_masterVolume;

        for (Clip* clip : track->clips()) {
            qint64 clipStartSample = (clip->startTime() * 48000) / 1000000;
            qint64 clipEndSample = (clip->endTime() * 48000) / 1000000;

            if (startSample < clipEndSample && clipStartSample < endSample) {
                QString srcFile = clip->sourceFile();
                if (!m_audioCache.contains(srcFile)) {
                    continue;
                }

                AudioReader* reader = m_audioCache[srcFile];
                const auto& samples = reader->samples();
                if (samples.empty()) {
                    continue;
                }

                qint64 mixStart = (std::max)(startSample, clipStartSample);
                qint64 mixEnd = (std::min)(endSample, clipEndSample);

                for (qint64 s = mixStart; s < mixEnd; ++s) {
                    qint64 outFrameIdx = s - startSample;
                    
                    qint64 sampleOffsetInClip = s - clipStartSample;
                    qint64 srcStartSample = (clip->sourceStart() * 48000) / 1000000;
                    qint64 srcFrameIdx = srcStartSample + sampleOffsetInClip;

                    if (srcFrameIdx >= 0 && srcFrameIdx < reader->totalSamples()) {
                        pOutput[outFrameIdx * 2]     += samples[srcFrameIdx * 2] * targetVol;
                        pOutput[outFrameIdx * 2 + 1] += samples[srcFrameIdx * 2 + 1] * targetVol;
                    }
                }
            }
        }
    }
}

void AudioEngine::updatePlayheadFromAudio() {
    if (m_isPlaying.load()) {
        qint64 currentSample = m_playbackSample.load();
        qint64 microseconds = (currentSample * 1000000) / 48000;
        
        // Push microsecond updates to GUI thread smoothly
        m_isUpdatingPlayheadFromAudio = true;
        m_timelineManager->setCurrentPlayheadTime(microseconds);
        m_isUpdatingPlayheadFromAudio = false;

        // If the playhead was changed during the update (e.g., reset by QML looping logic),
        // we must sync the audio engine's sample position to the new playhead time.
        qint64 finalPlayheadTime = m_timelineManager->currentPlayheadTime();
        if (finalPlayheadTime != microseconds) {
            m_playbackSample.store((finalPlayheadTime * 48000) / 1000000);
        }
    }
}

} // namespace ncktv
