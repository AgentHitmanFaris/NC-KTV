"""
Wizard Mode GUI for NC-KTV
Simple step-by-step interface for beginners
"""

from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QStackedWidget,
    QPushButton, QLabel, QFileDialog, QMessageBox,
    QComboBox, QCheckBox, QGroupBox, QProgressDialog
)
from PyQt6.QtCore import Qt, pyqtSignal, QTimer
from PyQt6.QtGui import QFont, QDragEnterEvent, QDropEvent
from pathlib import Path
from typing import Optional

from core.project import Project
from core.audio_processor import AudioProcessor
from workers.processing_worker import ProcessingWorker
from utils.config import Config


class WizardMode(QWidget):
    """Wizard-style interface for karaoke video creation
    
    Streamlined Flow:
    1. New Project Dialog (File, Mode, Transcription)
    2. Auto-Processing (UVR -> Transcription)
    3. Auto-Switch to Editor
    """
    
    # Signals
    project_created = pyqtSignal(Project)
    
    def __init__(self, config: Config):
        super().__init__()
        self.config = config
        self.project: Optional[Project] = None
        self.processing_worker: Optional[ProcessingWorker] = None
        self.transcription_worker = None # Dynamic import
        self.search_worker = None # Dynamic import
        
        self._init_ui()
        
        # Launch dialog immediately after UI is ready
        from PyQt6.QtCore import QTimer
        QTimer.singleShot(100, self._launch_new_project_dialog)
    
    def _init_ui(self):
        """Initialize UI"""
        layout = QVBoxLayout(self)
        
        # Dashboard showing progress
        self.status_label = QLabel("Waiting for project...")
        self.status_label.setStyleSheet("font-size: 18px; font-weight: bold; color: #ccc;")
        self.status_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        layout.addWidget(self.status_label)
        
        # Progress area
        self.progress_group = QGroupBox("Processing Status")
        self.progress_group.hide()
        progress_layout = QVBoxLayout()
        
        # 1. Base Processing (UVR)
        self.lbl_uvr = QLabel("Step 1: Vocal Separation")
        # Removed accidental QProgressDialog popup
        
        # We use simple QProgressBar for embedded look
        from PyQt6.QtWidgets import QProgressBar
        
        self.bar_uvr = QProgressBar()
        progress_layout.addWidget(self.lbl_uvr)
        progress_layout.addWidget(self.bar_uvr)
        
        # 2. Transcription
        self.lbl_trans = QLabel("Step 2: AI Transcription (Waiting...)")
        self.bar_trans = QProgressBar()
        self.bar_trans.setValue(0)
        self.lbl_trans.setEnabled(False)
        self.bar_trans.setEnabled(False)
        progress_layout.addWidget(self.lbl_trans)
        progress_layout.addWidget(self.bar_trans)
        
        self.progress_group.setLayout(progress_layout)
        layout.addWidget(self.progress_group)
        
        layout.addStretch()
        
        # Cancel button
        self.btn_cancel = QPushButton("Cancel Processing")
        self.btn_cancel.clicked.connect(self._cancel_processing)
        self.btn_cancel.hide()
        layout.addWidget(self.btn_cancel)
        
    def _launch_new_project_dialog(self):
        """Open the new project settings dialog"""
        from gui.dialogs.new_project_dialog import NewProjectDialog
        
        dialog = NewProjectDialog(self.config, self)
        if dialog.exec():
            data = dialog.get_data()
            self._start_workflow(data)
        else:
            # User cancelled dialog. Switch to Editor Mode with blank project.
            from core.project import Project
            self.project_created.emit(Project())
            
    def _start_workflow(self, data: dict):
        """Start the automated workflow"""
        file_path = data['file_path']
        uvr_model = data['uvr_model']
        do_transcribe = data['transcribe']
        whisper_model = data['whisper_model']
        
        # 1. Initialize Project
        try:
            self.project = Project(source_file=file_path)
            # Apply settings
            self.project.settings.uvr_model = uvr_model
            # Store transcription choice for later
            self.project.settings.auto_transcribe = do_transcribe
            self.project.settings.transcription_method = data.get('transcription_method', 'whisper')
            self.project.settings.whisper_model = whisper_model
            
            # 2. Update UI
            self.status_label.setText(f"Processing: {file_path.name}")
            self.progress_group.show()
            self.btn_cancel.show()
            self.bar_uvr.setValue(0)
            self.bar_trans.setValue(0)
            
            # 3. Start UVR
            self._run_uvr()
            
        except Exception as e:
             QMessageBox.critical(self, "Error", f"Failed to initialize project: {e}")
             
    def _run_uvr(self):
        """step 1: Vocal Removal"""
        self.lbl_uvr.setText("Step 1: Validating and Separating Vocals...")
        self.lbl_uvr.setStyleSheet("font-weight: bold; color: #2196F3;")
        
        self.processing_worker = ProcessingWorker(self.config, self.project)
        self.processing_worker.progress_updated.connect(lambda val, msg: self._update_uvr_progress(val, msg))
        self.processing_worker.processing_complete.connect(self._on_uvr_complete)
        self.processing_worker.error_occurred.connect(self._on_error)
        self.processing_worker.start()
        
    def _update_uvr_progress(self, val, msg):
        self.bar_uvr.setValue(int(val))
        self.lbl_uvr.setText(f"Step 1: {msg}")
        
    def _on_uvr_complete(self, result):
        self.bar_uvr.setValue(100)
        self.lbl_uvr.setText("Step 1: Vocal Separation Complete ✅")
        self.lbl_uvr.setStyleSheet("color: green;")
        
        # Proceed to next step
        if self.project.settings.auto_transcribe:
            method = getattr(self.project.settings, 'transcription_method', 'whisper')
            if method == 'online':
                self._run_online_search()
            else:
                self._run_transcription()
        else:
            self._finish()
            
    def _run_transcription(self):
        """Step 2: AI Transcription"""
        self.lbl_trans.setEnabled(True)
        self.bar_trans.setEnabled(True)
        self.lbl_trans.setText("Step 2: Initializing Whisper AI...")
        self.lbl_trans.setStyleSheet("font-weight: bold; color: #2196F3;")
        
        from workers.transcription_worker import TranscriptionWorker
        
        # We need vocals file
        vocals_path = self.project.vocals_file
        if not vocals_path or not vocals_path.exists():
            # Fallback to source if no vocals (unlikely unless instrumental only mode)
             vocals_path = self.project.audio_file
             
        model = self.project.settings.whisper_model
        
        self.transcription_worker = TranscriptionWorker(vocals_path, model_name=model)
        self.transcription_worker.progress_updated.connect(lambda msg: self.lbl_trans.setText(f"Step 2: {msg}"))
        # Simulating progress bar for transcription (it's indeterminate mostly, but we can Pulse)
        self.bar_trans.setRange(0, 0) # Indeterminate
        
        self.transcription_worker.transcription_complete.connect(self._on_transcription_complete)
        self.transcription_worker.error_occurred.connect(self._on_error)
        self.transcription_worker.start()
        
    def _on_transcription_complete(self, result):
        self.bar_trans.setRange(0, 100)
        self.bar_trans.setValue(100)
        self.lbl_trans.setText("Step 2: Transcription Complete ✅")
        self.lbl_trans.setStyleSheet("color: green;")
        
        # Save lyrics to project
        segments = result.get('segments', [])
        # Convert segments to LyricsData structure
        # We need to import LyricsData
        from core.lyrics import LyricsData, LyricsLine, LyricsToken
        
        lyrics_data = LyricsData()
        
        for seg in segments:
            text = seg['text']
            start = seg['start']
            end = seg['end']
            words = []
            
            # If word-level data exists
            if 'words' in seg and seg['words']:
                for w in seg['words']:
                    # Whisper words often have leading space, strip carefully
                    w_text = w['word']
                    words.append({
                        'text': w_text,
                        'start': w['start'],
                        'end': w['end']
                    })
            
            # Create line using helper method
            lyrics_data.add_line(text, start, end, tokens=words)
            
        self.project.lyrics = lyrics_data
        
        # Done
        self._finish()
        
    def _finish(self):
        """All steps done, open editor"""
        # Brief pause to let user see green checks?
        # Use instance timer to prevent RuntimeError if widget is destroyed
        self._finish_timer = QTimer(self)
        self._finish_timer.setSingleShot(True)
        self._finish_timer.timeout.connect(lambda: self.project_created.emit(self.project))
        self._finish_timer.start(800)
        
    def _on_error(self, error_msg):
        QMessageBox.critical(self, "Processing Error", str(error_msg))
        self.status_label.setText("Error occurred. Please try again.")
        self.btn_cancel.hide()
        
    def _cancel_processing(self):
        if self.processing_worker and self.processing_worker.isRunning():
            self.processing_worker.cancel()
        if self.transcription_worker and self.transcription_worker.isRunning():
            self.transcription_worker.cancel()
        if self.search_worker and self.search_worker.isRunning():
            self.search_worker.terminate()
            self.search_worker.wait()
            
        self.status_label.setText("Processing cancelled.")

    def _run_online_search(self):
        """Step 2: Online Lyrics Search"""
        self.lbl_trans.setEnabled(True)
        self.bar_trans.setEnabled(True)
        self.lbl_trans.setText("Step 2: Searching lyrics online...")
        self.lbl_trans.setStyleSheet("font-weight: bold; color: #2196F3;")
        self.bar_trans.setRange(0, 0) # Indeterminate
        
        from workers.online_search_worker import OnlineSearchWorker
        
        # Use source audio file for search (title extraction) and duration
        audio_file = self.project.source_file
        
        self.search_worker = OnlineSearchWorker(audio_file)
        self.search_worker.finished.connect(self._on_search_complete)
        self.search_worker.error_occurred.connect(self._on_search_error)
        self.search_worker.start()
        
    def _on_search_complete(self, lyrics_data):
        self.bar_trans.setRange(0, 100)
        self.bar_trans.setValue(100)
        self.lbl_trans.setText("Step 2: Lyrics Found & Downloaded ✅")
        self.lbl_trans.setStyleSheet("color: green;")
        
        self.project.lyrics = lyrics_data
        
        self._finish()

    def _on_search_error(self, error_msg):
        self.bar_trans.setRange(0, 100)
        self.bar_trans.setValue(0)
        self.lbl_trans.setText(f"Step 2: Search Failed")
        self.lbl_trans.setStyleSheet("color: red;")
        
        # Ask user how to proceed
        reply = QMessageBox.question(
            self, 
            "Lyrics Not Found",
            f"{error_msg}\n\nDo you want to continue with empty lyrics?\n(No will cancel processing)",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No
        )
        
        if reply == QMessageBox.StandardButton.Yes:
            self._finish()
        else:
            self.status_label.setText("Processing stopped.")
            self.btn_cancel.hide()
