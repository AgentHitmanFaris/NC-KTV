"""
NC-KTV Python Bridge — AI Transcription & Vocal Separation
===========================================================
Provides CLI commands and a persistent JSON-RPC TCP Server for the C++ Qt6 app.
Commands:
  - transcribe    : Vanilla OpenAI Whisper
  - transcribe-x  : WhisperX engine (forced alignment)
  - align         : Force-align known lyrics text to audio
  - gemini        : Google Gemini API transcription
  - separate      : UVR vocal separation via audio-separator
"""

import sys
import os
import json
import argparse
import warnings
import re
import threading
import socketserver
from pathlib import Path

# ─── Locate and Expose FFmpeg Globally ───────────────────────────────────────
_script_dir = Path(__file__).parent.resolve()

if getattr(sys, 'frozen', False):
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
        os.environ["PATH"] = str(_fd.absolute()) + os.pathsep + os.environ.get("PATH", "")
        break

warnings.filterwarnings("ignore")

if str(_script_dir) not in sys.path:
    sys.path.insert(0, str(_script_dir))

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
    import numpy as np
    frame_length = int(0.05 * sr)
    hop_length = frame_length
    n_frames = len(audio) // hop_length
    if n_frames == 0:
        return 0.0, len(audio) / sr

    rms = np.array([
        np.sqrt(np.mean(audio[i * hop_length : i * hop_length + frame_length] ** 2))
        for i in range(n_frames)
    ])

    threshold = 10 ** (threshold_db / 20)
    active = rms > threshold
    active_indices = np.where(active)[0]

    if len(active_indices) == 0:
        return 0.0, len(audio) / sr

    start_frame = max(0, active_indices[0] - int(0.5 / (hop_length / sr)))
    end_frame = min(n_frames - 1, active_indices[-1] + int(0.5 / (hop_length / sr)))

    start_sec = max(0.0, start_frame * hop_length / sr)
    end_sec = min(len(audio) / sr, (end_frame * hop_length + frame_length) / sr)

    return round(start_sec, 3), round(end_sec, 3)

def highpass_filter(audio, sr=16000, cutoff=80):
    try:
        from scipy.signal import butter, sosfilt
        sos = butter(5, cutoff, btype='high', fs=sr, output='sos')
        return sosfilt(sos, audio).astype(audio.dtype)
    except ImportError:
        return audio

def normalize_rms(audio, target_db=-20):
    import numpy as np
    rms = np.sqrt(np.mean(audio ** 2))
    if rms < 1e-10:
        return audio
    target_rms = 10 ** (target_db / 20)
    gain = target_rms / rms
    return (audio * gain).astype(audio.dtype)

# ─── WhisperX Engine ─────────────────────────────────────────────────────────

def run_transcription_whisperx(audio_file, model_name="large-v3", language=None, beam_size=5, batch_size=16):
    import whisperx
    import torch
    import numpy as np

    if language and language.lower() == "auto":
        language = None

    device = "cuda" if torch.cuda.is_available() else "cpu"
    compute_type = "float16" if device == "cuda" else "float32"

    if device == "mps":
        device = "cpu"
        compute_type = "float32"

    print(f"[WhisperX] Using device: {device}, compute_type: {compute_type}", file=sys.stderr)
    print(f"[WhisperX] Loading audio: {audio_file}", file=sys.stderr)
    full_audio = whisperx.load_audio(str(audio_file))
    duration_secs = len(full_audio) / 16000

    print(f"[WhisperX] Detecting vocal region...", file=sys.stderr)
    vocal_start, vocal_end = detect_vocal_region(full_audio)
    trim_start = int(vocal_start * 16000)
    trim_end = int(vocal_end * 16000)
    audio = full_audio[trim_start:trim_end]

    audio = highpass_filter(audio)
    audio = normalize_rms(audio)

    hf_cache = _script_dir / "models" / "huggingface"
    hf_cache.mkdir(parents=True, exist_ok=True)
    os.environ.setdefault("HF_HOME", str(hf_cache))

    asr_options = {
        "beam_size": beam_size,
        "initial_prompt": (
            "Everything before GO is INSTRUCTIONS. DON'T INCLUDE IN TRANSCRIPT. "
            "Song Lyrics transcript. Split lines with punctuation. "
            "No annotations or descriptions. GO"
        ),
    }

    model = whisperx.load_model(
        model_name, device, compute_type=compute_type, task="transcribe",
        language=language, asr_options=asr_options,
    )

    result = model.transcribe(audio, batch_size=batch_size, task="transcribe", language=language, chunk_size=30)
    detected_language = result.get("language", language or "en")

    del model
    _free_gpu()

    raw_segments = result.get("segments", [])
    for seg in raw_segments:
        seg["start"] = round(seg.get("start", 0) + vocal_start, 3)
        seg["end"] = round(seg.get("end", 0) + vocal_start, 3)

    try:
        from hallucination_filter import filter_hallucinated_segments
        raw_segments = filter_hallucinated_segments(raw_segments, duration_secs)
    except ImportError:
        pass

    align_device = "cpu" if device == "mps" else device
    try:
        align_model, metadata = whisperx.load_align_model(language_code=detected_language, device=align_device)
        aligned = whisperx.align(raw_segments, align_model, metadata, full_audio, align_device)
        del align_model
        _free_gpu()
    except Exception as e:
        print(f"[WhisperX] Alignment failed ({e}), using raw timestamps", file=sys.stderr)
        aligned = {"segments": raw_segments}

    output_segments = aligned.get("segments", [])
    _recover_dropped_words(raw_segments, output_segments)
    all_words = _interpolate_words(output_segments)

    try:
        from hallucination_filter import remove_hallucinated_words
        all_words = remove_hallucinated_words(all_words)
    except ImportError:
        pass

    segments = _build_segments(all_words)
    
    return {
        "segments": segments,
        "engine": "whisperx",
        "language": detected_language,
    }

# ─── Lyrics Forced Alignment ─────────────────────────────────────────────────

def run_alignment(audio_file, lyrics_text, language="en"):
    import whisperx
    import torch
    import numpy as np

    if language and language.lower() == "auto":
        language = "en"

    device = "cuda" if torch.cuda.is_available() else "cpu"
    if device == "mps":
        device = "cpu"

    audio = whisperx.load_audio(str(audio_file))
    vocal_start, vocal_end = detect_vocal_region(audio)
    clean_lines = [line.strip() for line in lyrics_text.strip().split("\n") if line.strip()]

    if not clean_lines:
        return {"error": "No lyrics text provided", "segments": []}

    full_text = " ".join(clean_lines)
    raw_segments = [{"text": full_text, "start": vocal_start, "end": vocal_end}]
    align_device = "cpu" if device == "mps" else device

    try:
        align_model, metadata = whisperx.load_align_model(language_code=language, device=align_device)
        align_result = whisperx.align(raw_segments, align_model, metadata, audio, align_device)
        del align_model
        _free_gpu()
    except Exception as e:
        return {"error": f"Alignment error: {e}", "segments": []}

    segments = _map_words_to_lines(align_result, clean_lines)
    return {
        "segments": segments,
        "engine": "whisperx-align",
        "language": language,
    }

# ─── Legacy Whisper Engine ───────────────────────────────────────────────────

def run_transcription(audio_file, model_name="small", language=None):
    import whisper
    import torch
    
    if language and language.lower() == "auto":
        language = None
        
    device = "cuda" if torch.cuda.is_available() else "cpu"
    possible_dirs = [
        _script_dir / "models" / "whisper",
        Path("D:/Document/NC-Project/NC-KTV/models/whisper"),
        Path.cwd() / "models" / "whisper"
    ]
    
    final_models_dir = _script_dir / "models" / "whisper"
    found_model_path = None
    
    for d in possible_dirs:
        if not d.exists(): continue
        look_for = [f"{model_name}.pt", f"{model_name}.dat"]
        if model_name == "turbo":
            look_for.extend(["large-v3-turbo.pt", "large-v3-turbo.dat"])
            
        for name in look_for:
            test_path = d / name
            if test_path.exists() and (test_path.stat().st_size / (1024 * 1024)) > 100:
                final_models_dir = d
                found_model_path = test_path
                break
        if found_model_path: break

    if found_model_path is not None:
        model = whisper.load_model(str(found_model_path.absolute()), device=device)
    else:
        final_models_dir.mkdir(parents=True, exist_ok=True)
        model = whisper.load_model(model_name, download_root=str(final_models_dir.absolute()), device=device)
    
    result = model.transcribe(str(audio_file), verbose=False, language=language, word_timestamps=True, fp16=(device == "cuda"))
    output = {"segments": [], "engine": "whisper"}
    for segment in result.get("segments", []):
        output["segments"].append({
            "text": segment["text"].strip(),
            "start": segment["start"],
            "end": segment["end"],
            "words": segment.get("words", [])
        })
    return output

# ─── Gemini Engine ───────────────────────────────────────────────────────────

def run_gemini_transcription(audio_file, api_key=None, model_name="gemini-1.5-pro", language=None):
    try:
        import google.generativeai as genai
    except ImportError:
        return {"error": "google-generativeai package not installed."}

    resolved_key = api_key or os.environ.get("GEMINI_API_KEY", "")
    if not resolved_key:
        return {"error": "No Gemini API key provided."}

    genai.configure(api_key=resolved_key)
    audio_path = Path(audio_file)
    file_ref = genai.upload_file(str(audio_path.absolute()), mime_type="audio/mpeg")

    lang_hint = f"The primary language is {language}." if language and language.lower() != "auto" else ""
    prompt = f"""You are a precise audio transcription assistant. Transcribe the given audio file completely and return ONLY a valid JSON object (no markdown, no explanation) in the following format:
{{
  "segments": [
    {{
      "text": "full line text here", "start": 0.0, "end": 2.5,
      "words": [{{"word": "full", "start": 0.0, "end": 0.3}}]
    }}
  ]
}}
Rules: Each segment represents one lyric phrase. Timestamps in seconds. {lang_hint}"""

    gen_model = genai.GenerativeModel(model_name)
    response = gen_model.generate_content([prompt, file_ref])
    raw = response.text.strip()

    if raw.startswith("```"):
        raw = "\n".join(line for line in raw.split("\n") if not line.startswith("```"))

    try:
        result = json.loads(raw)
        if "segments" not in result:
            raise ValueError("Response missing 'segments' key")
        result["engine"] = "gemini"
        return result
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON parse failed: {e}", "raw": raw}

# ─── Vocal Separation ───────────────────────────────────────────────────────

def run_separation(audio_file, model_name, output_dir):
    from audio_separator.separator import Separator
    original_get_distribution = Separator.get_package_distribution
    def dummy_get_package_distribution(self, package_name):
        dist = original_get_distribution(self, package_name)
        if dist is None:
            class DummyDist: version = "unknown"
            return DummyDist()
        return dist
    Separator.get_package_distribution = dummy_get_package_distribution

    models_dir = _script_dir / "models" / "uvr"
    if not models_dir.exists():
        models_dir = Path("models/uvr")
    models_dir.mkdir(parents=True, exist_ok=True)
    
    sep = Separator(output_dir=str(output_dir), model_file_dir=str(models_dir.absolute()), output_format="WAV")
    sep.load_model(model_name)
    output_files = sep.separate(str(audio_file))
    
    if not output_files:
        return {"error": "Separation failed silently (empty output)."}
    return {"files": output_files}

# ─── Word Recovery & Interpolation Utilities ─────────────────────────────────

def _normalize_word(word: str) -> str:
    return re.sub(r"[^\w]", "", word).lower()

def _free_gpu():
    import gc
    gc.collect()
    try:
        import torch
        if torch.cuda.is_available():
            torch.cuda.empty_cache()
    except Exception:
        pass

def _recover_dropped_words(raw_segments: list, aligned_segments: list):
    if len(raw_segments) != len(aligned_segments): return
    for raw_seg, aligned_seg in zip(raw_segments, aligned_segments):
        raw_words = raw_seg.get("text", "").split()
        aligned_words = aligned_seg.get("words", [])
        if not raw_words: continue

        aligned_norms = [_normalize_word(w.get("word", "")) for w in aligned_words]
        matched_raw, matched_aligned, ai = set(), set(), 0

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
        if not missing_indices: continue

        for orig_idx in reversed(missing_indices):
            insert_pos = len(aligned_words)
            for check_ri in range(orig_idx + 1, len(raw_words)):
                check_norm = _normalize_word(raw_words[check_ri])
                for ai_pos, an in enumerate(aligned_norms):
                    if an == check_norm:
                        insert_pos = ai_pos
                        break
                if insert_pos < len(aligned_words): break

            aligned_words.insert(insert_pos, {"word": raw_words[orig_idx]})
            aligned_norms.insert(insert_pos, _normalize_word(raw_words[orig_idx]))
        aligned_seg["words"] = aligned_words

def _interpolate_words(output_segments: list) -> list:
    all_words = []
    for seg in output_segments:
        raw_words = seg.get("words", [])
        if not raw_words: continue

        seg_start, seg_end = seg.get("start", 0), seg.get("end", 0)
        entries = [{"word": w.get("word", "").strip(), "start": w.get("start"), "end": w.get("end"), "score": w.get("score"), "aligned": ("start" in w and "end" in w)} for w in raw_words if w.get("word", "").strip()]
        if not entries: continue

        anchors = [(i, e) for i, e in enumerate(entries) if e["aligned"]]
        if not anchors:
            n, dur = len(entries), (seg_end - seg_start) / len(entries) if seg_end > seg_start else 0.1
            for j, e in enumerate(entries):
                e["start"], e["end"] = seg_start + j * dur, seg_start + (j + 1) * dur
        else:
            first_idx = anchors[0][0]
            if first_idx > 0: _fill_range(entries, 0, first_idx, seg_start, entries[first_idx]["start"])
            for ai_idx in range(len(anchors) - 1):
                a_idx, b_idx = anchors[ai_idx][0], anchors[ai_idx + 1][0]
                if b_idx - a_idx > 1: _fill_range(entries, a_idx + 1, b_idx, entries[a_idx]["end"], entries[b_idx]["start"])
            last_idx = anchors[-1][0]
            if last_idx < len(entries) - 1: _fill_range(entries, last_idx + 1, len(entries), entries[last_idx]["end"], seg_end)

        for e in entries:
            if e["start"] is not None and e["end"] is not None:
                word_entry = {"word": e["word"], "start": round(e["start"], 3), "end": round(e["end"], 3)}
                if e.get("score") is not None: word_entry["score"] = round(e["score"], 3)
                all_words.append(word_entry)
    return all_words

def _fill_range(entries, start_idx, end_idx, gap_start, gap_end):
    n = end_idx - start_idx
    if n <= 0: return
    d = (gap_end - gap_start) / n if gap_end > gap_start else 0
    for j in range(n):
        entries[start_idx + j]["start"] = gap_start + j * d
        entries[start_idx + j]["end"] = (gap_start + (j + 1) * d) if d > 0 else gap_start + 0.1

def _build_segments(all_words: list) -> list:
    if not all_words: return []
    def _flush(words): return {"text": " ".join(w["word"] for w in words), "start": words[0]["start"], "end": words[-1]["end"], "words": words}

    segments, current_words = [], []
    for w in all_words:
        if current_words:
            gap = w["start"] - current_words[-1]["end"]
            if gap > 3.0 or (len(current_words) >= 3 and gap >= 0.05 and current_words[-1]["word"].rstrip().endswith((".", "!", "?", ",")) and w["word"][:1].isupper()):
                segments.append(_flush(current_words))
                current_words = []
        current_words.append(w)
    if current_words: segments.append(_flush(current_words))
    return segments

def _map_words_to_lines(align_result: dict, clean_lines: list) -> list:
    all_aligned_words = [w for seg in align_result.get("segments", []) for w in seg.get("words", []) if w.get("word", "").strip() and "start" in w]
    word_times = {}
    for w in all_aligned_words:
        key = re.sub(r"[^\w]", "", w["word"]).lower()
        word_times.setdefault(key, []).append((w["start"], w["end"], w.get("score")))

    used_counts, segments = {}, []
    for line_text in clean_lines:
        word_entries = []
        for word_text in line_text.split():
            key = re.sub(r"[^\w]", "", word_text).lower()
            idx = used_counts.get(key, 0)
            if idx < len(word_times.get(key, [])):
                start, end, score = word_times[key][idx]
                word_entries.append({"word": word_text, "start": round(start, 3), "end": round(end, 3)})
                used_counts[key] = idx + 1
            else:
                word_entries.append({"word": word_text, "start": None, "end": None})

        unset = [i for i, e in enumerate(word_entries) if e["start"] is None]
        set_entries = [e for e in word_entries if e["start"] is not None]
        if unset and set_entries:
            for ui in unset:
                prev_end = next((word_entries[j]["end"] for j in range(ui - 1, -1, -1) if word_entries[j]["start"] is not None), set_entries[0]["start"])
                next_start = next((word_entries[j]["start"] for j in range(ui + 1, len(word_entries)) if word_entries[j]["start"] is not None), set_entries[-1]["end"])
                word_entries[ui]["start"], word_entries[ui]["end"] = round(prev_end, 3), round((prev_end + next_start) / 2, 3)

        valid_words = [e for e in word_entries if e["start"] is not None]
        if valid_words:
            segments.append({"text": line_text, "start": valid_words[0]["start"], "end": valid_words[-1]["end"], "words": valid_words})
    return segments

# ─── TCP JSON-RPC Server ─────────────────────────────────────────────────────

def run_server(host="127.0.0.1", port=50051):
    class JSONRPCServer(socketserver.ThreadingMixIn, socketserver.TCPServer):
        daemon_threads = True
        allow_reuse_address = True

    class JSONRPCHandler(socketserver.StreamRequestHandler):
        def handle(self):
            print(f"[Server] Connection established from {self.client_address}", file=sys.stderr)
            for line in self.rfile:
                if not line.strip(): continue
                try:
                    req = json.loads(line.decode('utf-8'))
                    method = req.get('method')
                    params = req.get('params', {})
                    req_id = req.get('id', None)

                    result, error = None, None
                    try:
                        if method == "transcribe-x":
                            result = run_transcription_whisperx(**params)
                        elif method == "transcribe":
                            result = run_transcription(**params)
                        elif method == "align":
                            result = run_alignment(**params)
                        elif method == "gemini":
                            result = run_gemini_transcription(**params)
                        elif method == "separate":
                            result = run_separation(**params)
                        else:
                            error = f"Unknown method: {method}"
                    except Exception as e:
                        import traceback
                        traceback.print_exc(file=sys.stderr)
                        error = str(e)

                    response = {"jsonrpc": "2.0"}
                    if req_id is not None: response["id"] = req_id
                    if error:
                        response["error"] = {"code": -32603, "message": error}
                    else:
                        response["result"] = result

                    self.wfile.write((json.dumps(response) + "\n").encode('utf-8'))
                    self.wfile.flush()
                except json.JSONDecodeError:
                    self.wfile.write((json.dumps({"jsonrpc": "2.0", "error": {"code": -32700, "message": "Parse error"}, "id": None}) + "\n").encode('utf-8'))
                    self.wfile.flush()

    print(f"[Server] Starting JSON-RPC server on {host}:{port}", file=sys.stderr)
    server = JSONRPCServer((host, port), JSONRPCHandler)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("[Server] Shutting down...", file=sys.stderr)
        server.server_close()

# ─── CLI Entry Point ─────────────────────────────────────────────────────────

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="NC-KTV Python Bridge")
    parser.add_argument("--serve", action="store_true", help="Run as a persistent JSON-RPC TCP server")
    parser.add_argument("--port", type=int, default=50051, help="Port for the JSON-RPC server")
    
    subparsers = parser.add_subparsers(dest="command")

    trans_parser = subparsers.add_parser("transcribe")
    trans_parser.add_argument("file")
    trans_parser.add_argument("--model", default="small")
    trans_parser.add_argument("--lang", default=None)

    wx_parser = subparsers.add_parser("transcribe-x")
    wx_parser.add_argument("file")
    wx_parser.add_argument("--model", default="large-v3")
    wx_parser.add_argument("--lang", default=None)
    wx_parser.add_argument("--beam-size", type=int, default=5)
    wx_parser.add_argument("--batch-size", type=int, default=16)

    align_parser = subparsers.add_parser("align")
    align_parser.add_argument("file")
    align_parser.add_argument("--lyrics", required=True)
    align_parser.add_argument("--lang", default="en")

    gemini_parser = subparsers.add_parser("gemini")
    gemini_parser.add_argument("file")
    gemini_parser.add_argument("--api-key", default=None)
    gemini_parser.add_argument("--model", default="gemini-1.5-pro")
    gemini_parser.add_argument("--lang", default=None)

    sep_parser = subparsers.add_parser("separate")
    sep_parser.add_argument("file")
    sep_parser.add_argument("model")
    sep_parser.add_argument("outdir")

    args = parser.parse_args()

    if args.serve:
        run_server(port=args.port)
        sys.exit(0)

    try:
        result = {}
        if args.command == "transcribe":
            result = run_transcription(args.file, args.model, args.lang)
        elif args.command == "transcribe-x":
            result = run_transcription_whisperx(args.file, args.model, args.lang, args.beam_size, args.batch_size)
        elif args.command == "align":
            lyrics_text = Path(args.lyrics).read_text(encoding="utf-8")
            result = run_alignment(args.file, lyrics_text, args.lang)
        elif args.command == "gemini":
            result = run_gemini_transcription(args.file, args.api_key, args.model, args.lang)
        elif args.command == "separate":
            result = run_separation(args.file, args.model, args.outdir)
        else:
            parser.print_help()
            sys.exit(1)
            
        print(json.dumps(result))
    except Exception as e:
        import traceback
        traceback.print_exc(file=sys.stderr)
        print(json.dumps({"error": str(e)}))
        sys.exit(1)