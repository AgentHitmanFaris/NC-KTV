/*
 * NC-KTV Core — Effect Compositor Implementation
 * Port of effect_compositor.py (224 lines)
 */

#include "effect_compositor.h"
#include <cmath>
#include <algorithm>

namespace ncktv {

QVector<const Effect*> EffectCompositor::getActiveEffects(const Clip& clip, double time) const {
    return clip.getEffectsAtTime(time);
}

// ─── Bezier Evaluation ───────────────────────────────────────────────────────

double EffectCompositor::evaluateBezier(const QVector<QPair<double,double>>& points,
                                         double t) const
{
    // De Casteljau's algorithm for numerical stability
    if (points.size() < 2) return t;

    QVector<QPair<double,double>> work = points;
    int n = work.size();

    for (int i = 1; i < n; ++i) {
        for (int j = 0; j < n - i; ++j) {
            work[j].first  = (1.0 - t) * work[j].first  + t * work[j + 1].first;
            work[j].second = (1.0 - t) * work[j].second + t * work[j + 1].second;
        }
    }

    return std::clamp(work[0].second, 0.0, 1.0);
}

double EffectCompositor::evaluateCurve(EasingCurve easing, double t,
                                        const QVector<QPair<double,double>>& customPoints) const
{
    t = std::clamp(t, 0.0, 1.0);

    switch (easing) {
    case EasingCurve::Linear:
        return t;

    case EasingCurve::EaseIn: {
        // Cubic ease-in: P1=(0.42, 0), P2=(1, 1)
        QVector<QPair<double,double>> pts = {{0,0}, {0.42, 0}, {1,1}, {1,1}};
        return evaluateBezier(pts, t);
    }
    case EasingCurve::EaseOut: {
        // Cubic ease-out: P1=(0, 0), P2=(0.58, 1)
        QVector<QPair<double,double>> pts = {{0,0}, {0,0}, {0.58, 1}, {1,1}};
        return evaluateBezier(pts, t);
    }
    case EasingCurve::EaseInOut: {
        // Cubic ease-in-out: P1=(0.42, 0), P2=(0.58, 1)
        QVector<QPair<double,double>> pts = {{0,0}, {0.42, 0}, {0.58, 1}, {1,1}};
        return evaluateBezier(pts, t);
    }
    case EasingCurve::CustomBezier:
        if (customPoints.size() >= 4)
            return evaluateBezier(customPoints, t);
        return t;
    }

    return t;
}

double EffectCompositor::calculateEffectProgress(const Effect& effect, double time) const {
    if (effect.duration <= 0.0) return 1.0;
    double raw = (time - effect.startTime) / effect.duration;
    return std::clamp(raw, 0.0, 1.0);
}

// ─── Per-type Calculations ───────────────────────────────────────────────────

double EffectCompositor::calculateFadeValue(const Effect& effect, double time) const {
    double progress = calculateEffectProgress(effect, time);
    double eased = evaluateCurve(effect.easing, progress, effect.customCurvePoints);

    if (effect.effectType == EffectType::FadeIn)
        return eased;           // 0 → 1
    else // FadeOut
        return 1.0 - eased;    // 1 → 0
}

QPair<double,double> EffectCompositor::calculateSlideOffset(const Effect& effect,
                                                             double time) const
{
    double progress = calculateEffectProgress(effect, time);
    double eased = evaluateCurve(effect.easing, progress, effect.customCurvePoints);

    double distance = 100.0;  // Default 100 pixels
    if (effect.properties.contains("distance"))
        distance = effect.properties["distance"].get<double>();

    double offset = distance * (1.0 - eased);   // slides from distance to 0

    if (effect.effectType == EffectType::SlideLeft)
        return {-offset, 0.0};
    else // SlideRight
        return {offset, 0.0};
}

double EffectCompositor::calculateZoomScale(const Effect& effect, double time) const {
    double progress = calculateEffectProgress(effect, time);
    double eased = evaluateCurve(effect.easing, progress, effect.customCurvePoints);

    double startScale = 1.0, endScale = 1.0;
    if (effect.properties.contains("start_scale"))
        startScale = effect.properties["start_scale"].get<double>();
    if (effect.properties.contains("end_scale"))
        endScale = effect.properties["end_scale"].get<double>();

    if (effect.effectType == EffectType::ZoomIn)
        return startScale + eased * (endScale - startScale);
    else  // ZoomOut
        return startScale + (1.0 - eased) * (endScale - startScale);
}

double EffectCompositor::calculateBlurRadius(const Effect& effect, double time) const {
    double progress = calculateEffectProgress(effect, time);
    double eased = evaluateCurve(effect.easing, progress, effect.customCurvePoints);

    double maxRadius = 10.0;
    if (effect.properties.contains("max_radius"))
        maxRadius = effect.properties["max_radius"].get<double>();

    return maxRadius * (1.0 - eased);
}

QColor EffectCompositor::calculateColorBlend(const Effect& effect, double time,
                                              const QColor& originalColor) const
{
    double progress = calculateEffectProgress(effect, time);
    double eased = evaluateCurve(effect.easing, progress, effect.customCurvePoints);

    // Target color from properties
    int tr = 255, tg = 255, tb = 255;
    if (effect.properties.contains("target_color")) {
        auto& tc = effect.properties["target_color"];
        if (tc.is_array() && tc.size() >= 3) {
            tr = tc[0].get<int>();
            tg = tc[1].get<int>();
            tb = tc[2].get<int>();
        }
    }

    int r = static_cast<int>(originalColor.red()   + eased * (tr - originalColor.red()));
    int g = static_cast<int>(originalColor.green() + eased * (tg - originalColor.green()));
    int b = static_cast<int>(originalColor.blue()  + eased * (tb - originalColor.blue()));

    return QColor(std::clamp(r, 0, 255), std::clamp(g, 0, 255), std::clamp(b, 0, 255));
}

// ─── Composite ───────────────────────────────────────────────────────────────

EffectCompositor::PainterEffectResult
EffectCompositor::applyEffects(const Clip& clip, double time,
                                const QColor& baseColor) const
{
    PainterEffectResult result;
    result.color   = baseColor;
    result.opacity = 1.0;
    result.offsetX = 0.0;
    result.offsetY = 0.0;
    result.scale   = 1.0;

    auto activeEffects = getActiveEffects(clip, time);
    for (const auto* effect : activeEffects) {
        switch (effect->effectType) {
        case EffectType::FadeIn:
        case EffectType::FadeOut:
            result.opacity *= calculateFadeValue(*effect, time);
            break;

        case EffectType::SlideLeft:
        case EffectType::SlideRight: {
            auto [dx, dy] = calculateSlideOffset(*effect, time);
            result.offsetX += dx;
            result.offsetY += dy;
            break;
        }
        case EffectType::ZoomIn:
        case EffectType::ZoomOut:
            result.scale *= calculateZoomScale(*effect, time);
            break;

        case EffectType::ColorShift:
            result.color = calculateColorBlend(*effect, time, result.color);
            break;

        case EffectType::Blur:
        case EffectType::Custom:
            // Blur handled at render level, Custom is pass-through
            break;
        }
    }

    return result;
}

void EffectCompositor::clearCache() {
    m_bezierCache.clear();
}

} // namespace ncktv
