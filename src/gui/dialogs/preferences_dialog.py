"""
Preferences Dialog
"""

from PyQt6.QtWidgets import (
    QDialog, QVBoxLayout, QHBoxLayout, QLabel, 
    QComboBox, QPushButton, QCheckBox, QTabWidget,
    QWidget, QFormLayout, QSpinBox, QLineEdit, QFileDialog
)
from PyQt6.QtCore import Qt
from utils.config import Config

class PreferencesDialog(QDialog):
    """Application preferences dialog"""
    
    def __init__(self, config: Config, parent=None):
        super().__init__(parent)
        self.config = config
        self.setWindowTitle("Preferences")
        self.resize(600, 400)
        
        layout = QVBoxLayout(self)
        
        # Tabs
        self.tabs = QTabWidget()
        self.tabs.addTab(self._create_general_tab(), "General")
        self.tabs.addTab(self._create_paths_tab(), "Paths")
        self.tabs.addTab(self._create_ai_tab(), "AI Models")
        
        layout.addWidget(self.tabs)
        
        # Buttons
        btn_layout = QHBoxLayout()
        btn_cancel = QPushButton("Cancel")
        btn_cancel.clicked.connect(self.reject)
        
        btn_save = QPushButton("Save")
        btn_save.clicked.connect(self._save_preferences)
        btn_save.setStyleSheet("background-color: #4CAF50; color: white; font-weight: bold;")
        
        btn_layout.addStretch()
        btn_layout.addWidget(btn_cancel)
        btn_layout.addWidget(btn_save)
        
        layout.addLayout(btn_layout)
        
    def _create_general_tab(self):
        widget = QWidget()
        form = QFormLayout(widget)
        
        # Theme
        self.combo_theme = QComboBox()
        self.combo_theme.addItems(["dark", "light", "system"])
        self.combo_theme.setCurrentText(self.config.get('gui.theme', 'dark'))
        form.addRow("Theme:", self.combo_theme)
        
        # Auto-save
        self.chk_autosave = QCheckBox("Enable Auto-save")
        self.chk_autosave.setChecked(self.config.get('gui.autosave', True))
        form.addRow("Auto-save:", self.chk_autosave)
        
        self.spin_interval = QSpinBox()
        self.spin_interval.setRange(60, 3600)
        self.spin_interval.setSuffix(" seconds")
        self.spin_interval.setValue(self.config.get('gui.autosave_interval', 300))
        form.addRow("Interval:", self.spin_interval)
        
        return widget
        
    def _create_paths_tab(self):
        widget = QWidget()
        form = QFormLayout(widget)
        
        # Output Dir
        self.txt_output = QLineEdit(self.config.get('processing.output_dir', 'output/'))
        btn_output = QPushButton("Browse...")
        btn_output.clicked.connect(lambda: self._browse_dir(self.txt_output))
        
        out_layout = QHBoxLayout()
        out_layout.addWidget(self.txt_output)
        out_layout.addWidget(btn_output)
        form.addRow("Output Directory:", out_layout)
        
        # Temp Dir
        self.txt_temp = QLineEdit(self.config.get('processing.temp_dir', 'temp/'))
        btn_temp = QPushButton("Browse...")
        btn_temp.clicked.connect(lambda: self._browse_dir(self.txt_temp))
        
        temp_layout = QHBoxLayout()
        temp_layout.addWidget(self.txt_temp)
        temp_layout.addWidget(btn_temp)
        form.addRow("Temp Directory:", temp_layout)
        
        return widget
        
    def _create_ai_tab(self):
        widget = QWidget()
        form = QFormLayout(widget)
        
        # UVR
        form.addRow(QLabel("<b>Vocal Separation (UVR)</b>"))
        self.chk_gpu = QCheckBox("Use GPU Acceleration")
        self.chk_gpu.setChecked(self.config.get('uvr.use_gpu', True))
        form.addRow("Use GPU:", self.chk_gpu)
        
        # Whisper
        form.addRow(QLabel("<b>Lyrics Transcription (Whisper)</b>"))
        self.combo_whisper = QComboBox()
        self.combo_whisper.addItems(["tiny", "base", "small", "medium", "large"])
        self.combo_whisper.setCurrentText(self.config.get('lyrics.whisper_model', 'base'))
        form.addRow("Model Size:", self.combo_whisper)
        
        return widget
        
    def _browse_dir(self, line_edit):
        path = QFileDialog.getExistingDirectory(self, "Select Directory", line_edit.text())
        if path:
            line_edit.setText(path)
            
    def _save_preferences(self):
        # Save values to config
        self.config.set('gui.theme', self.combo_theme.currentText())
        self.config.set('gui.autosave', self.chk_autosave.isChecked())
        self.config.set('gui.autosave_interval', self.spin_interval.value())
        
        self.config.set('processing.output_dir', self.txt_output.text())
        self.config.set('processing.temp_dir', self.txt_temp.text())
        
        self.config.set('uvr.use_gpu', self.chk_gpu.isChecked())
        self.config.set('lyrics.whisper_model', self.combo_whisper.currentText())
        
        # Save to file
        self.config.save()
        self.accept()
