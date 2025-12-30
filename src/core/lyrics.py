"""
Core lyrics data structures for NC-KTV
"""

from dataclasses import dataclass, field
from typing import List, Optional
import json
from pathlib import Path


@dataclass
class LyricsLine:
    """Represents a single line of lyrics"""
    text: str
    start_time: float = 0.0  # Seconds
    end_time: float = 0.0    # Seconds
    tokens: List['LyricsToken'] = field(default_factory=list)
    
    @property
    def duration(self) -> float:
        """Duration of the line in seconds"""
        return max(0.0, self.end_time - self.start_time)
    
    def to_dict(self) -> dict:
        """Convert to dictionary"""
        return {
            'text': self.text,
            'start_time': self.start_time,
            'end_time': self.end_time,
            'tokens': [t.to_dict() for t in self.tokens]
        }
    
    @classmethod
    def from_dict(cls, data: dict) -> 'LyricsLine':
        """Create from dictionary"""
        line = cls(
            text=data.get('text', ''),
            start_time=data.get('start_time', 0.0),
            end_time=data.get('end_time', 0.0)
        )
        if 'tokens' in data:
            line.tokens = [LyricsToken.from_dict(t) for t in data['tokens']]
        return line


@dataclass
class LyricsToken:
    """Represents a single token (word/syllable) within a line"""
    text: str
    start_time: float = 0.0
    end_time: float = 0.0
    
    def to_dict(self) -> dict:
        return {
            'text': self.text,
            'start_time': self.start_time,
            'end_time': self.end_time
        }
    
    @classmethod
    def from_dict(cls, data: dict) -> 'LyricsToken':
        return cls(
            text=data.get('text', ''),
            start_time=data.get('start_time', 0.0),
            end_time=data.get('end_time', 0.0)
        )


class LyricsData:
    """Manages a collection of lyrics lines"""
    
    def __init__(self):
        self.lines: List[LyricsLine] = []
        self.metadata: dict = {}
    
    def add_line(self, text: str, start: float = 0.0, end: float = 0.0):
        """Add a new line"""
        self.lines.append(LyricsLine(text, start, end))
    
    def clear(self):
        """Clear all lyrics"""
        self.lines = []
    
    def import_from_text(self, text: str):
        """Import lyrics from plain text (one line per line)"""
        self.lines = [LyricsLine(line.strip()) for line in text.splitlines() if line.strip()]
    
    def save_to_json(self, path: Path):
        """Save to JSON file"""
        data = {
            'metadata': self.metadata,
            'lines': [line.to_dict() for line in self.lines]
        }
        with open(path, 'w', encoding='utf-8') as f:
            json.dump(data, f, indent=2, ensure_ascii=False)
            
    def load_from_json(self, path: Path):
        """Load from JSON file"""
        if not path.exists():
            return
            
        with open(path, 'r', encoding='utf-8') as f:
            data = json.load(f)
            
        self.metadata = data.get('metadata', {})
        self.lines = [LyricsLine.from_dict(l) for l in data.get('lines', [])]
