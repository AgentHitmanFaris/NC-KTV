#pragma once

#include <QObject>
#include <QMap>
#include <QTimer>
#include <QSet>
#include <atomic>
#include <mutex>
#include "audio_reader.h"
#include "../timeline/timeline_manager.h"
#include <miniaudio.h>

namespace ncktv {

class AudioEngine : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool isPlaying READ isPlaying WRITE setIsPlaying NOTIFY isPlayingChanged)
    Q_PROPERTY(float masterVolume READ masterVolume WRITE setMasterVolume NOTIFY masterVolumeChanged)
    Q_PROPERTY(float playbackRate READ playbackRate WRITE setPlaybackRate NOTIFY playbackRateChanged)

public:
    explicit AudioEngine(TimelineManager* timelineManager, QObject* parent = nullptr);
    virtual ~AudioEngine() override;

    // Singleton access for visual waveform components to query loaded sample buffers
    static AudioEngine* instance();

    [[nodiscard]] bool isPlaying() const { return m_isPlaying; }
    void setIsPlaying(bool playing);

    [[nodiscard]] float masterVolume() const { return m_masterVolume; }
    void setMasterVolume(float vol);

    [[nodiscard]] float playbackRate() const { return m_playbackRate.load(); }
    void setPlaybackRate(float rate);

    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void stop();

    // Cache management
    Q_INVOKABLE void preloadFile(const QString& filePath);
    Q_INVOKABLE void clearCache();

    // Returns loaded reader or nullptr
    Q_INVOKABLE ncktv::AudioReader* getReader(const QString& filePath) const;

    // Callback from background thread
    Q_INVOKABLE void onFileDecoded(const QString& filePath, void* readerPtr, bool success);

    // For unit testing mixing and gain smoothing
    void setCachedReaderForTesting(const QString& filePath, AudioReader* reader) {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        if (m_audioCache.contains(filePath)) {
            delete m_audioCache.take(filePath);
        }
        m_audioCache[filePath] = reader;
    }

    // Real-time mixing callback (thread-safe, lock-free, called by miniaudio device)
    void mixAudio(float* pOutput, unsigned int frameCount);
    void mixOffline(float* pOutput, unsigned int frameCount, qint64 startSample, bool excludeVocals = false);

signals:
    void isPlayingChanged();
    void masterVolumeChanged();
    void playbackRateChanged();

private slots:
    void updatePlayheadFromAudio();

private:
    static AudioEngine* s_instance;

    TimelineManager* m_timelineManager = nullptr;
    
    // miniaudio device struct
    ma_device m_device;
    bool m_deviceInitialized = false;

    // Playback control states
    std::atomic<bool> m_isPlaying{false};
    std::atomic<qint64> m_playbackSample{0}; // Playhead position in samples
    
    QTimer* m_syncTimer = nullptr;
    
    // Decoded audio cache and thread-safety
    mutable std::mutex m_cacheMutex;
    QMap<QString, AudioReader*> m_audioCache;
    QSet<QString> m_loadingFiles;

    // Tracks volume state map for smoothing
    QMap<QString, float> m_prevVolumes;

    float m_masterVolume = 1.0f;
    bool m_isUpdatingPlayheadFromAudio = false;
    
    std::atomic<float> m_playbackRate{1.0f};
    double m_playbackSampleAccumulator = 0.0;
};

} // namespace ncktv
