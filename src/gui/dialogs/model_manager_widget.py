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

logger = logging.getLogger(__name__)


class DownloadWorker(QThread):
    """Background worker for downloading models"""
    progress = pyqtSignal(int, int)  # current, total
    finished = pyqtSignal(bool, str)  # success, message
    
    def __init__(self, url: str, destination: Path):
        super().__init__()
        self.url = url
        self.destination = destination
    
    def run(self):
        try:
            def report_progress(block_num, block_size, total_size):
                downloaded = block_num * block_size
                self.progress.emit(downloaded, total_size)
            
            urllib.request.urlretrieve(self.url, str(self.destination), report_progress)
            self.finished.emit(True, "Download completed successfully!")
        except Exception as e:
            self.finished.emit(False, f"Download failed: {e}")


class ModelManagerWidget(QWidget):
    """Widget for managing AI models (Whisper, UVR)"""
    
    # Whisper model download URLs
    WHISPER_MODELS = {
        "tiny.pt": {
            "url": "https://openaipublic.azureedge.net/main/whisper/models/65147644a518d12f04e32d6f3b26facc3f8dd46e5390956a9424a650c0ce22b9/tiny.pt",
            "size": "152 MB",
            "description": "Fastest, lowest accuracy. Good for testing."
        },
        "base.pt": {
            "url": "https://openaipublic.azureedge.net/main/whisper/models/ed3a0b6b1c0edf879ad9b11b1af5a0e6ab5db9205f891f668f8b0e6c6326e34e/base.pt",
            "size": "290 MB",
            "description": "Fast, decent accuracy for simple tasks."
        },
        "small.pt": {
            "url": "https://openaipublic.azureedge.net/main/whisper/models/9ecf779972d90ba49c06d968637d720dd632c55bbf19d441fb42bf17a411e794/small.pt",
            "size": "967 MB",
            "description": "Balanced speed and accuracy."
        },
        "medium.pt": {
            "url": "https://openaipublic.azureedge.net/main/whisper/models/345ae4da62f9b3d59415adc60127b97c714f32e89e936602e85993674d08dcb1/medium.pt",
            "size": "3.1 GB",
            "description": "Best accuracy for most use cases."
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
            self.whisper_table.setItem(row, 0, QTableWidgetItem(model_name))
            
            # Size
            self.whisper_table.setItem(row, 1, QTableWidgetItem(info["size"]))
            
            # Status
            model_path = models_dir / model_name
            status = "✅ Installed" if model_path.exists() else "❌ Not installed"
            status_item = QTableWidgetItem(status)
            status_item.setForeground(Qt.GlobalColor.darkGreen if model_path.exists() else Qt.GlobalColor.red)
            self.whisper_table.setItem(row, 2, status_item)
            
            # Description
            self.whisper_table.setItem(row, 3, QTableWidgetItem(info["description"]))
            
            # Action button
            if not model_path.exists():
                btn_download = QPushButton("📥 Download")
                btn_download.clicked.connect(lambda checked, m=model_name: self._download_whisper_model(m))
                self.whisper_table.setCellWidget(row, 4, btn_download)
            else:
                label = QLabel("Installed")
                label.setAlignment(Qt.AlignmentFlag.AlignCenter)
                self.whisper_table.setCellWidget(row, 4, label)
    
    def _populate_uvr_table(self):
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
