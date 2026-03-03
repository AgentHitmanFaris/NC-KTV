#include <gtest/gtest.h>
#include "timeline/timeline_data.h"

using namespace ncktv;

TEST(TimelineData, ClipSplit) {
    Clip clip;
    clip.clipId = "c1";
    clip.startTime = 1.0;
    clip.duration = 4.0;
    auto right = clip.splitAt(3.0);
    ASSERT_TRUE(right.has_value());
    EXPECT_DOUBLE_EQ(clip.duration, 2.0);
    EXPECT_DOUBLE_EQ(right->startTime, 3.0);
    EXPECT_DOUBLE_EQ(right->duration, 2.0);
}

TEST(TimelineData, ClipSplitInvalid) {
    Clip clip;
    clip.startTime = 1.0;
    clip.duration = 4.0;
    EXPECT_FALSE(clip.splitAt(0.5).has_value());
    EXPECT_FALSE(clip.splitAt(5.0).has_value());
}

TEST(TimelineData, TrackOverlap) {
    Track track;
    Clip c1; c1.clipId = "c1"; c1.startTime = 0; c1.duration = 5;
    track.addClip(c1);
    Clip c2; c2.clipId = "c2"; c2.startTime = 3; c2.duration = 4;
    EXPECT_TRUE(track.checkOverlap(c2));
    Clip c3; c3.clipId = "c3"; c3.startTime = 5; c3.duration = 3;
    EXPECT_FALSE(track.checkOverlap(c3));
}

TEST(TimelineData, FindFreeSlot) {
    Track track;
    Clip c; c.clipId = "c1"; c.startTime = 0; c.duration = 5;
    track.addClip(c);
    EXPECT_DOUBLE_EQ(track.findNextFreeSlot(3.0, 0.0), 5.0);
}

TEST(TimelineData, JsonRoundTrip) {
    TimelineData td;
    auto& t = td.addTrack("t1", TrackType::Audio, "Audio 1");
    Clip c; c.clipId = "c1"; c.trackId = "t1"; c.startTime = 0; c.duration = 10;
    t.addClip(c);
    td.recalculateDuration();
    auto j = td.toJson();
    auto restored = TimelineData::fromJson(j);
    EXPECT_EQ(restored.tracks.size(), 1);
    EXPECT_EQ(restored.tracks[0].clips.size(), 1);
}
