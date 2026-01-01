"""
Waveform Visualization Widget for NC-KTV
Displays audio waveform with playback cursor
"""

from PyQt6.QtWidgets import QWidget
from PyQt6.QtCore import Qt, pyqtSignal, QPointF
from PyQt6.QtGui import QPainter, QColor, QPen, QPainterPath
from pathlib import Path
import numpy as np
import logging

logger = logging.getLogger(__name__)


class WaveformWidget(QWidget):
    """Widget to display audio waveform"""
    
    # Signal emitted when user clicks on waveform to seek
    seek_requested = pyqtSignal(int)  # Position in milliseconds
    
    def __init__(self, parent=None):
        super().__init__(parent)
        
        self.waveform_data = None  # numpy array of amplitude samples
        self.duration_ms = 0
        self.position_ms = 0
        
        # Visual settings
        self.background_color = QColor(30, 30, 30)
        self.waveform_color = QColor(100, 200, 255, 180)
        self.cursor_color = QColor(255, 200, 0)
        
        self.setMinimumHeight(80)
        self.setMaximumHeight(120)
        
    def load_audio(self, file_path: Path):
        """Load audio file and extract waveform data (asynchronously)"""
        # Start loading in background thread to avoid blocking UI
        from PyQt6.QtCore import QThread, pyqtSignal
        
        class WaveformLoader(QThread):
            loaded = pyqtSignal(object, int)  # waveform_data, duration_ms
            
            def __init__(self, path):
                super().__init__()
                self.path = path
                
            def run(self):
                try:
                    import librosa
                    logger.info(f"Loading waveform from: {self.path}")
                    
                    # Load audio with librosa (auto-resamples to 22050 Hz by default)
                    y, sr = librosa.load(str(self.path), sr=None, mono=True)
                    
                    # Downsample waveform for visualization (approx 2000 samples)
                    target_samples = 2000
                    hop_length = max(1, len(y) // target_samples)
                    
                    # Extract envelope using RMS
                    waveform_data = librosa.feature.rms(y=y, frame_length=hop_length*4, hop_length=hop_length)[0]
                    
                    # Normalize to [-1, 1]
                    if waveform_data.max() > 0:
                        waveform_data = waveform_data / waveform_data.max()
                    
                    # Get duration
                    duration_ms = int((len(y) / sr) * 1000)
                    
                    logger.info(f"Waveform loaded: {len(waveform_data)} samples, {duration_ms}ms duration")
                    self.loaded.emit(waveform_data, duration_ms)
                    
                except ImportError:
                    logger.warning("librosa not installed, waveform disabled. Install: pip install librosa")
                    self.loaded.emit(None, 0)
                    
                except Exception as e:
                    logger.error(f"Failed to load waveform: {e}")
                    self.loaded.emit(None, 0)
        
        # Show placeholder immediately
        self.waveform_data = None
        self.update()
        
        # Start background loading
        self._loader = WaveformLoader(file_path)
        self._loader.loaded.connect(self._on_waveform_loaded)
        self._loader.start()
    
    def _on_waveform_loaded(self, waveform_data, duration_ms):
        """Callback when waveform finishes loading"""
        self.waveform_data = waveform_data
        self.duration_ms = duration_ms
        self.update()
    
    def set_position(self, position_ms: int):
        """Update playback position"""
        self.position_ms = position_ms
        self.update()
    
    def paintEvent(self, event):
        """Draw waveform"""
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)
        
        # Background
        painter.fillRect(self.rect(), self.background_color)
        
        if self.waveform_data is None:
            painter.setPen(QColor(150, 150, 150))
            # Show loading message if loader is active
            if hasattr(self, '_loader') and self._loader.isRunning():
                painter.drawText(self.rect(), Qt.AlignmentFlag.AlignCenter, "Loading waveform...")
            else:
                painter.drawText(self.rect(), Qt.AlignmentFlag.AlignCenter, "Waveform unavailable")
            return
        
        if len(self.waveform_data) == 0:
            return
        
        # Draw waveform
        width = self.width()
        height = self.height()
        center_y = height / 2
        
        painter.setPen(QPen(self.waveform_color, 1))
        
        path = QPainterPath()
        samples = len(self.waveform_data)
        
        for i in range(samples):
            x = (i / samples) * width
            amplitude = self.waveform_data[i]
            y_offset = amplitude * (height / 2 - 5)
            
            # Draw symmetric waveform (top and bottom)
            if i == 0:
                path.moveTo(x, center_y - y_offset)
            else:
                path.lineTo(x, center_y - y_offset)
        
        painter.drawPath(path)
        
        # Mirror for bottom half
        path_bottom = QPainterPath()
        for i in range(samples):
            x = (i / samples) * width
            amplitude = self.waveform_data[i]
            y_offset = amplitude * (height / 2 - 5)
            
            if i == 0:
                path_bottom.moveTo(x, center_y + y_offset)
            else:
                path_bottom.lineTo(x, center_y + y_offset)
        
        painter.drawPath(path_bottom)
        
        # Draw playback cursor
        if self.duration_ms > 0:
            cursor_x = (self.position_ms / self.duration_ms) * width
            painter.setPen(QPen(self.cursor_color, 2))
            painter.drawLine(int(cursor_x), 0, int(cursor_x), height)
    
    def mousePressEvent(self, event):
        """Handle click to seek"""
        if self.duration_ms > 0 and event.button() == Qt.MouseButton.LeftButton:
            click_ratio = event.position().x() / self.width()
            seek_ms = int(click_ratio * self.duration_ms)
            self.seek_requested.emit(seek_ms)
