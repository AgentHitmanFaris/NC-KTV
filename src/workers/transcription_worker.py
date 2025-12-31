"""
Worker for AI Lyrics Transcription using OpenAI Whisper
"""

from PyQt6.QtCore import QThread, pyqtSignal
import whisper
from pathlib import Path
import warnings

# Suppress warnings
warnings.filterwarnings("ignore")

class TranscriptionWorker(QThread):
    """Background worker for Whisper transcription"""
    
    progress_updated = pyqtSignal(str)  # Status message
    transcription_complete = pyqtSignal(dict)  # Result data
    error_occurred = pyqtSignal(str)
    
    def __init__(self, audio_file: Path, model_name: str = "small", language: str = None):
        super().__init__()
        self.audio_file = audio_file
        self.model_name = model_name
        self.language = language
        self._is_cancelled = False
        
        # Check for local models first
        self.models_dir = Path("models/whisper")
        
    def run(self):
        try:
            if self._is_cancelled: return
            
            self.progress_updated.emit("Loading Whisper model...")
            
            # Determine model path
            download_root = str(self.models_dir.absolute())
            self.models_dir.mkdir(parents=True, exist_ok=True)
            
            # Load model
            # Whisper handles local files if they exist in the download_root
            model = whisper.load_model(self.model_name, download_root=download_root)
            
            # Check for GPU
            import torch
            device = "cuda" if torch.cuda.is_available() else "cpu"
            if device == "cuda":
                self.progress_updated.emit("Using GPU (CUDA) for transcription...")
            
            if self._is_cancelled: return
            
            if self.language:
                self.progress_updated.emit(f"Transcribing audio in {self.language}...")
            else:
                self.progress_updated.emit("Transcribing audio (auto-detecting language)...")
            
            # Transcribe
            # We use the vocals file usually, but instrumental-only might not work well.
            # Assuming we are passing the vocals file (if available) or original audio.
            result = model.transcribe(
                str(self.audio_file),
                verbose=False,
                language=self.language,
                word_timestamps=True, # REQUEST WORD TIMESTAMPS
                fp16=(device=="cuda") # Enable fp16 on GPU for speed
            )
            
            if self._is_cancelled: return
            
            self.progress_updated.emit("Processing segments...")
            
            # Process result into our format
            segments = []
            for segment in result["segments"]:
                segments.append({
                    "text": segment["text"].strip(),
                    "start": segment["start"],
                    "end": segment["end"],
                    "words": segment.get("words", []) # Extract words
                })
            
            self.transcription_complete.emit({"segments": segments})
            
        except Exception as e:
            self.error_occurred.emit(str(e))
            
    def cancel(self):
        self._is_cancelled = True
