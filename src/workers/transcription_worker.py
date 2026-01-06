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
            
            # --- DLL LOADING FIX ---
            # Try to find user-provided cuDNN folders (e.g. cudn12/bin) and add to PATH
            # This helps if the user downloaded DLLs but didn't put them in System32
            import os
            
            # Search in project root and models dir
            search_roots = [Path("."), self.models_dir]
            
            # Also check specific model folders if we are using partial loading
            if str(self.models_dir) in str(self.model_name):
                 search_roots.append(Path(self.model_name))
            
            for root in search_roots:
                if not root.exists(): continue
            for root in search_roots:
                if not root.exists(): continue
                
                # Recursively find any 'bin' folder or folder containing 'cudnn64*.dll'
                # Use rglob but limit depth potentially? No, rglob is fine.
                # Method 1: Look for cudnn64*.dll and add its folder
                for dll in root.rglob("cudnn64*.dll"):
                    dll_dir = dll.parent
                    p_str = str(dll_dir.absolute())
                    if p_str not in os.environ['PATH']:
                        print(f"Adding DLL path: {p_str}")
                        os.environ['PATH'] = p_str + os.pathsep + os.environ['PATH']
                        try:
                            # os.add_dll_directory works on Python 3.8+ Windows
                            os.add_dll_directory(p_str)
                        except:
                            pass
                
                # Keep original check just in case
                for p in root.glob("cudn*/bin"):
                     if p.is_dir():
                        p_str = str(p.absolute())
                        if p_str not in os.environ['PATH']:
                            os.environ['PATH'] = p_str + os.pathsep + os.environ['PATH']
                            try: os.add_dll_directory(p_str)
                            except: pass
                                
            # Add torch lib for zlibwapi.dll if present
            torch_lib = Path("python_embed/Lib/site-packages/torch/lib")
            if torch_lib.exists():
                p_str = str(torch_lib.absolute())
                if p_str not in os.environ['PATH']:
                    os.environ['PATH'] = p_str + os.pathsep + os.environ['PATH']
                    try:
                        os.add_dll_directory(p_str)
                    except:
                        pass
            # -----------------------
            
            # Load model (Faster Whisper)
            # WhisperModel(model_size_or_path, device="cuda" or "cpu", compute_type="float16" or "int8")
            from faster_whisper import WhisperModel
            import torch
            
            device = "cuda" if torch.cuda.is_available() else "cpu"
            compute_type = "float16" if device == "cuda" else "int8"
            
            if device == "cuda":
                self.progress_updated.emit("Using GPU (CUDA) for accelerated transcription...")
            else:
                self.progress_updated.emit("Using CPU for transcription (slower)...")
            
            # fast-whisper handles model downloading automatically if name is passed
            # If local file path is passed (e.g. "large-v3.pt"), it might need conversion or be handled differently.
            # However, faster-whisper typically uses a directory (CTranslate2 format), not just a .pt file.
            # If the user selected a local .pt file (OpenAI format), faster-whisper CANNOT load it directly.
            # But we can fallback to openai-whisper if it's a .pt file!
            
            is_openai_model = str(self.model_name).endswith(".pt")
            
            if is_openai_model:
                self.progress_updated.emit("Detected original OpenAI model format. Using standard engine...")
                import whisper
                
                # Check if it's a local file path or just a name
                model_path = self.model_name
                possible_local = self.models_dir / self.model_name
                if possible_local.exists():
                    model_path = str(possible_local)
                
                self.progress_updated.emit(f"Loading model from: {model_path}")
                model = whisper.load_model(model_path, download_root=download_root)
                
                result = model.transcribe(
                    str(self.audio_file),
                    verbose=False,
                    language=self.language,
                    word_timestamps=True,
                    fp16=(device=="cuda")
                )
                
                segments = []
                for segment in result["segments"]:
                    segments.append({
                        "text": segment["text"].strip(),
                        "start": segment["start"],
                        "end": segment["end"],
                        "words": segment.get("words", [])
                    })
                    
            else:
                # Use Faster Whisper (CTranslate2)
                self.progress_updated.emit("Initializing Faster-Whisper engine...")
                
                def _run_faster_whisper(dev, compute):
                    # Helper to run transcription
                    model = WhisperModel(self.model_name, device=dev, compute_type=compute, download_root=download_root)
                    
                    if self._is_cancelled: return None
                    
                    if self.language:
                        self.progress_updated.emit(f"Transcribing audio in {self.language} ({dev})...")
                    else:
                        self.progress_updated.emit(f"Transcribing audio (auto-detecting language) ({dev})...")
                    
                    seg_gen, info = model.transcribe(
                        str(self.audio_file),
                        language=self.language,
                        word_timestamps=True
                    )
                    
                    if not self.language:
                         self.progress_updated.emit(f"Detected language: {info.language} (probability: {info.language_probability:.2f})")
                    
                    self.progress_updated.emit("Processing segments...")
                    
                    # Consume generator
                    collected = []
                    for s in seg_gen:
                        if self._is_cancelled: break
                        collected.append(s)
                    return collected

                # Try GPU first if available
                segments_raw = None
                
                if device == "cuda":
                    try:
                        segments_raw = _run_faster_whisper("cuda", compute_type) # Try float16
                    except Exception as e:
                        print(f"GPU (float16) failed: {e}")
                        
                        # Try GPU with int8/float32 fallback
                        try:
                            self.progress_updated.emit("GPU (float16) failed, retrying GPU with int8...")
                            segments_raw = _run_faster_whisper("cuda", "int8")
                        except Exception as e2:
                            print(f"GPU (int8) failed: {e2}")
                            self.progress_updated.emit("GPU Error, switching to CPU...")
                            # Fallback to CPU
                            segments_raw = _run_faster_whisper("cpu", "int8")
                else:
                    # CPU only
                    segments_raw = _run_faster_whisper("cpu", "int8")

                if self._is_cancelled: return
                
                segments = []
                if segments_raw:
                    for segment in segments_raw:
                        words = []
                        if segment.words:
                            for w in segment.words:
                                words.append({
                                    "word": w.word,
                                    "start": w.start,
                                    "end": w.end,
                                    "probability": w.probability
                                })
                                
                        segments.append({
                            "text": segment.text.strip(),
                            "start": segment.start,
                            "end": segment.end,
                            "words": words
                        })
            
            self.transcription_complete.emit({"segments": segments})
            
            self.transcription_complete.emit({"segments": segments})
            
        except Exception as e:
            self.error_occurred.emit(str(e))
            
    def cancel(self):
        self._is_cancelled = True
