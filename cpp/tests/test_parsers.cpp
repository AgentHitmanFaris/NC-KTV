#include <gtest/gtest.h>
#include "parsers/subtitle_parser.h"
using namespace ncktv;

TEST(Parsers, SrtParsing) {
    QString content = "1\n00:00:01,000 --> 00:00:04,500\nHello world\n\n"
                      "2\n00:00:05,000 --> 00:00:08,000\nSecond line\n";
    auto data = SubtitleParser::parseSrt(content);
    EXPECT_EQ(data.lines.size(), 2);
    EXPECT_EQ(data.lines[0].text, "Hello world");
    EXPECT_DOUBLE_EQ(data.lines[0].startTime, 1.0);
    EXPECT_DOUBLE_EQ(data.lines[0].endTime, 4.5);
}

TEST(Parsers, LrcParsing) {
    QString content = "[ti:Test]\n[ar:Artist]\n[00:05.00]First line\n[00:10.00]Second line\n";
    auto data = SubtitleParser::parseLrc(content);
    EXPECT_EQ(data.title.value(), "Test");
    EXPECT_EQ(data.artist.value(), "Artist");
    EXPECT_EQ(data.lines.size(), 2);
    EXPECT_DOUBLE_EQ(data.lines[0].startTime, 5.0);
}

TEST(Parsers, FormatDetection) {
    EXPECT_EQ(SubtitleParser::detectFormat("test.srt"), SubtitleParser::Format::SRT);
    EXPECT_EQ(SubtitleParser::detectFormat("test.lrc"), SubtitleParser::Format::LRC);
    EXPECT_EQ(SubtitleParser::detectFormat("test.ass"), SubtitleParser::Format::ASS);
    EXPECT_EQ(SubtitleParser::detectFormat("test.vtt"), SubtitleParser::Format::VTT);
}
