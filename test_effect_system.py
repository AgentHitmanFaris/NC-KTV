"""
Quick test for effect panel and curve editor
"""
import sys
sys.path.insert(0, 'src')

from core.timeline_data import Clip, Effect, EffectType, EasingCurve

print("✅ Imports successful")

# Test creating a clip with effects
clip = Clip(
    clip_id="test_effect_clip",
    track_id="test",
    start_time=0.0,
    duration=10.0
)

# Add some effects
fade_in = Effect(
    effect_type=EffectType.FADE_IN,
    start_time=0.0,
    duration=2.0,
    easing=EasingCurve.EASE_IN,
    properties={'intensity': 1.0}
)

zoom_out = Effect(
    effect_type=EffectType.ZOOM_OUT,
    start_time=5.0,
    duration=3.0,
    easing=EasingCurve.EASE_OUT,
    properties={'scale': 2.0}
)

clip.add_effect(fade_in)
clip.add_effect(zoom_out)

print(f"✅ Created clip with {len(clip.effects)} effects")

# Test effect properties
for i, effect in enumerate(clip.effects):
    print(f"  Effect {i+1}: {effect.effect_type.value}")
    print(f"    Time: {effect.start_time:.2f}s - {effect.end_time:.2f}s")
    print(f"    Easing: {effect.easing.value}")
    print(f"    Properties: {effect.properties}")

# Test custom curve points
custom_effect = Effect(
    effect_type=EffectType.SLIDE_LEFT,
    start_time=7.0,
    duration=1.5,
    easing=EasingCurve.CUSTOM_BEZIER,
    custom_curve_points=[(0.0, 0.0), (0.5, 0.2), (0.8, 0.9), (1.0, 1.0)]
)

clip.add_effect(custom_effect)

print(f"\n✅ Added custom curve effect")
print(f"  Total effects: {len(clip.effects)}")
print(f"  Custom curve points: {custom_effect.custom_curve_points}")

# Test serialization
clip_dict = clip.to_dict()
print(f"\n✅ Serialization successful")
print(f"  Effects in dict: {len(clip_dict['effects'])}")

# Test deserialization
restored_clip = Clip.from_dict(clip_dict)
print(f"✅ Deserialization successful")
print(f"  Restored effects: {len(restored_clip.effects)}")

print("\n🎉 All effect system tests passed!")
