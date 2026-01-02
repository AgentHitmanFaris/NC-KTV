"""
SRT (SubRip) Subtitle Format Parser and Exporter
Supports importing/exporting lyrics in SRT format
"""

import re
import sys
from pathlib import Path
from typing import List

# Add parent directory to path for imports
sys.path.insert(0, str(Path(__file__).parent.parent))

from sync.sync_data import LyricsData, LyricLine


def parse_srt_timestamp(timestamp: str) -> float:
    """
    Parse SRT timestamp to seconds
    Format: HH:MM:SS,mmm
    Example: 00:01:23,456 -> 83.456
    """
    # Match HH:MM:SS,mmm or HH:MM:SS.mmm
    match = re.match(r'(\d{2}):(\d{2}):(\d{2})[,.](\d{3})', timestamp)
    if not match:
        return 0.0
    
    hours, minutes, seconds, milliseconds = match.groups()
    
    total_seconds = (
        int(hours) * 3600 +
        int(minutes) * 60 +
        int(seconds) +
        int(milliseconds) / 1000.0
    )
    
    return total_seconds


def format_srt_timestamp(seconds: float) -> str:
    """
    Format seconds to SRT timestamp
    Example: 83.456 -> 00:01:23,456
    """
    hours = int(seconds // 3600)
    seconds %= 3600
    minutes = int(seconds // 60)
    seconds %= 60
    secs = int(seconds)
    milliseconds = int((seconds - secs) * 1000)
    
    return f"{hours:02d}:{minutes:02d}:{secs:02d},{milliseconds:03d}"


def import_srt(file_path: Path) -> LyricsData:
    """
    Import lyrics from SRT subtitle file
    
    SRT Format:
    1
    00:00:01,000 --> 00:00:04,000
    First subtitle line
    
    2
    00:00:05,000 --> 00:00:08,000
    Second subtitle line
    
    Args:
        file_path: Path to SRT file
    
    Returns:
        LyricsData with imported lyrics
    """
    lyrics_data = LyricsData()
    
    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # Normalize line endings (Windows uses \r\n)
    content = content.replace('\r\n', '\n').replace('\r', '\n')
    
    # Split by double newline to get subtitle blocks
    blocks = re.split(r'\n\n+', content.strip())
    
    for block in blocks:
        lines = block.strip().split('\n')
        
        if len(lines) < 2:
            continue  # Need at least timestamp + text
        
        # Find the timestamp line (contains -->)
        timestamp_idx = -1
        for i, line in enumerate(lines):
            if '-->' in line:
                timestamp_idx = i
                break
        
        if timestamp_idx == -1:
            continue  # No timestamp found
        
        timestamp_line = lines[timestamp_idx]
        text_lines = lines[timestamp_idx + 1:]  # Everything after timestamp is text
        
        # Parse timestamps
        match = re.match(r'([\d:,\.]+)\s*-->\s*([\d:,\.]+)', timestamp_line)
        if not match:
            continue
        
        start_str, end_str = match.groups()
        start_time = parse_srt_timestamp(start_str)
        end_time = parse_srt_timestamp(end_str)
        
        # Join multi-line text
        text = ' '.join(line.strip() for line in text_lines if line.strip())
        
        # Remove HTML tags if present (some SRT files have them)
        text = re.sub(r'<[^>]+>', '', text)
        
        # Skip empty text
        if not text:
            continue
        
        # Create lyrics line
        line = LyricLine(
            text=text,
            start_time=start_time,
            end_time=end_time
        )
        
        lyrics_data.lines.append(line)
    
    return lyrics_data


def export_srt(lyrics_data: LyricsData, output_path: Path):
    """
    Export lyrics to SRT subtitle file
    
    Args:
        lyrics_data: LyricsData to export
        output_path: Path to save SRT file
    """
    with open(output_path, 'w', encoding='utf-8') as f:
        for i, line in enumerate(lyrics_data.lines, start=1):
            # Index
            f.write(f"{i}\n")
            
            # Timestamps
            start_str = format_srt_timestamp(line.start_time)
            end_str = format_srt_timestamp(line.end_time)
            f.write(f"{start_str} --> {end_str}\n")
            
            # Text
            f.write(f"{line.text}\n")
            
            # Blank line separator
            f.write("\n")


# Example usage:
if __name__ == "__main__":
    # Test SRT parsing
    test_srt = """1
00:00:01,000 --> 00:00:04,000
First line of lyrics

2
00:00:05,000 --> 00:00:08,000
Second line of lyrics

3
00:00:09,500 --> 00:00:12,300
Third line of lyrics
"""
    
    # Write test file
    test_file = Path("test.srt")
    test_file.write_text(test_srt, encoding='utf-8')
    
    # Import
    lyrics = import_srt(test_file)
    print(f"Imported {len(lyrics.lines)} lines:")
    for line in lyrics.lines:
        print(f"  {line.start_time:.2f}s - {line.end_time:.2f}s: {line.text}")
    
    # Export
    export_srt(lyrics, Path("test_export.srt"))
    print("\nExported to test_export.srt")
    
    # Cleanup
    test_file.unlink()
