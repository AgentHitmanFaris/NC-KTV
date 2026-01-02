"""
Quick validation for timeline clip manipulation
"""
import sys
sys.path.insert(0, 'src')

from core.timeline_data import Clip, Track, TrackType, Effect, EffectType, EasingCurve

print("✅ Timeline data import successful")

# Test clip manipulation
clip = Clip(
    clip_id="test_clip",
    track_id="test_track",
    start_time=5.0,
    duration=10.0
)

print(f"Initial clip: start={clip.start_time}s, duration={clip.duration}s, end={clip.end_time}s")

# Test move
clip.move_to(8.0)
assert clip.start_time == 8.0, "Move failed"
print(f"✅ Move to 8.0s: start={clip.start_time}s")

# Test resize
clip.resize(12.0)
assert clip.duration == 12.0, "Resize failed"
print(f"✅ Resize to 12.0s: duration={clip.duration}s, end={clip.end_time}s")

# Test split
clip2 = Clip(
    clip_id="split_test",
    track_id="test_track",
    start_time=0.0,
    duration=10.0
)
right_clip = clip2.split_at(6.0)
assert clip2.duration == 6.0, "Left clip duration wrong"
assert right_clip.start_time == 6.0, "Right clip start wrong"
assert right_clip.duration == 4.0, "Right clip duration wrong"
print(f"✅ Split at 6.0s: left={clip2.duration}s, right={right_clip.duration}s")

# Test effect handling
effect = Effect(
    effect_type=EffectType.FADE_IN,
    start_time=0.0,
    duration=2.0,
    easing=EasingCurve.EASE_IN
)
clip.add_effect(effect)
assert len(clip.effects) == 1, "Effect not added"
print(f"✅ Add effect: {len(clip.effects)} effects on clip")

# Test overlap detection
track = Track(
    track_id="test_track",
    track_type=TrackType.AUDIO,
    name="Test Track"
)
clip_a = Clip("a", "test_track", 0.0, 5.0)
clip_b = Clip("b", "test_track", 3.0, 5.0)  # Overlaps with A
clip_c = Clip("c", "test_track", 10.0, 5.0)  # No overlap

track.add_clip(clip_a)
assert track.check_overlap(clip_b) == True, "Overlap detection failed"
assert track.check_overlap(clip_c) == False, "No overlap should be detected"
print(f"✅ Overlap detection working correctly")

print("\n🎉 All timeline manipulation tests passed!")
