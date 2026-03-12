/**
 * @file ncktv_effect_compositor.hpp
 * @brief Effect calculation and Bezier curve evaluation (C++17)
 */

#pragma once

#include "ncktv_core_data.hpp"
#include <vector>
#include <map>
#include <tuple>
#include <cmath>
#include <algorithm>
#include <mutex>

namespace ncktv {
namespace core {

struct Color {
    int r{255}, g{255}, b{255}, a{255};
};

class EffectCompositor {
private:
    // Simple caching mechanism for bezier curves to avoid re-calculating exact floats
    // Using a quantized tuple for hashability or a map
    std::map<std::tuple<float, float, float, float, float, float, float, float, int>, float> bezier_cache_;
    std::mutex cache_mutex_;

    float evaluate_bezier_curve(const std::vector<Point>& points, float t) {
        if (points.size() != 4) return t; // Fallback

        // Quantize t to 3 decimal places for cache
        int t_quant = static_cast<int>(std::round(t * 1000.0f));
        
        auto key = std::make_tuple(
            points[0].x, points[0].y,
            points[1].x, points[1].y,
            points[2].x, points[2].y,
            points[3].x, points[3].y,
            t_quant
        );

        {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            auto it = bezier_cache_.find(key);
            if (it != bezier_cache_.end()) {
                return it->second;
            }
        }

        float one_minus_t = 1.0f - t;
        
        // B(t) = (1-t)^3*P0 + 3(1-t)^2*t*P1 + 3(1-t)*t^2*P2 + t^3*P3
        float y = (std::pow(one_minus_t, 3) * points[0].y) +
                  (3.0f * std::pow(one_minus_t, 2) * t * points[1].y) +
                  (3.0f * one_minus_t * std::pow(t, 2) * points[2].y) +
                  (std::pow(t, 3) * points[3].y);

        float result = std::max(0.0f, std::min(1.0f, y));

        {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            bezier_cache_[key] = result;
        }

        return result;
    }

public:
    EffectCompositor() = default;

    [[nodiscard]] std::vector<Effect> get_active_effects(const Clip& clip, float time) const {
        return clip.get_effects_at_time(time - clip.start_time);
    }

    [[nodiscard]] float evaluate_curve(EasingCurve easing, float t, const std::optional<std::vector<Point>>& custom_points = std::nullopt) {
        t = std::max(0.0f, std::min(1.0f, t));

        switch (easing) {
            case EasingCurve::LINEAR:
                return t;
            case EasingCurve::EASE_IN:
                return t * t * t;
            case EasingCurve::EASE_OUT:
                return 1.0f - std::pow(1.0f - t, 3);
            case EasingCurve::EASE_IN_OUT:
                if (t < 0.5f) {
                    return 4.0f * t * t * t;
                } else {
                    return 1.0f - std::pow(-2.0f * t + 2.0f, 3) / 2.0f;
                }
            case EasingCurve::CUSTOM_BEZIER:
                if (custom_points && custom_points->size() == 4) {
                    return evaluate_bezier_curve(*custom_points, t);
                }
                return t;
            default:
                return t;
        }
    }

    [[nodiscard]] float calculate_effect_progress(const Effect& effect, float time) {
        if (effect.duration <= 0.0f) return 1.0f;
        float progress = (time - effect.start_time) / effect.duration;
        return std::max(0.0f, std::min(1.0f, progress));
    }

    [[nodiscard]] float calculate_fade_value(const Effect& effect, float time) {
        float progress = calculate_effect_progress(effect, time);
        float curve_value = evaluate_curve(effect.easing, progress, effect.custom_curve_points);
        
        float intensity = 1.0f;
        auto it = effect.properties.find("intensity");
        if (it != effect.properties.end() && it->second.type() == typeid(float)) {
            intensity = std::any_cast<float>(it->second);
        }

        if (effect.effect_type == EffectType::FADE_IN) {
            return curve_value * intensity;
        } else if (effect.effect_type == EffectType::FADE_OUT) {
            return (1.0f - curve_value) * intensity;
        }
        return 1.0f;
    }

    [[nodiscard]] std::pair<int, int> calculate_slide_offset(const Effect& effect, float time) {
        float progress = calculate_effect_progress(effect, time);
        float curve_value = evaluate_curve(effect.easing, progress, effect.custom_curve_points);
        
        float distance = 100.0f;
        auto it = effect.properties.find("distance");
        if (it != effect.properties.end()) {
            if (it->second.type() == typeid(float)) distance = std::any_cast<float>(it->second);
            else if (it->second.type() == typeid(int)) distance = static_cast<float>(std::any_cast<int>(it->second));
        }

        float offset = distance * (1.0f - curve_value);

        if (effect.effect_type == EffectType::SLIDE_LEFT) {
            return {-static_cast<int>(offset), 0};
        } else if (effect.effect_type == EffectType::SLIDE_RIGHT) {
            return {static_cast<int>(offset), 0};
        }
        return {0, 0};
    }

    void clear_cache() {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        bezier_cache_.clear();
    }
};

} // namespace core
} // namespace ncktv
