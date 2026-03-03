#pragma once
/*
 * NC-KTV Core — Effect Compositor
 * Port of effect_compositor.py — Bezier-driven animation effects
 */

#include <QColor>
#include <QPainter>
#include <QPair>
#include <QVector>
#include <QHash>
#include <optional>

#include "timeline/timeline_data.h"

namespace ncktv {

class EffectCompositor {
public:
    EffectCompositor() = default;

    /// Get all effects active at the given time (relative to clip start)
    QVector<const Effect*> getActiveEffects(const Clip& clip, double time) const;

    /// Evaluate easing curve at parameter t ∈ [0, 1] → [0, 1]
    double evaluateCurve(EasingCurve easing, double t,
                         const QVector<QPair<double,double>>& customPoints = {}) const;

    /// Calculate normalized progress (0–1) through the effect
    double calculateEffectProgress(const Effect& effect, double time) const;

    // ── Per-effect-type calculations ─────────────────────────────────────
    double calculateFadeValue(const Effect& effect, double time) const;
    QPair<double,double> calculateSlideOffset(const Effect& effect, double time) const;
    double calculateZoomScale(const Effect& effect, double time) const;
    double calculateBlurRadius(const Effect& effect, double time) const;
    QColor calculateColorBlend(const Effect& effect, double time,
                               const QColor& originalColor) const;

    /// Apply all active effects to a QPainter. Returns {modifiedColor, opacity}.
    struct PainterEffectResult {
        QColor color = Qt::white;
        double opacity = 1.0;
        double offsetX = 0.0;
        double offsetY = 0.0;
        double scale   = 1.0;
    };
    PainterEffectResult applyEffects(const Clip& clip, double time,
                                      const QColor& baseColor = Qt::white) const;

    /// Clear Bezier evaluation cache
    void clearCache();

private:
    /// Evaluate cubic Bezier using De Casteljau's algorithm
    double evaluateBezier(const QVector<QPair<double,double>>& points, double t) const;

    mutable QHash<uint64_t, double> m_bezierCache;
};

} // namespace ncktv
