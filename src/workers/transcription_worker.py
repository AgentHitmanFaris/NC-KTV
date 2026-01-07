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
            # Reverted to standard embedded python loading to prevent conflicts
            
            # Load model (Faster Whisper)
            # WhisperModel(model_size_or_path, device="cuda" or "cpu", compute_type="float16" or "int8")
            from faster_whisper import WhisperModel
            import torch
            
            device = "cuda" if torch.cuda.is_available() else "cpu"
            
            # Intelligent GPU detection for optimal precision
            compute_type = "int8"  # Default for CPU
            gpu_info = ""
            
            if device == "cuda":
                try:
                    gpu_name = torch.cuda.get_device_name(0)
                    gpu_info = f" ({gpu_name})"
                    
                    # Detect GPU architecture and choose optimal precision
                    gpu_lower = gpu_name.lower()
                    
                    # Pascal Architecture (GTX 10-series): Use INT8
                    # - GTX 1060, 1070, 1080, Titan X/Xp
                    # - Limited FP16 support (emulated, not native)
                    if any(x in gpu_lower for x in ["gtx 10", "titan x", "tesla p"]):
                        compute_type = "int8"
                        self.progress_updated.emit(
                            f"Using GPU{gpu_info} with INT8 precision\n"
                            f"(Optimized for Pascal architecture - faster than FP16 on this GPU)"
                        )
                    
                    # Turing/Ampere/Ada Architecture (RTX series): Use FP16
                    # - RTX 20/30/40 series, A-series GPUs
                    # - Native FP16 tensor cores
                    elif any(x in gpu_lower for x in ["rtx", "tesla t4", "tesla a", "a100", "a6000", "l4", "l40"]):
                        compute_type = "float16"
                        self.progress_updated.emit(
                            f"Using GPU{gpu_info} with FP16 precision\n"
                            f"(Utilizing native tensor cores for maximum speed)"
                        )
                    
                    # Maxwell and older (GTX 9-series and below): Use INT8
                    elif any(x in gpu_lower for x in ["gtx 9", "gtx 8", "gtx 7"]):
                        compute_type = "int8"
                        self.progress_updated.emit(
                            f"Using GPU{gpu_info} with INT8 precision\n"
                            f"(Optimized for Maxwell/Kepler architecture)"
                        )
                    
                    # Unknown/Future GPUs: Try FP16 with fallback
                    else:
                        compute_type = "float16"
                        self.progress_updated.emit(
                            f"Using GPU{gpu_info} with FP16 precision\n"
                            f"(Will fallback to INT8 if compatibility issues occur)"
                        )
                        
                except Exception as e:
                    # If we can't detect GPU, use safer INT8
                    compute_type = "int8"
                    self.progress_updated.emit(
                        f"Using GPU (CUDA) with INT8 precision\n"
                        f"(GPU detection failed: {e})"
                    )
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
                    
                    # Check for local model folder first
                    local_model_path = self.models_dir / self.model_name
                    if local_model_path.exists():
                        # Use exact local path
                        load_path = str(local_model_path)
                    else:
                        # Use name (allows auto-download)
                        load_path = self.model_name

                    model = WhisperModel(load_path, device=dev, compute_type=compute, download_root=download_root)
                    
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

                # Execute transcription with optimal compute type
                segments_raw = None
                
                if device == "cuda":
                    try:
                        segments_raw = _run_faster_whisper("cuda", compute_type)
                    except Exception as e:
                        print(f"GPU ({compute_type}) failed: {e}")
                        
                        # Fallback strategy based on what failed
                        if compute_type == "float16":
                            # FP16 failed (unknown GPU), try INT8 on GPU
                            try:
                                self.progress_updated.emit("GPU (FP16) failed, retrying with INT8...")
                                segments_raw = _run_faster_whisper("cuda", "int8")
                            except Exception as e2:
                                print(f"GPU (int8) failed: {e2}")
                                self.progress_updated.emit("GPU failed completely, switching to CPU...")
                                segments_raw = _run_faster_whisper("cpu", "int8")
                        else:
                            # INT8 failed on GPU (rare), go straight to CPU
                            msg = f"GPU failed: {str(e)}"
                            print(msg)
                            try:
                                with open("gpu_debug.log", "a") as f:
                                    f.write(f"\n[GPU ERROR] {msg}\n")
                            except: pass
                            
                            # Use a friendly message for the UI
                            self.progress_updated.emit("Switching to standard CPU mode...")
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
