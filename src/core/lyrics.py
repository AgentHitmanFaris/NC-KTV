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
    romanized_text: Optional[str] = None  # Romanized version for non-Latin scripts
    
    @property
    def duration(self) -> float:
        """Duration of the line in seconds"""
        return max(0.0, self.end_time - self.start_time)
    
    def to_dict(self) -> dict:
        """Convert to dictionary"""
        data = {
            'text': self.text,
            'start_time': self.start_time,
            'end_time': self.end_time,
            'tokens': [t.to_dict() for t in self.tokens]
        }
        if self.romanized_text:
            data['romanized_text'] = self.romanized_text
        return data
    
    @classmethod
    def from_dict(cls, data: dict) -> 'LyricsLine':
        """Create from dictionary"""
        line = cls(
            text=data.get('text', ''),
            start_time=data.get('start_time', 0.0),
            end_time=data.get('end_time', 0.0),
            romanized_text=data.get('romanized_text')
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
    romanized_text: Optional[str] = None  # Romanized version
    
    def to_dict(self) -> dict:
        data = {
            'text': self.text,
            'start_time': self.start_time,
            'end_time': self.end_time
        }
        if self.romanized_text:
            data['romanized_text'] = self.romanized_text
        return data
    
    @classmethod
    def from_dict(cls, data: dict) -> 'LyricsToken':
        return cls(
            text=data.get('text', ''),
            start_time=data.get('start_time', 0.0),
            end_time=data.get('end_time', 0.0),
            romanized_text=data.get('romanized_text')
        )


class LyricsData:
    """Manages a collection of lyrics lines"""
    
    def __init__(self):
        self.lines: List[LyricsLine] = []
        self.metadata: dict = {}
    
    def add_line(self, text: str, start: float = 0.0, end: float = 0.0, tokens: List[dict] = None):
        """Add a new line"""
        line = LyricsLine(text, start, end)
        if tokens:
            # Convert dict tokens to LyricsToken objects
            line.tokens = [
                LyricsToken(
                    text=t.get('word', t.get('text', '')).strip(),
                    start_time=t['start'],
                    end_time=t['end']
                ) for t in tokens
            ]
        self.lines.append(line)
    
    def clear(self):
        """Clear all lyrics"""
        self.lines = []
    
    def import_from_text(self, text: str):
        """Import lyrics from plain text (one line per line)"""
        self.lines = [LyricsLine(line.strip()) for line in text.splitlines() if line.strip()]
    
    def to_dict(self) -> dict:
        """Convert to dictionary"""
        return {
            'metadata': self.metadata,
            'lines': [line.to_dict() for line in self.lines]
        }
    
    @classmethod
    def from_dict(cls, data: dict) -> 'LyricsData':
        """Create from dictionary"""
        lyrics = cls()
        lyrics.metadata = data.get('metadata', {})
        lyrics.lines = [LyricsLine.from_dict(l) for l in data.get('lines', [])]
        return lyrics

    def save_to_json(self, path: Path):
        """Save to JSON file"""
        # Use to_dict for consistency
        data = self.to_dict()
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
