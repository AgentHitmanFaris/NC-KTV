#include <gtest/gtest.h>
#include "audio/audio_clock.h"
using namespace ncktv;

TEST(AudioClock, SampleConversion) {
    AudioClock clock(48000);
    EXPECT_DOUBLE_EQ(clock.samplesToSeconds(48000), 1.0);
    EXPECT_EQ(clock.secondsToSamples(1.0), 48000);
}

TEST(AudioClock, MasterOffset) {
    AudioClock clock(44100);
    clock.setMasterOffset(0.1);
    EXPECT_DOUBLE_EQ(clock.samplesToSeconds(0), 0.1);
    EXPECT_EQ(clock.secondsToSamples(0.1), 0);
}

TEST(AudioClock, SyncPoint) {
    AudioClock clock(48000);
    clock.setLatency("playback", 0.05);
    double synced = clock.syncPoint(1.0, "playback");
    EXPECT_DOUBLE_EQ(synced, 1.05);
}

TEST(AudioClock, JsonRoundTrip) {
    AudioClock clock(44100);
    clock.setMasterOffset(0.025);
    clock.setLatency("uvr", 0.01);
    auto j = clock.toJson();
    auto restored = AudioClock::fromJson(j);
    EXPECT_EQ(restored.sampleRate(), 44100);
    EXPECT_DOUBLE_EQ(restored.masterOffset(), 0.025);
    EXPECT_DOUBLE_EQ(restored.getLatency("uvr"), 0.01);
}
