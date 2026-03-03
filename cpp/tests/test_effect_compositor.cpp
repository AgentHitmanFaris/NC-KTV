#include <gtest/gtest.h>
#include "effects/effect_compositor.h"
using namespace ncktv;

TEST(EffectCompositor, LinearCurve) {
    EffectCompositor ec;
    EXPECT_DOUBLE_EQ(ec.evaluateCurve(EasingCurve::Linear, 0.0), 0.0);
    EXPECT_DOUBLE_EQ(ec.evaluateCurve(EasingCurve::Linear, 0.5), 0.5);
    EXPECT_DOUBLE_EQ(ec.evaluateCurve(EasingCurve::Linear, 1.0), 1.0);
}

TEST(EffectCompositor, FadeValue) {
    EffectCompositor ec;
    Effect fade;
    fade.effectType = EffectType::FadeIn;
    fade.startTime = 0.0;
    fade.duration = 2.0;
    fade.easing = EasingCurve::Linear;
    EXPECT_NEAR(ec.calculateFadeValue(fade, 0.0), 0.0, 0.01);
    EXPECT_NEAR(ec.calculateFadeValue(fade, 1.0), 0.5, 0.01);
    EXPECT_NEAR(ec.calculateFadeValue(fade, 2.0), 1.0, 0.01);
}

TEST(EffectCompositor, EaseCurveBounds) {
    EffectCompositor ec;
    // All easing curves should be in [0, 1]
    for (auto curve : {EasingCurve::EaseIn, EasingCurve::EaseOut, EasingCurve::EaseInOut}) {
        for (double t = 0; t <= 1.0; t += 0.1) {
            double v = ec.evaluateCurve(curve, t);
            EXPECT_GE(v, 0.0);
            EXPECT_LE(v, 1.0);
        }
}

TEST(EffectCompositor, EffectProgress) {
    EffectCompositor ec;
    Effect e;
    e.startTime = 1.0;
    e.duration = 2.0;

    EXPECT_DOUBLE_EQ(ec.calculateEffectProgress(e, 0.0), 0.0);
    EXPECT_DOUBLE_EQ(ec.calculateEffectProgress(e, 1.0), 0.0);
    EXPECT_DOUBLE_EQ(ec.calculateEffectProgress(e, 2.0), 0.5);
    EXPECT_DOUBLE_EQ(ec.calculateEffectProgress(e, 3.0), 1.0);
    EXPECT_DOUBLE_EQ(ec.calculateEffectProgress(e, 4.0), 1.0);
}

TEST(EffectCompositor, SlideOffset) {
    EffectCompositor ec;
    Effect e;
    e.startTime = 0.0;
    e.duration = 1.0;
    e.easing = EasingCurve::Linear;
    e.properties["distance"] = 200.0;
    
    e.effectType = EffectType::SlideLeft;
    auto [dx1, dy1] = ec.calculateSlideOffset(e, 0.0);
    EXPECT_DOUBLE_EQ(dx1, -200.0);
    auto [dx2, dy2] = ec.calculateSlideOffset(e, 1.0);
    EXPECT_DOUBLE_EQ(dx2, 0.0);

    e.effectType = EffectType::SlideRight;
    auto [dx3, dy3] = ec.calculateSlideOffset(e, 0.0);
    EXPECT_DOUBLE_EQ(dx3, 200.0);
    auto [dx4, dy4] = ec.calculateSlideOffset(e, 1.0);
    EXPECT_DOUBLE_EQ(dx4, 0.0);
}

TEST(EffectCompositor, ZoomScale) {
    EffectCompositor ec;
    Effect e;
    e.startTime = 0.0;
    e.duration = 1.0;
    e.easing = EasingCurve::Linear;
    e.properties["start_scale"] = 0.5;
    e.properties["end_scale"] = 2.0;
    
    e.effectType = EffectType::ZoomIn;
    EXPECT_DOUBLE_EQ(ec.calculateZoomScale(e, 0.0), 0.5);
    EXPECT_DOUBLE_EQ(ec.calculateZoomScale(e, 1.0), 2.0);

    e.effectType = EffectType::ZoomOut;
    EXPECT_DOUBLE_EQ(ec.calculateZoomScale(e, 0.0), 2.0);
    EXPECT_DOUBLE_EQ(ec.calculateZoomScale(e, 1.0), 0.5);
}

TEST(EffectCompositor, BlurRadius) {
    EffectCompositor ec;
    Effect e;
    e.startTime = 0.0;
    e.duration = 1.0;
    e.easing = EasingCurve::Linear;
    e.properties["max_radius"] = 20.0;

    EXPECT_DOUBLE_EQ(ec.calculateBlurRadius(e, 0.0), 20.0);
    EXPECT_DOUBLE_EQ(ec.calculateBlurRadius(e, 0.5), 10.0);
    EXPECT_DOUBLE_EQ(ec.calculateBlurRadius(e, 1.0), 0.0);
}

TEST(EffectCompositor, ColorBlend) {
    EffectCompositor ec;
    Effect e;
    e.startTime = 0.0;
    e.duration = 1.0;
    e.easing = EasingCurve::Linear;
    e.properties["target_color"] = {100, 150, 200}; // R, G, B

    QColor original(0, 0, 0);
    
    QColor c1 = ec.calculateColorBlend(e, 0.0, original);
    EXPECT_EQ(c1.red(), 0);
    EXPECT_EQ(c1.green(), 0);
    EXPECT_EQ(c1.blue(), 0);

    QColor c2 = ec.calculateColorBlend(e, 1.0, original);
    EXPECT_EQ(c2.red(), 100);
    EXPECT_EQ(c2.green(), 150);
    EXPECT_EQ(c2.blue(), 200);

    QColor c3 = ec.calculateColorBlend(e, 0.5, original);
    EXPECT_EQ(c3.red(), 50);
    EXPECT_EQ(c3.green(), 75);
    EXPECT_EQ(c3.blue(), 100);
}

TEST(EffectCompositor, ApplyEffects) {
    EffectCompositor ec;
    Clip clip;
    clip.startTime = 0.0;
    clip.duration = 5.0;

    // Fade In effect
    Effect fadein;
    fadein.effectType = EffectType::FadeIn;
    fadein.startTime = 0.0;
    fadein.duration = 1.0;
    fadein.easing = EasingCurve::Linear;
    clip.addEffect(fadein);

    // Zoom In effect
    Effect zoomin;
    zoomin.effectType = EffectType::ZoomIn;
    zoomin.startTime = 0.0;
    zoomin.duration = 1.0;
    zoomin.easing = EasingCurve::Linear;
    zoomin.properties["start_scale"] = 0.5;
    zoomin.properties["end_scale"] = 1.0;
    clip.addEffect(zoomin);

    // Test at t=0.0
    auto res1 = ec.applyEffects(clip, 0.0, Qt::white);
    EXPECT_DOUBLE_EQ(res1.opacity, 0.0);
    EXPECT_DOUBLE_EQ(res1.scale, 0.5);

    // Test at t=0.5
    auto res2 = ec.applyEffects(clip, 0.5, Qt::white);
    EXPECT_DOUBLE_EQ(res2.opacity, 0.5);
    EXPECT_DOUBLE_EQ(res2.scale, 0.75);

    // Test at t=1.0
    auto res3 = ec.applyEffects(clip, 1.0, Qt::white);
    EXPECT_DOUBLE_EQ(res3.opacity, 1.0);
    EXPECT_DOUBLE_EQ(res3.scale, 1.0);
}

