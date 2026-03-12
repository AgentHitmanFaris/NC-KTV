/**
 * @file ncktv_core_data.hpp
 * @brief Header-only library for NC-KTV core data structures and utilities (C++17)
 * 
 * This file replaces the Python core data structures (timeline_data.py, lyrics.py)
 * providing high-performance, type-safe C++17 equivalents.
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <any>
#include <chrono>
#include <nlohmann/json.hpp>

namespace ncktv {
namespace core {

// ============================================================================
// Enums
// ============================================================================

enum class TrackType {
    AUDIO,
    VIDEO,
    EFFECTS,
    LYRICS
};

enum class EffectType {
    FADE_IN,
    FADE_OUT,
    SLIDE_LEFT,
    SLIDE_RIGHT,
    ZOOM_IN,
    ZOOM_OUT,
    BLUR,
    COLOR_SHIFT,
    CUSTOM
};

enum class EasingCurve {
    LINEAR,
    EASE_IN,
    EASE_OUT,
    EASE_IN_OUT,
    CUSTOM_BEZIER
};

// ============================================================================
// Basic Types
// ============================================================================

using PropertiesMap = std::map<std::string, std::any>;

struct Point {
    float x{0.0f};
    float y{0.0f};
};

// ============================================================================
// Timeline Data Structures
// ============================================================================

class Effect {
public:
    EffectType effect_type{EffectType::CUSTOM};
    float start_time{0.0f}; // Seconds
    float duration{0.0f};   // Seconds
    EasingCurve easing{EasingCurve::LINEAR};
    std::optional<std::vector<Point>> custom_curve_points;
    PropertiesMap properties;

    Effect() = default;
    Effect(EffectType type, float start, float dur, EasingCurve ease = EasingCurve::LINEAR)
        : effect_type(type), start_time(start), duration(dur), easing(ease) {}

    [[nodiscard]] float end_time() const {
        return start_time + duration;
    }
};

class Clip {
public:
    std::string clip_id;
    std::string track_id;
    float start_time{0.0f}; // Position on timeline (seconds)
    float duration{0.0f};   // Clip duration (seconds)
    std::optional<std::string> source_file;
    float source_start{0.0f};
    std::optional<float> source_end;
    std::vector<Effect> effects;
    PropertiesMap properties;

    Clip() = default;
    Clip(std::string cid, std::string tid, float start, float dur)
        : clip_id(std::move(cid)), track_id(std::move(tid)), start_time(start), duration(dur) {}

    [[nodiscard]] float end_time() const {
        return start_time + duration;
    }

    void move_to(float new_start_time) {
        start_time = std::max(0.0f, new_start_time);
    }

    void resize(float new_duration, bool from_start = false) {
        constexpr float MIN_DURATION = 0.1f;
        new_duration = std::max(MIN_DURATION, new_duration);

        if (from_start) {
            float time_diff = duration - new_duration;
            start_time += time_diff;
            source_start += time_diff;
        }

        duration = new_duration;

        if (source_end.has_value()) {
            source_end = source_start + new_duration;
        }
    }

    std::optional<Clip> split_at(float time, const std::string& new_clip_id) {
        if (time <= start_time || time >= end_time()) {
            return std::nullopt; // Time out of bounds
        }

        float left_duration = time - start_time;
        float right_duration = end_time() - time;

        Clip right_clip;
        right_clip.clip_id = new_clip_id;
        right_clip.track_id = track_id;
        right_clip.start_time = time;
        right_clip.duration = right_duration;
        right_clip.source_file = source_file;
        right_clip.source_start = source_start + left_duration;
        right_clip.source_end = source_end;
        right_clip.properties = properties; // Copy properties

        // Move effects to right clip if they start after the split point
        std::vector<Effect> remaining_effects;
        for (const auto& effect : effects) {
            float effect_global_time = start_time + effect.start_time;
            if (effect_global_time >= time) {
                Effect new_effect = effect;
                new_effect.start_time = effect.start_time - left_duration;
                right_clip.effects.push_back(std::move(new_effect));
            } else {
                remaining_effects.push_back(effect);
            }
        }
        effects = std::move(remaining_effects);

        // Adjust left (current) clip
        duration = left_duration;
        if (source_end.has_value()) {
            source_end = source_start + left_duration;
        }

        return right_clip;
    }

    void add_effect(Effect effect) {
        effects.push_back(std::move(effect));
        std::sort(effects.begin(), effects.end(), 
            [](const Effect& a, const Effect& b) { return a.start_time < b.start_time; }
        );
    }

    bool remove_effect(size_t index) {
        if (index < effects.size()) {
            effects.erase(effects.begin() + index);
            return true;
        }
        return false;
    }

    [[nodiscard]] std::vector<Effect> get_effects_at_time(float time) const {
        std::vector<Effect> active;
        for (const auto& e : effects) {
            if (e.start_time <= time && time <= e.end_time()) {
                active.push_back(e);
            }
        }
        return active;
    }
};

class Track {
public:
    std::string track_id;
    TrackType track_type{TrackType::AUDIO};
    std::string name;
    std::vector<Clip> clips;
    bool is_muted{false};
    bool is_solo{false};
    bool is_locked{false};
    bool is_visible{true};
    int height{60};
    PropertiesMap properties;

    Track() = default;
    Track(std::string id, TrackType type, std::string n)
        : track_id(std::move(id)), track_type(type), name(std::move(n)) {}

    void add_clip(Clip clip) {
        clip.track_id = track_id;
        clips.push_back(std::move(clip));
        std::sort(clips.begin(), clips.end(), [](const Clip& a, const Clip& b) {
            return a.start_time < b.start_time;
        });
    }

    bool remove_clip(const std::string& cid) {
        auto it = std::remove_if(clips.begin(), clips.end(),
            [&](const Clip& c) { return c.clip_id == cid; });
        if (it != clips.end()) {
            clips.erase(it, clips.end());
            return true;
        }
        return false;
    }

    Clip* get_clip(const std::string& cid) {
        for (auto& clip : clips) {
            if (clip.clip_id == cid) return &clip;
        }
        return nullptr;
    }

    [[nodiscard]] std::vector<Clip> get_clips_at_time(float time) const {
        std::vector<Clip> active;
        for (const auto& c : clips) {
            if (c.start_time <= time && time <= c.end_time()) {
                active.push_back(c);
            }
        }
        return active;
    }

    [[nodiscard]] bool check_overlap(const Clip& new_clip, const std::string& exclude_clip_id = "") const {
        for (const auto& existing : clips) {
            if (exclude_clip_id == existing.clip_id) continue;
            
            // Check for overlap standard formula
            if (std::max(new_clip.start_time, existing.start_time) < 
                std::min(new_clip.end_time(), existing.end_time())) {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] float find_next_free_slot(float required_duration, float after_time = 0.0f) const {
        float candidate_time = std::max(0.0f, after_time);

        // Assuming clips are sorted
        for (const auto& clip : clips) {
            if (clip.start_time >= candidate_time) {
                if (clip.start_time - candidate_time >= required_duration) {
                    return candidate_time;
                }
                candidate_time = clip.end_time();
            }
        }
        return candidate_time;
    }
};

class TimelineData {
public:
    std::vector<Track> tracks;
    float duration{0.0f};
    float playhead_position{0.0f};
    float zoom_level{1.0f};
    bool snap_to_grid{true};
    float grid_interval{1.0f};
    PropertiesMap metadata;

    void add_track(Track track) {
        tracks.push_back(std::move(track));
        update_duration();
    }

    bool remove_track(const std::string& track_id) {
        auto it = std::remove_if(tracks.begin(), tracks.end(),
            [&](const Track& t) { return t.track_id == track_id; });
        if (it != tracks.end()) {
            tracks.erase(it, tracks.end());
            update_duration();
            return true;
        }
        return false;
    }

    Track* get_track(const std::string& track_id) {
        for (auto& track : tracks) {
            if (track.track_id == track_id) return &track;
        }
        return nullptr;
    }

    [[nodiscard]] std::vector<Track*> get_tracks_by_type(TrackType type) {
        std::vector<Track*> result;
        for (auto& track : tracks) {
            if (track.track_type == type) {
                result.push_back(&track);
            }
        }
        return result;
    }

    void update_duration() {
        float max_end = 0.0f;
        for (const auto& track : tracks) {
            for (const auto& clip : track.clips) {
                if (clip.end_time() > max_end) {
                    max_end = clip.end_time();
                }
            }
        }
        duration = max_end;
    }

    [[nodiscard]] float snap_time(float time) const {
        if (!snap_to_grid || grid_interval <= 0.0f) return time;
        return std::round(time / grid_interval) * grid_interval;
    }

    void clear() {
        tracks.clear();
        duration = 0.0f;
        playhead_position = 0.0f;
    }
};

// ============================================================================
// Lyrics Data Structures
// ============================================================================

struct LyricsToken {
    std::string text;
    float start_time{0.0f};
    float end_time{0.0f};
    std::optional<std::string> romanized_text;

    LyricsToken() = default;
    LyricsToken(std::string t, float s, float e)
        : text(std::move(t)), start_time(s), end_time(e) {}
};

class LyricsLine {
public:
    std::string text;
    float start_time{0.0f};
    float end_time{0.0f};
    std::vector<LyricsToken> tokens;
    std::optional<std::string> romanized_text;

    // Timeline Effects bindings
    PropertiesMap effects; 
    std::optional<std::string> animation_curve;
    std::optional<std::vector<Point>> custom_curve_points;

    LyricsLine() = default;
    LyricsLine(std::string t, float s, float e)
        : text(std::move(t)), start_time(s), end_time(e) {}

    [[nodiscard]] float duration() const {
        return std::max(0.0f, end_time - start_time);
    }
};

class LyricsData {
public:
    std::vector<LyricsLine> lines;
    PropertiesMap metadata;

    void add_line(const std::string& text, float start = 0.0f, float end = 0.0f, const std::vector<LyricsToken>& tokens = {}) {
        LyricsLine line(text, start, end);
        line.tokens = tokens;
        lines.push_back(std::move(line));
    }

    void clear() {
        lines.clear();
    }

    void importFromWhisperJson(const std::string& jsonString) {
        try {
            auto j = nlohmann::json::parse(jsonString);
            if (!j.contains("segments")) return;
            
            clear();
            for (const auto& seg : j["segments"]) {
                LyricsLine line;
                line.text = seg.value("text", "");
                line.start_time = seg.value("start", 0.0f);
                line.end_time = seg.value("end", 0.0f);
                
                if (seg.contains("words")) {
                    for (const auto& w : seg["words"]) {
                        line.tokens.emplace_back(
                            w.value("word", ""),
                            w.value("start", 0.0f),
                            w.value("end", 0.0f)
                        );
                    }
                }
                lines.push_back(std::move(line));
            }
        } catch (...) {}
    }
};

} // namespace core
} // namespace ncktv
