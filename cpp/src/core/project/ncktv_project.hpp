/**
 * @file ncktv_project.hpp
 * @brief Project management and data structures for NC-KTV (C++17)
 */

#pragma once

#include "../timeline/ncktv_core_data.hpp"
#include "../audio/ncktv_audio_clock.hpp"
#include <string>
#include <optional>
#include <filesystem>
#include <chrono>

namespace ncktv {
namespace core {

struct ProjectSettings {
    std::string uvr_model{"UVR_MDXNET_KARA_2.onnx"};
    bool use_gpu{true};
    int sample_rate{44100};
    std::string karaoke_style{"classic"};
    std::string output_format{"mp4"};
};

class Project {
public:
    inline static const std::string PROJECT_VERSION = "0.7";
    inline static const std::string PROJECT_EXT = ".nctv";

    std::optional<std::filesystem::path> source_file;
    std::string project_name{"Untitled"};
    
    ProjectSettings settings;
    LyricsData lyrics;
    TimelineData timeline;
    AudioClock audio_clock;
    
    PropertiesMap timing_metadata;

    std::optional<std::filesystem::path> audio_file;
    std::optional<std::filesystem::path> instrumental_file;
    std::optional<std::filesystem::path> vocals_file;
    std::optional<std::filesystem::path> output_video;
    std::optional<std::filesystem::path> project_file;

    std::string created_date;
    std::string modified_date;

    Project() {
        timing_metadata["sample_rate_validated"] = false;
        timing_metadata["transcription_source"] = std::string("original");
        timing_metadata["uvr_delay_measured"] = 0.0f;
        timing_metadata["global_offset"] = 0.0f;
        
        auto now = std::chrono::system_clock::now();
        created_date = std::to_string(std::chrono::system_clock::to_time_t(now));
        modified_date = created_date;
    }

    explicit Project(std::filesystem::path source) : Project() {
        source_file = source;
        if (source.has_stem()) {
            project_name = source.stem().string();
        }
    }

    [[nodiscard]] std::filesystem::path get_temp_dir() const {
        std::filesystem::path temp_dir = std::filesystem::path("temp") / project_name;
        return temp_dir;
    }

    [[nodiscard]] std::pair<bool, std::string> validate() const {
        if (!source_file.has_value()) {
            return {false, "No source file specified"};
        }
        if (!std::filesystem::exists(source_file.value())) {
            return {false, "Source file not found: " + source_file.value().string()};
        }
        return {true, "OK"};
    }
};

} // namespace core
} // namespace ncktv
