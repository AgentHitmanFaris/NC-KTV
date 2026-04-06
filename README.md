<div align="center">

<img src="assets/logo.png" alt="NC-KTV Logo" width="180"/>

# NC-KTV (C++ Edition) v1.3.2

**Next-Generation Professional Music Video Karaoke Maker**

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg?style=for-the-badge)](https://github.com/nc-ktv/cpp)
[![Qt Version](https://img.shields.io/badge/Qt-6.10.2-41CD52.svg?style=for-the-badge&logo=qt)](https://www.qt.io/)
[![CMake](https://img.shields.io/badge/CMake-3.21+-064F8C.svg?style=for-the-badge&logo=cmake)](https://cmake.org)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg?style=for-the-badge)](https://opensource.org/licenses/MIT)

*High-performance, native desktop application for creating professional karaoke videos with AI-powered vocal separation, automatic transcription, and synchronized lyrics.*

[**Features**](#core-features) • [**Installation**](#installation--build) • [**Architecture**](DOCS.md) • [**Changelog**](CHANGELOG.md)

</div>

---

## Overview

Welcome to the **C++ Qt6 Rebirth** of NC-KTV. 

Originally written in Python, we have completely overhauled the NC-KTV engine using modern **C++20 and Qt6** to achieve unparalleled performance, hardware-accelerated rendering, and a butter-smooth editing experience. This marks a massive leap in processing speed and UI responsiveness, enabling real-time waveform rendering, precise audio seeking, and seamless subtitle processing.

NC-KTV automates the entire karaoke video creation workflow:
1. **Import** any media file natively via FFmpeg.
2. **Extract** vocals cleanly using UVR-compatible **ONNX** models (MDX-Net) with a native C++ Mixed-Radix FFT DSP pipeline.
3. **Transcribe** lyrics automatically (Whisper) or import industry standards.
4. **Sync** lyrics with sub-millisecond precision using the new Hardware-Accelerated Timeline and **one-click Set Start/End** controls.
5. **Export** to professional-grade formats (ASS, MP4, MKV) with **GPU-accelerated encoding** and custom resolution scaling.

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
- **One-Click Timing Sync**: Dedicated "Set Start" and "Set End" buttons in the transport bar to instantly align lyrics to the current playhead.
- **GPU-Accelerated Export**: High-performance video rendering utilizing NVENC (NVIDIA), QSV (Intel), or AMF (AMD) with configurable resolution scaling (1080p to 360p).
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
- **MinGW-w64 (GCC 13.x)**: Installed via Qt at `D:\ProgramData\Qt\Tools\mingw1310_64`.
- **CMake 3.25+**: Installed via Qt at `D:\ProgramData\Qt\Tools\CMake_64`.
- **Qt 6.10.2**: Core, Gui, Widgets, Multimedia, Network — installed at `D:\ProgramData\Qt\6.10.2\mingw_64`.
- **Python 3.10+**: For the AI bridge (Whisper/UVR).

### Building from Source (Windows)

NC-KTV now features a streamlined, high-performance build pipeline using MinGW and a custom PowerShell script that handles both C++ compilation and Python environment bundling.

```powershell
# 1. Open PowerShell and navigate to the project root

# 2. Run the automated build script
# This script configures CMake, builds the C++ engine (Ninja),
# packages the Python AI bridge (PyInstaller), and assembles 
# the portable directory with all necessary DLLs.
.\build_portable_release.ps1
```

Once completed, the final portable application will be available in `NC-KTV-Portable\` with all dependencies (Qt, FFmpeg, ONNX, Python Bridge) properly staged.

For manual development/debugging:
1. Open the project in **VS Code** or **Qt Creator**.
2. Select the `windows-debug` or `windows-release` CMake preset.
3. Build using the standard CMake workflow (`Ctrl+Shift+B` in VS Code).

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
