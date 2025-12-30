"""
Audio Player Component for NC-KTV
Handles playback of instrumental tracks
"""

from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QPushButton, 
    QSlider, QLabel, QStyle
)
from PyQt6.QtMultimedia import QMediaPlayer, QAudioOutput
from PyQt6.QtCore import Qt, QUrl, pyqtSignal
from pathlib import Path


class AudioPlayer(QWidget):
    """Audio player widget with controls"""
    
    # Signals
    position_changed = pyqtSignal(int)  # ms
    duration_changed = pyqtSignal(int)  # ms
    state_changed = pyqtSignal(bool)    # True = playing
    
    def __init__(self):
        super().__init__()
        
        self.media_player = QMediaPlayer()
        self.audio_output = QAudioOutput()
        self.media_player.setAudioOutput(self.audio_output)
        
        self._init_ui()
        self._connect_signals()
        
    def _init_ui(self):
        """Initialize UI"""
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        
        # Seek bar
        seek_layout = QHBoxLayout()
        self.lbl_current = QLabel("00:00")
        self.lbl_total = QLabel("00:00")
        
        self.slider_seek = QSlider(Qt.Orientation.Horizontal)
        self.slider_seek.setRange(0, 0)
        
        seek_layout.addWidget(self.lbl_current)
        seek_layout.addWidget(self.slider_seek)
        seek_layout.addWidget(self.lbl_total)
        
        layout.addLayout(seek_layout)
        
        # Controls
        controls_layout = QHBoxLayout()
        
        # Play/Pause button
        self.btn_play = QPushButton()
        self.btn_play.setIcon(self.style().standardIcon(QStyle.StandardPixmap.SP_MediaPlay))
        self.btn_play.clicked.connect(self.toggle_playback)
        
        # Stop button
        self.btn_stop = QPushButton()
        self.btn_stop.setIcon(self.style().standardIcon(QStyle.StandardPixmap.SP_MediaStop))
        self.btn_stop.clicked.connect(self.stop)
        
        # Volume slider
        self.slider_vol = QSlider(Qt.Orientation.Horizontal)
        self.slider_vol.setRange(0, 100)
        self.slider_vol.setValue(70)
        self.slider_vol.setFixedWidth(100)
        self.audio_output.setVolume(0.7)
        
        controls_layout.addStretch()
        controls_layout.addWidget(self.btn_play)
        controls_layout.addWidget(self.btn_stop)
        controls_layout.addSpacing(20)
        controls_layout.addWidget(QLabel("Vol:"))
        controls_layout.addWidget(self.slider_vol)
        controls_layout.addStretch()
        
        layout.addLayout(controls_layout)
        
    def _connect_signals(self):
        """Connect media player signals"""
        self.media_player.positionChanged.connect(self._on_position_changed)
        self.media_player.durationChanged.connect(self._on_duration_changed)
        self.media_player.playbackStateChanged.connect(self._on_state_changed)
        
        # UI signals
        self.slider_seek.sliderMoved.connect(self.media_player.setPosition)
        self.slider_seek.sliderPressed.connect(self._on_slider_pressed)
        self.slider_seek.sliderReleased.connect(self._on_slider_released)
        
        self.slider_vol.valueChanged.connect(self._set_volume)
        
    def load_audio(self, file_path: Path):
        """Load audio file"""
        self.media_player.setSource(QUrl.fromLocalFile(str(file_path)))
        self.btn_play.setEnabled(True)
        
    def toggle_playback(self):
        """Toggle play/pause"""
        if self.media_player.playbackState() == QMediaPlayer.PlaybackState.PlayingState:
            self.media_player.pause()
        else:
            self.media_player.play()
            
    def stop(self):
        """Stop playback"""
        self.media_player.stop()
        
    def _set_volume(self, value):
        """Set volume"""
        self.audio_output.setVolume(value / 100)
        
    def _on_position_changed(self, position):
        """Handle position update"""
        if not self.slider_seek.isSliderDown():
            self.slider_seek.setValue(position)
        
        self.lbl_current.setText(self._format_time(position))
        self.position_changed.emit(position)
        
    def _on_duration_changed(self, duration):
        """Handle duration update"""
        self.slider_seek.setRange(0, duration)
        self.lbl_total.setText(self._format_time(duration))
        self.duration_changed.emit(duration)
        
    def _on_state_changed(self, state):
        """Handle state change"""
        if state == QMediaPlayer.PlaybackState.PlayingState:
            self.btn_play.setIcon(self.style().standardIcon(QStyle.StandardPixmap.SP_MediaPause))
            self.state_changed.emit(True)
        else:
            self.btn_play.setIcon(self.style().standardIcon(QStyle.StandardPixmap.SP_MediaPlay))
            self.state_changed.emit(False)
            
    def _on_slider_pressed(self):
        """Handle slider press"""
        self.was_playing = self.media_player.playbackState() == QMediaPlayer.PlaybackState.PlayingState
        if self.was_playing:
            self.media_player.pause()
            
    def _on_slider_released(self):
        """Handle slider release"""
        self.media_player.setPosition(self.slider_seek.value())
        if self.was_playing:
            self.media_player.play()
            
    def _format_time(self, ms):
        """Format milliseconds to MM:SS"""
        seconds = (ms // 1000) % 60
        minutes = (ms // 60000)
        return f"{minutes:02}:{seconds:02}"
