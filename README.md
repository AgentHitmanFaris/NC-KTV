<div align="center">

<img src="assets/logo.png" alt="NC-KTV Logo" width="200"/>

# NC-KTV

**Professional Music Video Karaoke Maker**

![Version](https://img.shields.io/badge/version-0.10.3-blue.svg)
![License](https://img.shields.io/badge/license-MIT-green.svg)
![Python](https://img.shields.io/badge/python-3.10-blue.svg)
![Status](https://img.shields.io/badge/status-beta-orange.svg)

Windows desktop application for creating professional karaoke videos with AI-powered vocal separation, automatic transcription, and synchronized lyrics.

[Features](#features) • [Installation](#installation) • [Quick Start](#quick-start) • [Plugins & Themes](#plugins--themes) • [Technical Docs](DOCS.md) • [Changelog](CHANGELOG.md)

</div>

---

## Overview

NC-KTV automates the entire karaoke video creation workflow:
1. **Import** any audio/video file
2. **Separate** vocals from instrumentals using AI
3. **Transcribe** lyrics automatically or import from files
4. **Sync** lyrics with precise timing controls
5. **Export** professional karaoke videos

---

## Features

###  AI Vocal Separation
- **UVR Integration**: High-quality vocal removal using MDX-Net and VR models
- **GPU Acceleration**: CUDA support for 5-10x faster processing
- **Multiple Models**: KARA_2 (quality), 6_HP-Karaoke (balanced), 5_HP-Karaoke (fast)

###  Professional Lyrics Editor
- **Waveform Visualization**: See audio peaks for precise timing
- **Dual-Line Preview**: Live karaoke preview with active/upcoming lines
- **Tap-to-Sync**: Spacebar timing for natural rhythm matching
- **Word-Level Editing**: Fine-tune individual word timings
- **AI Transcription**: Faster-Whisper integration (4x faster than standard models)
- **Multi-Format Import**: SRT, LRC, VTT, TTML, ASS/SSA subtitles
- **Romanization**: Automatic Korean/Japanese → Latin script
- **Local Model Detection**: Automatically finds and uses installed AI models

### ⏱ Advanced Timeline
- **Multi-Track Editing**: Separate tracks for audio, video, effects, lyrics
- **Clip Manipulation**: Drag, resize, split, delete clips
- **Effect System**: 8 effect types with custom Bezier curves
- **Snap-to-Grid**: Precise alignment with configurable grid
- **Sample-Accurate Timing**: Eliminates drift with AudioClock system

###  Flexible Video Export
| Mode | Video | Audio | Use Case |
|------|-------|-------|----------|
| **Karaoke Video** | Music Video | Instrumental | Sing-along karaoke |
| **Lyrics Music Video** | Music Video | Original | Music video with subtitles |
| **Karaoke (No Video)** | Solid Color | Instrumental | Classic karaoke style |
| **Lyrics Video** | Solid Color | Original | Lyric video |

###  Animation Styles
- **Linear Wipe**: Classic fill animation
- **Syllable Step**: Word-by-word highlighting
- **Glow Pulse**: Dynamic pulsing effect
- **Fade In**: Smooth opacity transitions
- **Bouncing Ball**: Retro bouncing indicator
- **Match Preview**: Export with exact preview styling
- **Countdown**: Automatic "3, 2, 1, GO" start display

---

## Subtitle Format Support

| Format | Extensions | Import | Export |
|--------|------------|:------:|:------:|
| SubRip | .srt | ✅ | ✅ |
| LRC Lyrics | .lrc | ✅ | ✅ |
| WebVTT | .vtt | ✅ | ✅ |
| TTML/DFXP | .ttml, .dfxp, .xml | ✅ | - |
| ASS/SSA | .ass, .ssa | ✅ | ✅ |
| Plain Text | .txt | ✅ | - |

---

## Plugins & Themes

###  Plugin System
Extend NC-KTV with custom functionality:
- **Effect Plugins**: Create custom visual effects and animations
- **Export Templates**: Add platform-specific export formats (YouTube, TikTok, etc.)
- **UI Extensions**: Add new tools and widgets

**Getting Started**:
- Browse installed plugins: **Settings → Plugins**
- Install `.nckplugin` packages
- Enable/disable plugins on the fly
- [Plugin Development Guide](PLUGIN_DEVELOPMENT.md)

###  Theme System
Customize NC-KTV's appearance:
- **UI Themes**: Change colors, fonts, and widget styles
- **Karaoke Styles**: Define custom video export styles
- **Built-in Themes**: Dark, Light (more community themes available)

**Getting Started**:
- Browse themes: **Settings → Themes**
- Import `.ncktheme` packages
- Apply themes without restart
- [Theme Creation Guide](THEME_CREATION.md)

###  Packaging Format
- **`.nckplugin`**: Plugin packages with manifest and code
- **`.ncktheme`**: Theme packages with YAML definitions
- Easy import/export and sharing

---

## Installation

### Prerequisites
- **Windows 10/11** (64-bit)
- **FFmpeg** in PATH ([Download](https://ffmpeg.org/download.html))
- **NVIDIA GPU** with CUDA 12.x (recommended for GPU acceleration)

### Quick Install
```powershell
# Clone repository
git clone https://github.com/AgentHitmanFaris/NC-KTV.git
cd NC-KTV

# Setup Python environment
.\setup_python.ps1

# Launch application
.\python_embed\python.exe main.py
```

### Verify Installation
```powershell
# Check FFmpeg
ffmpeg -version

# Check CUDA
.\python_embed\python.exe -c "import torch; print(f'CUDA: {torch.cuda.is_available()}')"
```

See [DOCS.md](DOCS.md) for detailed installation instructions.

---

## Quick Start

### 1. Wizard Mode
```
📁 Select File → 🎵 Start Processing → ✏️ Edit Lyrics
```

### 2. Lyrics Editor
- **Input Tab**: Paste or import lyrics
- **Sync Tab**: Use Spacebar to mark line timings
- **Preview**: Watch real-time karaoke display

### 3. Export
- **Ctrl+E**: Open export dialog
- Choose export mode and style
- Wait for rendering

### Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| Space | Set line start time |
| ↑/↓ | Navigate lines |
| J/K/L | Rewind/Pause/Forward |
| Ctrl+S | Save project |
| Ctrl+Z | Undo |
| Ctrl+E | Export video |
| F1 | Help |

---

## Architecture

```
NC-KTV/
├── src/
│   ├── core/           # Core processing (audio, video, timing)
│   ├── gui/            # PyQt6 user interface
│   ├── sync/           # Lyrics data structures
│   └── utils/          # Utilities (parsers, exporters)
├── models/             # UVR and Whisper models
├── assets/             # Icons and resources
└── Testing/            # Sample projects
```

See [DOCS.md](DOCS.md) for technical documentation with mathematical formulas.

---

## Roadmap

- [x] Phase 1-2: Core Backend & Wizard Mode
- [x] Phase 3: Lyrics Processing & Syncing
- [x] Phase 4: Video Generation & Styles
- [x] Phase 5: Word-Level Precision
- [x] Phase 6: Advanced Timeline & Effects
- [x] Phase 7: Community Themes & Plugins

---

## Performance

| Operation | GPU (Faster-Whisper) | CPU Only |
|-----------|----------------------|----------|
| Vocal Separation (3 min) | 15-30s | 2-5 min |
| AI Transcription (Faster-Whisper) | 10-20s | 40-80s |
| AI Transcription (Standard) | 40-80s | 2-4 min |
| Video Export (1080p) | 30-60s | 3-5 min |

---

## Contributing

Contributions welcome! Please read [DOCS.md](DOCS.md) for guidelines.

```
<type>(<scope>): <subject>

Types: feat, fix, docs, style, refactor, perf, test, chore
```

---

## License

MIT License - see [LICENSE](LICENSE).

---

## Acknowledgments

- **[UVR](https://github.com/Anjok07/ultimatevocalremovergui)** - Vocal removal AI models
- **[audio-separator](https://github.com/nomadkaraoke/python-audio-separator)** - Python UVR wrapper
- **[Faster-Whisper](https://github.com/SYSTRAN/faster-whisper)** - Optimized speech recognition (CTranslate2)
- **[OpenAI Whisper](https://github.com/openai/whisper)** - Original speech recognition models
- **[PyTorch](https://pytorch.org/)** - Deep learning framework
- **[PyQt6](https://www.riverbankcomputing.com/software/pyqt/)** - GUI framework
- **[FFmpeg](https://ffmpeg.org/)** - Video processing

---

<div align="center">

**Made for the karaoke community** 🎤

</div>
