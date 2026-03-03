#pragma once
/*
 * NC-KTV Core — LRC Parser
 * Thin wrapper around SubtitleParser::parseLrc for compatibility
 */

#include "subtitle_parser.h"

namespace ncktv {

class LrcParser {
public:
    static LyricsData parse(const QString& filePath) {
        return SubtitleParser::parseFile(filePath);
    }
    static LyricsData parseContent(const QString& content) {
        return SubtitleParser::parseLrc(content);
    }
};

} // namespace ncktv
