#pragma once
/*
 * NC-KTV Core — GPU Detector
 * Port of gpu_detector.py
 */

#include <QString>

namespace ncktv {

class GPUDetector {
public:
    static bool    isCudaAvailable();
    static QString getGpuName();
    static void    clearCache();
};

} // namespace ncktv
