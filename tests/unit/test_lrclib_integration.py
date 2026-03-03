import pytest
from unittest.mock import MagicMock, patch
from pathlib import Path
import sys
import os

# Add src to path
sys.path.insert(0, str(Path(__file__).parents[2] / 'src'))

from utils.lrclib_client import LRCLibClient, parse_lrc_to_core_lyrics
from workers.online_search_worker import OnlineSearchWorker
from core.lyrics import LyricsData

MOCK_SEARCH_RESULT = [
    {
        "id": 123,
        "trackName": "Test Song",
        "artistName": "Test Artist",
        "albumName": "Test Album",
        "duration": 180.0,
        "syncedLyrics": "[00:10.00] Line 1\n[00:20.00] Line 2"
    }
]

def test_lrclib_client_search():
    client = LRCLibClient()
    with patch('requests.Session.get') as mock_get:
        mock_response = MagicMock()
        mock_response.json.return_value = MOCK_SEARCH_RESULT
        mock_response.raise_for_status.return_value = None
        mock_get.return_value = mock_response
        
        results = client.search("Test Song")
        assert len(results) == 1
        assert results[0]['trackName'] == "Test Song"
        
        # Verify call args
        args, kwargs = mock_get.call_args
        assert kwargs['params']['q'] == "Test Song"

def test_parse_lrc():
    lrc = "[00:01.50] Hello World\n[00:03.00] Goodbye"
    lyrics = parse_lrc_to_core_lyrics(lrc)
    
    assert isinstance(lyrics, LyricsData)
    assert len(lyrics.lines) == 2
    
    line1 = lyrics.lines[0]
    assert line1.text == "Hello World"
    assert line1.start_time == 1.5
    assert line1.end_time == 3.0
    
    line2 = lyrics.lines[1]
    assert line2.text == "Goodbye"
    assert line2.start_time == 3.0
    # Custom logic: end time defaults to start + 5.0 if last line
    assert line2.end_time == 8.0 

def test_online_search_worker(qtbot):
    # Mock dependencies
    with patch('workers.online_search_worker.AudioProcessor') as MockAP, \
         patch('workers.online_search_worker.LRCLibClient') as MockClient:
        
        # Setup mocks
        mock_ap_instance = MockAP.return_value
        mock_ap_instance.get_duration.return_value = 180.0
        
        mock_client_instance = MockClient.return_value
        mock_client_instance.search.return_value = MOCK_SEARCH_RESULT
        
        # Create worker
        # We need a dummy file that 'exists' for the initial check, 
        # but we are mocking run() logic or parts of it?
        # The worker checks self.audio_file.exists().
        # We can mock Path.exists or just use a real temporary file.
        
        test_file = Path("test_audio.mp3")
        
        # Patch Path.exists to return True
        with patch('pathlib.Path.exists', return_value=True):
             worker = OnlineSearchWorker(test_file, query_override="Test Song")
             # qtbot.add_widget(worker) # Removed: QThread is not a widget
             
             with qtbot.waitSignal(worker.finished, timeout=1000) as blocker:
                 worker.start()
                 
             lyrics_data = blocker.args[0]
             assert lyrics_data.metadata['title'] == "Test Song"
             assert len(lyrics_data.lines) == 2
             
             # Verify search was called with query override
             mock_client_instance.search.assert_called_with("Test Song")

def test_online_search_worker_filename_parsing(qtbot):
    """Test standard filename parsing"""
    with patch('workers.online_search_worker.AudioProcessor') as MockAP, \
         patch('workers.online_search_worker.LRCLibClient') as MockClient, \
         patch('pathlib.Path.exists', return_value=True):
        
        mock_ap_instance = MockAP.return_value
        mock_ap_instance.get_duration.return_value = 180.0
        
        mock_client_instance = MockClient.return_value
        mock_client_instance.search.return_value = MOCK_SEARCH_RESULT
        
        # Filename: "Artist - Title (Official Video).mp3"
        # Expected Query: "Artist Title"
        
        # We need to mock audio_file.stem
        # Since we pass a path object, we can't easily mock stem on the real object unless we subclass or mock Path entirely.
        # But we can just pass a string to Path() and rely on logic.
        # "Artist - Title (Official Video).mp3" stem is "Artist - Title (Official Video)"
        
        path_str = "Artist - Title (Official Video).mp3"
        worker = OnlineSearchWorker(path_str)
        
        # We catch the failure or success. Logic says it calls search().
        # We don't need to run the full thread, just test the run logic if possible, 
        # but running it is better integration test.
        
        with qtbot.waitSignal(worker.finished, timeout=1000):
            worker.start()
            
        # Check call args
        # "Artist - Title (Official Video)" -> "Artist Title"
        mock_client_instance.search.assert_called_with("Artist Title")

