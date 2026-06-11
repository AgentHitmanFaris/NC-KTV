#include "ncktv_bridge.h"
#include <QApplication>
#include <QThread>
#include <QMetaObject>
#include <QDir>
#include <QFileInfo>
#ifdef _WIN32
#include <windows.h>
#endif
#include <thread>
#include <atomic>
#include <mutex>
#include <iostream>
#include <vector>
#include <string>
#include "../core/timeline/timeline_manager.h"
#include "../core/audio/audio_engine.h"
#include "../core/youtube/youtube_manager.h"
#include <nlohmann/json.hpp>

// Globals
static std::unique_ptr<std::thread> g_qtThread;
static std::atomic<bool> g_qtThreadStarted{false};
static ncktv::TimelineManager* g_timelineManager = nullptr;
static ncktv::AudioEngine* g_audioEngine = nullptr;
static ncktv::YoutubeManager* g_youtubeManager = nullptr;
static QApplication* g_qtApp = nullptr;

// Callbacks
static PlayheadChangedCallback g_playheadCb = nullptr;
static RenderProgressCallback g_renderProgressCb = nullptr;
static SeparationProgressCallback g_separationProgressCb = nullptr;
static SeparationCompletedCallback g_separationCompletedCb = nullptr;
static SeparationFailedCallback g_separationFailedCb = nullptr;

static YoutubeSearchCompletedCallback g_ytSearchCompletedCb = nullptr;
static YoutubeDownloadProgressCallback g_ytDownloadProgressCb = nullptr;
static YoutubeDownloadCompletedCallback g_ytDownloadCompletedCb = nullptr;
static YoutubeDownloadFailedCallback g_ytDownloadFailedCb = nullptr;

// Thread-safe strings for System Info cache
static std::string g_cpuInfo;
static std::string g_gpuInfo;
static std::string g_ramInfo;
static std::string g_osInfo;
static std::string g_onnxInfo;

// Helper function to invoke a method on the Qt thread blocking until it returns
template<typename Func>
void runOnQtThread(Func&& func) {
    if (g_timelineManager && QThread::currentThread() != g_timelineManager->thread()) {
        // Simple helper using QMetaObject::invokeMethod
        struct Event : public QEvent {
            Func f;
            Event(Func&& f) : QEvent(QEvent::None), f(std::forward<Func>(f)) {}
            ~Event() override = default;
        };
        // Post a custom event or use direct lambda invoke
        QMetaObject::invokeMethod(g_timelineManager, std::forward<Func>(func), Qt::BlockingQueuedConnection);
    } else {
        func();
    }
}

// Background thread entry point
static void qtThreadFunc(const std::string& modelsDir) {
    int argc = 1;
    char dummy_arg[] = "ncktv_bridge";
    char* argv[] = { dummy_arg, nullptr };

#ifdef _WIN32
    wchar_t path[MAX_PATH];
    HMODULE hm = GetModuleHandleW(L"ncktv_bridge.dll");
    if (hm) {
        GetModuleFileNameW(hm, path, MAX_PATH);
        QString dllPath = QString::fromWCharArray(path);
        QFileInfo info(dllPath);
        QString dllDir = info.absolutePath();
        
        // Add DLL directory to the Windows DLL search path
        SetDllDirectoryW(dllDir.toStdWString().c_str());
        
        QCoreApplication::addLibraryPath(dllDir);
    } else {
        QCoreApplication::addLibraryPath(QCoreApplication::applicationDirPath());
    }
#else
    QCoreApplication::addLibraryPath(QCoreApplication::applicationDirPath());
#endif
    g_qtApp = new QApplication(argc, argv);
    g_qtApp->setQuitOnLastWindowClosed(false);

    g_timelineManager = new ncktv::TimelineManager();
    g_audioEngine = new ncktv::AudioEngine(g_timelineManager);
    g_youtubeManager = new ncktv::YoutubeManager();

    // Setup initial default tracks with static IDs
    g_timelineManager->addTrack(0, "Background Instrumental", "Track_Instrumental");
    g_timelineManager->addTrack(0, "Guide Vocals Track", "Track_Vocals");
    g_timelineManager->addTrack(2, "Karaoke Subtitles", "Track_Lyric");

    if (!modelsDir.empty()) {
        g_timelineManager->setModelsDirPath(QString::fromStdString(modelsDir));
        g_timelineManager->scanModelsDir();
    }

    // Connect signals to callbacks
    QObject::connect(g_timelineManager, &ncktv::TimelineManager::currentPlayheadTimeChanged, []() {
        if (g_playheadCb && g_timelineManager) {
            g_playheadCb(g_timelineManager->currentPlayheadTime());
        }
    });

    QObject::connect(g_timelineManager, &ncktv::TimelineManager::renderProgressChanged, []() {
        if (g_renderProgressCb && g_timelineManager) {
            g_renderProgressCb(g_timelineManager->renderProgress(), g_timelineManager->renderStatusText().toUtf8().constData());
        }
    });

    QObject::connect(g_timelineManager, &ncktv::TimelineManager::separationProgressChanged, []() {
        if (g_separationProgressCb && g_timelineManager) {
            g_separationProgressCb(g_timelineManager->separationProgress(), g_timelineManager->separationStatusText().toUtf8().constData());
        }
    });

    QObject::connect(g_timelineManager, &ncktv::TimelineManager::mediaSeparationCompleted, [](const QString& vocals, const QString& inst) {
        if (g_separationCompletedCb) {
            g_separationCompletedCb(vocals.toUtf8().constData(), inst.toUtf8().constData());
        }
    });

    QObject::connect(g_timelineManager, &ncktv::TimelineManager::mediaSeparationFailed, [](const QString& errorMessage) {
        if (g_separationFailedCb) {
            g_separationFailedCb(errorMessage.toUtf8().constData());
        }
    });

    // YouTube connections
    QObject::connect(g_youtubeManager, &ncktv::YoutubeManager::searchCompleted, g_youtubeManager, [](const QVariantList& results) {
        if (g_ytSearchCompletedCb) {
            nlohmann::json jsonList = nlohmann::json::array();
            for (const QVariant& var : results) {
                QVariantMap map = var.toMap();
                nlohmann::json jsonItem;
                jsonItem["id"] = map["id"].toString().toStdString();
                jsonItem["title"] = map["title"].toString().toStdString();
                jsonItem["channel"] = map["channel"].toString().toStdString();
                jsonItem["durationStr"] = map["durationStr"].toString().toStdString();
                jsonItem["thumbnail"] = map["thumbnail"].toString().toStdString();
                jsonList.push_back(jsonItem);
            }
            g_ytSearchCompletedCb(jsonList.dump().c_str());
        }
    });

    QObject::connect(g_youtubeManager, &ncktv::YoutubeManager::moreResultsLoaded, g_youtubeManager, [](const QVariantList& results) {
        if (g_ytSearchCompletedCb) {
            nlohmann::json jsonList = nlohmann::json::array();
            for (const QVariant& var : results) {
                QVariantMap map = var.toMap();
                nlohmann::json jsonItem;
                jsonItem["id"] = map["id"].toString().toStdString();
                jsonItem["title"] = map["title"].toString().toStdString();
                jsonItem["channel"] = map["channel"].toString().toStdString();
                jsonItem["durationStr"] = map["durationStr"].toString().toStdString();
                jsonItem["thumbnail"] = map["thumbnail"].toString().toStdString();
                jsonList.push_back(jsonItem);
            }
            g_ytSearchCompletedCb(jsonList.dump().c_str());
        }
    });

    QObject::connect(g_youtubeManager, &ncktv::YoutubeManager::searchFailed, g_youtubeManager, [](const QString& error) {
        if (g_ytSearchCompletedCb) {
            g_ytSearchCompletedCb("[]"); // return empty array on failure
        }
    });

    QObject::connect(g_youtubeManager, &ncktv::YoutubeManager::downloadProgress, g_youtubeManager, [](const QString& videoId, double progress) {
        if (g_ytDownloadProgressCb) {
            g_ytDownloadProgressCb(videoId.toUtf8().constData(), progress);
        }
    });

    QObject::connect(g_youtubeManager, &ncktv::YoutubeManager::downloadCompleted, g_youtubeManager, [](const QString& videoId, const QString& filePath, bool audioOnly) {
        if (g_ytDownloadCompletedCb) {
            g_ytDownloadCompletedCb(videoId.toUtf8().constData(), filePath.toUtf8().constData(), audioOnly);
        }
    });

    QObject::connect(g_youtubeManager, &ncktv::YoutubeManager::downloadFailed, g_youtubeManager, [](const QString& videoId, const QString& errorMessage) {
        if (g_ytDownloadFailedCb) {
            g_ytDownloadFailedCb(videoId.toUtf8().constData(), errorMessage.toUtf8().constData());
        }
    });

    // Cache system information (safe to read from any thread since these don't change)
    g_cpuInfo = g_timelineManager->cpuInfo().toStdString();
    g_gpuInfo = g_timelineManager->gpuInfo().toStdString();
    g_ramInfo = g_timelineManager->ramInfo().toStdString();
    g_osInfo = g_timelineManager->osInfo().toStdString();
    g_onnxInfo = g_timelineManager->onnxProviderInfo().toStdString();

    g_qtThreadStarted = true;

    std::cout << "[BRIDGE] Qt Event Loop thread started successfully." << std::endl;

    g_qtApp->exec();

    // Cleanup
    delete g_audioEngine;
    delete g_timelineManager;
    delete g_youtubeManager;
    delete g_qtApp;
    g_audioEngine = nullptr;
    g_timelineManager = nullptr;
    g_youtubeManager = nullptr;
    g_qtApp = nullptr;
    g_qtThreadStarted = false;
    std::cout << "[BRIDGE] Qt Event Loop thread exited." << std::endl;
}

// DLL Export API Implementations

void ncktv_initialize(const char* modelsDir) {
    if (g_qtThreadStarted) return;
    std::string dir = modelsDir ? modelsDir : "";
    g_qtThread = std::make_unique<std::thread>(qtThreadFunc, dir);
    
    // Wait until initialized
    while (!g_qtThreadStarted) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void ncktv_shutdown() {
    if (!g_qtThreadStarted) return;
    
    if (g_qtApp) {
        QMetaObject::invokeMethod(g_qtApp, "quit", Qt::QueuedConnection);
    }
    
    if (g_qtThread && g_qtThread->joinable()) {
        g_qtThread->join();
    }
    g_qtThread.reset();
}

void ncktv_process_events() {
    if (g_qtApp) {
        QMetaObject::invokeMethod(g_qtApp, "processEvents", Qt::QueuedConnection);
    }
}

const char* ncktv_get_cpu_info() { return g_cpuInfo.c_str(); }
const char* ncktv_get_gpu_info() { return g_gpuInfo.c_str(); }
const char* ncktv_get_ram_info() { return g_ramInfo.c_str(); }
const char* ncktv_get_os_info() { return g_osInfo.c_str(); }
const char* ncktv_get_onnx_provider_info() { return g_onnxInfo.c_str(); }

void ncktv_set_playhead_changed_callback(PlayheadChangedCallback cb) { g_playheadCb = cb; }
void ncktv_set_render_progress_callback(RenderProgressCallback cb) { g_renderProgressCb = cb; }
void ncktv_set_separation_progress_callback(SeparationProgressCallback cb) { g_separationProgressCb = cb; }
void ncktv_set_separation_completed_callback(SeparationCompletedCallback cb) { g_separationCompletedCb = cb; }
void ncktv_set_separation_failed_callback(SeparationFailedCallback cb) { g_separationFailedCb = cb; }

void ncktv_play() {
    if (g_audioEngine) {
        QMetaObject::invokeMethod(g_audioEngine, "play", Qt::BlockingQueuedConnection);
    }
}

void ncktv_pause() {
    if (g_audioEngine) {
        QMetaObject::invokeMethod(g_audioEngine, "pause", Qt::BlockingQueuedConnection);
    }
}

void ncktv_stop() {
    if (g_audioEngine) {
        QMetaObject::invokeMethod(g_audioEngine, "stop", Qt::BlockingQueuedConnection);
    }
}

void ncktv_set_playhead(int64_t timeUs) {
    if (g_timelineManager) {
        QMetaObject::invokeMethod(g_timelineManager, "setCurrentPlayheadTime", Qt::BlockingQueuedConnection, Q_ARG(qint64, timeUs));
    }
}

int64_t ncktv_get_playhead() {
    if (!g_timelineManager) return 0;
    qint64 val = 0;
    runOnQtThread([&]() {
        val = g_timelineManager->currentPlayheadTime();
    });
    return val;
}

bool ncktv_is_playing() {
    if (!g_audioEngine) return false;
    bool val = false;
    runOnQtThread([&]() {
        val = g_audioEngine->isPlaying();
    });
    return val;
}

float ncktv_get_master_volume() {
    if (!g_audioEngine) return 0.0f;
    float val = 0.0f;
    runOnQtThread([&]() {
        val = g_audioEngine->masterVolume();
    });
    return val;
}

void ncktv_set_master_volume(float volume) {
    if (g_audioEngine) {
        QMetaObject::invokeMethod(g_audioEngine, "setMasterVolume", Qt::BlockingQueuedConnection, Q_ARG(float, volume));
    }
}

bool ncktv_load_project(const char* filePath) {
    if (!g_timelineManager) return false;
    bool success = false;
    runOnQtThread([&]() {
        success = g_timelineManager->loadProject(QString::fromUtf8(filePath));
    });
    return success;
}

bool ncktv_save_project(const char* filePath) {
    if (!g_timelineManager) return false;
    bool success = false;
    runOnQtThread([&]() {
        success = g_timelineManager->saveProject(QString::fromUtf8(filePath));
    });
    return success;
}

void ncktv_clear_project() {
    if (g_timelineManager) {
        QMetaObject::invokeMethod(g_timelineManager, "clearProject", Qt::BlockingQueuedConnection);
    }
}

bool ncktv_is_dirty() {
    if (!g_timelineManager) return false;
    bool val = false;
    runOnQtThread([&]() {
        val = g_timelineManager->isDirty();
    });
    return val;
}

void ncktv_set_dirty(bool dirty) {
    if (g_timelineManager) {
        QMetaObject::invokeMethod(g_timelineManager, "setDirty", Qt::BlockingQueuedConnection, Q_ARG(bool, dirty));
    }
}

bool ncktv_add_track(int type, const char* name, char* outTrackId, int maxLen) {
    if (!g_timelineManager) return false;
    QString trackId;
    runOnQtThread([&]() {
        trackId = g_timelineManager->addTrack(type, QString::fromUtf8(name));
    });
    if (trackId.isEmpty()) return false;
    strncpy_s(outTrackId, maxLen, trackId.toUtf8().constData(), _TRUNCATE);
    return true;
}

bool ncktv_remove_track(const char* trackId) {
    if (!g_timelineManager) return false;
    bool success = false;
    runOnQtThread([&]() {
        success = g_timelineManager->removeTrack(QString::fromUtf8(trackId));
    });
    return success;
}

int ncktv_get_track_count() {
    if (!g_timelineManager || !g_timelineManager->trackListModel()) return 0;
    int count = 0;
    runOnQtThread([&]() {
        count = g_timelineManager->trackListModel()->rowCount();
    });
    return count;
}

void ncktv_clear_track_clips(const char* trackId) {
    if (!g_timelineManager || !trackId) return;
    runOnQtThread([&]() {
        ncktv::Track* track = g_timelineManager->trackListModel()->getTrackById(QString::fromUtf8(trackId));
        if (track) {
            track->clearClips();
        }
    });
}

bool ncktv_add_clip(const char* trackId, const char* clipId, int type, int64_t startTimeUs, int64_t durationUs, const char* sourceFile, const char* lyricText) {
    if (!g_timelineManager) return false;
    bool success = false;
    runOnQtThread([&]() {
        success = g_timelineManager->addClipToTrack(
            QString::fromUtf8(trackId),
            QString::fromUtf8(clipId),
            type,
            startTimeUs,
            durationUs,
            QString::fromUtf8(sourceFile),
            QString::fromUtf8(lyricText)
        );
    });
    return success;
}

bool ncktv_add_clip_with_source_start(const char* trackId, const char* clipId, int type, int64_t startTimeUs, int64_t durationUs, int64_t sourceStartUs, const char* sourceFile, const char* lyricText) {
    if (!g_timelineManager) return false;
    bool success = false;
    runOnQtThread([&]() {
        success = g_timelineManager->addClipToTrackWithSourceStart(
            QString::fromUtf8(trackId),
            QString::fromUtf8(clipId),
            type,
            startTimeUs,
            durationUs,
            sourceStartUs,
            QString::fromUtf8(sourceFile),
            QString::fromUtf8(lyricText)
        );
    });
    return success;
}

bool ncktv_split_clip(const char* trackId, const char* clipId, int64_t splitTimeUs) {
    if (!g_timelineManager) return false;
    bool success = false;
    runOnQtThread([&]() {
        success = g_timelineManager->splitClip(QString::fromUtf8(trackId), QString::fromUtf8(clipId), splitTimeUs);
    });
    return success;
}

bool ncktv_import_lyrics_from_file(const char* trackId, const char* filePath) {
    if (!g_timelineManager) return false;
    bool success = false;
    runOnQtThread([&]() {
        success = g_timelineManager->importLyricsFromFile(QString::fromUtf8(trackId), QString::fromUtf8(filePath));
    });
    return success;
}

bool ncktv_import_lyrics_from_string(const char* trackId, const char* rawLrcContent) {
    if (!g_timelineManager) return false;
    bool success = false;
    runOnQtThread([&]() {
        success = g_timelineManager->importLyricsFromString(QString::fromUtf8(trackId), QString::fromUtf8(rawLrcContent));
    });
    return success;
}

int ncktv_get_waveform_peaks(const char* filePath, int64_t startUs, int64_t durationUs, int maxPoints, float* outMinPeaks, float* outMaxPeaks) {
    if (!g_audioEngine || !filePath || maxPoints <= 0) return 0;
    
    int pointsWritten = 0;
    runOnQtThread([&]() {
        ncktv::AudioReader* reader = g_audioEngine->getReader(QString::fromUtf8(filePath));
        if (!reader) {
            // Preload to force decodes in background
            g_audioEngine->preloadFile(QString::fromUtf8(filePath));
            return;
        }
        
        // Decide which peak level to use (peaks4096 or peaks256)
        double totalFrames = (durationUs / 1000000.0) * 48000.0;
        double framesPerPixel = totalFrames / maxPoints;
        const auto& peaks = (framesPerPixel >= 2048.0) ? reader->peaks4096() : reader->peaks256();
        int64_t N = (framesPerPixel >= 2048.0) ? 4096 : 256;
        
        if (peaks.empty()) return;
        
        for (int i = 0; i < maxPoints; ++i) {
            double progress = static_cast<double>(i) / maxPoints;
            int64_t sampleTimeUs = startUs + static_cast<int64_t>(progress * durationUs);
            int64_t peakIndex = (sampleTimeUs * 48000) / (N * 1000000);
            
            if (peakIndex >= 0 && peakIndex < static_cast<int64_t>(peaks.size())) {
                outMinPeaks[i] = peaks[peakIndex].minVal;
                outMaxPeaks[i] = peaks[peakIndex].maxVal;
                pointsWritten++;
            } else {
                outMinPeaks[i] = 0.0f;
                outMaxPeaks[i] = 0.0f;
            }
        }
    });
    
    return pointsWritten;
}

void ncktv_separate_stems(const char* clipId) {
    if (g_timelineManager) {
        QMetaObject::invokeMethod(g_timelineManager, "separateStems", Qt::BlockingQueuedConnection, Q_ARG(QString, QString::fromUtf8(clipId)));
    }
}

void ncktv_separate_stems_for_file(const char* filePath) {
    if (g_timelineManager) {
        QMetaObject::invokeMethod(g_timelineManager, "separateStemsForFile", Qt::BlockingQueuedConnection, Q_ARG(QString, QString::fromUtf8(filePath)));
    }
}

void ncktv_start_export(const char* outputPath, int width, int height, int fps, int videoBitrate, int audioBitrate, const char* audioCodec) {
    if (g_timelineManager) {
        QMetaObject::invokeMethod(g_timelineManager, "startExport", Qt::BlockingQueuedConnection,
                                  Q_ARG(QString, QString::fromUtf8(outputPath)),
                                  Q_ARG(int, width),
                                  Q_ARG(int, height),
                                  Q_ARG(int, fps),
                                  Q_ARG(int, videoBitrate),
                                  Q_ARG(int, audioBitrate),
                                  Q_ARG(QString, QString::fromUtf8(audioCodec)));
    }
}

void ncktv_cancel_export() {
    if (g_timelineManager) {
        QMetaObject::invokeMethod(g_timelineManager, "cancelExport", Qt::BlockingQueuedConnection);
    }
}

void ncktv_youtube_set_search_completed_callback(YoutubeSearchCompletedCallback cb) { g_ytSearchCompletedCb = cb; }
void ncktv_youtube_set_download_progress_callback(YoutubeDownloadProgressCallback cb) { g_ytDownloadProgressCb = cb; }
void ncktv_youtube_set_download_completed_callback(YoutubeDownloadCompletedCallback cb) { g_ytDownloadCompletedCb = cb; }
void ncktv_youtube_set_download_failed_callback(YoutubeDownloadFailedCallback cb) { g_ytDownloadFailedCb = cb; }

void ncktv_youtube_search(const char* query) {
    if (g_youtubeManager) {
        QMetaObject::invokeMethod(g_youtubeManager, "search", Qt::BlockingQueuedConnection, Q_ARG(QString, QString::fromUtf8(query)));
    }
}

void ncktv_youtube_search_more() {
    if (g_youtubeManager) {
        QMetaObject::invokeMethod(g_youtubeManager, "searchMore", Qt::BlockingQueuedConnection);
    }
}

void ncktv_youtube_download(const char* videoId, bool audioOnly, const char* saveDir, const char* title) {
    if (g_youtubeManager) {
        QMetaObject::invokeMethod(g_youtubeManager, "download", Qt::BlockingQueuedConnection,
                                  Q_ARG(QString, QString::fromUtf8(videoId)),
                                  Q_ARG(bool, audioOnly),
                                  Q_ARG(QString, QString::fromUtf8(saveDir)),
                                  Q_ARG(QString, QString::fromUtf8(title)));
    }
}

void ncktv_youtube_cancel_download(const char* videoId) {
    if (g_youtubeManager) {
        QMetaObject::invokeMethod(g_youtubeManager, "cancelDownload", Qt::BlockingQueuedConnection, Q_ARG(QString, QString::fromUtf8(videoId)));
    }
}

bool ncktv_youtube_is_searching() {
    if (!g_youtubeManager) return false;
    bool val = false;
    runOnQtThread([&]() {
        val = g_youtubeManager->isSearching();
    });
    return val;
}
int64_t ncktv_get_total_duration() {
    if (!g_timelineManager) return 0;
    qint64 val = 0;
    runOnQtThread([&]() {
        val = g_timelineManager->totalDuration();
    });
    return val;
}

void ncktv_load_soundtrack(const char* filePath) {
    if (!g_timelineManager || !g_audioEngine || !filePath) return;
    
    QString path = QString::fromUtf8(filePath);
    
    // Clear existing clips from default audio tracks
    runOnQtThread([&]() {
        ncktv::Track* trackVoc = g_timelineManager->trackListModel()->getTrackById("Track_Vocals");
        if (trackVoc) trackVoc->clearClips();
        ncktv::Track* trackInst = g_timelineManager->trackListModel()->getTrackById("Track_Instrumental");
        if (trackInst) trackInst->clearClips();
        
        // Force preloading/decoding the file in the audio engine so we get its duration
        g_audioEngine->preloadFile(path);
    });

    // Wait briefly for decoding to finish (usually very fast for small files, or we can query it)
    int retries = 0;
    ncktv::AudioReader* reader = nullptr;
    while (retries < 150) { // Max 1.5 seconds wait
        reader = g_audioEngine->getReader(path);
        if (reader && reader->totalSamples() > 0) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        retries++;
    }
    
    runOnQtThread([&]() {
        qint64 durationUs = 180 * 1000000; // Default fallback to 3 mins
        if (reader && reader->totalSamples() > 0) {
            durationUs = (static_cast<qint64>(reader->totalSamples()) * 1000000) / 48000;
        }
        
        // Add clip to the Guide Vocals track
        g_timelineManager->addClipToTrack("Track_Vocals", "MainSoundtrackClip", 1, 0, durationUs, path, "");
    });
}
