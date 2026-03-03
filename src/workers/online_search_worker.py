from PyQt6.QtCore import QThread, pyqtSignal
from pathlib import Path
import re
from utils.lrclib_client import LRCLibClient, parse_lrc_to_core_lyrics
from core.audio_processor import AudioProcessor
from core.lyrics import LyricsData

class OnlineSearchWorker(QThread):
    """
    Worker thread for searching and downloading lyrics from lrclib.net
    """
    finished = pyqtSignal(LyricsData)  # Emits parsed LyricsData
    error_occurred = pyqtSignal(str)
    
    def __init__(self, audio_file: Path, query_override: str = None):
        super().__init__()
        self.audio_file = Path(audio_file)
        self.query_override = query_override
        self.client = LRCLibClient()
        self.processor = AudioProcessor()
        
    def run(self):
        try:
            # 1. Get Duration
            if not self.audio_file.exists():
                raise FileNotFoundError(f"Audio file not found: {self.audio_file}")
                
            # Use AudioProcessor to get duration
            # Note: This might block for a moment if it runs ffprobe
            duration = self.processor.get_duration(self.audio_file)
            
            # 2. Determine Query
            if self.query_override:
                query = self.query_override
            else:
                # Use filename stem
                # Clean up known patterns
                # e.g. "Artist - Title", "Title (Official Video)" etc.
                raw_stem = self.audio_file.stem
                
                # Remove (...) and [...] 
                clean_stem = re.sub(r'\(.*?\)', '', raw_stem)
                clean_stem = re.sub(r'\[.*?\]', '', clean_stem)
                
                # Replace underscores/dashes with spaces
                clean_stem = clean_stem.replace('_', ' ').replace('-', ' ')
                
                # Remove extra spaces
                query = re.sub(r'\s+', ' ', clean_stem).strip()
            
            if not query:
                self.error_occurred.emit("Could not determine search query from filename.")
                return

            # 3. Search
            results = self.client.search(query)
            
            if not results:
                self.error_occurred.emit(f"No lyrics found for '{query}'")
                return
                
            # 4. Find Best Match
            best_match = None
            min_diff = float('inf')
            
            for res in results:
                # Check if it has synced lyrics
                if not res.get('syncedLyrics'):
                    continue
                    
                res_duration = res.get('duration', 0)
                diff = abs(res_duration - duration)
                
                if diff < min_diff:
                    min_diff = diff
                    best_match = res
            
            # Tolerance: 10 seconds?
            if best_match and min_diff < 15.0:
                lrc_text = best_match['syncedLyrics']
                lyrics_data = parse_lrc_to_core_lyrics(lrc_text)
                
                # Add metadata if missing
                if not lyrics_data.metadata.get('title'):
                    lyrics_data.metadata['title'] = best_match.get('trackName')
                if not lyrics_data.metadata.get('artist'):
                    lyrics_data.metadata['artist'] = best_match.get('artistName')
                if not lyrics_data.metadata.get('album'):
                    lyrics_data.metadata['album'] = best_match.get('albumName')
                
                self.finished.emit(lyrics_data)
            else:
                msg = f"No matching synced lyrics found for '{query}'."
                if min_diff != float('inf'):
                    msg += f" (Closest match duration difference: {min_diff:.1f}s)"
                self.error_occurred.emit(msg)
                
        except Exception as e:
            import traceback
            traceback.print_exc()
            self.error_occurred.emit(str(e))
