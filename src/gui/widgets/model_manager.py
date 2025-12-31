"""
Model Manager Dialog
Shows installed models and allows downloading new ones
"""

from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QLabel,
    QPushButton, QGroupBox, QScrollArea, QProgressBar
)
from PyQt6.QtCore import Qt, QThread, pyqtSignal
from pathlib import Path
import urllib.request
import logging

logger = logging.getLogger(__name__)


class ModelDownloadWorker(QThread):
    """Background worker for model downloads"""
    
    progress_updated = pyqtSignal(int)  # Progress percentage
    download_complete = pyqtSignal()
    error_occurred = pyqtSignal(str)
    
    def __init__(self, url: str, save_path: Path):
        super().__init__()
        self.url = url
        self.save_path = save_path
        self._is_cancelled = False
    
    def run(self):
        try:
            save_path.parent.mkdir(parents=True, exist_ok=True)
            
            def report_progress(block_num, block_size, total_size):
                if self._is_cancelled:
                    raise Exception("Download cancelled")
                
                if total_size > 0:
                    progress = int((block_num * block_size) / total_size * 100)
                    self.progress_updated.emit(min(progress, 100))
            
            urllib.request.urlretrieve(self.url, str(self.save_path), report_progress)
            self.download_complete.emit()
            
        except Exception as e:
            self.error_occurred.emit(str(e))
    
    def cancel(self):
        self._is_cancelled = True


class ModelManagerWidget(QWidget):
    """Model manager tab for preferences"""
    
    # Model database with download URLs and descriptions
    WHISPER_MODELS = {
        "tiny": {
            "size": "~39 MB",
            "description": "Fastest, lowest quality. Good for testing.",
            "best_for": "Quick tests, real-time transcription",
            "url": "https://openaipublic.azureedge.net/main/whisper/models/65147644a518d12f04e32d6f3b26facc3f8dd46e5390956a9424a650c0ce22b9/tiny.pt"
        },
        "base": {
            "size": "~74 MB",
            "description": "Fast with decent quality.",
            "best_for": "Balanced speed and accuracy",
            "url": "https://openaipublic.azureedge.net/main/whisper/models/ed3a0b6b1c0edf879ad9b11b1af5a0e6ab5db9205f891f668f8b0e6c6326e34e/base.pt"
        },
        "small": {
            "size": "~244 MB",
            "description": "Good balance of speed and accuracy.",
            "best_for": "Most karaoke projects (recommended)",
            "url": "https://openaipublic.azureedge.net/main/whisper/models/9ecf779972d90ba49c06d968637d720dd632c55bbf19d441fb42bf17a411e794/small.pt"
        },
        "medium": {
            "size": "~769 MB",
            "description": "High accuracy, slower processing.",
            "best_for": "Professional projects, complex lyrics",
            "url": "https://openaipublic.azureedge.net/main/whisper/models/345ae4da62f9b3d59415adc60127b97c714f32e89e936602e85993674d08dcb1/medium.pt"
        },
        "large-v2": {
            "size": "~2.9 GB",
            "description": "Highest accuracy, very slow.",
            "best_for": "Maximum quality, batch processing",
            "url": "https://openaipublic.azureedge.net/main/whisper/models/81f7c96c852ee8fc832187b0132e569d6c3065a3252ed18e56effd0b6a73e524/large-v2.pt"
        }
    }
    
    UVR_MODELS = {
        "KARA_2": {
            "file": "UVR_MDXNET_KARA_2.onnx",
            "size": "~120 MB",
            "type": "MDX-Net",
            "description": "Best quality karaoke separation. Slower but excellent results.",
            "best_for": "Final karaoke videos, complex vocals",
            "speed": "30-60s (GPU)"
        },
        "5_HP-Karaoke": {
            "file": "5_HP-Karaoke-UVR.pth",
            "size": "~80 MB",
            "type": "VR Architecture",
            "description": "Fast karaoke separation with very good quality.",
            "best_for": "Quick processing, testing",
            "speed": "10-20s (GPU)"
        },
        "6_HP-Karaoke": {
            "file": "6_HP-Karaoke-UVR.pth",
            "size": "~80 MB",
            "type": "VR Architecture",
            "description": "Alternative fast model, slightly more aggressive.",
            "best_for": "Alternative to 5_HP when quality differs",
            "speed": "10-20s (GPU)"
        }
    }
    
    def __init__(self):
        super().__init__()
        self._init_ui()
    
    def _init_ui(self):
        layout = QVBoxLayout(self)
        
        # Scroll area for models
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setHorizontalScrollBarPolicy(Qt.ScrollBarPolicy.ScrollBarAlwaysOff)
        
        content = QWidget()
        content_layout = QVBoxLayout(content)
        
        # Whisper Models Section
        whisper_group = self._create_whisper_section()
        content_layout.addWidget(whisper_group)
        
        # UVR Models Section
        uvr_group = self._create_uvr_section()
        content_layout.addWidget(uvr_group)
        
        content_layout.addStretch()
        scroll.setWidget(content)
        layout.addWidget(scroll)
    
    def _create_whisper_section(self) -> QGroupBox:
        """Create Whisper models section"""
        group = QGroupBox("Whisper Models (AI Lyrics Transcription)")
        layout = QVBoxLayout()
        
        whisper_dir = Path("models/whisper")
        installed_models = []
        if whisper_dir.exists():
            installed_models = [f.stem for f in whisper_dir.glob("*.pt")]
        
        for model_name, info in self.WHISPER_MODELS.items():
            model_widget = self._create_model_card(
                name=model_name,
                info=info,
                is_installed=(model_name in installed_models),
                download_path=whisper_dir / f"{model_name}.pt",
                model_type="whisper"
            )
            layout.addWidget(model_widget)
        
        group.setLayout(layout)
        return group
    
    def _create_uvr_section(self) -> QGroupBox:
        """Create UVR models section"""
        group = QGroupBox("UVR Models (Vocal Separation)")
        layout = QVBoxLayout()
        
        models_dir = Path("models")
        
        for model_key, info in self.UVR_MODELS.items():
            model_file = models_dir / info["file"]
            is_installed = model_file.exists()
            
            model_widget = self._create_model_card(
                name=model_key,
                info=info,
                is_installed=is_installed,
                download_path=model_file,
                model_type="uvr"
            )
            layout.addWidget(model_widget)
        
        group.setLayout(layout)
        return group
    
    def _create_model_card(self, name: str, info: dict, is_installed: bool, 
                          download_path: Path, model_type: str) -> QWidget:
        """Create a model card widget"""
        card = QWidget()
        card.setStyleSheet("""
            QWidget {
                background-color: #f5f5f5;
                border: 1px solid #ddd;
                border-radius: 6px;
                padding: 10px;
            }
        """)
        
        layout = QVBoxLayout(card)
        
        # Header
        header_layout = QHBoxLayout()
        
        # Model name and status
        name_label = QLabel(f"<b>{name}</b>")
        name_label.setStyleSheet("font-size: 12pt;")
        header_layout.addWidget(name_label)
        
        if is_installed:
            status_label = QLabel("✓ Installed")
            status_label.setStyleSheet("color: green; font-weight: bold;")
        else:
            status_label = QLabel("Not installed")
            status_label.setStyleSheet("color: #999;")
        header_layout.addWidget(status_label)
        
        header_layout.addStretch()
        
        # Size
        size_label = QLabel(info.get("size", "Unknown size"))
        size_label.setStyleSheet("color: #666;")
        header_layout.addWidget(size_label)
        
        layout.addLayout(header_layout)
        
        # Description
        desc_label = QLabel(info.get("description", ""))
        desc_label.setWordWrap(True)
        layout.addWidget(desc_label)
        
        # Best for
        best_for = QLabel(f"<b>Best for:</b> {info.get('best_for', 'General use')}")
        best_for.setWordWrap(True)
        best_for.setStyleSheet("color: #0066cc; font-size: 9pt;")
        layout.addWidget(best_for)
        
        # Speed info for UVR
        if model_type == "uvr" and "speed" in info:
            speed_label = QLabel(f"⚡ Speed: {info['speed']}")
            speed_label.setStyleSheet("color: #666; font-size: 9pt; font-style: italic;")
            layout.addWidget(speed_label)
        
        # Action button
        if is_installed:
            btn = QPushButton("✓ Installed")
            btn.setEnabled(False)
            btn.setStyleSheet("background-color: #4CAF50; color: white;")
        else:
            if "url" in info:
                btn = QPushButton("📥 Download")
                btn.clicked.connect(lambda: self._start_download(name, info["url"], download_path, btn))
            else:
                btn = QPushButton("Manual Install Required")
                btn.setEnabled(False)
                btn.setToolTip("This model must be downloaded manually from UVR website")
        
        layout.addWidget(btn)
        
        return card
    
    def _start_download(self, model_name: str, url: str, save_path: Path, button: QPushButton):
        """Start model download"""
        button.setText("Downloading...")
        button.setEnabled(False)
        
        # Create progress bar
        progress = QProgressBar()
        button.parent().layout().addWidget(progress)
        
        # Start download worker
        worker = ModelDownloadWorker(url, save_path)
        worker.progress_updated.connect(progress.setValue)
        worker.download_complete.connect(lambda: self._on_download_complete(model_name, button, progress))
        worker.error_occurred.connect(lambda err: self._on_download_error(err, button, progress))
        worker.start()
        
        # Store worker reference
        button.setProperty("worker", worker)
    
    def _on_download_complete(self, model_name: str, button: QPushButton, progress: QProgressBar):
        """Handle download completion"""
        button.setText("✓ Installed")
        button.setStyleSheet("background-color: #4CAF50; color: white;")
        progress.deleteLater()
        
        logger.info(f"Model {model_name} downloaded successfully")
    
    def _on_download_error(self, error: str, button: QPushButton, progress: QProgressBar):
        """Handle download error"""
        button.setText("❌ Download Failed")
        button.setEnabled(True)
        progress.deleteLater()
        
        logger.error(f"Model download failed: {error}")
