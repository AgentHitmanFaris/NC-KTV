"""
LRC Lyrics Format Parser
Supports importing lyrics from .lrc files
"""

import re
import sys
from pathlib import Path
from typing import List, Optional

# Add parent directory to path for imports
sys.path.insert(0, str(Path(__file__).parent.parent))

from sync.sync_data import LyricsData, LyricLine


def parse_lrc_timestamp(timestamp: str) -> float:
    """
    Parse LRC timestamp to seconds
    Format: [mm:ss.xx] or [mm:ss.xxx]
    """
    try:
        # Remove brackets
        timestamp = timestamp.strip('[]')
        parts = timestamp.split(':')
        
        minutes = int(parts[0])
        seconds = float(parts[1])
        
        return minutes * 60 + seconds
    except (ValueError, IndexError):
        return 0.0


def parse_lrc_file(file_path: Path) -> LyricsData:
    """
    Parse LRC file into LyricsData
    
    Args:
        file_path: Path to .lrc file
        
    Returns:
        LyricsData object
    """
    lyrics_data = LyricsData()
    
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            lines = f.readlines()
            
        # Regex for standard LRC timestamp: [mm:ss.xx] or [mm:ss.xxx]
        timestamp_pattern = re.compile(r'\[(\d{2}):(\d{2}\.\d{2,3})\](.*)')
        
        metadata = {}
        parsed_lines = []
        
        for line in lines:
            line = line.strip()
            if not line:
                continue
                
            # Check for metadata tags [key:value]
            # e.g. [ti:Title], [ar:Artist]
            meta_match = re.match(r'\[([a-zA-Z]+):(.*)\]', line)
            if meta_match:
                key = meta_match.group(1).lower()
                value = meta_match.group(2).strip()
                metadata[key] = value
                
                if key == 'ti':
                    lyrics_data.title = value
                elif key == 'ar':
                    lyrics_data.artist = value
                elif key == 'la':
                    lyrics_data.language = value
                continue
            
            # Check for lyrics line
            match = timestamp_pattern.match(line)
            if match:
                minutes = int(match.group(1))
                seconds = float(match.group(2))
                text = match.group(3).strip()
                timestamp = minutes * 60 + seconds
                
                parsed_lines.append({
                    'time': timestamp,
                    'text': text
                })
        
        if not parsed_lines:
            return lyrics_data
            
        # Sort by time
        parsed_lines.sort(key=lambda x: x['time'])
        
        # Convert to LyricLine objects
        for i, item in enumerate(parsed_lines):
            start_time = item['time']
            text = item['text']
            
            # Predict end time (start of next line, or +3s for last line)
            if i < len(parsed_lines) - 1:
                end_time = parsed_lines[i + 1]['time']
            else:
                end_time = start_time + 3.0  # Default duration for last line
                
            # If gap is too long (e.g. instrumental break), cap duration
            if end_time - start_time > 10.0:
                 end_time = start_time + 5.0
            
            # Don't add empty lines unless they signify breaks (optional logic)
            if text:
                # Use LyricsLine from sync_data (needs start, end, text)
                # Note: sync_data.LyricLine expects (text, start_time, end_time)
                # But let's check init signature
                lyric_line = LyricLine(text=text, start_time=start_time, end_time=end_time)
                lyrics_data.add_line(lyric_line)
                
        return lyrics_data
        
    except Exception as e:
        import logging
        logging.getLogger(__name__).error(f"Failed to parse LRC file: {e}")
        return lyrics_data
