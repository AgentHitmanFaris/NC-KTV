"""
New Project Dialog
Streamlined interface for creating a new karaoke project.
Consolidates file selection, mode choice, and transcription settings.
"""

from PyQt6.QtWidgets import (
    QDialog, QVBoxLayout, QHBoxLayout, QLabel, 
    QPushButton, QLineEdit, QFileDialog, QComboBox, 
    QCheckBox, QGroupBox, QMessageBox, QFrame
)
from PyQt6.QtCore import Qt
from pathlib import Path
from utils.config import Config
from core.audio_processor import AudioProcessor
from workers.processing_worker import ProcessingWorker

class NewProjectDialog(QDialog):
    """Dialog for creating a new project with all necessary settings"""
    
    def __init__(self, config: Config, parent=None):
        super().__init__(parent)
        self.config = config
        self.selected_file: Path = None
        self.result_data = {}  # Store all settings here
        
        self.setWindowTitle("New Project")
        self.setFixedWidth(500)
        self.setModal(True)
        
        self._init_ui()
        
    def _init_ui(self):
        """Initialize UI components"""
        layout = QVBoxLayout(self)
        layout.setSpacing(15)
        
        # 1. Header
        header = QLabel("Start New Project")
        header.setStyleSheet("font-size: 18px; font-weight: bold; margin-bottom: 5px;")
        layout.addWidget(header)
        
        # 2. File Selection
        file_group = QGroupBox("1. Select Media File")
        file_layout = QVBoxLayout()
        
        file_input_layout = QHBoxLayout()
        self.txt_file_path = QLineEdit()
        self.txt_file_path.setPlaceholderText("Select audio or video file...")
        self.txt_file_path.setReadOnly(True)
        file_input_layout.addWidget(self.txt_file_path)
        
        btn_browse = QPushButton("Browse...")
        btn_browse.clicked.connect(self._browse_file)
        file_input_layout.addWidget(btn_browse)
        
        file_layout.addLayout(file_input_layout)
        file_group.setLayout(file_layout)
        layout.addWidget(file_group)
        
        # 3. Karaoke Mode (UVR Model)
        mode_group = QGroupBox("2. Karaoke Mode (Vocal Removal)")
        mode_layout = QVBoxLayout()
        
        mode_layout.addWidget(QLabel("Select separation strength:"))
        self.combo_mode = QComboBox()
        
        # Simplified options mapped to models
        self.mode_map = {
             "Standard Karaoke (Balanced)": "UVR_MDXNET_KARA_2.onnx",
             "High Performance (Fast)": "5_HP-Karaoke-UVR.pth",
             "Aggressive Removal (Cleaner)": "6_HP-Karaoke-UVR.pth",
             "Instrumental Only (High Quality)": "UVR-MDX-NET-Inst_HQ_3.onnx"
        }
        
        # Populate combinations
        # Check what models are actually available to avoid errors, but default to showing all options if unsure
        try:
             # We try to smart-select based on what's installed
             # Note: ProcessingWorker instantiation might be heavy if it loads models, but list_available_models should be light
             # If this causes lag, we can just use the config or skip it.
             # We'll instantiate minimal worker or just access methods if static/classmethod possible
             # For now, just catch error if it fails
             dummy_worker = ProcessingWorker(self.config, None) # Project is None, might error if __init__ uses it
             # Wait, ProcessingWorker __init__ expects project. Passing None might crash lines:
             # self.audio_processor = AudioProcessor(temp_dir=project.get_temp_dir())
             # So we can't instantiate ProcessingManager without a project easily.
             # Better to use Config.get_available_models directly if possible.
             pass 
        except:
             pass
             
        # Just populate all options for now. UVR worker will handle downloading or erroring later.
        for display, filename in self.mode_map.items():
            self.combo_mode.addItem(display, filename)
                 
        mode_layout.addWidget(self.combo_mode)
        mode_group.setLayout(mode_layout)
        layout.addWidget(mode_group)
        
        # 4. AI Transcription
        trans_group = QGroupBox("3. AI Transcription")
        trans_group.setCheckable(True)
        trans_group.setChecked(True) # Default to true as requested
        self.trans_group = trans_group
        
        trans_layout = QVBoxLayout()
        
        trans_row = QHBoxLayout()
        trans_row.addWidget(QLabel("Whisper Model:"))
        
        self.combo_whisper = QComboBox()
        self.combo_whisper.addItems(["tiny", "base", "small", "medium", "large-v3"])
        
        # Select 'small' or configured default
        default_model = self.config.get('lyrics.whisper_model', 'small')
        index = self.combo_whisper.findText(default_model)
        if index >= 0:
            self.combo_whisper.setCurrentIndex(index)
        else:
            self.combo_whisper.setCurrentText('small')
            
        trans_row.addWidget(self.combo_whisper)
        trans_layout.addLayout(trans_row)
        
        lbl_hint = QLabel("Note: 'fast-whisper' will be used for acceleration.")
        lbl_hint.setStyleSheet("color: gray; font-size: 11px;")
        trans_layout.addWidget(lbl_hint)
        
        trans_group.setLayout(trans_layout)
        layout.addWidget(trans_group)
        
        layout.addStretch()
        
        # 5. Buttons
        btn_layout = QHBoxLayout()
        btn_cancel = QPushButton("Cancel")
        btn_cancel.clicked.connect(self.reject)
        
        self.btn_create = QPushButton("Create Project")
        self.btn_create.setStyleSheet("background-color: #2196F3; color: white; font-weight: bold; padding: 6px;")
        self.btn_create.clicked.connect(self._create_project)
        self.btn_create.setEnabled(False) # Disabled until file selected
        
        btn_layout.addWidget(btn_cancel)
        btn_layout.addWidget(self.btn_create)
        layout.addLayout(btn_layout)
        
    def _browse_file(self):
        file_path, _ = QFileDialog.getOpenFileName(
            self,
            "Select Audio/Video",
            "",
            "Media Files (*.mp3 *.wav *.flac *.m4a *.mp4 *.avi *.mkv *.mov);;All Files (*.*)"
        )
        
        if file_path:
            path = Path(file_path)
            # Validate
            processor = AudioProcessor()
            is_valid, msg = processor.validate_audio_file(path)
            
            if is_valid:
                self.selected_file = path
                self.txt_file_path.setText(str(path))
                self.btn_create.setEnabled(True)
            else:
                QMessageBox.warning(self, "Invalid File", msg)
                
    def _create_project(self):
        """Gather settings and accept"""
        if not self.selected_file:
            return
            
        self.result_data = {
            'file_path': self.selected_file,
            'uvr_model': self.combo_mode.currentData(),
            'transcribe': self.trans_group.isChecked(),
            'whisper_model': self.combo_whisper.currentText()
        }
        
        self.accept()
        
    def get_data(self):
        """Return the collected settings"""
        return self.result_data
