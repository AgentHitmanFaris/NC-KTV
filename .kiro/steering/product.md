# NC-KTV — Product Overview

NC-KTV is a desktop karaoke video authoring tool for Windows. It lets users create synchronized karaoke videos by combining audio/video sources with timed lyrics.

## Core Workflow
1. **Wizard Mode** — project creation: load audio/video, run AI vocal separation (UVR/MDX-Net) and transcription (Whisper)
2. **Editor Mode** — synchronize lyrics to audio on a timeline with waveform reference
3. **Precision/Lyrical Pro Mode** — syllable-level timing fine-tuning (vertical teleprompter UI)
4. **Export** — burn subtitles into video via FFmpeg (MP4/H.264/H.265/VP9, hardware-accelerated)

## Key Features
- AI transcription via Whisper (local Python subprocess bridge) and LRCLib cloud search
- Vocal separation via UVR/MDX-Net (Python bridge, GPU-accelerated via ONNX Runtime)
- ASS subtitle generation with karaoke wipe effects
- Plugin and theme system (`.nckplugin` / `.ncktheme` packages)
- Portable distribution — zero-install, self-contained executable bundle

## AI Bridge
Heavy AI work (Whisper transcription, UVR separation) runs in a separate Python process (`python_bridge.py`), packaged via PyInstaller into `build_pyinstaller/python_bridge/`. The C++ GUI communicates with it via subprocess.
