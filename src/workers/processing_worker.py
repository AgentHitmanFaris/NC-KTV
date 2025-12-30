"""
Background processing worker using QThread
"""

from PyQt6.QtCore import QThread, pyqtSignal
from pathlib import Path
from typing import Optional
import logging

from core.audio_processor import AudioProcessor
from core.vocal_remover import VocalRemover
from core.project import Project
from workers.progress_reporter import ProgressReporter
from utils.config import Config


class ProcessingWorker(QThread):
    """Background worker for heavy processing tasks"""
    
    # Signals
    progress_updated = pyqtSignal(float, str)  # (percentage, message)
    stage_changed = pyqtSignal(str)  # stage name
    error_occurred = pyqtSignal(str)  # error message
    processing_complete = pyqtSignal(dict)  # result data
    
    def __init__(self, config: Config, project: Project):
        """Initialize worker
        
        Args:
            config: Application configuration
            project: Project to process
        """
        super().__init__()
        
        self.config = config
        self.project = project
        self.logger = logging.getLogger(__name__)
        
        self._is_cancelled = False
        self.progress_reporter = ProgressReporter()
        
        # Initialize processors
        self.audio_processor = AudioProcessor(temp_dir=project.get_temp_dir())
        self.vocal_remover = VocalRemover(config)
    
    def cancel(self):
        """Cancel processing"""
        self._is_cancelled = True
        self.logger.info("Processing cancelled by user")
    
    def run(self):
        """Main processing loop (runs in background thread)"""
        try:
            self.logger.info(f"Starting processing for: {self.project.source_file}")
            
            # Setup progress stages
            self._setup_progress()
            
            # Stage 1: Audio extraction
            if self._is_cancelled:
                return
            
            self._extract_audio()
            
            # Stage 2: Vocal removal
            if self._is_cancelled:
                return
            
            self._remove_vocals()
            
            # Complete
            self.progress_updated.emit(100.0, "Processing complete!")
            self.processing_complete.emit({
                'audio_file': str(self.project.audio_file),
                'instrumental_file': str(self.project.instrumental_file),
                'vocals_file': str(self.project.vocals_file) if self.project.vocals_file else None
            })
            
        except Exception as e:
            self.logger.error(f"Processing error: {e}", exc_info=True)
            self.error_occurred.emit(str(e))
    
    def _setup_progress(self):
        """Setup progress stages"""
        self.progress_reporter.add_stage("Audio Extraction", weight=20)
        self.progress_reporter.add_stage("Vocal Removal", weight=80)
    
    def _extract_audio(self):
        """Extract audio from source file"""
        self.progress_reporter.start_stage("Audio Extraction")
        self.stage_changed.emit("Audio Extraction")
        
        source_file = self.project.source_file
        temp_dir = self.project.get_temp_dir()
        
        # Check if source is video or audio
        is_valid, msg = self.audio_processor.validate_audio_file(source_file)
        
        if not is_valid:
            raise ValueError(msg)
        
        # Extract/convert audio
        if "video" in msg.lower():
            self.logger.info("Extracting audio from video...")
            self.progress_reporter.update_stage(30)
            self._update_progress()
            
            audio_file = self.audio_processor.extract_audio_from_video(
                source_file,
                temp_dir / f"{source_file.stem}_audio.wav"
            )
        else:
            # Already audio - just convert to standard format
            self.logger.info("Converting audio format...")
            self.progress_reporter.update_stage(30)
            self._update_progress()
            
            audio_file = self.audio_processor.convert_audio(
                source_file,
                temp_dir / f"{source_file.stem}_converted.wav"
            )
        
        self.project.audio_file = audio_file
        self.progress_reporter.update_stage(100)
        self._update_progress()
        self.progress_reporter.complete_stage()
        
        self.logger.info(f"Audio extracted: {audio_file}")
    
    def _remove_vocals(self):
        """Remove vocals using UVR"""
        self.progress_reporter.start_stage("Vocal Removal")
        self.stage_changed.emit("Vocal Removal")
        
        model_name = self.project.settings.uvr_model
        
        self.logger.info(f"Starting vocal removal with model: {model_name}")
        
        # Progress callback
        def on_progress(percentage: float, message: str):
            self.progress_reporter.update_stage(percentage)
            self._update_progress()
        
        # Perform separation
        result = self.vocal_remover.separate_vocals(
            self.project.audio_file,
            model_name=model_name,
            progress_callback=on_progress
        )
        
        # Store result paths
        self.project.instrumental_file = result['instrumental']
        self.project.vocals_file = result.get('vocals')
        
        self.progress_reporter.complete_stage()
        self._update_progress()
        
        self.logger.info(f"Vocal removal complete: {result['instrumental']}")
    
    def _update_progress(self):
        """Emit progress update signal"""
        progress = self.progress_reporter.get_overall_progress()
        message = self.progress_reporter.get_status_message()
        self.progress_updated.emit(progress, message)
