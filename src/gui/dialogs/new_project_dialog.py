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
        for display, filename in self.mode_map.items():
            self.combo_mode.addItem(display, filename)
                 
        mode_layout.addWidget(self.combo_mode)
        
        # Add Advanced Model Selection button
        btn_advanced_models = QPushButton("🎛️ Advanced Model Selection...")
        btn_advanced_models.clicked.connect(self._open_model_manager)
        btn_advanced_models.setToolTip("Browse and download additional vocal removal models")
        mode_layout.addWidget(btn_advanced_models)
        
        # Store currently selected model filename for Model Manager
        self.selected_model_filename = None
        
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
        
        # Populate Whisper models (Faster & OpenAI)
        from pathlib import Path
        whisper_dir = Path("models/whisper")
        
        # 1. Standard Faster-Whisper Models
        standard_models = ["tiny", "base", "small", "medium", "large-v2", "large-v3"]
        for model in standard_models:
            dir_name = f"faster-whisper-{model}"
            is_installed = (whisper_dir / dir_name).exists()
            label = f"{model} (Faster-Whisper){' ✓' if is_installed else ''}"
            self.combo_whisper.addItem(label, model)

        # 2. OpenAI Models (.pt files)
        if whisper_dir.exists():
            for pt_file in whisper_dir.glob("*.pt"):
                self.combo_whisper.addItem(f"{pt_file.stem} (OpenAI Original) ✓", pt_file.name)
        
        # Select configured default
        default_model = self.config.get('lyrics.whisper_model', 'small')
        index = self.combo_whisper.findData(default_model)
        if index >= 0:
            self.combo_whisper.setCurrentIndex(index)
        else:
            self.combo_whisper.setCurrentIndex(2) # Default to small (Faster)
            
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
    
    def _open_model_manager(self):
        """Open the advanced model manager dialog"""
        from gui.dialogs.model_manager_dialog import VocalRemovalModelDialog
        
        # Get current model selection
        current_model = self.selected_model_filename or self.combo_mode.currentData()
        
        dialog = VocalRemovalModelDialog(self.config, current_model, self)
        
        if dialog.exec() == QDialog.DialogCode.Accepted:
            selected_model = dialog.get_selected_model()
            self.selected_model_filename = selected_model
            
            # Check if this model is in the preset combo
            found = False
            for i in range(self.combo_mode.count()):
                if self.combo_mode.itemData(i) == selected_model:
                    self.combo_mode.setCurrentIndex(i)
                    found = True
                    break
            
            # If not in presets, add it as a custom option
            if not found:
                # Get model display name from dialog
                from gui.dialogs.model_manager_dialog import VocalRemovalModelDialog
                model_info = VocalRemovalModelDialog.AVAILABLE_MODELS.get(selected_model, {})
                display_name = model_info.get('display_name', selected_model)
                
                self.combo_mode.addItem(f"📦 {display_name} (Custom)", selected_model)
                self.combo_mode.setCurrentIndex(self.combo_mode.count() - 1)
                 
    def _create_project(self):
        """Gather settings and accept"""
        if not self.selected_file:
            return
            
        # Use custom selected model if available, otherwise use combo box selection
        uvr_model = self.selected_model_filename or self.combo_mode.currentData()
            
        self.result_data = {
            'file_path': self.selected_file,
            'uvr_model': uvr_model,
            'transcribe': self.trans_group.isChecked(),
            'whisper_model': self.combo_whisper.currentData()
        }
        
        self.accept()
        
    def get_data(self):
        """Return the collected settings"""
        return self.result_data
