/**
 * @file ncktv_whisper_engine.hpp
 * @brief Stub for the native whisper.cpp transcription engine.
 *
 * On MinGW, whisper.cpp cannot be compiled because ggml.c (~80k LOC) causes
 * gcc to crash with heap exhaustion. All transcription is routed through the
 * Python process bridge (TranscriptionWorker → QProcess → Python whisper CLI).
 *
 * This header provides the shared data structures (TranscribedWord,
 * TranscribedSegment) used by serializeSegmentsToJson() so that
 * transcription_worker.cpp compiles without the actual whisper runtime.
 */

#pragma once

#include <string>
#include <vector>
#include <future>

namespace ncktv {
namespace ai {

struct TranscribedWord {
    std::string text;
    float start_time  = 0.f;
    float end_time    = 0.f;
    float probability = 1.f;
};

struct TranscribedSegment {
    std::string text;
    float start_time = 0.f;
    float end_time   = 0.f;
    std::vector<TranscribedWord> words;
};

// WhisperEngine is intentionally NOT defined here for MinGW builds.
// TranscriptionWorker uses QProcess to call the Python whisper CLI instead.

} // namespace ai
} // namespace ncktv
