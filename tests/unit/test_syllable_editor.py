
import pytest
from PyQt6.QtWidgets import QApplication
from PyQt6.QtGui import QPainter, QImage, QColor
from PyQt6.QtCore import QPoint
import sys
from pathlib import Path

# Add src to path if needed (tests usually handle this, but for safety)
# sys.path.append(str(Path(__file__).parent.parent.parent / "src"))

from gui.components.syllable_editor_widget import SyllableEditorWidget
from core.lyrics import LyricsData, LyricsLine, LyricsToken

# Fixture for QApplication is often handled by pytest-qt but we'll do a simple one
@pytest.fixture(scope="session")
def qapp():
    app = QApplication.instance()
    if app is None:
        app = QApplication([])
    yield app

def test_syllable_editor_init(qapp):
    """Test initialization of the widget"""
    editor = SyllableEditorWidget()
    assert editor is not None
    assert editor.pixels_per_second == 100.0
    assert editor.lyrics_data is None

def test_syllable_editor_set_data_auto_tokenization(qapp):
    """Test that set_data automatically tokenizes lines if tokens are missing"""
    editor = SyllableEditorWidget()
    
    data = LyricsData()
    # Create a line with explicit timing but no tokens
    line = LyricsLine(text="Hello World", start_time=0.0, end_time=1.0)
    data.lines.append(line)
    
    waveform = [0.1, 0.2, 0.1]
    duration = 1000 # 1s
    
    editor.set_data(data, waveform, duration)
    
    # Check if tokens were created
    assert len(line.tokens) == 2
    assert line.tokens[0].text == "Hello"
    assert line.tokens[1].text == "World"
    
    # Check timings: 1.0s duration / 2 words = 0.5s each
    assert line.tokens[0].start_time == 0.0
    assert line.tokens[0].end_time == 0.5
    assert line.tokens[1].start_time == 0.5
    assert line.tokens[1].end_time == 1.0
    
    assert editor.waveform_data == waveform
    assert editor.waveform_duration == duration

def test_syllable_editor_set_data_no_overwrite(qapp):
    """Test that tokens are NOT overwritten if they already exist"""
    editor = SyllableEditorWidget()
    
    data = LyricsData()
    line = LyricsLine(text="Hello World", start_time=0.0, end_time=1.0)
    # Manually add tokens
    t1 = LyricsToken("Hello", 0.0, 0.8)
    t2 = LyricsToken("World", 0.8, 1.0)
    line.tokens = [t1, t2]
    
    data.lines.append(line)
    
    editor.set_data(data, [], 1000)
    
    assert len(line.tokens) == 2
    assert line.tokens[0].end_time == 0.8 # Preserved
    assert line.tokens[1].start_time == 0.8 # Preserved

def test_draw_waveform_integrity(qapp):
    """Test _draw_waveform runs without error using QPainter on QImage"""
    editor = SyllableEditorWidget()
    # Set dummy data
    waveform = [0.0, 0.5, 1.0, 0.5, 0.0] * 10
    editor.set_data(LyricsData(), waveform, 1000)
    
    # Prepare canvas
    canvas = editor.canvas
    canvas.resize(200, 200)
    
    # Render
    image = QImage(200, 200, QImage.Format.Format_ARGB32)
    image.fill(0)
    painter = QPainter(image)
    
    try:
        # Call the method we implemented
        canvas._draw_waveform(painter)
    finally:
        painter.end()
    
    # We can't easily assert pixel values without robust ground truth, 
    # but lack of crash is success for 'Fix anything' context.

def test_canvas_size_update(qapp):
    """Test that canvas resizes based on content duration"""
    editor = SyllableEditorWidget()
    
    data = LyricsData()
    # Line ends at 10s
    line = LyricsLine(text="End", start_time=9.0, end_time=10.0)
    data.lines.append(line)
    
    # Waveform duration 20s
    editor.set_data(data, [], 20000)
    
    pps = editor.pixels_per_second # 100
    
    # Expected width: 20s * 100 = 2000 (roughly)
    # The code takes max(waveform_duration, last_lyric_end + 5)
    # max(20, 15) = 20.
    
    expected_width = int(20.0 * 100.0)
    assert editor.canvas.width() == expected_width
    
    # Zoom
    editor._on_zoom_changed(200)
    assert editor.pixels_per_second == 200.0
    expected_width_zoomed = int(20.0 * 200.0)
    assert editor.canvas.width() == expected_width_zoomed
