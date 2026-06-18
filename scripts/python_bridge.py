"""
NC-KTV Python Bridge — AI Sync
===========================================================
Provides CLI commands for the C++ / WPF Karaoke timeline editor.
Commands:
  - align         : Force-align known lyrics text to audio (even distribution)
"""

import sys
import os
import json
import argparse
from pathlib import Path

def parse_line_to_words_and_syllables(line_text, language="en"):
    is_cjk = language in ["zh", "ko", "ja"]
    words = []
    
    if is_cjk:
        for char in line_text:
            if char.strip():
                words.append([char])
    else:
        raw_words = line_text.split()
        for rw in raw_words:
            parts = rw.split('-')
            word_syls = []
            for idx, part in enumerate(parts):
                if not part: continue
                if idx < len(parts) - 1:
                    word_syls.append(part + "-")
                else:
                    word_syls.append(part)
            if word_syls:
                words.append(word_syls)
    return words

def get_audio_duration(file_path):
    try:
        import wave
        with wave.open(str(file_path), "rb") as f:
            frames = f.getnframes()
            rate = f.getframerate()
            return frames / float(rate)
    except Exception:
        pass

    try:
        import subprocess
        cmd = ["ffprobe", "-v", "quiet", "-print_format", "json", "-show_format", str(file_path)]
        output = subprocess.check_output(cmd)
        data = json.loads(output)
        return float(data["format"]["duration"])
    except Exception:
        return 180.0

def run_alignment(audio_file, lyrics_text, language="en"):
    duration = get_audio_duration(audio_file)
    start_time = min(2.0, duration * 0.05)
    end_time = max(start_time + 10.0, duration * 0.95)
    total_active_dur = end_time - start_time
    
    raw_lines = [line.strip() for line in lyrics_text.strip().split("\n") if line.strip()]
    if not raw_lines:
        return {"error": "No lyrics text provided", "segments": []}
        
    parsed_lines = []
    for line_text in raw_lines:
        words = parse_line_to_words_and_syllables(line_text, language)
        if not words:
            continue
        parsed_lines.append({
            "original_text": line_text,
            "parsed_words": words
        })
        
    if not parsed_lines:
        return {"error": "No valid lyric lines parsed", "segments": []}
        
    n_lines = len(parsed_lines)
    line_duration = total_active_dur / n_lines
    
    segments = []
    for line_idx, line in enumerate(parsed_lines):
        line_start = start_time + line_idx * line_duration
        line_end = line_start + line_duration * 0.8
        
        all_syls = []
        for word in line["parsed_words"]:
            for syl in word:
                all_syls.append(syl)
                
        if not all_syls:
            continue
            
        n_syls = len(all_syls)
        syl_dur = (line_end - line_start) / n_syls
        
        line_syllables = []
        syl_counter = 0
        for word in line["parsed_words"]:
            for syl in word:
                s_start = line_start + syl_counter * syl_dur
                s_end = s_start + syl_dur
                line_syllables.append({
                    "word": syl,
                    "start": round(s_start, 3),
                    "end": round(s_end, 3)
                })
                syl_counter += 1
                
        segments.append({
            "text": line["original_text"],
            "start": round(line_syllables[0]["start"], 3),
            "end": round(line_syllables[-1]["end"], 3),
            "words": line_syllables
        })
        
    return {
        "segments": segments,
        "engine": "even-distribution-align",
        "language": language
    }

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="NC-KTV Python Bridge")
    subparsers = parser.add_subparsers(dest="command")

    align_parser = subparsers.add_parser("align")
    align_parser.add_argument("file")
    align_parser.add_argument("--lyrics", required=True)
    align_parser.add_argument("--lang", default="en")

    args = parser.parse_args()

    try:
        result = {}
        if args.command == "align":
            lyrics_text = Path(args.lyrics).read_text(encoding="utf-8")
            result = run_alignment(args.file, lyrics_text, args.lang)
        else:
            parser.print_help()
            sys.exit(1)
            
        print(json.dumps(result))
    except Exception as e:
        import traceback
        traceback.print_exc(file=sys.stderr)
        print(json.dumps({"error": str(e)}))
        sys.exit(1)