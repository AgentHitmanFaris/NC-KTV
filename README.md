# NC-KTV - Music Video Karaoke Maker

<div align="center">

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Python](https://img.shields.io/badge/python-3.10-blue.svg)
![PyQt6](https://img.shields.io/badge/PyQt6-6.6+-green.svg)
![Status](https://img.shields.io/badge/status-alpha-orange.svg)

**Professional Windows desktop application for creating karaoke videos with automatic vocal removal and synchronized lyrics.**

[Features](#features) • [Installation](#installation) • [Usage](#usage) • [Documentation](#documentation) • [Contributing](#contributing)

</div>

---

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Screenshots](#screenshots)
- [Installation](#installation)
- [Quick Start](#quick-start)
- [Usage](#usage)
- [Documentation](#documentation)
- [Roadmap](#roadmap)
- [Contributing](#contributing)
- [License](#license)
- [Acknowledgments](#acknowledgments)

---

## Overview

NC-KTV automates the process of creating professional karaoke videos from music files or videos. Using state-of-the-art AI models for vocal removal and speech recognition, it generates synchronized lyric videos with customizable effects.

**Perfect for:**
- Content creators making karaoke videos
- Musicians creating backing tracks
- Educators preparing language learning materials
- Anyone who loves karaoke!

---

## Features

### Currently Implemented (Phase 2)

- **Automatic Vocal Removal**
  - GPU-accelerated processing using UVR 5 models
  - Support for VR_Models and MDX_Net_Models
  - High-quality instrumental track separation
  - CUDA 11.8 support for NVIDIA GPUs

- **Multi-Format Support**
  - Audio: MP3, WAV, FLAC, M4A, AAC, OGG
  - Video: MP4, AVI, MKV, MOV, WMV, FLV, WEBM

- **Wizard Mode Interface**
  - Step-by-step workflow for beginners
  - Drag-and-drop file selection
  - Real-time progress tracking
  - GPU status monitoring

- **High Performance**
  - Background processing with Qt threading
  - Progress reporting with ETA estimation
  - Cancellation support
  - GPU memory optimization

### Coming Soon (Phase 3+)

- Manual lyrics input with timeline editor
- AI-powered lyrics synchronization (Whisper)
- Professional karaoke video generation
- Customizable text effects and animations
- Advanced timeline editor with waveform visualization
- Batch processing support

---

## Screenshots

> Screenshots coming soon after UI refinement

---

## Installation

### Prerequisites

1. **Windows 10/11** (64-bit)
2. **FFmpeg** - [Download](https://ffmpeg.org/download.html) and add to PATH
3. **NVIDIA GPU** (optional, recommended for faster processing)
   - CUDA 11.8 compatible GPU
   - GTX 1060 or newer recommended

### Quick Install

#### Option 1: Automated Setup (Recommended)

```powershell
# 1. Clone the repository
git clone https://github.com/AgentHitmanFaris/NC-KTV.git
cd NC-KTV

# 2. Download Python 3.10.11 embedded
# Visit: https://www.python.org/ftp/python/3.10.11/python-3.10.11-embed-amd64.zip
# Extract to: python_embed\

# 3. Run automated setup
.\setup_python.ps1

# This will:
# - Enable pip
# - Install PyTorch with CUDA 11.8
# - Install all dependencies
# - Verify installation
```

#### Option 2: Manual PyTorch Installation

If you have slow internet or want to control the download:

```powershell
# See PYTORCH_MANUAL_INSTALL.md for detailed instructions
.\install_torch_manual.ps1
```

### Download UVR Models

NC-KTV requires UVR models for vocal removal:

```powershell
# Run the model migration script if you have UVR installed
.\migrate_uvr_models.ps1

# Or manually download recommended models to models/ directory:
# - UVR_MDXNET_KARA_2.onnx (recommended)
# - UVR-MDX-NET-Inst_HQ_3.onnx
```

**Download from:** [TRvlvr/model_repo](https://github.com/TRvlvr/model_repo/releases)

---

## Quick Start

```powershell
# Launch the application
python_embed\python.exe main.py
```

**First-time workflow:**

1. **Select File** - Choose your music/video file
2. **Configure Settings** - Select UVR model and enable GPU
3. **Start Processing** - Click "Start Processing" and wait
4. **Get Results** - Find instrumental track in `output/` directory

---

## Usage

### Wizard Mode (Beginner-Friendly)

**Step 1: File Selection**
- Use "Browse" button or drag-and-drop
- Supported formats displayed automatically
- File validation with clear error messages

**Step 2: Vocal Removal Settings**
- Choose from available UVR models
- Toggle GPU acceleration (if available)
- Monitor processing progress in real-time

### Advanced Features (Coming in Phase 6)

- Timeline-based editor
- Waveform visualization
- Manual timestamp adjustment
- Split/merge lyric segments

---

## Documentation

- **[SETUP.md](SETUP.md)** - Detailed setup instructions
- **[PYTORCH_MANUAL_INSTALL.md](PYTORCH_MANUAL_INSTALL.md)** - Manual PyTorch installation guide
- **[CHANGELOG.md](CHANGELOG.md)** - Version history and changes
- **[CONTRIBUTING.md](CONTRIBUTING.md)** - Contribution guidelines

### Configuration

Edit `config.yaml` to customize:

```yaml
# UVR Settings
uvr:
  default_model: "UVR_MDXNET_KARA_2.onnx"
  use_gpu: true
  
# Processing
processing:
  sample_rate: 44100
  output_dir: "output/"

# GUI
gui:
  theme: "dark"
  start_mode: "wizard"
```

---

## Roadmap

- [x] **Phase 1:** Research & Planning
- [x] **Phase 2:** Core Backend & Wizard Mode (Steps 1-2)
- [ ] **Phase 3:** Lyrics Processing & Synchronization
- [ ] **Phase 4:** Video Generation Pipeline
- [ ] **Phase 5:** Wizard Mode (Steps 3-5)
- [ ] **Phase 6:** Advanced Editor Mode
- [ ] **Phase 7:** Integration & Polish
- [ ] **Phase 8:** Testing & Release

See [task.md](.gemini/task.md) for detailed progress.

---

## Contributing

Contributions are welcome! Please see [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

### Development Setup

```powershell
# Clone repository
git clone https://github.com/AgentHitmanFaris/NC-KTV.git
cd NC-KTV

# Create feature branch
git checkout -b feature/your-feature-name

# Make changes and test
python_embed\python.exe main.py

# Commit and push
git add .
git commit -m "Description of changes"
git push origin feature/your-feature-name
```

### Areas for Contribution

- Bug fixes and testing
- Documentation improvements
- Internationalization (i18n)
- UI/UX enhancements
- Performance optimizations

---

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

---

## Acknowledgments

### Core Technologies

- **[UVR (Ultimate Vocal Remover)](https://github.com/Anjok07/ultimatevocalremovergui)** - Vocal removal models
- **[audio-separator](https://github.com/nomadkaraoke/python-audio-separator)** - Python wrapper for UVR
- **[PyTorch](https://pytorch.org/)** - Deep learning framework
- **[PyQt6](https://www.riverbankcomputing.com/software/pyqt/)** - GUI framework
- **[FFmpeg](https://ffmpeg.org/)** - Multimedia processing
- **[OpenAI Whisper](https://github.com/openai/whisper)** - Speech recognition (Phase 3)

### Inspiration

- [AnthemScore](https://www.lunaverus.com/) - Professional music transcription software
- [UVR5-UI](https://github.com/Eddycrack864/UVR5-UI) - UVR Gradio interface

---

## Support

- **Issues:** [GitHub Issues](https://github.com/AgentHitmanFaris/NC-KTV/issues)
- **Discussions:** [GitHub Discussions](https://github.com/AgentHitmanFaris/NC-KTV/discussions)

---

## Project Status

**Current Version:** 0.1.0-alpha  
**Status:** Active Development  
**Last Updated:** December 2025

NC-KTV is under active development. Phase 2 (Core Backend) is complete. Expect frequent updates as we implement Phase 3 (Lyrics Processing).

---

<div align="center">

**Made with ❤️ for the karaoke community**

[⬆ Back to Top](#nc-ktv---music-video-karaoke-maker)

</div>
