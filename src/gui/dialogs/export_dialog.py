"""
Enhanced Export Dialog for NC-KTV
Supports multiple export modes: Karaoke, Lyrics Video, etc.
"""

from PyQt6.QtWidgets import (
    QDialog, QVBoxLayout, QHBoxLayout, QLabel, 
    QComboBox, QPushButton, QCheckBox, QFileDialog,
    QLineEdit, QFormLayout, QGroupBox, QRadioButton,
    QButtonGroup, QColorDialog, QMessageBox
)
from PyQt6.QtGui import QColor
from PyQt6.QtCore import Qt
from pathlib import Path


class ExportDialog(QDialog):
    """Enhanced dialog for video export with multiple modes"""
    
    # Export mode presets
    MODE_KARAOKE_VIDEO = "karaoke_video"
    MODE_LYRICS_VIDEO = "lyrics_video"
    MODE_KARAOKE_NO_VIDEO = "karaoke_no_video"
    MODE_LYRICS_NO_VIDEO = "lyrics_no_video"
    MODE_CUSTOM = "custom"
    
    def __init__(self, parent=None, default_filename="karaoke.mp4", current_animation="Linear Wipe", project=None):
        super().__init__(parent)
        self.setWindowTitle("Export Video - NC-KTV")
        self.resize(650, 700)
        self.output_path = None
        self.current_animation = current_animation
        self.project = project
        self.background_color = QColor(20, 20, 30)  # Dark blue default
        
        self._init_ui(default_filename)
        
        # Set default mode
        self._apply_preset(self.MODE_KARAOKE_VIDEO)
    
    def _init_ui(self, default_filename):
        """Initialize UI"""
        layout = QVBoxLayout(self)
        
        # Title
        title = QLabel("🎬 Export Your Karaoke Video")
        title.setStyleSheet("font-size: 16pt; font-weight: bold; padding: 10px;")
        layout.addWidget(title)
        
        # === Preset Modes ===
        preset_group = QGroupBox("Quick Presets")
        preset_layout = QVBoxLayout()
        
        # Preset buttons
        btn_layout = QHBoxLayout()
        
        self.btn_karaoke_video = QPushButton("🎤 Karaoke Video\n(Music Video + Instrumental)")
        self.btn_karaoke_video.setCheckable(True)
        self.btn_karaoke_video.setMinimumHeight(60)
        self.btn_karaoke_video.clicked.connect(lambda: self._apply_preset(self.MODE_KARAOKE_VIDEO))
        btn_layout.addWidget(self.btn_karaoke_video)
        
        self.btn_lyrics_video = QPushButton("🎵 Lyrics Music Video\n(Music Video + Original Audio)")
        self.btn_lyrics_video.setCheckable(True)
        self.btn_lyrics_video.setMinimumHeight(60)
        self.btn_lyrics_video.clicked.connect(lambda: self._apply_preset(self.MODE_LYRICS_VIDEO))
        btn_layout.addWidget(self.btn_lyrics_video)
        
        preset_layout.addLayout(btn_layout)
        
        btn_layout2 = QHBoxLayout()
        
        self.btn_karaoke_no_video = QPushButton("📺 Karaoke (No Video)\n(Solid BG + Instrumental)")
        self.btn_karaoke_no_video.setCheckable(True)
        self.btn_karaoke_no_video.setMinimumHeight(60)
        self.btn_karaoke_no_video.clicked.connect(lambda: self._apply_preset(self.MODE_KARAOKE_NO_VIDEO))
        btn_layout2.addWidget(self.btn_karaoke_no_video)
        
        self.btn_lyrics_no_video = QPushButton("📝 Lyrics Video\n(Solid BG + Original Audio)")
        self.btn_lyrics_no_video.setCheckable(True)
        self.btn_lyrics_no_video.setMinimumHeight(60)
        self.btn_lyrics_no_video.clicked.connect(lambda: self._apply_preset(self.MODE_LYRICS_NO_VIDEO))
        btn_layout2.addWidget(self.btn_lyrics_no_video)
        
        preset_layout.addLayout(btn_layout2)
        
        # Button group for mutual exclusivity
        self.preset_group = QButtonGroup()
        self.preset_group.addButton(self.btn_karaoke_video, 0)
        self.preset_group.addButton(self.btn_lyrics_video, 1)
        self.preset_group.addButton(self.btn_karaoke_no_video, 2)
        self.preset_group.addButton(self.btn_lyrics_no_video, 3)
        
        preset_group.setLayout(preset_layout)
        layout.addWidget(preset_group)
        
        # === Custom Options ===
        options_group = QGroupBox("Custom Options")
        options_layout = QFormLayout()
        
        # Video Source
        self.combo_video_source = QComboBox()
        self.combo_video_source.addItems([
            "Music Video (if available)",
            "Solid Color Background",
            "Custom Image",
            "No Background (Transparent)"
        ])
        self.combo_video_source.currentIndexChanged.connect(self._on_video_source_changed)
        options_layout.addRow("Video Source:", self.combo_video_source)
        
        # Background Color (for solid bg)
        bg_layout = QHBoxLayout()
        self.btn_bg_color = QPushButton("Choose Color...")
        self.btn_bg_color.clicked.connect(self._choose_bg_color)
        self.lbl_bg_preview = QLabel("   ")
        self.lbl_bg_preview.setFixedSize(50, 25)
        self.lbl_bg_preview.setStyleSheet(f"background-color: {self.background_color.name()}; border: 1px solid #888;")
        bg_layout.addWidget(self.btn_bg_color)
        bg_layout.addWidget(self.lbl_bg_preview)
        bg_layout.addStretch()
        options_layout.addRow("Background Color:", bg_layout)
        
        # Audio Source
        self.combo_audio_source = QComboBox()
        self.combo_audio_source.addItems([
            "Instrumental Only (Karaoke)",
            "Original Audio (With Vocals)",
            "Instrumental + Vocals Mixed"
        ])
        options_layout.addRow("Audio Track:", self.combo_audio_source)
        
        # Animation Style
        animation_layout = QHBoxLayout()
        self.lbl_animation = QLabel(f"{current_animation}")
        self.lbl_animation.setStyleSheet("font-weight: bold; color: #4CAF50;")
        animation_layout.addWidget(self.lbl_animation)
        animation_layout.addWidget(QLabel("(from preview)"))
        animation_layout.addStretch()
        options_layout.addRow("Animation:", animation_layout)
        
        # Visual Style
        self.combo_style = QComboBox()
        self.combo_style.addItems(["Neon Gold", "Classic Blue", "Clean White", "Custom"])
        options_layout.addRow("Lyrics Style:", self.combo_style)
        
        options_group.setLayout(options_layout)
        layout.addWidget(options_group)
        
        # === Output File ===
        output_group = QGroupBox("Output File")
        output_layout = QFormLayout()
        
        file_layout = QHBoxLayout()
        self.txt_path = QLineEdit(default_filename)
        self.btn_browse = QPushButton("Browse...")
        self.btn_browse.clicked.connect(self._browse)
        file_layout.addWidget(self.txt_path)
        file_layout.addWidget(self.btn_browse)
        output_layout.addRow("Save As:", file_layout)
        
        output_group.setLayout(output_layout)
        layout.addWidget(output_group)
        
        layout.addStretch()
        
        # === Buttons ===
        btn_layout = QHBoxLayout()
        
        btn_cancel = QPushButton("Cancel")
        btn_cancel.clicked.connect(self.reject)
        btn_layout.addWidget(btn_cancel)
        
        btn_layout.addStretch()
        
        btn_export = QPushButton("🎬 Export Video")
        btn_export.clicked.connect(self._validate_and_accept)
        btn_export.setStyleSheet(
            "background-color: #E91E63; color: white; font-weight: bold; "
            "padding: 10px 20px; font-size: 12pt;"
        )
        btn_layout.addWidget(btn_export)
        
        layout.addLayout(btn_layout)
    
    def _apply_preset(self, mode):
        """Apply a preset mode"""
        self.current_mode = mode
        
        if mode == self.MODE_KARAOKE_VIDEO:
            # Music video BG + Instrumental audio
            self.combo_video_source.setCurrentIndex(0)  # Music Video
            self.combo_audio_source.setCurrentIndex(0)  # Instrumental
            self.btn_karaoke_video.setChecked(True)
            self.btn_karaoke_video.setStyleSheet("background-color: #4CAF50; color: white;")
            
        elif mode == self.MODE_LYRICS_VIDEO:
            # Music video BG + Original audio
            self.combo_video_source.setCurrentIndex(0)  # Music Video
            self.combo_audio_source.setCurrentIndex(1)  # Original
            self.btn_lyrics_video.setChecked(True)
            self.btn_lyrics_video.setStyleSheet("background-color: #4CAF50; color: white;")
            
        elif mode == self.MODE_KARAOKE_NO_VIDEO:
            # Solid BG + Instrumental
            self.combo_video_source.setCurrentIndex(1)  # Solid Color
            self.combo_audio_source.setCurrentIndex(0)  # Instrumental
            self.btn_karaoke_no_video.setChecked(True)
            self.btn_karaoke_no_video.setStyleSheet("background-color: #4CAF50; color: white;")
            
        elif mode == self.MODE_LYRICS_NO_VIDEO:
            # Solid BG + Original audio
            self.combo_video_source.setCurrentIndex(1)  # Solid Color
            self.combo_audio_source.setCurrentIndex(1)  # Original
            self.btn_lyrics_no_video.setChecked(True)
            self.btn_lyrics_no_video.setStyleSheet("background-color: #4CAF50; color: white;")
        
        # Reset other buttons
        for btn in [self.btn_karaoke_video, self.btn_lyrics_video, 
                    self.btn_karaoke_no_video, self.btn_lyrics_no_video]:
            if not btn.isChecked():
                btn.setStyleSheet("")
        
        self._on_video_source_changed()
    
    def _on_video_source_changed(self):
        """Enable/disable background color based on video source"""
        is_solid_bg = self.combo_video_source.currentIndex() == 1
        self.btn_bg_color.setEnabled(is_solid_bg)
        self.lbl_bg_preview.setVisible(is_solid_bg)
    
    def _choose_bg_color(self):
        """Choose background color"""
        color = QColorDialog.getColor(self.background_color, self, "Choose Background Color")
        if color.isValid():
            self.background_color = color
            self.lbl_bg_preview.setStyleSheet(
                f"background-color: {color.name()}; border: 1px solid #888;"
            )
    
    def _browse(self):
        """Browse for output file"""
        path, _ = QFileDialog.getSaveFileName(
            self, "Save Video", self.txt_path.text(), "MP4 Video (*.mp4)"
        )
        if path:
            self.txt_path.setText(path)
    
    def _validate_and_accept(self):
        """Validate and accept"""
        if not self.txt_path.text():
            QMessageBox.warning(self, "No Output File", "Please specify an output file.")
            return
        
        # Check if video source is available
        video_idx = self.combo_video_source.currentIndex()
        if video_idx == 0:  # Music video
            if not self.project or not self.project.source_file:
                QMessageBox.warning(
                    self,
                    "No Music Video",
                    "Music video source not available. Please choose a different video source."
                )
                return
        
        self.output_path = Path(self.txt_path.text())
        self.accept()
    
    def get_options(self):
        """Get export options"""
        video_idx = self.combo_video_source.currentIndex()
        audio_idx = self.combo_audio_source.currentIndex()
        
        return {
            "path": self.output_path,
            "mode": self.current_mode,
            "video_source": {
                0: "music_video",
                1: "solid_color",
                2: "custom_image",
                3: "transparent"
            }[video_idx],
            "audio_source": {
                0: "instrumental",
                1: "original",
                2: "mixed"
            }[audio_idx],
            "background_color": self.background_color.name(),
            "style": self.combo_style.currentText(),
            "animation": self.current_animation,
        }
