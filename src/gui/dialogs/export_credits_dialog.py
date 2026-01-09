"""
Dialog for configuring intro credits overlay
"""

from PyQt6.QtWidgets import (
    QDialog, QVBoxLayout, QHBoxLayout, QLabel, 
    QLineEdit, QCheckBox, QSpinBox, QPushButton, 
    QDialogButtonBox, QGroupBox, QDoubleSpinBox
)
from PyQt6.QtCore import Qt

class ExportCreditsDialog(QDialog):
    def __init__(self, parent=None, default_title="", default_artist=""):
        super().__init__(parent)
        self.setWindowTitle("Intro Credits Configuration")
        self.setFixedWidth(400)
        
        self.layout = QVBoxLayout(self)
        
        # Enable Checkbox
        self.chk_enable = QCheckBox("Enable Intro Credits Overlay")
        self.chk_enable.setChecked(True)
        self.chk_enable.toggled.connect(self._toggle_fields)
        self.layout.addWidget(self.chk_enable)
        
        # Configuration Group
        self.group = QGroupBox("Credits Info")
        group_layout = QVBoxLayout()
        
        # Title
        group_layout.addWidget(QLabel("Song Title:"))
        self.txt_title = QLineEdit(default_title)
        group_layout.addWidget(self.txt_title)
        
        # Artist
        group_layout.addWidget(QLabel("Artist:"))
        self.txt_artist = QLineEdit(default_artist)
        group_layout.addWidget(self.txt_artist)
        
        # Version
        self.chk_version = QCheckBox("Show 'NC-KTV Version'")
        self.chk_version.setChecked(True)
        group_layout.addWidget(self.chk_version)
        
        # Duration
        duration_layout = QHBoxLayout()
        duration_layout.addWidget(QLabel("Duration (seconds):"))
        self.spin_duration = QDoubleSpinBox()
        self.spin_duration.setRange(1.0, 10.0)
        self.spin_duration.setValue(5.0)
        self.spin_duration.setSingleStep(0.5)
        duration_layout.addWidget(self.spin_duration)
        group_layout.addLayout(duration_layout)
        
        # Intro Mode
        self.chk_preroll = QCheckBox("Intro Mode (Play before song)")
        self.chk_preroll.setToolTip("If checked, credits play as a separate intro before the music starts.")
        group_layout.addWidget(self.chk_preroll)
        
        self.group.setLayout(group_layout)
        self.layout.addWidget(self.group)
        
        # Buttons
        self.buttons = QDialogButtonBox(
            QDialogButtonBox.StandardButton.Ok | 
            QDialogButtonBox.StandardButton.Cancel
        )
        self.buttons.accepted.connect(self.accept)
        self.buttons.rejected.connect(self.reject)
        self.layout.addWidget(self.buttons)
        
        self._toggle_fields(True)
        
    def _toggle_fields(self, enabled):
        self.group.setEnabled(enabled)
        
    def get_info(self):
        return {
            'enabled': self.chk_enable.isChecked(),
            'title': self.txt_title.text().strip(),
            'artist': self.txt_artist.text().strip(),
            'show_version': self.chk_version.isChecked(),
            'duration': self.spin_duration.value(),
            'intro_mode': self.chk_preroll.isChecked()
        }
