from PyQt6.QtWidgets import (
    QDialog, QVBoxLayout, QHBoxLayout, QLabel, 
    QComboBox, QPushButton, QCheckBox, QFileDialog,
    QLineEdit, QFormLayout
)
from pathlib import Path

class ExportDialog(QDialog):
    """Dialog for video export options"""
    
    def __init__(self, parent=None, default_filename="karaoke.mp4", current_animation="Linear Wipe"):
        super().__init__(parent)
        self.setWindowTitle("Export Video")
        self.resize(500, 300)
        self.output_path = None
        
        self.layout = QVBoxLayout(self)
        
        # Form Layout for options
        form = QFormLayout()
        
        # 1. Output File
        file_layout = QHBoxLayout()
        self.txt_path = QLineEdit(default_filename)
        self.btn_browse = QPushButton("Browse...")
        self.btn_browse.clicked.connect(self._browse)
        file_layout.addWidget(self.txt_path)
        file_layout.addWidget(self.btn_browse)
        form.addRow("Output File:", file_layout)
        
        # 2. Visual Style
        self.combo_style = QComboBox()
        self.combo_style.addItems(["Neon Gold", "Classic Blue", "Clean White"])
        form.addRow("Visual Style:", self.combo_style)
        
        # 3. Animation - show current animation from preview
        self.lbl_animation = QLabel(f"Using: {current_animation}")
        self.lbl_animation.setStyleSheet("font-weight: bold; color: #4CAF50;")
        form.addRow("Animation:", self.lbl_animation)
        self.current_animation = current_animation
        
        self.layout.addLayout(form)
        
        # Info label
        info = QLabel("ℹ️ Animation from preview will be applied to export")
        info.setStyleSheet("color: #888; font-style: italic;")
        self.layout.addWidget(info)
        
        self.layout.addStretch()
        
        # Buttons
        btn_layout = QHBoxLayout()
        btn_cancel = QPushButton("Cancel")
        btn_cancel.clicked.connect(self.reject)
        
        btn_export = QPushButton("Export")
        btn_export.clicked.connect(self._validate_and_accept)
        btn_export.setStyleSheet("background-color: #4CAF50; color: white; font-weight: bold;")
        
        btn_layout.addStretch()
        btn_layout.addWidget(btn_cancel)
        btn_layout.addWidget(btn_export)
        
        self.layout.addLayout(btn_layout)
        
    def _browse(self):
        path, _ = QFileDialog.getSaveFileName(
            self, "Save Video", self.txt_path.text(), "MP4 Video (*.mp4)"
        )
        if path:
            self.txt_path.setText(path)
            
    def _validate_and_accept(self):
        if not self.txt_path.text():
            return
        self.output_path = Path(self.txt_path.text())
        self.accept()
        
    def get_options(self):
        return {
            "path": self.output_path,
            "style": self.combo_style.currentText(),
            "animation": self.current_animation,
        }
