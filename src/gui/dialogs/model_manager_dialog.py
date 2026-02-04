"""
Vocal Removal Model Manager Dialog
Allows users to view, select, and download different UVR models
"""

from PyQt6.QtWidgets import (
    QDialog, QVBoxLayout, QHBoxLayout, QLabel, 
    QPushButton, QListWidget, QListWidgetItem, QTextEdit,
    QGroupBox, QMessageBox, QProgressDialog, QComboBox
)
from PyQt6.QtCore import Qt, pyqtSignal, QThread
from pathlib import Path
import requests
from typing import Dict, List

class ModelDownloadWorker(QThread):
    """Worker thread for downloading models"""
    progress_updated = pyqtSignal(int, str)  # percentage, message
    download_complete = pyqtSignal(str)  # model_filename
    error_occurred = pyqtSignal(str)
    
    def __init__(self, model_info: dict, models_dir: Path):
        super().__init__()
        self.model_info = model_info
        self.models_dir = models_dir
        
    def run(self):
        try:
            self.models_dir.mkdir(parents=True, exist_ok=True)
            
            url = self.model_info['url']
            filename = self.model_info['filename']
            output_path = self.models_dir / filename
            
            self.progress_updated.emit(0, f"Downloading {filename}...")
            
            # Download with progress
            response = requests.get(url, stream=True)
            response.raise_for_status()
            
            total_size = int(response.headers.get('content-length', 0))
            downloaded = 0
            
            with open(output_path, 'wb') as f:
                for chunk in response.iter_content(chunk_size=8192):
                    if chunk:
                        f.write(chunk)
                        downloaded += len(chunk)
                        
                        if total_size > 0:
                            percentage = int((downloaded / total_size) * 100)
                            self.progress_updated.emit(percentage, f"Downloading {filename}...")
            
            self.progress_updated.emit(100, "Download complete!")
            self.download_complete.emit(filename)
            
        except Exception as e:
            self.error_occurred.emit(str(e))


class VocalRemovalModelDialog(QDialog):
    """Dialog for managing vocal removal models"""
    
    # Available UVR models with download links
    AVAILABLE_MODELS = {
        "UVR_MDXNET_KARA_2.onnx": {
            "display_name": "UVR MDX-Net KARA 2",
            "type": "MDX-Net",
            "description": "Balanced karaoke model. Good for most songs.",
            "quality": "★★★★☆",
            "speed": "★★★★☆",
            "size_mb": 129,
            "url": "https://github.com/TRvlvr/model_repo/releases/download/all_public_uvr_models/UVR_MDXNET_KARA_2.onnx"
        },
        "5_HP-Karaoke-UVR.pth": {
            "display_name": "HP Karaoke (Fast)",
            "type": "VR Architecture",
            "description": "High performance, fast processing. Great for quick results.",
            "quality": "★★★☆☆",
            "speed": "★★★★★",
            "size_mb": 81,
            "url": "https://github.com/TRvlvr/model_repo/releases/download/all_public_uvr_models/5_HP-Karaoke-UVR.pth"
        },
        "6_HP-Karaoke-UVR.pth": {
            "display_name": "HP Karaoke Aggressive",
            "type": "VR Architecture",
            "description": "Aggressive vocal removal. Cleaner instrumentals.",
            "quality": "★★★★★",
            "speed": "★★★★☆",
            "size_mb": 81,
            "url": "https://github.com/TRvlvr/model_repo/releases/download/all_public_uvr_models/6_HP-Karaoke-UVR.pth"
        },
        "UVR-MDX-NET-Inst_HQ_3.onnx": {
            "display_name": "MDX-Net Instrumental HQ",
            "type": "MDX-Net",
            "description": "Highest quality instrumentals. Slower but best results.",
            "quality": "★★★★★",
            "speed": "★★★☆☆",
            "size_mb": 129,
            "url": "https://github.com/TRvlvr/model_repo/releases/download/all_public_uvr_models/UVR-MDX-NET-Inst_HQ_3.onnx"
        },
        "UVR-MDX-NET-Voc_FT.onnx": {
            "display_name": "MDX-Net Vocals (Fine-Tuned)",
            "type": "MDX-Net",
            "description": "Optimized for extracting clean vocals.",
            "quality": "★★★★☆",
            "speed": "★★★☆☆",
            "size_mb": 129,
            "url": "https://github.com/TRvlvr/model_repo/releases/download/all_public_uvr_models/UVR-MDX-NET-Voc_FT.onnx"
        },
        "Kim_Vocal_2.onnx": {
            "display_name": "Kim Vocal 2",
            "type": "MDX-Net",
            "description": "Community favorite for vocal extraction.",
            "quality": "★★★★☆",
            "speed": "★★★★☆",
            "size_mb": 129,
            "url": "https://github.com/TRvlvr/model_repo/releases/download/all_public_uvr_models/Kim_Vocal_2.onnx"
        },
        "Reverb_HQ_By_FoxJoy.onnx": {
            "display_name": "Reverb HQ",
            "type": "MDX-Net",
            "description": "Removes reverb from vocals/instrumentals.",
            "quality": "★★★★☆",
            "speed": "★★★☆☆",
            "size_mb": 129,
            "url": "https://github.com/TRvlvr/model_repo/releases/download/all_public_uvr_models/Reverb_HQ_By_FoxJoy.onnx"
        }
    }
    
    model_selected = pyqtSignal(str)  # Emits selected model filename
    
    def __init__(self, config, current_model: str = None, parent=None):
        super().__init__(parent)
        self.config = config
        self.current_model = current_model or "UVR_MDXNET_KARA_2.onnx"
        self.models_dir = Path(self.config.get('uvr.models_path', 'models'))
        
        self.setWindowTitle("Vocal Removal Model Manager")
        self.setMinimumSize(700, 600)
        self.setModal(True)
        
        self._init_ui()
        self._load_installed_models()
        
    def _init_ui(self):
        """Initialize UI"""
        layout = QVBoxLayout(self)
        
        # Header
        header = QLabel("🎵 Vocal Removal Model Manager")
        header.setStyleSheet("font-size: 18px; font-weight: bold; margin-bottom: 10px;")
        layout.addWidget(header)
        
        # Filter
        filter_layout = QHBoxLayout()
        filter_layout.addWidget(QLabel("Filter:"))
        
        self.combo_filter = QComboBox()
        self.combo_filter.addItems(["All Models", "Installed Only", "Not Installed", "MDX-Net", "VR Architecture"])
        self.combo_filter.currentTextChanged.connect(self._apply_filter)
        filter_layout.addWidget(self.combo_filter)
        filter_layout.addStretch()
        
        layout.addLayout(filter_layout)
        
        # Model List
        list_group = QGroupBox("Available Models")
        list_layout = QVBoxLayout()
        
        self.model_list = QListWidget()
        self.model_list.currentItemChanged.connect(self._on_selection_changed)
        list_layout.addWidget(self.model_list)
        
        list_group.setLayout(list_layout)
        layout.addWidget(list_group, 3)
        
        # Model Info Panel
        info_group = QGroupBox("Model Information")
        info_layout = QVBoxLayout()
        
        self.info_text = QTextEdit()
        self.info_text.setReadOnly(True)
        self.info_text.setMaximumHeight(150)
        info_layout.addWidget(self.info_text)
        
        # Action buttons
        btn_layout = QHBoxLayout()
        
        self.btn_download = QPushButton("📥 Download Model")
        self.btn_download.clicked.connect(self._download_model)
        self.btn_download.setEnabled(False)
        btn_layout.addWidget(self.btn_download)
        
        self.btn_delete = QPushButton("🗑️ Delete Model")
        self.btn_delete.clicked.connect(self._delete_model)
        self.btn_delete.setEnabled(False)
        btn_layout.addWidget(self.btn_delete)
        
        info_layout.addLayout(btn_layout)
        info_group.setLayout(info_layout)
        layout.addWidget(info_group, 2)
        
        # Bottom buttons
        bottom_layout = QHBoxLayout()
        
        btn_refresh = QPushButton("🔄 Refresh")
        btn_refresh.clicked.connect(self._load_installed_models)
        bottom_layout.addWidget(btn_refresh)
        
        bottom_layout.addStretch()
        
        btn_cancel = QPushButton("Cancel")
        btn_cancel.clicked.connect(self.reject)
        bottom_layout.addWidget(btn_cancel)
        
        btn_select = QPushButton("✅ Select Model")
        btn_select.setStyleSheet("background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #4CAF50, stop:1 #388E3C); color: white; font-weight: bold; padding: 8px;")
        btn_select.clicked.connect(self._accept_selection)
        bottom_layout.addWidget(btn_select)
        
        layout.addLayout(bottom_layout)
        
    def _load_installed_models(self):
        """Load list of available and installed models"""
        self.model_list.clear()
        
        self.models_dir.mkdir(parents=True, exist_ok=True)
        installed_models = {f.name for f in self.models_dir.glob("*") if f.suffix in ['.pth', '.onnx', '.pt']}
        
        for filename, info in self.AVAILABLE_MODELS.items():
            is_installed = filename in installed_models
            
            item = QListWidgetItem()
            
            # Display text
            display = f"{'✅' if is_installed else '📦'} {info['display_name']}"
            if filename == self.current_model:
                display += " (Current)"
            
            item.setText(display)
            item.setData(Qt.ItemDataRole.UserRole, filename)
            item.setData(Qt.ItemDataRole.UserRole + 1, is_installed)
            
            self.model_list.addItem(item)
            
            # Select current model
            if filename == self.current_model:
                self.model_list.setCurrentItem(item)
        
        self._apply_filter()
        
    def _apply_filter(self):
        """Apply filter to model list"""
        filter_text = self.combo_filter.currentText()
        
        for i in range(self.model_list.count()):
            item = self.model_list.item(i)
            filename = item.data(Qt.ItemDataRole.UserRole)
            is_installed = item.data(Qt.ItemDataRole.UserRole + 1)
            model_info = self.AVAILABLE_MODELS.get(filename, {})
            
            should_show = True
            
            if filter_text == "Installed Only":
                should_show = is_installed
            elif filter_text == "Not Installed":
                should_show = not is_installed
            elif filter_text == "MDX-Net":
                should_show = model_info.get('type') == "MDX-Net"
            elif filter_text == "VR Architecture":
                should_show = model_info.get('type') == "VR Architecture"
            
            item.setHidden(not should_show)
    
    def _on_selection_changed(self, current, previous):
        """Update info panel when selection changes"""
        if not current:
            return
        
        filename = current.data(Qt.ItemDataRole.UserRole)
        is_installed = current.data(Qt.ItemDataRole.UserRole + 1)
        model_info = self.AVAILABLE_MODELS.get(filename, {})
        
        # Update info text
        info_html = f"""
        <h3>{model_info.get('display_name', filename)}</h3>
        <p><b>Type:</b> {model_info.get('type', 'Unknown')}</p>
        <p><b>Quality:</b> {model_info.get('quality', 'N/A')}</p>
        <p><b>Speed:</b> {model_info.get('speed', 'N/A')}</p>
        <p><b>Size:</b> ~{model_info.get('size_mb', 0)} MB</p>
        <p><b>Status:</b> {'✅ Installed' if is_installed else '📦 Not Installed'}</p>
        <p><i>{model_info.get('description', '')}</i></p>
        """
        
        self.info_text.setHtml(info_html)
        
        # Enable/disable buttons
        self.btn_download.setEnabled(not is_installed)
        self.btn_delete.setEnabled(is_installed and filename != self.current_model)
        
    def _download_model(self):
        """Download selected model"""
        current_item = self.model_list.currentItem()
        if not current_item:
            return
        
        filename = current_item.data(Qt.ItemDataRole.UserRole)
        model_info = self.AVAILABLE_MODELS.get(filename, {})
        
        # Confirm download
        reply = QMessageBox.question(
            self,
            "Download Model",
            f"Download {model_info.get('display_name')}?\n\nSize: ~{model_info.get('size_mb')} MB",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
        )
        
        if reply != QMessageBox.StandardButton.Yes:
            return
        
        # Progress dialog
        self.progress_dialog = QProgressDialog(f"Downloading {filename}...", "Cancel", 0, 100, self)
        self.progress_dialog.setWindowModality(Qt.WindowModality.WindowModal)
        self.progress_dialog.show()
        
        # Start download worker
        self.download_worker = ModelDownloadWorker(model_info, self.models_dir)
        self.download_worker.progress_updated.connect(self._on_download_progress)
        self.download_worker.download_complete.connect(self._on_download_complete)
        self.download_worker.error_occurred.connect(self._on_download_error)
        self.download_worker.start()
        
    def _on_download_progress(self, percentage, message):
        """Update download progress"""
        self.progress_dialog.setValue(percentage)
        self.progress_dialog.setLabelText(message)
        
    def _on_download_complete(self, filename):
        """Handle download completion"""
        self.progress_dialog.close()
        QMessageBox.information(self, "Success", f"{filename} downloaded successfully!")
        self._load_installed_models()
        
    def _on_download_error(self, error):
        """Handle download error"""
        self.progress_dialog.close()
        QMessageBox.critical(self, "Download Error", f"Failed to download model:\n{error}")
        
    def _delete_model(self):
        """Delete selected model"""
        current_item = self.model_list.currentItem()
        if not current_item:
            return
        
        filename = current_item.data(Qt.ItemDataRole.UserRole)
        model_info = self.AVAILABLE_MODELS.get(filename, {})
        
        # Confirm deletion
        reply = QMessageBox.question(
            self,
            "Delete Model",
            f"Delete {model_info.get('display_name')}?\n\nThis will free up ~{model_info.get('size_mb')} MB.",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
        )
        
        if reply != QMessageBox.StandardButton.Yes:
            return
        
        try:
            model_path = self.models_dir / filename
            if model_path.exists():
                model_path.unlink()
                QMessageBox.information(self, "Success", f"{filename} deleted successfully!")
                self._load_installed_models()
        except Exception as e:
            QMessageBox.critical(self, "Error", f"Failed to delete model:\n{e}")
        
    def _accept_selection(self):
        """Accept selected model"""
        current_item = self.model_list.currentItem()
        if not current_item:
            QMessageBox.warning(self, "No Selection", "Please select a model first.")
            return
        
        filename = current_item.data(Qt.ItemDataRole.UserRole)
        is_installed = current_item.data(Qt.ItemDataRole.UserRole + 1)
        
        if not is_installed:
            QMessageBox.warning(self, "Model Not Installed", "Please download the model before selecting it.")
            return
        
        self.model_selected.emit(filename)
        self.accept()
        
    def get_selected_model(self) -> str:
        """Get currently selected model filename"""
        current_item = self.model_list.currentItem()
        if current_item:
            return current_item.data(Qt.ItemDataRole.UserRole)
        return self.current_model
