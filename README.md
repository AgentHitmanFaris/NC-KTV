<div align="center">

<img src="assets/logo.png" alt="NC-KTV Logo" width="200"/>

# NC-KTV

**Music Video Karaoke Maker**

![Version](https://img.shields.io/badge/version-0.6.1-blue.svg)
![License](https://img.shields.io/badge/license-MIT-green.svg)
![Python](https://img.shields.io/badge/python-3.10-blue.svg)
![Status](https://img.shields.io/badge/status-beta-orange.svg)

Professional Windows desktop application for creating karaoke videos with automatic vocal removal and synchronized lyrics.

[Features](#features) • [Installation](#installation) • [Usage](#usage) • [Docs](DOCS.md) • [Changelog](CHANGELOG.md)

</div>

---

## Overview

NC-KTV automates the process of creating professional karaoke videos from music files or videos. It combines AI-powered vocal separation (UVR), automatic transcription (Whisper), and a professional lyrics editor to let you create karaoke tracks in minutes.

---

## Features

### AI Vocal Separation
- **UVR Integration**: Remove vocals from any song with high precision
- **GPU Acceleration**: CUDA support for fast processing
- **Instrumental & Vocal Tracks**: Automatically splits audio for mixing
- **Multiple Models**: KARA_2 (fast), 5_HP-Karaoke (balanced), 6_HP-Karaoke (quality)

### Professional Lyrics Editor
- **Waveform View**: Visualize audio for precise timing
- **Dual-Line Preview**: See active and upcoming lyrics in real-time
- **Tap-to-Sync**: Spacebar tapping for easy rhythm matching
- **Word-Level Editing**: Fine-tune individual word timings
- **Auto-Transcription**: Generate initial lyrics using AI (Whisper)
- **📥 File Import**: Import from .txt or .lrc files (NEW v0.6.0)
- **⚡ Speed Control**: Adjust playback 0.5x-2.0x for easier syncing (NEW v0.6.0)
- **Undo/Redo**: Ctrl+Z/Y to undo changes
- **Save Prompts**: Warns before closing unsaved work (NEW v0.6.0)

### Project Management
- **🔐 Encrypted .nctv Format**: Secure binary project files (NEW v0.6.0)
- **📦 Cross-Project Import**: Share lyrics/audio between projects (NEW v0.6.0)
- **Model Manager**: One-click Whisper model downloads (NEW v0.6.0)
- **Auto-Save**: Automatic backups every 5 minutes

### Karaoke Video Export
- **1080p MP4 Export**: High-quality video output
- **5 Animation Types**: Linear Wipe, Syllable Step, Glow Pulse, Fade In, Bouncing Ball
- **3 Color Styles**: Neon Gold, Classic Blue, Clean White
- **Background Options**: Use original video or custom backgrounds

---

## Installation

### Prerequisites
1. **Windows 10/11** (64-bit)
2. **FFmpeg** in system PATH
3. **NVIDIA GPU** recommended (CUDA 11.8)

### Quick Start
```powershell
# Clone and setup
git clone https://github.com/AgentHitmanFaris/NC-KTV.git
cd NC-KTV
.\setup_python.ps1

# Run
.\python_embed\python.exe main.py
```

See [DOCS.md](DOCS.md) for detailed installation instructions.

---

## Usage

### 1. Wizard Mode
- **Select File**: Drag and drop your audio or video file
- **Separate**: Click "Start Processing" to separate vocals
- **Edit**: Click "Edit Lyrics" to enter the Editor

### 2. Lyrics Editor
- **Input**: Paste lyrics or use "Auto-Transcribe"
- **Sync**: Play track and use **Spacebar** to set line starts
- **Fine-tune**: Right-click context menu to "Edit Word Timings" for precision
- **Preview**: Watch the real-time karaoke preview

### 3. Export
- Click **"Export Video"**
- Choose animation style
- Wait for rendering

---

## Roadmap

- [x] Phase 1-2: Core Backend & Wizard Mode
- [x] Phase 3: Lyrics Processing & Syncing
- [x] Phase 4: Video Generation & Styles
- [x] Phase 5: Word-Level Precision
- [ ] Phase 6: Advanced Timeline Effects
- [ ] Phase 7: Community Themes

---

## Contributing

Contributions welcome! See [DOCS.md](DOCS.md) for guidelines.

---

## License

MIT License - see [LICENSE](LICENSE).

---

## Acknowledgments

- **[UVR](https://github.com/Anjok07/ultimatevocalremovergui)** - Vocal removal models
- **[audio-separator](https://github.com/nomadkaraoke/python-audio-separator)** - Python UVR wrapper
- **[PyTorch](https://pytorch.org/)** - Deep learning framework
- **[PyQt6](https://www.riverbankcomputing.com/software/pyqt/)** - GUI framework
- **[OpenAI Whisper](https://github.com/openai/whisper)** - Speech recognition

---

<div align="center">

**Made for the karaoke community**

</div>
