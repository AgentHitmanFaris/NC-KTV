/*
 * NC-KTV Core — FFmpeg Utilities Implementation
 */

#include "ffmpeg_utils.h"
#include <QProcess>

namespace ncktv {

bool checkFfmpeg() {
    QProcess proc;
    proc.start("ffmpeg", {"-version"});
    proc.waitForFinished(3000);
    return proc.exitCode() == 0;
}

bool checkFfprobe() {
    QProcess proc;
    proc.start("ffprobe", {"-version"});
    proc.waitForFinished(3000);
    return proc.exitCode() == 0;
}

QString getFfmpegVersion() {
    QProcess proc;
    proc.start("ffmpeg", {"-version"});
    proc.waitForFinished(3000);
    if (proc.exitCode() != 0) return {};

    QString output = proc.readAllStandardOutput();
    // First line is typically "ffmpeg version X.Y.Z ..."
    int newline = output.indexOf('\n');
    return newline > 0 ? output.left(newline).trimmed() : output.trimmed();
}

} // namespace ncktv
