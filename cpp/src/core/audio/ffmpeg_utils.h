#pragma once
/*
 * NC-KTV Core — FFmpeg Utilities
 * Port of ffmpeg_utils.py
 */

#include <QString>

namespace ncktv {

/// Check if FFmpeg is available in PATH
bool checkFfmpeg();

/// Check if FFprobe is available in PATH
bool checkFfprobe();

/// Get FFmpeg version string
QString getFfmpegVersion();

} // namespace ncktv
