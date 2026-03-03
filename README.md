<div align="center">

<img src="assets/logo.png" alt="NC-KTV Logo" width="180"/>

# NC-KTV (C++ Edition)

**Next-Generation Professional Music Video Karaoke Maker**

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg?style=for-the-badge)](https://github.com/nc-ktv/cpp)
[![Qt Version](https://img.shields.io/badge/Qt-6.x-41CD52.svg?style=for-the-badge&logo=qt)](https://www.qt.io/)
[![CMake](https://img.shields.io/badge/CMake-3.21+-064F8C.svg?style=for-the-badge&logo=cmake)](https://cmake.org)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg?style=for-the-badge)](https://opensource.org/licenses/MIT)

*High-performance, native desktop application for creating professional karaoke videos with AI-powered vocal separation, automatic transcription, and synchronized lyrics.*

[**Features**](#-core-features) • [**Installation**](#-installation--build) • [**Architecture**](DOCS.md) • [**Changelog**](CHANGELOG.md)

</div>

---

## Overview

Welcome to the **C++ Qt6 Rebirth** of NC-KTV. 

Originally written in Python, we have completely overhauled the NC-KTV engine using modern **C++17 and Qt6** to achieve unparalleled performance, hardware-accelerated rendering, and a butter-smooth editing experience. This marks a massive leap in processing speed and UI responsiveness, enabling real-time waveform rendering, precise audio seeking, and seamless subtitle processing.

NC-KTV automates the entire karaoke video creation workflow:
1. **Import** any media file natively via FFmpeg.
2. **Separate** vocals from instrumentals via hardware-accelerated AI models.
3. **Transcribe** lyrics automatically (Whisper/ONNX) or import industry standards.
4. **Sync** lyrics with sub-millisecond precision using the new Hardware-Accelerated Timeline.
5. **Export** to professional-grade formats (ASS, MP4, MKV).

---

## Core Features

###  AI Vocal Separation (Lightning Fast)
- **High-Performance Inference**: Re-implemented with native ONNX Runtime for drastically reduced latency.
- **Hardware Acceleration**: Automatic target detection for CUDA (NVIDIA), DirectML (Windows), and CoreML (Apple Silicon).
- **Supported Models**: Native integration with MDX-Net and Ultimate Vocal Remover (UVR) ecosystems.

###  Hardware-Accelerated Studio Editor
- **Native Qt6 UI**: Butter-smooth 60fps+ rendering of complex timeline data via `QPainter` and Hardware Accel.
- **Interactive Waveforms**: Zoom, scrub, and manipulate gigabytes of audio data instantaneously without UI blocking.
- **Precision Syllable Editing**: Fine-tune word and syllable timings natively without lag.
- **Live Karaoke Preview**: Configurable zero-latency ASS subtitle rendering overlaid onto the active video track.

###  Intelligent Transcription & Sync
- **Format Agnostic**: Blazing-fast C++ parsers for SRT, LRC, VTT, TTML, ASS, and SSA.
- **Tap-to-Sync Engine**: Rebuilt event-driven synchronization for perfect rhythm matching.
- **Auto-Romanization**: Lightning-fast transliteration of global scripts (Korean/Japanese to Latin).

---

##  Installation & Build

NC-KTV is now built using standard `CMake` and requires a modern C++17 compliant toolchain.

### Prerequisites
- **Visual Studio 2022** (Windows) / **GCC 11+** (Linux) / **Clang 14+** (macOS)
- **CMake** 3.21 or higher
- **Qt 6.x** (Core, Gui, Widgets)
- **vcpkg** (for automatic dependency management)

### Building from Source

```bash
# 1. Clone the repository
git clone https://github.com/nc-ktv/cpp.git
cd cpp

# 2. Configure the project via CMake presets
cmake --preset windows-release

# 3. Build the application
cmake --build --preset windows-release

# 4. Run tests
ctest --preset windows-release
```

Once compiled, the `ncktv` executable and tests will be placed in `cpp/out/build/windows-release/`.

---

##  Architecture & Documentation

For a deep dive into the completely revamped C++ architecture, hardware-accelerated UI patterns, and the multithreaded audio pipeline, please see our dedicated [**Technical Documentation (DOCS.md)**](DOCS.md).

---

##  Contributing

We welcome contributions to the NC-KTV C++ engine! 
- Please ensure PRs targeting core systems compile successfully across MSVC, GCC, and Clang.
- Run the included `GTest` suite via `ctest` before opening a pull request.

---

##  License
This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
