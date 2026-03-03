import requests
import urllib.parse
from typing import List, Optional, Dict, Any
from core.lyrics import LyricsData, LyricsLine

class LRCLibClient:
    """
    Client for lrclib.net API
    Documentation: https://lrclib.net/docs
    """
    
    BASE_URL = "https://lrclib.net/api"
    USER_AGENT = "NC-KTV/1.0 (https://github.com/nc-ktv)"

    def __init__(self, timeout: int = 10):
        self.session = requests.Session()
        self.session.headers.update({"User-Agent": self.USER_AGENT})
        self.timeout = timeout

    def search(self, query: str) -> List[Dict[str, Any]]:
        """
        Search for lyrics by query (track name, artist, etc.)
        """
        params = {"q": query}
        try:
            response = self.session.get(f"{self.BASE_URL}/search", params=params, timeout=self.timeout)
            response.raise_for_status()
            return response.json()
        except requests.RequestException as e:
            print(f"Error searching lyrics: {e}")
            return []

    def get_by_id(self, track_id: int) -> Optional[Dict[str, Any]]:
        """
        Get lyrics by LRCLIB track ID
        """
        try:
            response = self.session.get(f"{self.BASE_URL}/get/{track_id}", timeout=self.timeout)
            response.raise_for_status()
            return response.json()
        except requests.RequestException as e:
            print(f"Error getting lyrics by ID: {e}")
            return None

    def get_by_signature(self, track_name: str, artist_name: str, album_name: str, duration: float) -> Optional[Dict[str, Any]]:
        """
        Get lyrics by track signature (best match)
        """
        params = {
            "track_name": track_name,
            "artist_name": artist_name,
            "album_name": album_name,
            "duration": duration
        }
        try:
            response = self.session.get(f"{self.BASE_URL}/get", params=params, timeout=self.timeout)
            response.raise_for_status()
            return response.json()
        except requests.RequestException as e:
            print(f"Error getting lyrics by signature: {e}")
            return None

def parse_lrc_to_core_lyrics(lrc_text: str) -> LyricsData:
    """
    Convert LRC text content to core.lyrics.LyricsData
    """
    lyrics_data = LyricsData()
    if not lrc_text:
        return lyrics_data

    import re
    # Regex for standard LRC timestamp: [mm:ss.xx] or [mm:ss.xxx]
    timestamp_pattern = re.compile(r'\[(\d{1,2}):(\d{1,2}(?:\.\d{1,3})?)\](.*)')
    
    lines = lrc_text.splitlines()
    parsed_lines = []

    for line in lines:
        line = line.strip()
        if not line:
            continue
            
        # Parse metadata (simple check)
        if line.startswith('[ti:'):
            lyrics_data.metadata['title'] = line[4:-1]
            continue
        elif line.startswith('[ar:'):
            lyrics_data.metadata['artist'] = line[4:-1]
            continue
        elif line.startswith('[al:'):
            lyrics_data.metadata['album'] = line[4:-1]
            continue
            
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
    
    # Convert to LyricsLine objects
    for i, item in enumerate(parsed_lines):
        start_time = item['time']
        text = item['text']
        
        # Estimate end time
        if i < len(parsed_lines) - 1:
            end_time = parsed_lines[i + 1]['time']
        else:
            end_time = start_time + 5.0  # Default duration for last line
            
        # Cap max duration if gap is too large
        if end_time - start_time > 10.0:
            end_time = start_time + 8.0
            
        # Add to lyrics data
        # LyricsLine(text, start, end)
        lyrics_data.add_line(text, start_time, end_time)
        
    return lyrics_data
