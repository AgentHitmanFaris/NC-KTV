#pragma once
#include "subtitle_parser.h"
namespace ncktv {
class SrtParser {
public:
    static LyricsData parse(const QString& filePath) { return SubtitleParser::parseFile(filePath); }
    static LyricsData parseContent(const QString& content) { return SubtitleParser::parseSrt(content); }
};
} // namespace ncktv
