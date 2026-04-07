# NC-KTV Technical Documentation (C++ Architecture)

**Version:** 1.3.4 (Lyrics Display Modes + Tools Panel + Drag-to-Timeline)
**Last Updated:** April 7, 2026

Comprehensive technical documentation covering the internal architecture, memory models, hardware-accelerated rendering logic, and Windows executable resource management.

---

## Table of Contents

1. [Architectural Overview](#1-architectural-overview)
2. [Build System & Dependencies](#2-build-system--dependencies)
3. [Core Logic (`src/core`)](#3-core-logic)
4. [GUI rendering (`src/gui`)](#4-gui-rendering)
5. [Threading & Concurrency](#5-threading--concurrency)
6. [Plugin & Component Management](#6-plugin--component-management)

---

## 1. Architectural Overview

The migration from Python/PyQt6 to pure C++20/Qt6 was motivated by performance upper-bounds encountered during heavy waveform rendering and real-time audio separation. The new architecture completely decoupling the computationally intense engine operations from the main UI thread.

**Key Design Pillars:**
- **Zero-Copy Serialization**: Minimizing redundant state transformations between UI memory and playback buffers.
- **Immediate-Mode UI Influences**: The heavy `QPainter` widgets (e.g., `TimelineWidget`, `WaveformWidget`, `SyllableEditor`) function almost statelessly, deriving all visual transformations strictly from atomic `TimelineData` objects.
- **FFmpeg & ONNX Native**: Direct memory access to decode frames and infer via ONNX models without intermediate abstractions.

---

NC-KTV utilizes a modern **CMake** implementation with **CMake Presets** configured in `cpp/CMakePresets.json`. All tool paths point to the system Qt installation at `D:\ProgramData\Qt\` — no local `qt/` or `bin/` folder is required.

### Windows Resource Integration
The project includes a native Windows resource file (`cpp/src/gui/resources/app.rc`) that embeds the application icon (`logo.ico`) directly into the `.exe`. This ensures the application displays the correct brand icon in Windows Explorer and the Taskbar independently of the high-level `setWindowIcon` call.

### Core Dependencies:
- **Qt 6.10.2** (`Core`, `Gui`, `Widgets`, `Multimedia`): The foundational windowing and event backbone. Installed at `D:\ProgramData\Qt\6.10.2\mingw_64`.
- **FFmpeg 7.x**: Core A/V decoding/encoding logic mapped directly in `src/core/audio/ffmpeg_utils.cpp`. The Python AI bridge implements a robust auto-detection system to locate `ffmpeg.exe` and `ffprobe.exe` across development (native `.py`) and portable (PyInstaller `.exe`) environments. It injects the `ffmpeg/bin` directory directly into the global `PATH` at runtime, ensuring that dependencies like `audio-separator` and `pydub` operate reliably without requiring a manual system-wide installation.
- **ONNX Runtime**: Used extensively in `src/core/audio/vocal_remover.cpp` to execute the MDX-Net algorithms natively.
- **nlohmann_json**: High-speed, intuitive configuration and project state serialization.
- **yaml-cpp**: Powering the recursive `ConfigManager`.

---

## 3. Core Logic 

Located in `src/core/`, this library (`ncktv_core.lib`/`.a`) operates completely independent of the GUI. It handles project serialization, mathematical audio effects, subtitle parsing, and data modeling.

### Core Modules:
- **`TimelineData` & Structures**: Represents the hierarchical structure of a karaoke project.
  - `Track`: Represents a single Audio, Video, Effects, or Lyrics channel.
  - `Clip`: Atomic, movable timing units containing specific `Effect` or subtitle metadata.
- **`SubtitleParser`**: Lightning-fast RegEx parser for TTML, VTT, LRC, SRT. Converts all string constraints to standard `LyricsData` structures.
- **`AssGenerator`**: Constructs robust, deeply stylized `.ass` scripts. Features a dual-model interface supporting both Qt-native `LyricsData` (QString-based) and high-performance `core::LyricsData` (std::string-based), enabling seamless transitions between the UI and export engine.
- **`AudioProcessor`**: Manages FFT buffering via a native Mixed-Radix FFT (supporting arbitrary window sizes such as 6144, 7680), stem extraction, and vocal rendering.

---

## 4. GUI Rendering

Located in `src/gui/`, the UI utilizes Qt's `QWidget` event loops for complex timeline manipulations. Standard layout systems are augmented with highly optimized `paintEvent` overrides.

### The Editor Workspace (`EditorMode`):
Constructed entirely using nested `QSplitter` layouts, allowing robust resizing:
- **`AudioPlayer`**: Controls global playback state and implements hardware `QTimer` invalidations (refreshing at ~60Hz) specifically designed to circumvent asynchronous lag inherent to native `QMediaPlayer::seek` methodologies.
- **`KaraokePreview`**: Renders lyrics over video or on a pure black background. Two display modes — `VideoOverlay` (bottom-third, 60pt, 2 lines) and `CenteredBlack` (centered, 72pt, up to 4 lines) — toggled via a button in the Source Monitor panel header. HD rendering via `SmoothPixmapTransform` + `TextAntialiasing`.
- **`PrecisionMode` & `WordCanvas`**: A complete vertical cascading UI mirroring traditional Karaoke Builder Studio workflows. Employs a fixed left-aligned Y-axis waveform, enabling syllable blocks to be dragged vertically independently without interfering with standard X-axis track data.
- **`TimelineWidget`**: Responsible for the top-level horizontal composition view of multitracked clips. Accepts drag-and-drop of lyric blocks from the Lyrics Blocks Panel (MIME type `application/x-ncktv-lyric-index`), showing a green dashed drop preview and emitting `lyricDropped(lineIdx, time)` on release. Render operations are strictly debounced utilizing a >= 1.0 pixel threshold to radically lower `update()` saturation.
- **`WaveformWidget`**: Employs Level-of-Detail (LOD) downsampling; waveforms are generated dynamically using pixel-bucketing algorithms tracking the min/max variance inside a static screen column.
- **Lyrics Blocks Panel**: A 64px scrollable strip below the timeline showing all lyric lines as draggable `QPushButton` blocks. Click to seek; drag to timeline to reposition timing.
- **Tools Panel**: 8 tools matching Premiere Pro — Selection (V), Track Select (A), Rolling Edit (N), Razor (C), Slip (Y), Slide (U), Pen (P), Zoom (Z) — all with keyboard shortcuts and `QButtonGroup` mutual exclusion.

To prevent signal-flooding, UI interactions are debounced leveraging `QTimer::singleShot` proxies before committing large block mutations to `UndoManager`.

---

## 5. Threading & Concurrency

The separation of GUI and Processing relies extensively on an asynchronous event model.

- **Audio Extraction**: `VocalRemover` operates within a `std::thread` worker pool (or QThread/QRunnable). Results are dispatched back to the main thread via standard Qt `signals`.
- **AI Transcription (`Whisper`)**: Driven by child `QProcess` streams. Bridges the C++ app to the system Python `whisper` module via automatic path discovery (bundled portable, local venv, or system Python). A custom modal overlay actively consumes `stdout` signals to inform a real-time progress bar while disabling overlapping interface modifications.
- **State Mutation**: The `TimelineData` tree is strictly accessible by the GUI thread unless explicitly mutexed. Long-running structural mutations clone the project state entirely.
- **Undo / Redo Paradigm**: Deep copies (handled natively via `nlohmann_json` stringification or clone constructors) exist isolated from the `QObject` lifecycles.

---

## 6. Plugin & Component Management

The `PluginManager` allows for independent shared-object (`.dll`/`.so`) execution at runtime. This modular approach is currently employed by individual audio DSP effects (Reverbs, EQs) and visual shader implementations parsed natively from YAML logic constraints.

---

*For detailed API reference, Doxygen comments are embedded throughout all `.h` interfaces.*
