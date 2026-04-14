"""
NC-KTV Python Bridge — AI Transcription & Vocal Separation
===========================================================
Provides CLI commands for the C++ Qt6 application to invoke AI models:
  - transcribe    : Vanilla OpenAI Whisper (legacy, attention-based word timestamps)
  - transcribe-x  : WhisperX engine (forced alignment via Wav2Vec2, ±30ms precision)
  - align         : Force-align known lyrics text to audio (WhisperX)
  - gemini        : Google Gemini API transcription
  - separate      : UVR vocal separation via audio-separator

The JSON output format is consistent across all engines so the C++ side
can consume results identically regardless of which engine was used.
"""

import sys
import os
import json
import argparse
import warnings
import re
from pathlib import Path

# ─── Locate and Expose FFmpeg Globally (MUST RUN BEFORE OTHER IMPORTS) ────────
# This ensures that libraries like audio-separator and imageio find the correct binaries.
_script_dir = Path(__file__).parent.resolve()

# Handle PyInstaller _MEI_xxxx temporary directory or executable directory
if getattr(sys, 'frozen', False):
    # If running as a bundled executable, _script_dir is the dir containing the EXE
    # or the temp folder if onedir/onefile is used.
    _base_dir = Path(sys._MEIPASS).resolve() if hasattr(sys, '_MEIPASS') else Path(sys.executable).parent.resolve()
    _exe_dir = Path(sys.executable).parent.resolve()
else:
    _base_dir = _script_dir
    _exe_dir = _script_dir

_possible_ffmpeg_dirs = [
    _exe_dir / "ffmpeg" / "bin",
    _base_dir / "ffmpeg" / "bin",
    _exe_dir / "bin",
    Path("D:/NC-KTV/ffmpeg/bin"),
    Path("C:/ffmpeg/bin"),
]

for _fd in _possible_ffmpeg_dirs:
    if (_fd / "ffmpeg.exe").exists():
        # Prepend to PATH to ensure our version takes priority
        os.environ["PATH"] = str(_fd.absolute()) + os.pathsep + os.environ.get("PATH", "")
        # Also set imageio_ffmpeg's internal path if possible, though PATH is usually enough
        break

# Suppress annoying warnings
warnings.filterwarnings("ignore")

# Ensure this script's directory is on sys.path for sibling imports
if str(_script_dir) not in sys.path:
    sys.path.insert(0, str(_script_dir))

# Force PyInstaller to detect dynamic imports used by audio_separator
# These imports are moved AFTER path setup to ensure they pick up our FFmpeg if they check on import
try:
    import onnx
    import onnxruntime
    import audio_separator.separator.architectures.mdxc
    import audio_separator.separator.architectures.mdx_separator
    import audio_separator.separator.architectures.vrc
    import audio_separator.separator.architectures.demucs_separator
    import imageio_ffmpeg
except ImportError:
    pass



# ─── Audio Preprocessing Utilities ───────────────────────────────────────────

def detect_vocal_region(audio, sr=16000, threshold_db=-40, min_duration=1.0):
    """Detect the start and end of the vocal region in audio using RMS energy.
    
    Returns (start_seconds, end_seconds) of the detected vocal region.
    """
    import numpy as np

    frame_length = int(0.05 * sr)  # 50ms frames
    hop_length = frame_length

    # Compute RMS energy per frame
    n_frames = len(audio) // hop_length
    if n_frames == 0:
        return 0.0, len(audio) / sr

    rms = np.array([
        np.sqrt(np.mean(audio[i * hop_length : i * hop_length + frame_length] ** 2))
        for i in range(n_frames)
    ])

    # Convert threshold from dB
    threshold = 10 ** (threshold_db / 20)

    # Find first and last frame above threshold
    active = rms > threshold
    active_indices = np.where(active)[0]

    if len(active_indices) == 0:
        return 0.0, len(audio) / sr

    start_frame = max(0, active_indices[0] - int(0.5 / (hop_length / sr)))  # 0.5s padding
    end_frame = min(n_frames - 1, active_indices[-1] + int(0.5 / (hop_length / sr)))

    start_sec = max(0.0, start_frame * hop_length / sr)
    end_sec = min(len(audio) / sr, (end_frame * hop_length + frame_length) / sr)

    return round(start_sec, 3), round(end_sec, 3)


def highpass_filter(audio, sr=16000, cutoff=80):
    """Apply a simple high-pass filter to remove rumble below cutoff Hz."""
    try:
        from scipy.signal import butter, sosfilt
        sos = butter(5, cutoff, btype='high', fs=sr, output='sos')
        return sosfilt(sos, audio).astype(audio.dtype)
    except ImportError:
        return audio  # scipy not available, skip filtering


def normalize_rms(audio, target_db=-20):
    """Normalize audio to a target RMS level (dB)."""
    import numpy as np

    rms = np.sqrt(np.mean(audio ** 2))
    if rms < 1e-10:
        return audio

    target_rms = 10 ** (target_db / 20)
    gain = target_rms / rms
    return (audio * gain).astype(audio.dtype)


# ─── WhisperX Engine (Phase 1: Forced Alignment Transcription) ───────────────

def run_transcription_whisperx(audio_file, model_name="large-v3", language=None,
                                beam_size=5, batch_size=16):
    """Transcribe audio using WhisperX with forced alignment for precise word timestamps.
    
    Pipeline:
      1. Load audio, detect vocal region, preprocess (highpass + normalize)
      2. Detect language (multi-window or user override)
      3. Transcribe with WhisperX (batched CTC model)
      4. Filter hallucinations
      5. Run forced alignment via Wav2Vec2 phoneme model
      6. Recover dropped words + interpolate missing timestamps
      7. Build final segments with clean word-level timing
    """
    import whisperx
    import torch
    import numpy as np

    if language and language.lower() == "auto":
        language = None

    device = "cuda" if torch.cuda.is_available() else "cpu"
    compute_type = "float16" if device == "cuda" else "float32"

    # Force CPU for MPS (not well-supported by WhisperX alignment)
    if device == "mps":
        device = "cpu"
        compute_type = "float32"

    print(f"[WhisperX] Using device: {device}, compute_type: {compute_type}", file=sys.stderr)

    # ── Step 1: Load and preprocess audio ────────────────────────────────────
    print(f"[WhisperX] Loading audio: {audio_file}", file=sys.stderr)
    full_audio = whisperx.load_audio(str(audio_file))
    duration_secs = len(full_audio) / 16000
    print(f"[WhisperX] Audio loaded: {len(full_audio)} samples ({duration_secs:.1f}s)", file=sys.stderr)

    # Detect vocal region and trim
    print(f"[WhisperX] Detecting vocal region...", file=sys.stderr)
    vocal_start, vocal_end = detect_vocal_region(full_audio)
    trim_start = int(vocal_start * 16000)
    trim_end = int(vocal_end * 16000)
    audio = full_audio[trim_start:trim_end]
    trimmed_duration = len(audio) / 16000
    print(f"[WhisperX] Vocal region: {vocal_start:.1f}s - {vocal_end:.1f}s ({trimmed_duration:.1f}s)", file=sys.stderr)

    # Apply audio preprocessing
    audio = highpass_filter(audio)
    audio = normalize_rms(audio)

    # ── Step 2: Load model and transcribe ────────────────────────────────────
    print(f"[WhisperX] Loading model: {model_name} (beam_size={beam_size}, batch_size={batch_size})", file=sys.stderr)

    # Resolve model cache directory
    script_dir = Path(__file__).parent
    possible_dirs = [
        script_dir / "models" / "whisper",
        Path.cwd() / "models" / "whisper"
    ]
    
    # WhisperX handles model downloads itself via HuggingFace,
    # but we can set the cache directory
    hf_cache = script_dir / "models" / "huggingface"
    hf_cache.mkdir(parents=True, exist_ok=True)
    os.environ.setdefault("HF_HOME", str(hf_cache))

    asr_options = {
        "beam_size": beam_size,
        "initial_prompt": (
            "Everything before GO is INSTRUCTIONS. DON'T INCLUDE IN TRANSCRIPT. "
            "Song Lyrics transcript. Split lines with punctuation. "
            "No annotations or descriptions. "
            "GO"
        ),
    }

    if language:
        print(f"[WhisperX] Using language override: '{language}'", file=sys.stderr)
        model = whisperx.load_model(
            model_name, device, compute_type=compute_type,
            task="transcribe", language=language,
            asr_options=asr_options,
        )
    else:
        model = whisperx.load_model(
            model_name, device, compute_type=compute_type,
            task="transcribe",
            asr_options=asr_options,
        )

    print(f"[WhisperX] Transcribing...", file=sys.stderr)
    result = model.transcribe(
        audio,
        batch_size=batch_size,
        task="transcribe",
        language=language,
        chunk_size=30,
    )

    detected_language = result.get("language", language or "en")
    print(f"[WhisperX] Detected language: '{detected_language}'", file=sys.stderr)

    # Free the Whisper model memory before alignment
    del model
    _free_gpu()

    # ── Step 3: Offset timestamps back to original timeline ──────────────────
    raw_segments = result.get("segments", [])
    for seg in raw_segments:
        seg["start"] = round(seg.get("start", 0) + vocal_start, 3)
        seg["end"] = round(seg.get("end", 0) + vocal_start, 3)

    total_raw_words = sum(len(s.get("text", "").split()) for s in raw_segments)
    print(f"[WhisperX] Transcription: {len(raw_segments)} segments, ~{total_raw_words} words", file=sys.stderr)

    # ── Step 4: Filter hallucinations ────────────────────────────────────────
    try:
        from hallucination_filter import filter_hallucinated_segments
        raw_segments = filter_hallucinated_segments(raw_segments, duration_secs)
    except ImportError:
        print("[WhisperX] Warning: hallucination_filter not found, skipping", file=sys.stderr)

    # ── Step 5: Forced alignment via Wav2Vec2 ────────────────────────────────
    align_device = "cpu" if device == "mps" else device

    print(f"[WhisperX] Running forced alignment (lang={detected_language}, device={align_device})...", file=sys.stderr)

    try:
        align_model, metadata = whisperx.load_align_model(
            language_code=detected_language, device=align_device
        )
        aligned = whisperx.align(
            raw_segments, align_model, metadata, full_audio, align_device
        )
        del align_model
        _free_gpu()
    except RuntimeError as e:
        if "out of memory" in str(e).lower():
            print("[WhisperX] OOM on GPU, falling back to CPU for alignment...", file=sys.stderr)
            _free_gpu()
            align_model, metadata = whisperx.load_align_model(
                language_code=detected_language, device="cpu"
            )
            aligned = whisperx.align(
                raw_segments, align_model, metadata, full_audio, "cpu"
            )
            del align_model
        else:
            raise
    except Exception as e:
        print(f"[WhisperX] Alignment failed ({e}), using raw timestamps", file=sys.stderr)
        aligned = {"segments": raw_segments}

    # ── Step 6: Recover dropped words + interpolate ──────────────────────────
    output_segments = aligned.get("segments", [])
    _recover_dropped_words(raw_segments, output_segments)

    all_words = _interpolate_words(output_segments)

    # Remove hallucinated words
    try:
        from hallucination_filter import remove_hallucinated_words
        all_words = remove_hallucinated_words(all_words)
    except ImportError:
        pass

    # ── Step 7: Build final segments ─────────────────────────────────────────
    segments = _build_segments(all_words)

    print(f"[WhisperX] Final result: {len(segments)} segments, "
          f"{sum(len(s.get('words', [])) for s in segments)} words", file=sys.stderr)

    output = {
        "segments": segments,
        "engine": "whisperx",
        "language": detected_language,
    }
    print(json.dumps(output))


# ─── Phase 2: Lyrics Forced Alignment ───────────────────────────────────────

def run_alignment(audio_file, lyrics_text, language="en"):
    """Force-align known lyrics text to audio using WhisperX.
    
    This is the biggest win for karaoke: when lyrics are already known
    (from Gemini, LRC import, or manual input), skip transcription entirely
    and just run forced alignment for precise word timing.
    
    Args:
        audio_file: Path to the audio file (vocals preferred).
        lyrics_text: Full lyrics as a single string (lines separated by newlines).
        language: ISO language code for the alignment model.
    """
    import whisperx
    import torch
    import numpy as np

    if language and language.lower() == "auto":
        language = "en"  # Alignment needs a specific language

    device = "cuda" if torch.cuda.is_available() else "cpu"

    # Force CPU for MPS
    if device == "mps":
        device = "cpu"

    print(f"[Align] Using device: {device}", file=sys.stderr)
    print(f"[Align] Loading audio: {audio_file}", file=sys.stderr)

    audio = whisperx.load_audio(str(audio_file))
    duration_secs = len(audio) / 16000
    print(f"[Align] Audio: {duration_secs:.1f}s", file=sys.stderr)

    # Detect vocal region
    vocal_start, vocal_end = detect_vocal_region(audio)
    print(f"[Align] Vocal region: {vocal_start:.1f}s - {vocal_end:.1f}s", file=sys.stderr)

    # Parse lyrics into clean lines
    clean_lines = [line.strip() for line in lyrics_text.strip().split("\n") if line.strip()]
    print(f"[Align] Lyrics: {len(clean_lines)} lines", file=sys.stderr)

    if not clean_lines:
        print(json.dumps({"error": "No lyrics text provided", "segments": []}))
        return

    # Create a single segment with all lyrics for alignment
    full_text = " ".join(clean_lines)
    raw_segments = [{"text": full_text, "start": vocal_start, "end": vocal_end}]

    # Load alignment model and run
    align_device = "cpu" if device == "mps" else device

    print(f"[Align] Loading alignment model (lang={language})...", file=sys.stderr)
    try:
        align_model, metadata = whisperx.load_align_model(
            language_code=language, device=align_device
        )
        align_result = whisperx.align(
            raw_segments, align_model, metadata, audio, align_device
        )
        del align_model
        _free_gpu()
    except RuntimeError as e:
        if "out of memory" in str(e).lower():
            print("[Align] OOM, falling back to CPU...", file=sys.stderr)
            _free_gpu()
            align_model, metadata = whisperx.load_align_model(
                language_code=language, device="cpu"
            )
            align_result = whisperx.align(
                raw_segments, align_model, metadata, audio, "cpu"
            )
            del align_model
        else:
            raise

    # Map aligned words back to original lyric lines
    segments = _map_words_to_lines(align_result, clean_lines)

    print(f"[Align] Result: {len(segments)} segments, "
          f"{sum(len(s.get('words', [])) for s in segments)} words", file=sys.stderr)

    output = {
        "segments": segments,
        "engine": "whisperx-align",
        "language": language,
    }
    print(json.dumps(output))


# ─── Legacy Whisper Engine ───────────────────────────────────────────────────

def run_transcription(audio_file, model_name="small", language=None):
    """Legacy transcription using vanilla OpenAI Whisper with attention-based word timestamps."""
    import whisper
    import torch
    
    if language and language.lower() == "auto":
        language = None
        
    device = "cuda" if torch.cuda.is_available() else "cpu"
    print(f"DEBUG: Using device: {device}", file=sys.stderr)
    
    # Load model (cached in models/whisper)
    script_dir = Path(__file__).parent
    
    # Priority search directories: Portable app folder -> Dev project folder -> CWD
    possible_dirs = [
        script_dir / "models" / "whisper",
        Path("D:/Document/NC-Project/NC-KTV/models/whisper"),
        Path.cwd() / "models" / "whisper"
    ]
    
    final_models_dir = script_dir / "models" / "whisper" # Default fallback
    found_model_path = None
    
    for d in possible_dirs:
        if not d.exists(): continue
        
        # Check for model name directly or known alias, including obfuscated .dat extension
        look_for = [f"{model_name}.pt", f"{model_name}.dat"]
        if model_name == "turbo":
            look_for.extend(["large-v3-turbo.pt", "large-v3-turbo.dat"])
            
        for name in look_for:
            test_path = d / name
            if test_path.exists():
                size_mb = test_path.stat().st_size / (1024 * 1024)
                if size_mb > 100: # Found a reasonably sized model
                    final_models_dir = d
                    found_model_path = test_path
                    print(f"DEBUG: Found existing model at: {found_model_path}", file=sys.stderr)
                    break
        if found_model_path: break

    # If we found a path, load it directly to avoid any download logic
    if found_model_path is not None:
        model_to_load = str(found_model_path.absolute())
        print(f"\n[INFO] Successfully found fully downloaded model at: {model_to_load}\n", file=sys.stderr)
        model = whisper.load_model(model_to_load, device=device)
    else:
        # Fallback to standard whisper download logic
        final_models_dir.mkdir(parents=True, exist_ok=True)
        
        # Clean up any partial garbage if it exists
        garbage = final_models_dir / f"{model_name}.pt"
        if not garbage.exists() and model_name == "turbo":
            garbage = final_models_dir / "large-v3-turbo.pt"
            
        if garbage.exists():
            size_mb = garbage.stat().st_size / (1024 * 1024)
            if size_mb < 700: # Turbo and large models are >> 700MB. If it's smaller, it's corrupt.
                print(f"\n[WARNING] Found an incomplete model file ({garbage.name}) which is only {size_mb:.1f} MB. Deleting and restarting download...\n", file=sys.stderr)
                try: garbage.unlink()
                except: pass
        
        print(f"\n[INFO] Model '{model_name}' is not present or was incomplete. Whisper will now download it to {final_models_dir.absolute()}.", file=sys.stderr)
        print(f"[INFO] Please wait. The 'turbo' model is ~1.5 GB.\n", file=sys.stderr)
        
        model = whisper.load_model(model_name, download_root=str(final_models_dir.absolute()), device=device)
    
    # Transcribe with word timestamps for "fast and proper" sync
    result = model.transcribe(
        str(audio_file),
        verbose=False,
        language=language,
        word_timestamps=True,
        fp16=(device == "cuda")
    )
    
    # Format result for C++ app
    output = {"segments": [], "engine": "whisper"}
    for segment in result.get("segments", []):
        output["segments"].append({
            "text": segment["text"].strip(),
            "start": segment["start"],
            "end": segment["end"],
            "words": segment.get("words", [])
        })
    
    print(json.dumps(output))


# ─── Gemini Engine ───────────────────────────────────────────────────────────

def run_gemini_transcription(audio_file, api_key=None, model_name="gemini-1.5-pro", language=None):
    """Transcribe audio using the Google Gemini API with word-level timestamps."""
    try:
        import google.generativeai as genai
    except ImportError:
        print(json.dumps({"error": "google-generativeai package not installed. Run: pip install google-generativeai"}))
        sys.exit(1)

    # Resolve API key: arg > env var
    resolved_key = api_key or os.environ.get("GEMINI_API_KEY", "")
    if not resolved_key:
        print(json.dumps({"error": "No Gemini API key provided. Pass --api-key or set GEMINI_API_KEY env var."}))
        sys.exit(1)

    genai.configure(api_key=resolved_key)

    audio_path = Path(audio_file)
    if not audio_path.exists():
        print(json.dumps({"error": f"Audio file not found: {audio_file}"}))
        sys.exit(1)

    print(f"[Gemini] Uploading audio file: {audio_path.name}", file=sys.stderr)

    # Upload file using the Files API
    file_ref = genai.upload_file(str(audio_path.absolute()), mime_type="audio/mpeg")
    print(f"[Gemini] Upload complete. URI: {file_ref.uri}", file=sys.stderr)

    # Build the prompt
    lang_hint = f"The primary language is {language}." if language and language.lower() != "auto" else ""
    prompt = f"""You are a precise audio transcription assistant. Transcribe the given audio file completely and return ONLY a valid JSON object (no markdown, no explanation) in the following format:

{{
  "segments": [
    {{
      "text": "full line text here",
      "start": 0.0,
      "end": 2.5,
      "words": [
        {{"word": "full", "start": 0.0, "end": 0.3}},
        {{"word": "line", "start": 0.35, "end": 0.6}},
        {{"word": "text", "start": 0.65, "end": 0.9}},
        {{"word": "here", "start": 0.95, "end": 1.2}}
      ]
    }}
  ]
}}

Rules:
- Each segment represents one lyric line or natural phrase.
- All timestamps are in seconds (float).
- Each word must have its own start and end time.
- Be as precise as possible with timestamps.
- Do NOT include any text before or after the JSON.
{lang_hint}"""

    print(f"[Gemini] Sending transcription request (model: {model_name})...", file=sys.stderr)

    gen_model = genai.GenerativeModel(model_name)
    response = gen_model.generate_content([prompt, file_ref])

    raw = response.text.strip()

    # Strip markdown code fences if present
    if raw.startswith("```"):
        lines_raw = raw.split("\n")
        inner = []
        in_block = False
        for line in lines_raw:
            if line.startswith("```") and not in_block:
                in_block = True
                continue
            if line.startswith("```") and in_block:
                break
            if in_block:
                inner.append(line)
        raw = "\n".join(inner)

    try:
        result = json.loads(raw)
        # Validate structure
        if "segments" not in result:
            raise ValueError("Response missing 'segments' key")
        result["engine"] = "gemini"
        print(f"[Gemini] Parsed {len(result['segments'])} segments successfully.", file=sys.stderr)
        print(json.dumps(result))
    except (json.JSONDecodeError, ValueError) as e:
        print(f"[Gemini] Failed to parse response as JSON: {e}", file=sys.stderr)
        print(f"[Gemini] Raw response:\n{raw}", file=sys.stderr)
        print(json.dumps({"error": f"JSON parse failed: {e}", "raw": raw}))
        sys.exit(1)


# ─── Vocal Separation ───────────────────────────────────────────────────────

def run_separation(audio_file, model_name, output_dir):
    from audio_separator.separator import Separator

    # Monkey patch to avoid AttributeError: 'NoneType' object has no attribute 'version' in PyInstaller builds
    original_get_distribution = Separator.get_package_distribution
    def dummy_get_package_distribution(self, package_name):
        dist = original_get_distribution(self, package_name)
        if dist is None:
            class DummyDist:
                version = "unknown"
            return DummyDist()
        return dist
    Separator.get_package_distribution = dummy_get_package_distribution

    # Initialize separator
    script_dir = Path(__file__).parent
    models_dir = script_dir / "models" / "uvr"
    
    if not models_dir.exists():
        models_dir = Path("models/uvr")
        
    models_dir.mkdir(parents=True, exist_ok=True)
    
    sep = Separator(
        output_dir=str(output_dir),
        model_file_dir=str(models_dir.absolute()),
        output_format="WAV"
    )
    
    print(f"DEBUG: Loading model {model_name}...", file=sys.stderr)
    sep.load_model(model_name)
    
    output_files = sep.separate(str(audio_file))
    
    if not output_files:
        raise RuntimeError("audio_separator returned an empty file list. Separation failed silently.")

    print(f"DEBUG: Separated files: {output_files}", file=sys.stderr)
    
    # The library returns a list of filenames created
    print(json.dumps({"files": output_files}))


# ─── Word Recovery & Interpolation Utilities ─────────────────────────────────

def _normalize_word(word: str) -> str:
    """Normalize a word for comparison (strip punctuation, lowercase)."""
    return re.sub(r"[^\w]", "", word).lower()


def _free_gpu():
    """Free GPU memory after model unloading."""
    import gc
    gc.collect()
    try:
        import torch
        if torch.cuda.is_available():
            torch.cuda.empty_cache()
    except Exception:
        pass


def _recover_dropped_words(raw_segments: list, aligned_segments: list):
    """Compare input vs aligned output and re-inject words the aligner silently dropped.
    
    WhisperX alignment sometimes drops words that couldn't be matched to phonemes.
    This function detects those and re-inserts them so they can be interpolated later.
    """
    if len(raw_segments) != len(aligned_segments):
        print(
            f"[WhisperX] Segment count mismatch (raw={len(raw_segments)}, "
            f"aligned={len(aligned_segments)}), skipping word recovery",
            file=sys.stderr,
        )
        return

    total_recovered = 0

    for raw_seg, aligned_seg in zip(raw_segments, aligned_segments):
        raw_text = raw_seg.get("text", "")
        raw_words = raw_text.split()
        aligned_words = aligned_seg.get("words", [])

        if not raw_words:
            continue

        aligned_norms = [_normalize_word(w.get("word", "")) for w in aligned_words]

        # Match raw words to aligned words
        matched_raw = set()
        matched_aligned = set()
        ai = 0

        for ri, rw in enumerate(raw_words):
            rn = _normalize_word(rw)
            if not rn:
                matched_raw.add(ri)
                continue
            for si in range(ai, min(ai + 8, len(aligned_norms))):
                if si not in matched_aligned and aligned_norms[si] == rn:
                    matched_raw.add(ri)
                    matched_aligned.add(si)
                    ai = si + 1
                    break

        missing_indices = [i for i in range(len(raw_words)) if i not in matched_raw]
        if not missing_indices:
            continue

        seg_start = aligned_seg.get("start", raw_seg.get("start", 0))
        seg_end = aligned_seg.get("end", raw_seg.get("end", 0))
        missing_text = " ".join(raw_words[i] for i in missing_indices)
        print(
            f"[WhisperX] Recovering {len(missing_indices)} dropped words in segment "
            f"[{seg_start:.1f}-{seg_end:.1f}]: {missing_text}",
            file=sys.stderr,
        )

        for orig_idx in reversed(missing_indices):
            insert_pos = len(aligned_words)
            for check_ri in range(orig_idx + 1, len(raw_words)):
                check_norm = _normalize_word(raw_words[check_ri])
                for ai_pos, an in enumerate(aligned_norms):
                    if an == check_norm:
                        insert_pos = ai_pos
                        break
                if insert_pos < len(aligned_words):
                    break

            recovered = {"word": raw_words[orig_idx]}
            aligned_words.insert(insert_pos, recovered)
            aligned_norms.insert(insert_pos, _normalize_word(raw_words[orig_idx]))
            total_recovered += 1

        aligned_seg["words"] = aligned_words

    if total_recovered > 0:
        print(f"[WhisperX] Total recovered words: {total_recovered}", file=sys.stderr)


def _interpolate_words(output_segments: list) -> list:
    """Extract all words from aligned segments, interpolating missing timestamps.
    
    After forced alignment, some words may lack timestamps. This function
    distributes timestamps evenly between known anchor points.
    """
    all_words = []
    total_aligned = 0
    total_interpolated = 0

    for seg in output_segments:
        raw_words = seg.get("words", [])
        if not raw_words:
            continue

        seg_start = seg.get("start", 0)
        seg_end = seg.get("end", 0)

        entries = []
        for w in raw_words:
            word_text = w.get("word", "").strip()
            if not word_text:
                continue
            has_ts = "start" in w and "end" in w
            entries.append({
                "word": word_text,
                "start": w.get("start"),
                "end": w.get("end"),
                "score": w.get("score"),
                "aligned": has_ts,
            })

        if not entries:
            continue

        anchors = [(i, e) for i, e in enumerate(entries) if e["aligned"]]

        if not anchors:
            # No anchors — distribute evenly across segment
            n = len(entries)
            dur = (seg_end - seg_start) / n if seg_end > seg_start else 0.1
            for j, e in enumerate(entries):
                e["start"] = seg_start + j * dur
                e["end"] = seg_start + (j + 1) * dur
        else:
            # Fill gaps between anchors
            first_idx = anchors[0][0]
            if first_idx > 0:
                _fill_range(entries, 0, first_idx, seg_start, entries[first_idx]["start"])

            for ai_idx in range(len(anchors) - 1):
                a_idx = anchors[ai_idx][0]
                b_idx = anchors[ai_idx + 1][0]
                if b_idx - a_idx > 1:
                    _fill_range(entries, a_idx + 1, b_idx, entries[a_idx]["end"], entries[b_idx]["start"])

            last_idx = anchors[-1][0]
            if last_idx < len(entries) - 1:
                _fill_range(entries, last_idx + 1, len(entries), entries[last_idx]["end"], seg_end)

        seg_interpolated = sum(1 for e in entries if not e["aligned"])
        total_aligned += len(entries) - seg_interpolated
        total_interpolated += seg_interpolated

        for e in entries:
            if e["start"] is None or e["end"] is None:
                continue
            word_entry = {
                "word": e["word"],
                "start": round(e["start"], 3),
                "end": round(e["end"], 3),
            }
            if e.get("score") is not None:
                word_entry["score"] = round(e["score"], 3)
            all_words.append(word_entry)

    print(f"[WhisperX] Word stats: {total_aligned} aligned, {total_interpolated} interpolated, {len(all_words)} total", file=sys.stderr)
    return all_words


def _fill_range(entries, start_idx, end_idx, gap_start, gap_end):
    """Evenly distribute timestamps across a range of unaligned words."""
    n = end_idx - start_idx
    if n <= 0:
        return
    if gap_end > gap_start:
        d = (gap_end - gap_start) / n
        for j in range(n):
            entries[start_idx + j]["start"] = gap_start + j * d
            entries[start_idx + j]["end"] = gap_start + (j + 1) * d
    else:
        for j in range(n):
            entries[start_idx + j]["start"] = gap_start
            entries[start_idx + j]["end"] = gap_start + 0.1


def _build_segments(all_words: list) -> list:
    """Group words into display segments based on time gaps and punctuation."""
    MAX_WORD_GAP = 3.0
    MIN_SENTENCE_GAP = 0.05
    MIN_WORDS_PER_LINE = 3
    MAX_WORDS_PER_LINE = 10

    def _flush(words):
        return {
            "text": " ".join(w["word"] for w in words),
            "start": words[0]["start"],
            "end": words[-1]["end"],
            "words": words,
        }

    if not all_words:
        return []

    segments = []
    current_words = []

    for w in all_words:
        if current_words:
            gap = w["start"] - current_words[-1]["end"]
            last_text = current_words[-1]["word"]
            next_text = w["word"]
            punctuation_end = last_text.rstrip().endswith((".", "!", "?", ","))
            capital_start = next_text[:1].isupper()
            long_enough = len(current_words) >= MIN_WORDS_PER_LINE

            if gap > MAX_WORD_GAP:
                segments.append(_flush(current_words))
                current_words = []
            elif long_enough and gap >= MIN_SENTENCE_GAP and punctuation_end and capital_start:
                segments.append(_flush(current_words))
                current_words = []
        current_words.append(w)

    if current_words:
        segments.append(_flush(current_words))

    # Merge very short segments into neighbors
    merged = True
    while merged:
        merged = False
        i = 0
        while i < len(segments):
            if len(segments[i]["words"]) < MIN_WORDS_PER_LINE and len(segments) > 1:
                if i == 0:
                    neighbor = i + 1
                elif i == len(segments) - 1:
                    neighbor = i - 1
                else:
                    gap_before = segments[i]["start"] - segments[i - 1]["end"]
                    gap_after = segments[i + 1]["start"] - segments[i]["end"]
                    neighbor = i - 1 if gap_before <= gap_after else i + 1
                if neighbor < i:
                    segments[neighbor] = _flush(segments[neighbor]["words"] + segments[i]["words"])
                    segments.pop(i)
                else:
                    segments[neighbor] = _flush(segments[i]["words"] + segments[neighbor]["words"])
                    segments.pop(i)
                merged = True
            else:
                i += 1

    # Split overly long segments
    split_segments = []
    for seg in segments:
        words = seg["words"]
        if len(words) <= MAX_WORDS_PER_LINE:
            split_segments.append(seg)
            continue
        remaining = words
        MIN_SPLIT_SIZE = 4
        while len(remaining) > MAX_WORDS_PER_LINE:
            search_end = min(len(remaining), MAX_WORDS_PER_LINE + MIN_SPLIT_SIZE)
            best_gap = -1.0
            best_idx = search_end // 2
            for j in range(MIN_SPLIT_SIZE, search_end):
                gap = remaining[j]["start"] - remaining[j - 1]["end"]
                if gap > best_gap:
                    best_gap = gap
                    best_idx = j
            split_segments.append(_flush(remaining[:best_idx]))
            remaining = remaining[best_idx:]
        if remaining:
            split_segments.append(_flush(remaining))

    return split_segments


def _map_words_to_lines(align_result: dict, clean_lines: list) -> list:
    """Map aligned word timestamps back to original lyric lines.
    
    Used by the lyrics alignment engine (Phase 2) to preserve
    the user's original line breaks while adding precise timing.
    """
    # Collect all aligned words
    all_aligned_words = []
    for seg in align_result.get("segments", []):
        for w in seg.get("words", []):
            word_text = w.get("word", "").strip()
            if word_text and "start" in w and "end" in w:
                all_aligned_words.append(w)

    print(f"[Align] Alignment produced {len(all_aligned_words)} words", file=sys.stderr)

    # Build word→timestamps lookup
    word_times = {}
    for w in all_aligned_words:
        key = re.sub(r"[^\w]", "", w["word"]).lower()
        if key not in word_times:
            word_times[key] = []
        word_times[key].append((w["start"], w["end"], w.get("score")))

    used_counts = {}
    segments = []

    for line_text in clean_lines:
        line_words = line_text.split()
        word_entries = []

        for word_text in line_words:
            key = re.sub(r"[^\w]", "", word_text).lower()
            idx = used_counts.get(key, 0)
            times_list = word_times.get(key, [])

            if idx < len(times_list):
                start, end, score = times_list[idx]
                entry = {"word": word_text, "start": round(start, 3), "end": round(end, 3)}
                if score is not None:
                    entry["score"] = round(score, 3)
                used_counts[key] = idx + 1
            else:
                entry = {"word": word_text, "start": None, "end": None, "estimated": True}
            word_entries.append(entry)

        # Interpolate missing timestamps
        _interpolate_missing_in_line(word_entries)

        valid_words = [e for e in word_entries if e["start"] is not None]
        if not valid_words:
            continue

        segments.append({
            "text": line_text,
            "start": valid_words[0]["start"],
            "end": valid_words[-1]["end"],
            "words": valid_words,
        })

    # Split overly long lines
    MAX_WORDS_PER_LINE = 10
    split_segments = []
    for seg in segments:
        words = seg["words"]
        if len(words) <= MAX_WORDS_PER_LINE:
            split_segments.append(seg)
            continue
        for chunk_start in range(0, len(words), MAX_WORDS_PER_LINE):
            chunk = words[chunk_start:chunk_start + MAX_WORDS_PER_LINE]
            split_segments.append({
                "text": " ".join(w["word"] for w in chunk),
                "start": chunk[0]["start"],
                "end": chunk[-1]["end"],
                "words": chunk,
            })

    return split_segments


def _interpolate_missing_in_line(word_entries: list):
    """Fill in timestamps for words the aligner couldn't place, using neighbors."""
    unset = [i for i, e in enumerate(word_entries) if e["start"] is None]
    set_entries = [e for e in word_entries if e["start"] is not None]

    if not unset or not set_entries:
        return

    for ui in unset:
        prev_end = set_entries[0]["start"]
        next_start = set_entries[-1]["end"]
        for j in range(ui - 1, -1, -1):
            if word_entries[j]["start"] is not None:
                prev_end = word_entries[j]["end"]
                break
        for j in range(ui + 1, len(word_entries)):
            if word_entries[j]["start"] is not None:
                next_start = word_entries[j]["start"]
                break
        mid = (prev_end + next_start) / 2
        word_entries[ui]["start"] = round(prev_end, 3)
        word_entries[ui]["end"] = round(mid, 3)
        # Remove the estimated flag since we now have timestamps
        word_entries[ui].pop("estimated", None)


# ─── CLI Entry Point ────────────────────────────────────────────────────────

if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="NC-KTV Python Bridge — AI Transcription & Vocal Separation"
    )
    subparsers = parser.add_subparsers(dest="command")

    # ── Legacy Whisper transcription ────────────────────────────────────────
    trans_parser = subparsers.add_parser("transcribe",
        help="Transcribe audio using vanilla OpenAI Whisper (attention-based word timestamps)")
    trans_parser.add_argument("file")
    trans_parser.add_argument("--model", default="small")
    trans_parser.add_argument("--lang", default=None)

    # ── WhisperX transcription (forced alignment) ──────────────────────────
    wx_parser = subparsers.add_parser("transcribe-x",
        help="Transcribe audio using WhisperX with forced alignment (±30ms word precision)")
    wx_parser.add_argument("file")
    wx_parser.add_argument("--model", default="large-v3",
        help="WhisperX model name (default: large-v3)")
    wx_parser.add_argument("--lang", default=None,
        help="Language code (e.g. 'en', 'ja'). Omit for auto-detect.")
    wx_parser.add_argument("--beam-size", type=int, default=5,
        help="Beam size for decoding (default: 5)")
    wx_parser.add_argument("--batch-size", type=int, default=16,
        help="Batch size for batched inference (default: 16)")

    # ── Lyrics forced alignment ────────────────────────────────────────────
    align_parser = subparsers.add_parser("align",
        help="Force-align known lyrics to audio using WhisperX (skip transcription)")
    align_parser.add_argument("file", help="Path to the audio file")
    align_parser.add_argument("--lyrics", required=True,
        help="Path to a text file containing lyrics (one line per lyric line)")
    align_parser.add_argument("--lang", default="en",
        help="Language code for alignment model (default: en)")

    # ── Gemini transcription ───────────────────────────────────────────────
    gemini_parser = subparsers.add_parser("gemini",
        help="Transcribe audio using the Google Gemini API")
    gemini_parser.add_argument("file", help="Path to the audio file (MP3/WAV/etc.)")
    gemini_parser.add_argument("--api-key", default=None, help="Gemini API key (or set GEMINI_API_KEY env var)")
    gemini_parser.add_argument("--model", default="gemini-1.5-pro", help="Gemini model name")
    gemini_parser.add_argument("--lang", default=None, help="Language hint (e.g. 'en', 'ja'). Omit for auto-detect.")

    # ── Vocal separation ───────────────────────────────────────────────────
    sep_parser = subparsers.add_parser("separate",
        help="Separate vocals from instrumentals using UVR models")
    sep_parser.add_argument("file")
    sep_parser.add_argument("model")
    sep_parser.add_argument("outdir")

    args = parser.parse_args()

    try:
        if args.command == "transcribe":
            run_transcription(args.file, args.model, args.lang)
        elif args.command == "transcribe-x":
            run_transcription_whisperx(
                args.file, args.model, args.lang,
                args.beam_size, args.batch_size
            )
        elif args.command == "align":
            lyrics_path = Path(args.lyrics)
            if not lyrics_path.exists():
                print(json.dumps({"error": f"Lyrics file not found: {args.lyrics}"}))
                sys.exit(1)
            lyrics_text = lyrics_path.read_text(encoding="utf-8")
            run_alignment(args.file, lyrics_text, args.lang)
        elif args.command == "gemini":
            run_gemini_transcription(args.file, args.api_key, args.model, args.lang)
        elif args.command == "separate":
            run_separation(args.file, args.model, args.outdir)
        else:
            parser.print_help()
    except Exception as e:
        import traceback
        traceback.print_exc(file=sys.stderr)
        print(json.dumps({"error": str(e)}))
        sys.exit(1)
