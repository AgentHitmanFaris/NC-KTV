import sys
import os
import json
import argparse
import warnings
from pathlib import Path

# Suppress annoying warnings
warnings.filterwarnings("ignore")

def run_transcription(audio_file, model_name="small", language=None):
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
    output = {"segments": []}
    for segment in result.get("segments", []):
        output["segments"].append({
            "text": segment["text"].strip(),
            "start": segment["start"],
            "end": segment["end"],
            "words": segment.get("words", [])
        })
    
    print(json.dumps(output))

def run_separation(audio_file, model_name, output_dir):
    from audio_separator.separator import Separator
    
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
    
    print(f"DEBUG: Separating {audio_file}...", file=sys.stderr)
    output_files = sep.separate(str(audio_file))
    
    # The library returns a list of filenames created
    print(json.dumps({"files": output_files}))

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest="command")
    
    # Transcribe command
    trans_parser = subparsers.add_parser("transcribe")
    trans_parser.add_argument("file")
    trans_parser.add_argument("--model", default="small")
    trans_parser.add_argument("--lang", default=None)
    
    # Separate command
    sep_parser = subparsers.add_parser("separate")
    sep_parser.add_argument("file")
    sep_parser.add_argument("model")
    sep_parser.add_argument("outdir")
    
    args = parser.parse_args()
    
    try:
        if args.command == "transcribe":
            run_transcription(args.file, args.model, args.lang)
        elif args.command == "separate":
            run_separation(args.file, args.model, args.outdir)
        else:
            parser.print_help()
    except Exception as e:
        import traceback
        traceback.print_exc(file=sys.stderr)
        print(json.dumps({"error": str(e)}))
        sys.exit(1)
