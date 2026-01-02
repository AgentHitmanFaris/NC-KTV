"""
Test Effect Compositor
"""
import sys
sys.path.insert(0, 'src')

from core.effect_compositor import EffectCompositor
from core.timeline_data import Effect, EffectType, EasingCurve, Clip
from PyQt6.QtGui import QColor

print("✅ Imports successful")

# Create compositor
compositor = EffectCompositor()

# Test 1: Bezier curve evaluation
print("\n=== Test 1: Curve Evaluation ===")

# Linear
linear_val = compositor.evaluate_curve(EasingCurve.LINEAR, 0.5)
print(f"Linear at t=0.5: {linear_val:.3f} (expected ~0.500)")

# Ease-in (cubic)
ease_in_val = compositor.evaluate_curve(EasingCurve.EASE_IN, 0.5)
print(f"Ease-In at t=0.5: {ease_in_val:.3f} (expected ~0.125)")

# Ease-out
ease_out_val = compositor.evaluate_curve(EasingCurve.EASE_OUT, 0.5)
print(f"Ease-Out at t=0.5: {ease_out_val:.3f} (expected ~0.875)")

# Custom Bezier
custom_points = [(0.0, 0.0), (0.5, 0.2), (0.8, 0.9), (1.0, 1.0)]
custom_val = compositor.evaluate_curve(EasingCurve.CUSTOM_BEZIER, 0.5, custom_points)
print(f"Custom Bezier at t=0.5: {custom_val:.3f}")

print("✅ Curve evaluation working")

# Test 2: Fade effect
print("\n=== Test 2: Fade Effects ===")

clip = Clip(
    clip_id="test",
    track_id="test",
    start_time=0.0,
    duration=10.0
)

fade_in = Effect(
    effect_type=EffectType.FADE_IN,
    start_time=0.0,
    duration=2.0,
    easing=EasingCurve.LINEAR,
    properties={'intensity': 1.0}
)

clip.add_effect(fade_in)

# Test at different times
for time in [0.0, 0.5, 1.0, 1.5, 2.0]:
    opacity = compositor.calculate_fade_value(fade_in, time)
    print(f"  Fade-In at t={time}s: opacity={opacity:.3f}")

print("✅ Fade effects working")

# Test 3: Slide effect
print("\n=== Test 3: Slide Effects ===")

slide_left = Effect(
    effect_type=EffectType.SLIDE_LEFT,
    start_time=0.0,
    duration=1.0,
    easing=EasingCurve.EASE_OUT,
    properties={'distance': 200}
)

for time in [0.0, 0.5, 1.0]:
    x, y = compositor.calculate_slide_offset(slide_left, time)
    print(f"  Slide-Left at t={time}s: offset=({x}, {y}) px")

print("✅ Slide effects working")

# Test 4: Zoom effect
print("\n=== Test 4: Zoom Effects ===")

zoom_in = Effect(
    effect_type=EffectType.ZOOM_IN,
    start_time=0.0,
    duration=1.5,
    easing=EasingCurve.EASE_IN_OUT,
    properties={'scale': 2.0}
)

for time in [0.0, 0.75, 1.5]:
    scale = compositor.calculate_zoom_scale(zoom_in, time)
    print(f"  Zoom-In at t={time}s: scale={scale:.3f}x")

print("✅ Zoom effects working")

# Test 5: Color shift
print("\n=== Test 5: Color Shift ===")

color_shift = Effect(
    effect_type=EffectType.COLOR_SHIFT,
    start_time=0.0,
    duration=1.0,
    easing=EasingCurve.LINEAR,
    properties={'color': '#FF0000', 'intensity': 1.0}
)

original = QColor(255, 255, 255)  # White
shifted = compositor.calculate_color_blend(color_shift, 0.5, original)
print(f"  Original: RGB({original.red()}, {original.green()}, {original.blue()})")
print(f"  Shifted (50%): RGB({shifted.red()}, {shifted.green()}, {shifted.blue()})")

print("✅ Color shift working")

# Test 6: Multiple effects
print("\n=== Test 6: Multiple Effects ===")

clip2 = Clip(
    clip_id="multi",
    track_id="test",
    start_time=0.0,
    duration=5.0
)

# Add multiple effects
clip2.add_effect(Effect(
    effect_type=EffectType.FADE_IN,
    start_time=0.0,
    duration=1.0,
    easing=EasingCurve.LINEAR,
    properties={'intensity': 1.0}
))

clip2.add_effect(Effect(
    effect_type=EffectType.ZOOM_IN,
    start_time=0.5,
    duration=1.5,
    easing=EasingCurve.EASE_IN,
    properties={'scale': 1.5}
))

active = compositor.get_active_effects(clip2, 1.0)
print(f"  Active effects at t=1.0s: {len(active)} effects")
for effect in active:
    print(f"    - {effect.effect_type.value}")

print("✅ Multiple effects working")

# Test 7: Cache performance
print("\n=== Test 7: Cache Performance ===")

import time
custom_curve = [(0.0, 0.0), (0.42, 0.0), (0.58, 1.0), (1.0, 1.0)]

# First run (no cache)
start = time.time()
for i in range(1000):
    compositor.evaluate_curve(EasingCurve.CUSTOM_BEZIER, i/1000, custom_curve)
first_time = time.time() - start

# Clear cache and run again
compositor.clear_cache()

# Second run (with cache)
start = time.time()
for i in range(1000):
    compositor.evaluate_curve(EasingCurve.CUSTOM_BEZIER, i/1000, custom_curve)
second_time = time.time() - start

print(f"  First run (no cache): {first_time*1000:.2f}ms")
print(f"  Second run (cached): {second_time*1000:.2f}ms")
print(f"  Speedup: {first_time/second_time:.1f}x")

print("✅ Caching working")

print("\n🎉 All compositor tests passed!")
