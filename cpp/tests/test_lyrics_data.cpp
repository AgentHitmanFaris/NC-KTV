#include <gtest/gtest.h>
#include "lyrics/lyrics_data.h"

using namespace ncktv;

TEST(LyricsData, CreateWord) {
    LyricWord w{"hello", 1.0, 2.0, 0.95};
    EXPECT_EQ(w.word, "hello");
    EXPECT_DOUBLE_EQ(w.duration(), 1.0);
}

TEST(LyricsData, CreateLine) {
    LyricLine line;
    line.text = "Hello world";
    line.startTime = 0.0;
    line.endTime = 3.0;
    line.addWord("Hello", 0.0, 1.5);
    line.addWord("world", 1.5, 3.0);
    EXPECT_EQ(line.words.size(), 2);
    EXPECT_DOUBLE_EQ(line.duration(), 3.0);
}

TEST(LyricsData, JsonRoundTrip) {
    LyricsData data;
    data.title = "Test Song";
    data.artist = "Test Artist";
    LyricLine line;
    line.text = "Hello world";
    line.startTime = 1.0;
    line.endTime = 4.0;
    data.addLine(line);

    auto j = data.toJson();
    auto restored = LyricsData::fromJson(j);
    EXPECT_EQ(restored.title.value(), "Test Song");
    EXPECT_EQ(restored.lines.size(), 1);
    EXPECT_DOUBLE_EQ(restored.lines[0].startTime, 1.0);
}

TEST(LyricsData, LrcExport) {
    LyricsData data;
    data.title = "Test";
    LyricLine line;
    line.text = "Hello";
    line.startTime = 65.5;  // 1:05.50
    data.addLine(line);
    QString lrc = data.toLrc();
    EXPECT_TRUE(lrc.contains("[01:05.50]Hello"));
}

TEST(LyricsData, SrtExport) {
    LyricsData data;
    LyricLine line;
    line.text = "Test line";
    line.startTime = 3661.5;  // 1:01:01,500
    line.endTime = 3665.0;
    data.addLine(line);
    QString srt = data.toSrt();
    EXPECT_TRUE(srt.contains("01:01:01,500"));
}
