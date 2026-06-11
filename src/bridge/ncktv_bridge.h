#pragma once

#ifdef NCKTV_BRIDGE_EXPORTS
#define NCKTV_BRIDGE_API __declspec(dllexport)
#else
#define NCKTV_BRIDGE_API __declspec(dllimport)
#endif

#include <stdint.h>

extern "C" {

// Callback types for event propagation
typedef void (*PlayheadChangedCallback)(int64_t timeUs);
typedef void (*RenderProgressCallback)(double fraction, const char* statusText);
typedef void (*SeparationProgressCallback)(double fraction, const char* statusText);
typedef void (*SeparationCompletedCallback)(const char* vocalsPath, const char* instPath);
typedef void (*SeparationFailedCallback)(const char* errorMessage);

// Initialization & Lifetime
NCKTV_BRIDGE_API void ncktv_initialize(const char* modelsDir);
NCKTV_BRIDGE_API void ncktv_shutdown();
NCKTV_BRIDGE_API void ncktv_process_events();

// System Info
NCKTV_BRIDGE_API const char* ncktv_get_cpu_info();
NCKTV_BRIDGE_API const char* ncktv_get_gpu_info();
NCKTV_BRIDGE_API const char* ncktv_get_ram_info();
NCKTV_BRIDGE_API const char* ncktv_get_os_info();
NCKTV_BRIDGE_API const char* ncktv_get_onnx_provider_info();

// Callback Registration
NCKTV_BRIDGE_API void ncktv_set_playhead_changed_callback(PlayheadChangedCallback cb);
NCKTV_BRIDGE_API void ncktv_set_render_progress_callback(RenderProgressCallback cb);
NCKTV_BRIDGE_API void ncktv_set_separation_progress_callback(SeparationProgressCallback cb);
NCKTV_BRIDGE_API void ncktv_set_separation_completed_callback(SeparationCompletedCallback cb);
NCKTV_BRIDGE_API void ncktv_set_separation_failed_callback(SeparationFailedCallback cb);

// Playback Control
NCKTV_BRIDGE_API void ncktv_play();
NCKTV_BRIDGE_API void ncktv_pause();
NCKTV_BRIDGE_API void ncktv_stop();
NCKTV_BRIDGE_API void ncktv_set_playhead(int64_t timeUs);
NCKTV_BRIDGE_API int64_t ncktv_get_playhead();
NCKTV_BRIDGE_API bool ncktv_is_playing();
NCKTV_BRIDGE_API float ncktv_get_master_volume();
NCKTV_BRIDGE_API void ncktv_set_master_volume(float volume);

// Project Operations
NCKTV_BRIDGE_API bool ncktv_load_project(const char* filePath);
NCKTV_BRIDGE_API bool ncktv_save_project(const char* filePath);
NCKTV_BRIDGE_API void ncktv_clear_project();
NCKTV_BRIDGE_API bool ncktv_is_dirty();
NCKTV_BRIDGE_API void ncktv_set_dirty(bool dirty);

// Track Management
NCKTV_BRIDGE_API bool ncktv_add_track(int type, const char* name, char* outTrackId, int maxLen);
NCKTV_BRIDGE_API bool ncktv_remove_track(const char* trackId);
NCKTV_BRIDGE_API int ncktv_get_track_count();
NCKTV_BRIDGE_API void ncktv_clear_track_clips(const char* trackId);

// Clip Management
NCKTV_BRIDGE_API bool ncktv_add_clip(const char* trackId, const char* clipId, int type, int64_t startTimeUs, int64_t durationUs, const char* sourceFile, const char* lyricText);
NCKTV_BRIDGE_API bool ncktv_add_clip_with_source_start(const char* trackId, const char* clipId, int type, int64_t startTimeUs, int64_t durationUs, int64_t sourceStartUs, const char* sourceFile, const char* lyricText);
NCKTV_BRIDGE_API bool ncktv_split_clip(const char* trackId, const char* clipId, int64_t splitTimeUs);

// Subtitles & Lyrics
NCKTV_BRIDGE_API bool ncktv_import_lyrics_from_file(const char* trackId, const char* filePath);
NCKTV_BRIDGE_API bool ncktv_import_lyrics_from_string(const char* trackId, const char* rawLrcContent);

// Waveform Processing
NCKTV_BRIDGE_API int ncktv_get_waveform_peaks(const char* filePath, int64_t startUs, int64_t durationUs, int maxPoints, float* outMinPeaks, float* outMaxPeaks);

// AI & Render Actions
NCKTV_BRIDGE_API void ncktv_separate_stems(const char* clipId);
NCKTV_BRIDGE_API void ncktv_separate_stems_for_file(const char* filePath);
NCKTV_BRIDGE_API void ncktv_start_export(const char* outputPath, int width, int height, int fps, int videoBitrate, int audioBitrate, const char* audioCodec);
// YouTube Callback types
typedef void (*YoutubeSearchCompletedCallback)(const char* resultsJson);
typedef void (*YoutubeDownloadProgressCallback)(const char* videoId, double progress);
typedef void (*YoutubeDownloadCompletedCallback)(const char* videoId, const char* filePath, bool audioOnly);
typedef void (*YoutubeDownloadFailedCallback)(const char* videoId, const char* errorMessage);

// YouTube Callback Setters
NCKTV_BRIDGE_API void ncktv_youtube_set_search_completed_callback(YoutubeSearchCompletedCallback cb);
NCKTV_BRIDGE_API void ncktv_youtube_set_download_progress_callback(YoutubeDownloadProgressCallback cb);
NCKTV_BRIDGE_API void ncktv_youtube_set_download_completed_callback(YoutubeDownloadCompletedCallback cb);
NCKTV_BRIDGE_API void ncktv_youtube_set_download_failed_callback(YoutubeDownloadFailedCallback cb);

// YouTube API
NCKTV_BRIDGE_API void ncktv_youtube_search(const char* query);
NCKTV_BRIDGE_API void ncktv_youtube_search_more();
NCKTV_BRIDGE_API void ncktv_youtube_download(const char* videoId, bool audioOnly, const char* saveDir, const char* title);
NCKTV_BRIDGE_API void ncktv_youtube_cancel_download(const char* videoId);
NCKTV_BRIDGE_API bool ncktv_youtube_is_searching();
NCKTV_BRIDGE_API void ncktv_load_soundtrack(const char* filePath);
NCKTV_BRIDGE_API int64_t ncktv_get_total_duration();

}
