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
        """Load audio file and extract waveform data"""
        try:
            import librosa
            logger.info(f"Loading waveform from: {file_path}")
            
            # Load audio with librosa (auto-resamples to 22050 Hz by default)
            y, sr = librosa.load(str(file_path), sr=None, mono=True)
            
            # Downsample waveform for visualization (approx 2000 samples)
            target_samples = 2000
            hop_length = max(1, len(y) // target_samples)
            
            # Extract envelope using RMS
            self.waveform_data = librosa.feature.rms(y=y, frame_length=hop_length*4, hop_length=hop_length)[0]
            
            # Normalize to [-1, 1]
            if self.waveform_data.max() > 0:
                self.waveform_data = self.waveform_data / self.waveform_data.max()
            
            # Get duration
            self.duration_ms = int((len(y) / sr) * 1000)
            
            logger.info(f"Waveform loaded: {len(self.waveform_data)} samples, {self.duration_ms}ms duration")
            self.update()
            
        except ImportError:
            logger.warning("librosa not installed, waveform disabled. Install: pip install librosa")
            self.waveform_data = None
            
        except Exception as e:
            logger.error(f"Failed to load waveform: {e}")
            self.waveform_data = None
    
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
        
        if self.waveform_data is None or len(self.waveform_data) == 0:
            painter.setPen(QColor(150, 150, 150))
            painter.drawText(self.rect(), Qt.AlignmentFlag.AlignCenter, "Waveform unavailable")
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
