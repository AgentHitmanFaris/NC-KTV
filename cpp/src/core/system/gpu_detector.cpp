/*
 * NC-KTV Core — GPU Detector Implementation
 */

#include "gpu_detector.h"
#include <QProcess>

namespace ncktv {

bool GPUDetector::isCudaAvailable() {
    QProcess proc;
    proc.start("nvidia-smi", {});
    proc.waitForFinished(3000);
    return proc.exitCode() == 0;
}

QString GPUDetector::getGpuName() {
    QProcess proc;
    proc.start("nvidia-smi", {"--query-gpu=name", "--format=csv,noheader,nounits"});
    proc.waitForFinished(3000);
    if (proc.exitCode() != 0) return {};
    return QString(proc.readAllStandardOutput()).trimmed().split('\n').first();
}

void GPUDetector::clearCache() {
    // No-op in C++ version — GPU memory is managed by ONNX Runtime / driver
}

} // namespace ncktv
