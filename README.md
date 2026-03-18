<div align="center">

<img src="assets/logo.png" alt="NC-KTV Logo" width="180"/>

# NC-KTV (C++ Edition)

**Next-Generation Professional Music Video Karaoke Maker**

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg?style=for-the-badge)](https://github.com/nc-ktv/cpp)
[![Qt Version](https://img.shields.io/badge/Qt-6.x-41CD52.svg?style=for-the-badge&logo=qt)](https://www.qt.io/)
[![CMake](https://img.shields.io/badge/CMake-3.21+-064F8C.svg?style=for-the-badge&logo=cmake)](https://cmake.org)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg?style=for-the-badge)](https://opensource.org/licenses/MIT)

*High-performance, native desktop application for creating professional karaoke videos with AI-powered vocal separation, automatic transcription, and synchronized lyrics.*

[**Features**](#core-features) • [**Installation**](#installation--build) • [**Architecture**](DOCS.md) • [**Changelog**](CHANGELOG.md)

</div>

---

## Overview

Welcome to the **C++ Qt6 Rebirth** of NC-KTV. 

Originally written in Python, we have completely overhauled the NC-KTV engine using modern **C++17 and Qt6** to achieve unparalleled performance, hardware-accelerated rendering, and a butter-smooth editing experience. This marks a massive leap in processing speed and UI responsiveness, enabling real-time waveform rendering, precise audio seeking, and seamless subtitle processing.

NC-KTV automates the entire karaoke video creation workflow:
1. **Import** any media file natively via FFmpeg.
2. **Extract** vocals cleanly using UVR-compatible **ONNX** models (MDX-Net) with a native C++ Mixed-Radix FFT DSP pipeline.
3. **Transcribe** lyrics automatically (Whisper) or import industry standards.
4. **Sync** lyrics with sub-millisecond precision using the new Hardware-Accelerated Timeline.
5. **Export** to professional-grade formats (ASS, MP4, MKV).

---

## Core Features

### AI Vocal Separation
- **High-Performance Bridge**: Driven by the authentic `audio-separator` Python library for 100% matching UVR quality.
- **Hardware Acceleration**: Automatic target detection for CUDA (NVIDIA) via PyTorch. Local GPU libraries in `models/whisper/cudn12/` are auto-bundled into the portable build.
- **Full Model Support**: Supports all UVR models including MDX-Net, VR Architecture, and Roformer (`.onnx` and `.pth`).

### Hardware-Accelerated Studio Editor
- **Native Qt6 UI**: Butter-smooth 60fps+ rendering of complex timeline data via `QPainter` and Hardware Accel, featuring smart render-debouncing.
- **Interactive Waveforms**: Zoom, scrub, and manipulate gigabytes of audio data instantaneously using pixel-bucketing compression without UI blocking.
- **Precision Karaoke Builder Studio Mode**: Brand-new fully vertical syllable tracking interface. Features cascading blocks locked to a Y-axis left-waveform display, a dedicated instant-update Lyrics Map sidebar for transcription tuning, fully-synchronized auto-scrolling, and millisecond-accurate "Play Segment" vocal isolation capabilities.
- **Dockable Workspace Panels**: The Synchronization Queue and Properties panels are full `QDockWidget` instances — tear off, float, and re-dock them anywhere for a completely custom workspace layout.
- **In-Editor AI Support**: Kick off Whisper transcripts dynamically directly from the editor mode, complete with language override parameters and dimming modal overlays.
- **Live Karaoke Preview**: Configurable zero-latency ASS subtitle rendering overlaid onto the active video track with a smooth **horizontal linear wipe** effect per word.

### Intelligent Transcription and Sync
- **Whisper Powered**: Uses OpenAI's Whisper via Python subprocess for robust, high-speed transcription.
- **Word-Level Precision**: Automatic word-timestamp generation for perfect syllable alignment.
- **Targeted AI Control**: Select Whisper models (base, small, medium, large, turbo) and specific ISO codes (en, id, ms, ja, ko) inside the UI to balance speed vs. accuracy.
- **Tap-to-Sync Engine**: Rebuilt event-driven synchronization for perfect rhythm matching.
- **Auto-Romanization**: Lightning-fast transliteration of global scripts (Korean/Japanese to Latin).

### Gemini AI Integration
- **Transcribe with Gemini**: One-click button in the Source Lyrics tab that compresses the active audio to a small MP3 file and opens your custom Gemini Gems link in the browser. Simply upload the MP3, copy Gemini's output, and click **Paste & Sync** in the app.
- **Smart Paste & Sync**: Parses Gemini/AI transcription text (plain or LRC format with range timestamps like `[00:15.15 - 00:19.30]`) and directly loads it into the Synchronization Queue.
- **Configurable URL**: Paste your own Gemini Gem link directly in the UI so the app always opens the right transcription tool.

---

## Installation and Build

NC-KTV is now built using standard `CMake` and requires a modern C++17 compliant toolchain.

### Prerequisites
- **Visual Studio 2022** (Windows) / **GCC 11+** (Linux) / **Clang 14+** (macOS)
- **CMake** 3.21 or higher
- **Qt 6.x** (Core, Gui, Widgets)
- **vcpkg** (for automatic dependency management)

### Building from Source (Windows)

To build the project on Windows using the provided presets:

```powershell
# 1. Initialize MSVC environment and configure
cmd /c "call \"D:\ProgramData\Microsoft Visual Studio\VC\Auxiliary\Build\vcvars64.bat\" && \"D:\ProgramData\Qt\Tools\CMake_64\bin\cmake.exe\" --preset windows-debug"

# 2. Build the application
cmd /c "call \"D:\ProgramData\Microsoft Visual Studio\VC\Auxiliary\Build\vcvars64.bat\" && \"D:\ProgramData\Qt\Tools\CMake_64\bin\cmake.exe\" --build --preset windows-debug"

# 3. Run tests
$env:PATH = "D:\ProgramData\Qt\6.10.2\msvc2022_64\bin;" + $env:PATH
.\out\build\windows-debug\tests\ncktv_tests.exe
```

The compiled executable will be at `out/build/windows-debug/src/gui/ncktv.exe`.

Once compiled, the `ncktv` executable and tests will be placed in `cpp/out/build/windows-release/`.

---

## Architecture & Documentation

For a deep dive into the completely revamped C++ architecture, hardware-accelerated UI patterns, and the multithreaded audio pipeline, please see our dedicated [**Technical Documentation (DOCS.md)**](DOCS.md).

---

## Contributing

We welcome contributions to the NC-KTV C++ engine! 
- Please ensure PRs targeting core systems compile successfully across MSVC, GCC, and Clang.
- Run the included `GTest` suite via `ctest` before opening a pull request.

---

## License
This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
