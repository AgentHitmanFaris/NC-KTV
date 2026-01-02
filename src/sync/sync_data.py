"""
Lyrics synchronization data structures
"""

from dataclasses import dataclass, field
from typing import List, Optional
import json
from pathlib import Path


@dataclass
class LyricWord:
    """Single word with timing"""
    word: str
    start_time: float  # seconds
    end_time: float  # seconds
    confidence: float = 1.0  # 0-1, for auto-synced words
    

@dataclass
class LyricLine:
    """Line of lyrics with word-level timing"""
    text: str
    start_time: float  # seconds
    end_time: float  # seconds
    words: List[LyricWord] = field(default_factory=list)
    romanized_text: Optional[str] = None  # Romanized version for non-Latin scripts
    
    @property
    def duration(self) -> float:
        """Duration of the line in seconds"""
        return max(0.0, self.end_time - self.start_time)
    
    def add_word(self, word: str, start: float, end: float, confidence: float = 1.0):
        """Add a word to this line"""
        self.words.append(LyricWord(word, start, end, confidence))
    
    def to_dict(self) -> dict:
        """Convert to dictionary"""
        return {
            'text': self.text,
            'start_time': self.start_time,
            'end_time': self.end_time,
            'words': [
                {
                    'word': w.word,
                    'start_time': w.start_time,
                    'end_time': w.end_time,
                    'confidence': w.confidence
                }
                for w in self.words
            ]
        }
    
    @classmethod
    def from_dict(cls, data: dict) -> 'LyricLine':
        """Create from dictionary"""
        line = cls(
            text=data['text'],
            start_time=data['start_time'],
            end_time=data['end_time']
        )
        
        for word_data in data.get('words', []):
            line.add_word(
                word_data['word'],
                word_data['start_time'],
                word_data['end_time'],
                word_data.get('confidence', 1.0)
            )
        
        return line


@dataclass
class LyricsData:
    """Complete lyrics with synchronization info"""
    lines: List[LyricLine] = field(default_factory=list)
    title: Optional[str] = None
    artist: Optional[str] = None
    language: Optional[str] = None
    
    def add_line(self, line: LyricLine):
        """Add a lyric line"""
        self.lines.append(line)
    
    def clear(self):
        """Clear all lyrics lines"""
        self.lines = []
    
    def import_from_text(self, text: str):
        """Import lyrics from plain text (one line per line)"""
        self.lines = [LyricLine(text=line.strip(), start_time=0.0, end_time=0.0) 
                      for line in text.splitlines() if line.strip()]
    
    def get_total_duration(self) -> float:
        """Get total duration of lyrics"""
        if not self.lines:
            return 0.0
        return max(line.end_time for line in self.lines)
    
    def get_line_at_time(self, time: float) -> Optional[LyricLine]:
        """Get lyric line at specific time
        
        Args:
            time: Time in seconds
        
        Returns:
            LyricLine if found, None otherwise
        """
        for line in self.lines:
            if line.start_time <= time <= line.end_time:
                return line
        return None
    
    def to_dict(self) -> dict:
        """Convert to dictionary"""
        return {
            'title': self.title,
            'artist': self.artist,
            'language': self.language,
            'lines': [line.to_dict() for line in self.lines]
        }
    
    @classmethod
    def from_dict(cls, data: dict) -> 'LyricsData':
        """Create from dictionary"""
        lyrics = cls(
            title=data.get('title'),
            artist=data.get('artist'),
            language=data.get('language')
        )
        
        for line_data in data.get('lines', []):
            lyrics.add_line(LyricLine.from_dict(line_data))
        
        return lyrics
    
    def save(self, file_path: Path):
        """Save to JSON file"""
        file_path.parent.mkdir(parents=True, exist_ok=True)
        with open(file_path, 'w', encoding='utf-8') as f:
            json.dump(self.to_dict(), f, indent=2, ensure_ascii=False)
    
    @classmethod
    def load(cls, file_path: Path) -> 'LyricsData':
        """Load from JSON file"""
        with open(file_path, 'r', encoding='utf-8') as f:
            data = json.load(f)
        return cls.from_dict(data)
    
    def to_lrc(self) -> str:
        """Export to LRC format (simple line-level timing)"""
        lines = []
        
        # Add metadata
        if self.title:
            lines.append(f"[ti:{self.title}]")
        if self.artist:
            lines.append(f"[ar:{self.artist}]")
        
        # Add timed lyrics
        for line in self.lines:
            minutes = int(line.start_time // 60)
            seconds = line.start_time % 60
            timestamp = f"[{minutes:02d}:{seconds:05.2f}]"
            lines.append(f"{timestamp}{line.text}")
        
        return '\n'.join(lines)
    
    def to_srt(self) -> str:
        """Export to SRT subtitle format"""
        lines = []
        
        for idx, line in enumerate(self.lines, 1):
            # Format: HH:MM:SS,mmm
            start = self._format_srt_time(line.start_time)
            end = self._format_srt_time(line.end_time)
            
            lines.append(f"{idx}")
            lines.append(f"{start} --> {end}")
            lines.append(line.text)
            lines.append("")  # Blank line separator
        
        return '\n'.join(lines)
    
    @staticmethod
    def _format_srt_time(seconds: float) -> str:
        """Format time for SRT (HH:MM:SS,mmm)"""
        hours = int(seconds // 3600)
        minutes = int((seconds % 3600) // 60)
        secs = int(seconds % 60)
        millis = int((seconds % 1) * 1000)
        return f"{hours:02d}:{minutes:02d}:{secs:02d},{millis:03d}"
