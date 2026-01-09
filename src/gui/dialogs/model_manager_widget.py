"""
Model Manager Widget for NC-KTV Preferences
Displays and manages Whisper and UVR models
"""

from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QTableWidget,
    QTableWidgetItem, QPushButton, QLabel, QMessageBox,
    QHeaderView, QProgressDialog
)
from PyQt6.QtCore import Qt, QThread, pyqtSignal
from pathlib import Path
import urllib.request
import logging
import threading

logger = logging.getLogger(__name__)


class FasterWhisperDownloadWorker(QThread):
    """Background worker for downloading CTranslate2 models via faster_whisper"""
    finished = pyqtSignal(bool, str)  # success, message
    
    def __init__(self, model_name: str, destination_dir: Path):
        super().__init__()
        self.model_name = model_name
        self.destination_dir = destination_dir
    
    def run(self):
        try:
            # Import inside thread to avoid GUI freeze if import is slow
            from faster_whisper import download_model
            
            # download_model returns the path to the downloaded model
            # We enforce our destination directory naming
            # It uses huggingface_hub internally
            logger.info(f"Downloading {self.model_name} to {self.destination_dir}")
            
            # We want to download to 'models/whisper/faster-whisper-{size}'
            # faster_whisper.download_model(size, output_dir=...) downloads into specific folder structure?
            # Actually download_model(model_size) downloads to cache usually.
            # We pass output_dir to specify location.
            
            download_model(self.model_name, output_dir=str(self.destination_dir))
            
            self.finished.emit(True, f"Model {self.model_name} downloaded successfully!")
        except Exception as e:
            logger.error(f"Download failed: {e}")
            self.finished.emit(False, f"Download failed: {e}")


class ModelManagerWidget(QWidget):
    """Widget for managing AI models (Whisper, UVR)"""
    
    # Faster-Whisper Models (CTranslate2)
    # These download directories from HuggingFace
    WHISPER_MODELS = {
        "tiny": {
            "size": "39 MB",
            "description": "Very fast, low accuracy. ~32x speed.",
            "dir_name": "faster-whisper-tiny"
        },
        "base": {
            "size": "74 MB",
            "description": "Fast, decent accuracy. ~16x speed.",
            "dir_name": "faster-whisper-base"
        },
        "small": {
            "size": "244 MB",
            "description": "Balanced. ~6x speed.",
            "dir_name": "faster-whisper-small"
        },
        "medium": {
            "size": "769 MB",
            "description": "Good accuracy. ~2x speed.",
            "dir_name": "faster-whisper-medium"
        },
        "large-v2": {
            "size": "1.5 GB",
            "description": "High accuracy. 1x speed.",
            "dir_name": "faster-whisper-large-v2"
        },
        "large-v3": {
            "size": "1.5 GB",
            "description": "Best accuracy (multilingual). 1x speed.",
            "dir_name": "faster-whisper-large-v3"
        },
        "distil-large-v3": {
            "size": "756 MB",
            "description": "Distilled Large V3. Faster with slightly less accuracy.",
            "dir_name": "faster-whisper-distil-large-v3"
        }
    }
    
    # UVR model info (manual installation)
    UVR_MODELS = {
        "UVR_MDXNET_KARA_2.onnx": {
            "size": "46 MB",
            "description": "Fastest. Best for quick previews.",
            "speed": "~10s for 3min song"
        },
        "UVR_MDXNET_5_HP-Karaoke.onnx": {
            "size": "64 MB",
            "description": "Balanced. Good quality and speed.",
            "speed": "~20s for 3min song"
        },
        "UVR_MDXNET_6_HP-Karaoke.onnx": {
            "size": "64 MB",
            "description": "Best quality. Use for final output.",
            "speed": "~30s for 3min song"
        }
    }
    
    def __init__(self, parent=None):
        super().__init__(parent)
        self._init_ui()
    
    def _init_ui(self):
        """Initialize UI"""
        layout = QVBoxLayout(self)
        
        # Whisper Models Section
        layout.addWidget(QLabel("<h3>🎤 Whisper Models (Auto-Transcription)</h3>"))
        
        self.whisper_table = QTableWidget()
        self.whisper_table.setColumnCount(5)
        self.whisper_table.setHorizontalHeaderLabels([
            "Model", "Size", "Status", "Description", "Action"
        ])
        self.whisper_table.horizontalHeader().setStretchLastSection(False)
        self.whisper_table.horizontalHeader().setSectionResizeMode(3, QHeaderView.ResizeMode.Stretch)
        self.whisper_table.setSelectionBehavior(QTableWidget.SelectionBehavior.SelectRows)
        self.whisper_table.setEditTriggers(QTableWidget.EditTrigger.NoEditTriggers)
        
        self._populate_whisper_table()
        layout.addWidget(self.whisper_table)
        
        # UVR Models Section
        layout.addWidget(QLabel("<h3>🎵 UVR Models (Vocal Separation)</h3>"))
        
        uvr_info = QLabel(
            "⚠️ UVR models must be manually installed to <code>models/UVR</code>.<br>"
            "Download from: <a href='https://github.com/TRvlvr/model_repo/releases'>UVR Model Repo</a>"
        )
        uvr_info.setOpenExternalLinks(True)
        uvr_info.setWordWrap(True)
        layout.addWidget(uvr_info)
        
        self.uvr_table = QTableWidget()
        self.uvr_table.setColumnCount(4)
        self.uvr_table.setHorizontalHeaderLabels([
            "Model", "Size", "Speed", "Description"
        ])
        self.uvr_table.horizontalHeader().setStretchLastSection(True)
        self.uvr_table.setSelectionBehavior(QTableWidget.SelectionBehavior.SelectRows)
        self.uvr_table.setEditTriggers(QTableWidget.EditTrigger.NoEditTriggers)
        
        self._populate_uvr_table()
        layout.addWidget(self.uvr_table)
    
    def _populate_whisper_table(self):
        """Populate Whisper models table"""
        models_dir = Path("models/whisper")
        models_dir.mkdir(parents=True, exist_ok=True)
        
        self.whisper_table.setRowCount(len(self.WHISPER_MODELS))
        
        for row, (model_name, info) in enumerate(self.WHISPER_MODELS.items()):
            # Model name
            self.whisper_table.setItem(row, 0, QTableWidgetItem(f"{model_name} (Faster)"))
            
            # Size
            self.whisper_table.setItem(row, 1, QTableWidgetItem(info["size"]))
            
            # Status check (Directory exists and contains model.bin)
            expected_dir = models_dir / info["dir_name"]
            is_installed = expected_dir.exists() and (expected_dir / "model.bin").exists()
            
            if not is_installed and expected_dir.exists():
                # Maybe partial download?
                pass
            
            status = "✅ Installed" if is_installed else "❌ Not installed"
            status_item = QTableWidgetItem(status)
            status_item.setForeground(Qt.GlobalColor.darkGreen if is_installed else Qt.GlobalColor.red)
            self.whisper_table.setItem(row, 2, status_item)
            
            # Description
            self.whisper_table.setItem(row, 3, QTableWidgetItem(info["description"]))
            
            # Action button
            if not is_installed:
                btn_download = QPushButton("📥 Download")
                # We simply pass the model name string here (e.g. "large-v3", "tiny")
                btn_download.clicked.connect(lambda checked, m=model_name: self._download_whisper_model(m))
                self.whisper_table.setCellWidget(row, 4, btn_download)
            else:
                label = QLabel("Installed")
                label.setAlignment(Qt.AlignmentFlag.AlignCenter)
                self.whisper_table.setCellWidget(row, 4, label)
                
    def _download_whisper_model(self, model_name: str):
        """Download a Whisper model using faster_whisper"""
        info = self.WHISPER_MODELS[model_name]
        
        # We want to save to models/whisper/{dir_name}
        # But faster_whisper.download_model(..., output_dir=X) will put files into X directly?
        # Yes.
        
        destination_dir = Path("models/whisper") / info["dir_name"]
        
        reply = QMessageBox.question(
            self,
            "Confirm Download",
            f"Download {model_name} ({info['size']})?\n\n"
            f"This will download the model files from Hugging Face.\n"
            f"Please wait while the download completes.",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
        )
        
        if reply != QMessageBox.StandardButton.Yes:
            return
        
        # Create progress dialog (Indeterminate)
        progress_dialog = QProgressDialog(
            f"Downloading {model_name}...",
            "Cancel",
            0,
            0,
            self
        )
        progress_dialog.setWindowModality(Qt.WindowModality.ApplicationModal)
        # progress_dialog.setAutoClose(True) # Don't auto-close immediately on 0
        progress_dialog.setMinimumDuration(0)
        
        # Start download worker
        self.download_worker = FasterWhisperDownloadWorker(model_name, destination_dir)
        
        def download_finished(success, message):
            progress_dialog.close()
            if success:
                QMessageBox.information(self, "Success", message)
                self._populate_whisper_table()  # Refresh table
            else:
                QMessageBox.critical(self, "Error", message)
        
        self.download_worker.finished.connect(download_finished)
        self.download_worker.start()
        
        progress_dialog.exec()
        """Populate UVR models table"""
        self.uvr_table.setRowCount(len(self.UVR_MODELS))
        
        for row, (model_name, info) in enumerate(self.UVR_MODELS.items()):
            self.uvr_table.setItem(row, 0, QTableWidgetItem(model_name))
            self.uvr_table.setItem(row, 1, QTableWidgetItem(info["size"]))
            self.uvr_table.setItem(row, 2, QTableWidgetItem(info["speed"]))
            self.uvr_table.setItem(row, 3, QTableWidgetItem(info["description"]))
    
    def _download_whisper_model(self, model_name: str):
        """Download a Whisper model"""
        info = self.WHISPER_MODELS[model_name]
        url = info["url"]
        destination = Path("models/whisper") / model_name
        
        reply = QMessageBox.question(
            self,
            "Confirm Download",
            f"Download {model_name} ({info['size']})?\n\nThis may take several minutes.",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
        )
        
        if reply != QMessageBox.StandardButton.Yes:
            return
        
        # Create progress dialog
        progress_dialog = QProgressDialog(
            f"Downloading {model_name}...",
            "Cancel",
            0,
            100,
            self
        )
        progress_dialog.setWindowModality(Qt.WindowModality.ApplicationModal)
        progress_dialog.setAutoClose(True)
        
        # Start download worker
        self.download_worker = DownloadWorker(url, destination)
        
        def update_progress(current, total):
            if total > 0:
                progress_dialog.setValue(int(current / total * 100))
        
        def download_finished(success, message):
            if success:
                QMessageBox.information(self, "Success", message)
                self._populate_whisper_table()  # Refresh table
            else:
                QMessageBox.critical(self, "Error", message)
        
        self.download_worker.progress.connect(update_progress)
        self.download_worker.finished.connect(download_finished)
        self.download_worker.start()
        
        progress_dialog.exec()
