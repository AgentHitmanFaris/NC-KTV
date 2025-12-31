"""
Video Options Dialog for NC-KTV
Allows user to choose background when source is audio-only
"""

from PyQt6.QtWidgets import (
    QDialog, QVBoxLayout, QHBoxLayout, QPushButton, 
    QLabel, QRadioButton, QButtonGroup, QFileDialog,
    QColorDialog, QGroupBox
)
from PyQt6.QtCore import Qt
from PyQt6.QtGui import QColor
from pathlib import Path
import logging

logger = logging.getLogger(__name__)


class VideoOptionsDialog(QDialog):
    """Dialog to choose video background for audio-only sources"""
    
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Choose Video Background")
        self.setModal(True)
        self.setMinimumWidth(500)
        
        self.selected_video = None
        self.selected_image = None
        self.selected_color = QColor(20, 20, 20)  # Default dark background
        
        self._init_ui()
        
    def _init_ui(self):
        """Initialize UI"""
        layout = QVBoxLayout(self)
        
        # Info label
        info = QLabel("Your source file is audio-only (MP3/WAV).\nChoose a background for your karaoke video:")
        info.setWordWrap(True)
        layout.addWidget(info)
        
        # Option group
        self.option_group = QButtonGroup(self)
        
        # Option 1: Add video
        video_box = QGroupBox("Option 1: Video Background")
        video_layout = QVBoxLayout()
        
        self.rb_video = QRadioButton("Use a video file as background")
        self.option_group.addButton(self.rb_video, 1)
        video_layout.addWidget(self.rb_video)
        
        btn_layout = QHBoxLayout()
        self.btn_browse_video = QPushButton("📁 Browse Video...")
        self.btn_browse_video.clicked.connect(self._browse_video)
        btn_layout.addWidget(self.btn_browse_video)
        
        self.lbl_video_path = QLabel("No video selected")
        self.lbl_video_path.setStyleSheet("color: gray; font-style: italic;")
        btn_layout.addWidget(self.lbl_video_path)
        btn_layout.addStretch()
        
        video_layout.addLayout(btn_layout)
        video_box.setLayout(video_layout)
        layout.addWidget(video_box)
        
        # Option 2: Image background
        image_box = QGroupBox("Option 2: Static Image")
        image_layout = QVBoxLayout()
        
        self.rb_image = QRadioButton("Use an image as static background")
        self.option_group.addButton(self.rb_image, 2)
        image_layout.addWidget(self.rb_image)
        
        img_btn_layout = QHBoxLayout()
        self.btn_browse_image = QPushButton("🖼️ Browse Image...")
        self.btn_browse_image.clicked.connect(self._browse_image)
        img_btn_layout.addWidget(self.btn_browse_image)
        
        self.lbl_image_path = QLabel("No image selected")
        self.lbl_image_path.setStyleSheet("color: gray; font-style: italic;")
        img_btn_layout.addWidget(self.lbl_image_path)
        img_btn_layout.addStretch()
        
        image_layout.addLayout(img_btn_layout)
        image_box.setLayout(image_layout)
        layout.addWidget(image_box)
        
        # Option 3: Solid color
        color_box = QGroupBox("Option 3: Solid Color")
        color_layout = QVBoxLayout()
        
        self.rb_color = QRadioButton("Use solid color background")
        self.rb_color.setChecked(True)  # Default
        self.option_group.addButton(self.rb_color, 3)
        color_layout.addWidget(self.rb_color)
        
        color_btn_layout = QHBoxLayout()
        self.btn_pick_color = QPushButton("🎨 Pick Color...")
        self.btn_pick_color.clicked.connect(self._pick_color)
        self._update_color_button()
        color_btn_layout.addWidget(self.btn_pick_color)
        color_btn_layout.addStretch()
        
        color_layout.addLayout(color_btn_layout)
        color_box.setLayout(color_layout)
        layout.addWidget(color_box)
        
        layout.addStretch()
        
        # Buttons
        button_layout = QHBoxLayout()
        button_layout.addStretch()
        
        btn_cancel = QPushButton("Cancel")
        btn_cancel.clicked.connect(self.reject)
        button_layout.addWidget(btn_cancel)
        
        btn_ok = QPushButton("Continue Export")
        btn_ok.setStyleSheet("background-color: #4CAF50; color: white; font-weight: bold; padding: 8px 16px;")
        btn_ok.clicked.connect(self.accept)
        button_layout.addWidget(btn_ok)
        
        layout.addLayout(button_layout)
    
    def _browse_video(self):
        """Open file dialog for video selection"""
        file_path, _ = QFileDialog.getOpenFileName(
            self,
            "Select Background Video",
            "",
            "Video Files (*.mp4 *.avi *.mkv *.mov);;All Files (*.*)"
        )
        
        if file_path:
            self.selected_video = Path(file_path)
            self.lbl_video_path.setText(self.selected_video.name)
            self.lbl_video_path.setStyleSheet("color: green;")
            self.rb_video.setChecked(True)
    
    def _browse_image(self):
        """Open file dialog for image selection"""
        file_path, _ = QFileDialog.getOpenFileName(
            self,
            "Select Background Image",
            "",
            "Image Files (*.jpg *.jpeg *.png *.bmp);;All Files (*.*)"
        )
        
        if file_path:
            self.selected_image = Path(file_path)
            self.lbl_image_path.setText(self.selected_image.name)
            self.lbl_image_path.setStyleSheet("color: green;")
            self.rb_image.setChecked(True)
    
    def _pick_color(self):
        """Open color picker"""
        color = QColorDialog.getColor(self.selected_color, self, "Pick Background Color")
        if color.isValid():
            self.selected_color = color
            self._update_color_button()
            self.rb_color.setChecked(True)
    
    def _update_color_button(self):
        """Update color button appearance"""
        self.btn_pick_color.setStyleSheet(
            f"background-color: {self.selected_color.name()}; "
            f"color: {'white' if self.selected_color.lightness() < 128 else 'black'}; "
            f"padding: 8px 16px; font-weight: bold;"
        )
        self.btn_pick_color.setText(f"🎨 {self.selected_color.name()}")
    
    def get_options(self):
        """Return selected background options"""
        selected_id = self.option_group.checkedId()
        
        result = {
            'type': None,
            'path': None,
            'color': None
        }
        
        if selected_id == 1:  # Video
            if not self.selected_video:
                return None
            result['type'] = 'video'
            result['path'] = self.selected_video
            
        elif selected_id == 2:  # Image
            if not self.selected_image:
                return None
            result['type'] = 'image'
            result['path'] = self.selected_image
            
        elif selected_id == 3:  # Color
            result['type'] = 'color'
            result['color'] = self.selected_color
        
        return result
