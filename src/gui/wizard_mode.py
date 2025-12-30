"""
Wizard Mode GUI for NC-KTV
Simple step-by-step interface for beginners
"""

from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QStackedWidget,
    QPushButton, QLabel, QFileDialog, QMessageBox,
    QComboBox, QCheckBox, QGroupBox, QProgressDialog
)
from PyQt6.QtCore import Qt, pyqtSignal
from PyQt6.QtGui import QFont, QDragEnterEvent, QDropEvent
from pathlib import Path
from typing import Optional

from core.project import Project
from core.audio_processor import AudioProcessor
from workers.processing_worker import ProcessingWorker
from utils.config import Config


class WizardMode(QWidget):
    """Wizard-style interface for karaoke video creation"""
    
    # Signals
    project_created = pyqtSignal(Project)
    
    def __init__(self, config: Config):
        """Initialize wizard mode
        
        Args:
            config: Application configuration
        """
        super().__init__()
        
        self.config = config
        self.project: Optional[Project] = None
        self.processing_worker: Optional[ProcessingWorker] = None
        
        self._init_ui()
    
    def _init_ui(self):
        """Initialize UI"""
        layout = QVBoxLayout(self)
        
        # Stacked widget for wizard pages
        self.pages = QStackedWidget()
        layout.addWidget(self.pages)
        
        # Create wizard pages
        self.page1_file_selection = self._create_page1()
        self.page2_vocal_removal = self._create_page2()
        
        self.pages.addWidget(self.page1_file_selection)
        self.pages.addWidget(self.page2_vocal_removal)
        
        # Navigation buttons
        nav_layout = QHBoxLayout()
        
        self.btn_back = QPushButton("← Back")
        self.btn_back.clicked.connect(self._go_back)
        self.btn_back.setEnabled(False)
        
        self.btn_next = QPushButton("Next →")
        self.btn_next.clicked.connect(self._go_next)
        self.btn_next.setEnabled(False)
        
        self.btn_cancel = QPushButton("Cancel")
        self.btn_cancel.clicked.connect(self._cancel)
        
        nav_layout.addWidget(self.btn_back)
        nav_layout.addStretch()
        nav_layout.addWidget(self.btn_cancel)
        nav_layout.addWidget(self.btn_next)
        
        layout.addLayout(nav_layout)
    
    def _create_page1(self) -> QWidget:
        """Create Step 1: File Selection"""
        page = QWidget()
        layout = QVBoxLayout(page)
        
        # Title
        title = QLabel("Step 1: Select Your Music or Video")
        title_font = QFont()
        title_font.setPointSize(16)
        title_font.setBold(True)
        title.setFont(title_font)
        layout.addWidget(title)
        
        # Description
        desc = QLabel(
            "Choose an audio file (MP3, WAV, FLAC) or video file (MP4, AVI, MKV).\n"
            "The audio will be extracted and vocals will be removed."
        )
        desc.setWordWrap(True)
        layout.addWidget(desc)
        
        layout.addSpacing(20)
        
        # File selection area
        file_group = QGroupBox("Source File")
        file_layout = QVBoxLayout()
        
        # File path display
        self.lbl_file_path = QLabel("No file selected")
        self.lbl_file_path.setStyleSheet(
            "background-color: #f0f0f0; padding: 10px; border-radius: 4px;"
        )
        self.lbl_file_path.setWordWrap(True)
        file_layout.addWidget(self.lbl_file_path)
        
        # Browse button
        btn_browse = QPushButton("📁 Browse Files...")
        btn_browse.clicked.connect(self._browse_file)
        btn_browse.setMinimumHeight(40)
        file_layout.addWidget(btn_browse)
        
        # Drag and drop hint
        hint = QLabel("💡 Or drag and drop a file here")
        hint.setAlignment(Qt.AlignmentFlag.AlignCenter)
        hint.setStyleSheet("color: #666; font-style: italic;")
        file_layout.addWidget(hint)
        
        file_group.setLayout(file_layout)
        layout.addWidget(file_group)
        
        # Enable drag and drop
        page.setAcceptDrops(True)
        page.dragEnterEvent = self._drag_enter_event
        page.dropEvent = self._drop_event
        
        layout.addStretch()
        
        return page
    
    def _create_page2(self) -> QWidget:
        """Create Step 2: Vocal Removal Settings"""
        page = QWidget()
        layout = QVBoxLayout(page)
        
        # Title
        title = QLabel("Step 2: Vocal Removal Settings")
        title_font = QFont()
        title_font.setPointSize(16)
        title_font.setBold(True)
        title.setFont(title_font)
        layout.addWidget(title)
        
        desc = QLabel("Configure how vocals should be removed from the audio.")
        desc.setWordWrap(True)
        layout.addWidget(desc)
        
        layout.addSpacing(20)
        
        # Model selection
        model_group = QGroupBox("UVR Model")
        model_layout = QVBoxLayout()
        
        model_layout.addWidget(QLabel("Select vocal removal model:"))
        
        self.combo_model = QComboBox()
        
        # Define recommended models with descriptions
        self.model_map = {
            "UVR_MDXNET_KARA_2.onnx (Best for Karaoke)": "UVR_MDXNET_KARA_2.onnx",
            "5_HP-Karaoke-UVR.pth (High Performance Karaoke)": "5_HP-Karaoke-UVR.pth",
            "6_HP-Karaoke-UVR.pth (Aggressive Karaoke)": "6_HP-Karaoke-UVR.pth",
            "UVR-MDX-NET-Inst_HQ_3.onnx (High Quality Instrumental)": "UVR-MDX-NET-Inst_HQ_3.onnx"
        }
        
        # Add available models
        try:
            available_models = self.config.get('uvr.available_models', []) or \
                             ProcessingWorker(self.config, None).vocal_remover.list_available_models()
        except:
            available_models = []

        # Add recommended ones first if they exist
        for display_name, filename in self.model_map.items():
            self.combo_model.addItem(display_name, filename)
            if filename in available_models:
                available_models.remove(filename)
        
        # Add remaining models
        for model in available_models:
            self.combo_model.addItem(model, model)
            
        model_layout.addWidget(self.combo_model)
        
        model_group.setLayout(model_layout)
        layout.addWidget(model_group)
        
        # GPU settings
        gpu_group = QGroupBox("Hardware Acceleration")
        gpu_layout = QVBoxLayout()
        
        self.chk_use_gpu = QCheckBox("Use GPU (NVIDIA CUDA)")
        self.chk_use_gpu.setChecked(self.config.get('uvr.use_gpu', True))
        gpu_layout.addWidget(self.chk_use_gpu)
        
        self.lbl_gpu_info = QLabel()
        self._update_gpu_info()
        gpu_layout.addWidget(self.lbl_gpu_info)
        
        gpu_group.setLayout(gpu_layout)
        layout.addWidget(gpu_group)
        
        layout.addSpacing(20)
        
        # Process button
        self.btn_process = QPushButton("🎵 Start Processing")
        self.btn_process.clicked.connect(self._start_processing)
        self.btn_process.setMinimumHeight(50)
        self.btn_process.setStyleSheet("""
            QPushButton {
                background-color: #4CAF50;
                color: white;
                font-size: 14pt;
                font-weight: bold;
                border-radius: 6px;
            }
            QPushButton:hover {
                background-color: #45a049;
            }
            QPushButton:disabled {
                background-color: #cccccc;
            }
        """)
        layout.addWidget(self.btn_process)
        
        layout.addStretch()
        
        return page
    
    def _browse_file(self):
        """Open file browser"""
        file_path, _ = QFileDialog.getOpenFileName(
            self,
            "Select Audio or Video File",
            "",
            "Media Files (*.mp3 *.wav *.flac *.m4a *.mp4 *.avi *.mkv *.mov);;All Files (*.*)"
        )
        
        if file_path:
            self._load_file(Path(file_path))
    
    def _load_file(self, file_path: Path):
        """Load selected file
        
        Args:
            file_path: Path to file
        """
        # Validate file
        processor = AudioProcessor()
        is_valid, message = processor.validate_audio_file(file_path)
        
        if not is_valid:
            QMessageBox.warning(self, "Invalid File", message)
            return
        
        # Create project
        self.project = Project(source_file=file_path)
        
        # Update UI
        self.lbl_file_path.setText(str(file_path))
        self.btn_next.setEnabled(True)
        
        QMessageBox.information(
            self,
            "File Loaded",
            f"Successfully loaded:\n{file_path.name}\n\n{message}"
        )
    
    def _drag_enter_event(self, event: QDragEnterEvent):
        """Handle drag enter"""
        if event.mimeData().hasUrls():
            event.acceptProposedAction()
    
    def _drop_event(self, event: QDropEvent):
        """Handle file drop"""
        urls = event.mimeData().urls()
        if urls:
            file_path = Path(urls[0].toLocalFile())
            self._load_file(file_path)
    
    def _update_gpu_info(self):
        """Update GPU information label"""
        from utils.gpu_detector import GPUDetector
        
        gpu_info = GPUDetector.get_gpu_info()
        
        if gpu_info:
            text = f"✅ {gpu_info['name']} ({gpu_info['total_memory_gb']:.1f} GB)"
            self.lbl_gpu_info.setStyleSheet("color: green;")
        else:
            text = "⚠️ No GPU detected - will use CPU (slower)"
            self.lbl_gpu_info.setStyleSheet("color: orange;")
            self.chk_use_gpu.setEnabled(False)
        
        self.lbl_gpu_info.setText(text)
    
    def _start_processing(self):
        """Start vocal removal processing"""
        if not self.project:
            return
        
        # Update project settings
        model_filename = self.combo_model.currentData()
        if not model_filename:
             model_filename = self.combo_model.currentText().split()[0]
             
        self.project.settings.uvr_model = model_filename
        self.project.settings.use_gpu = self.chk_use_gpu.isChecked()
        
        # Create progress dialog
        progress_dialog = QProgressDialog(
            "Processing...",
            "Cancel",
            0,
            100,
            self
        )
        progress_dialog.setWindowTitle("Vocal Removal")
        progress_dialog.setWindowModality(Qt.WindowModality.WindowModal)
        progress_dialog.setMinimumDuration(0)
        
        # Create and start worker
        self.processing_worker = ProcessingWorker(self.config, self.project)
        
        # Connect signals
        self.processing_worker.progress_updated.connect(
            lambda p, m: (progress_dialog.setValue(int(p)), progress_dialog.setLabelText(m))
        )
        
        self.processing_worker.processing_complete.connect(
            lambda result: self._on_processing_complete(progress_dialog, result)
        )
        
        self.processing_worker.error_occurred.connect(
            lambda err: self._on_processing_error(progress_dialog, err)
        )
        
        progress_dialog.canceled.connect(self.processing_worker.cancel)
        
        # Disable process button
        self.btn_process.setEnabled(False)
        
        # Start processing
        self.processing_worker.start()
    
    def _on_processing_complete(self, dialog: QProgressDialog, result: dict):
        """Handle processing completion
        
        Args:
            dialog: Progress dialog
            result: Processing result
        """
        dialog.close()
        
        QMessageBox.information(
            self,
            "Processing Complete",
            "Vocal removal completed successfully!\n\n"
            f"Instrumental track: {result['instrumental_file']}"
        )
        
        self.btn_process.setEnabled(True)
        self.btn_next.setEnabled(True)
        
        # Emit signal
        self.project_created.emit(self.project)
    
    def _on_processing_error(self, dialog: QProgressDialog, error: str):
        """Handle processing error
        
        Args:
            dialog: Progress dialog
            error: Error message
        """
        dialog.close()
        
        QMessageBox.critical(
            self,
            "Processing Error",
            f"An error occurred during processing:\n\n{error}"
        )
        
        self.btn_process.setEnabled(True)
    
    def _go_next(self):
        """Go to next page"""
        current = self.pages.currentIndex()
        if current < self.pages.count() - 1:
            self.pages.setCurrentIndex(current + 1)
            self._update_navigation()
    
    def _go_back(self):
        """Go to previous page"""
        current = self.pages.currentIndex()
        if current > 0:
            self.pages.setCurrentIndex(current - 1)
            self._update_navigation()
    
    def _update_navigation(self):
        """Update navigation button states"""
        current = self.pages.currentIndex()
        
        self.btn_back.setEnabled(current > 0)
        
        # Next button enabled based on page content
        if current == 0:
            self.btn_next.setEnabled(self.project is not None)
        elif current == 1:
            self.btn_next.setText("Next →")
    
    def _cancel(self):
        """Cancel wizard"""
        if self.processing_worker and self.processing_worker.isRunning():
            reply = QMessageBox.question(
                self,
                "Cancel Processing",
                "Processing is in progress. Are you sure you want to cancel?",
                QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
            )
            
            if reply == QMessageBox.StandardButton.Yes:
                self.processing_worker.cancel()
                self.processing_worker.wait()
        
        # Reset to first page
        self.pages.setCurrentIndex(0)
        self.project = None
        self.lbl_file_path.setText("No file selected")
        self.btn_next.setEnabled(False)
        self._update_navigation()
