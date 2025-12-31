from PyQt6.QtWidgets import (
    QDialog, QVBoxLayout, QHBoxLayout, QLabel, 
    QComboBox, QPushButton, QCheckBox, QFileDialog,
    QLineEdit, QFormLayout
)
from pathlib import Path

class ExportDialog(QDialog):
    """Dialog for video export options"""
    
    def __init__(self, parent=None, default_filename="karaoke.mp4"):
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
        
        # 3. Animation Type
        self.combo_anim = QComboBox()
        self.combo_anim.addItems([
            "Standard (Wipe)", 
            "Zoom In", 
            "Slide Up",
            "Fade In/Out"
        ])
        form.addRow("Animation:", self.combo_anim)
        
        # 4. Audio Options
        self.chk_inst = QCheckBox("Maximize Instrumental Volume")
        self.chk_inst.setChecked(True)
        # form.addRow("Audio:", self.chk_inst)
        
        self.layout.addLayout(form)
        
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
            "animation": self.combo_anim.currentText(),
            "enhance_audio": self.chk_inst.isChecked()
        }
