using System;
using System.Runtime.InteropServices;
using System.Text;

namespace NCKTV.Wpf
{
    public static class NativeBridge
    {
        private const string DllName = "ncktv_bridge.dll";

        // Delegate types for callbacks
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void PlayheadChangedCallback(long timeUs);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void RenderProgressCallback(double fraction, [MarshalAs(UnmanagedType.LPUTF8Str)] string statusText);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void SeparationProgressCallback(double fraction, [MarshalAs(UnmanagedType.LPUTF8Str)] string statusText);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void SeparationCompletedCallback([MarshalAs(UnmanagedType.LPUTF8Str)] string vocalsPath, [MarshalAs(UnmanagedType.LPUTF8Str)] string instPath);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void SeparationFailedCallback([MarshalAs(UnmanagedType.LPUTF8Str)] string errorMessage);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void YoutubeSearchCompletedCallback([MarshalAs(UnmanagedType.LPUTF8Str)] string resultsJson);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void YoutubeDownloadProgressCallback([MarshalAs(UnmanagedType.LPUTF8Str)] string videoId, double progress);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void YoutubeDownloadCompletedCallback([MarshalAs(UnmanagedType.LPUTF8Str)] string videoId, [MarshalAs(UnmanagedType.LPUTF8Str)] string filePath, bool audioOnly);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void YoutubeDownloadFailedCallback([MarshalAs(UnmanagedType.LPUTF8Str)] string videoId, [MarshalAs(UnmanagedType.LPUTF8Str)] string errorMessage);

        // Native function declarations
        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void ncktv_initialize([MarshalAs(UnmanagedType.LPUTF8Str)] string modelsDir);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void ncktv_load_soundtrack([MarshalAs(UnmanagedType.LPUTF8Str)] string filePath);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void ncktv_shutdown();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void ncktv_process_events();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr ncktv_get_cpu_info();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr ncktv_get_gpu_info();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr ncktv_get_ram_info();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr ncktv_get_os_info();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr ncktv_get_onnx_provider_info();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void ncktv_set_playhead_changed_callback(PlayheadChangedCallback cb);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void ncktv_set_render_progress_callback(RenderProgressCallback cb);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void ncktv_set_separation_progress_callback(SeparationProgressCallback cb);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void ncktv_set_separation_completed_callback(SeparationCompletedCallback cb);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void ncktv_set_separation_failed_callback(SeparationFailedCallback cb);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void ncktv_youtube_set_search_completed_callback(YoutubeSearchCompletedCallback cb);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void ncktv_youtube_set_download_progress_callback(YoutubeDownloadProgressCallback cb);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void ncktv_youtube_set_download_completed_callback(YoutubeDownloadCompletedCallback cb);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void ncktv_youtube_set_download_failed_callback(YoutubeDownloadFailedCallback cb);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void ncktv_play();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void ncktv_pause();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void ncktv_stop();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void ncktv_set_playhead(long timeUs);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern long ncktv_get_playhead();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern bool ncktv_is_playing();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern long ncktv_get_total_duration();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern float ncktv_get_master_volume();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void ncktv_set_master_volume(float volume);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern bool ncktv_load_project([MarshalAs(UnmanagedType.LPUTF8Str)] string filePath);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern bool ncktv_save_project([MarshalAs(UnmanagedType.LPUTF8Str)] string filePath);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void ncktv_clear_project();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern bool ncktv_is_dirty();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void ncktv_set_dirty(bool dirty);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern bool ncktv_add_track(int type, [MarshalAs(UnmanagedType.LPUTF8Str)] string name, StringBuilder outTrackId, int maxLen);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern bool ncktv_remove_track([MarshalAs(UnmanagedType.LPUTF8Str)] string trackId);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int ncktv_get_track_count();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void ncktv_clear_track_clips([MarshalAs(UnmanagedType.LPUTF8Str)] string trackId);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern bool ncktv_add_clip([MarshalAs(UnmanagedType.LPUTF8Str)] string trackId, [MarshalAs(UnmanagedType.LPUTF8Str)] string clipId, int type, long startTimeUs, long durationUs, [MarshalAs(UnmanagedType.LPUTF8Str)] string sourceFile, [MarshalAs(UnmanagedType.LPUTF8Str)] string lyricText);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern bool ncktv_add_clip_with_source_start([MarshalAs(UnmanagedType.LPUTF8Str)] string trackId, [MarshalAs(UnmanagedType.LPUTF8Str)] string clipId, int type, long startTimeUs, long durationUs, long sourceStartUs, [MarshalAs(UnmanagedType.LPUTF8Str)] string sourceFile, [MarshalAs(UnmanagedType.LPUTF8Str)] string lyricText);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern bool ncktv_split_clip([MarshalAs(UnmanagedType.LPUTF8Str)] string trackId, [MarshalAs(UnmanagedType.LPUTF8Str)] string clipId, long splitTimeUs);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern bool ncktv_import_lyrics_from_file([MarshalAs(UnmanagedType.LPUTF8Str)] string trackId, [MarshalAs(UnmanagedType.LPUTF8Str)] string filePath);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern bool ncktv_import_lyrics_from_string([MarshalAs(UnmanagedType.LPUTF8Str)] string trackId, [MarshalAs(UnmanagedType.LPUTF8Str)] string rawLrcContent);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int ncktv_get_waveform_peaks([MarshalAs(UnmanagedType.LPUTF8Str)] string filePath, long startUs, long durationUs, int maxPoints, float[] outMinPeaks, float[] outMaxPeaks);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void ncktv_separate_stems([MarshalAs(UnmanagedType.LPUTF8Str)] string clipId);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void ncktv_separate_stems_for_file([MarshalAs(UnmanagedType.LPUTF8Str)] string filePath);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void ncktv_start_export([MarshalAs(UnmanagedType.LPUTF8Str)] string outputPath, int width, int height, int fps, int videoBitrate, int audioBitrate, [MarshalAs(UnmanagedType.LPUTF8Str)] string audioCodec);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void ncktv_cancel_export();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void ncktv_youtube_search([MarshalAs(UnmanagedType.LPUTF8Str)] string query);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void ncktv_youtube_search_more();

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void ncktv_youtube_download([MarshalAs(UnmanagedType.LPUTF8Str)] string videoId, bool audioOnly, [MarshalAs(UnmanagedType.LPUTF8Str)] string saveDir, [MarshalAs(UnmanagedType.LPUTF8Str)] string title);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void ncktv_youtube_cancel_download([MarshalAs(UnmanagedType.LPUTF8Str)] string videoId);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern bool ncktv_youtube_is_searching();

        // Helper properties to convert native pointers to strings
        public static string CpuInfo => Marshal.PtrToStringAnsi(ncktv_get_cpu_info());
        public static string GpuInfo => Marshal.PtrToStringAnsi(ncktv_get_gpu_info());
        public static string RamInfo => Marshal.PtrToStringAnsi(ncktv_get_ram_info());
        public static string OsInfo => Marshal.PtrToStringAnsi(ncktv_get_os_info());
        public static string OnnxProviderInfo => Marshal.PtrToStringAnsi(ncktv_get_onnx_provider_info());
    }
}
