"""
NC-KTV — WhisperX Model Downloader
===================================
Pre-downloads WhisperX alignment models (Wav2Vec2) and the Whisper
model to the local models/ directory for offline karaoke use.

Usage:
    python download_whisperx_models.py [--lang en ja ko zh ms id]

This downloads:
  1. The WhisperX/CTranslate2 version of the Whisper large-v3 model
  2. Wav2Vec2 alignment models for each specified language

Models are cached in:
  - models/huggingface/   (Wav2Vec2 alignment + Whisper CTC models)
"""

import sys
import os
import argparse
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description="Download WhisperX models for NC-KTV")
    parser.add_argument(
        "--lang", nargs="+", default=["en"],
        help="Language codes to download alignment models for (default: en)"
    )
    parser.add_argument(
        "--whisper-model", default="large-v3",
        help="Whisper model size to download (default: large-v3)"
    )
    parser.add_argument(
        "--device", default=None,
        help="Device to use: cuda, cpu (default: auto-detect)"
    )
    args = parser.parse_args()

    # Set up model cache directory
    script_dir = Path(__file__).parent
    hf_cache = script_dir / "models" / "huggingface"
    hf_cache.mkdir(parents=True, exist_ok=True)
    os.environ["HF_HOME"] = str(hf_cache)

    print(f"\n{'='*60}")
    print(f"  NC-KTV WhisperX Model Downloader")
    print(f"{'='*60}")
    print(f"  Cache directory: {hf_cache}")
    print(f"  Whisper model:   {args.whisper_model}")
    print(f"  Languages:       {', '.join(args.lang)}")
    print(f"{'='*60}\n")

    # Import dependencies
    try:
        import torch
        import whisperx
    except ImportError as e:
        print(f"ERROR: Required package not installed: {e}")
        print("Run: pip install whisperx torch torchaudio")
        sys.exit(1)

    # Auto-detect device
    if args.device:
        device = args.device
    else:
        device = "cuda" if torch.cuda.is_available() else "cpu"
    
    compute_type = "float16" if device == "cuda" else "float32"
    print(f"Device: {device} (compute_type: {compute_type})\n")

    # ── Step 1: Download Whisper model ───────────────────────────────────────
    print(f"[1/2] Downloading Whisper model: {args.whisper_model}...")
    print(f"      (This may take a while for large models)\n")

    try:
        model = whisperx.load_model(
            args.whisper_model,
            device,
            compute_type=compute_type,
            task="transcribe",
        )
        print(f"  ✓ Whisper model '{args.whisper_model}' downloaded and verified\n")
        del model

        # Free GPU memory
        import gc
        gc.collect()
        if torch.cuda.is_available():
            torch.cuda.empty_cache()
    except Exception as e:
        print(f"  ✗ Failed to download Whisper model: {e}\n")

    # ── Step 2: Download alignment models per language ───────────────────────
    print(f"[2/2] Downloading Wav2Vec2 alignment models...")
    print(f"      Languages: {', '.join(args.lang)}\n")

    for lang in args.lang:
        print(f"  Downloading alignment model for '{lang}'...", end=" ", flush=True)
        try:
            align_model, metadata = whisperx.load_align_model(
                language_code=lang, device=device
            )
            print(f"✓")
            del align_model
            del metadata

            import gc
            gc.collect()
            if torch.cuda.is_available():
                torch.cuda.empty_cache()
        except Exception as e:
            print(f"✗ ({e})")

    print(f"\n{'='*60}")
    print(f"  Download complete!")
    print(f"  Models cached in: {hf_cache}")
    print(f"{'='*60}\n")


if __name__ == "__main__":
    main()
