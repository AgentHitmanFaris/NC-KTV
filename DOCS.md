# NC-KTV Technical Documentation (C++ Architecture)

**Version:** 1.0.0 (C++ Core Rebirth)
**Last Updated:** March 2026

Comprehensive technical documentation covering the internal architecture, memory models, and hardware-accelerated rendering logic used in the NC-KTV C++ suite.

---

##  Table of Contents

1. [Architectural Overview](#1-architectural-overview)
2. [Build System & Dependencies](#2-build-system--dependencies)
3. [Core Logic (`src/core`)](#3-core-logic)
4. [GUI rendering (`src/gui`)](#4-gui-rendering)
5. [Threading & Concurrency](#5-threading--concurrency)
6. [Plugin & Component Management](#6-plugin--component-management)

---

## 1. Architectural Overview

The migration from Python/PyQt6 to pure C++17/Qt6 was motivated by performance upper-bounds encountered during heavy waveform rendering and real-time audio separation. The new architecture completely decoupling the computationally intense engine operations from the main UI thread.

**Key Design Pillars:**
- **Zero-Copy Serialization**: Minimizing redundant state transformations between UI memory and playback buffers.
- **Immediate-Mode UI Influences**: The heavy `QPainter` widgets (e.g., `TimelineWidget`, `WaveformWidget`, `SyllableEditor`) function almost statelessly, deriving all visual transformations strictly from atomic `TimelineData` objects.
- **FFmpeg & ONNX Native**: Direct memory access to decode frames and infer via ONNX models without intermediate abstractions.

---

## 2. Build System & Dependencies

NC-KTV utilizes a modern **CMake** implementation. While it supports **vcpkg** for dependency management, the preferred workflow (especially on Windows) is using the pre-configured **CMake Presets** and Qt's bundled toolchain for a consistent, zero-config experience.

### Core Dependencies:
- **Qt 6.x** (`Core`, `Gui`, `Widgets`, `Multimedia`): The foundational windowing and event backbone.
- **FFmpeg 7.x**: Core A/V decoding/encoding logic mapped directly in `src/core/audio/ffmpeg_utils.cpp`.
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
- **`AssGenerator`**: Constructs robust, deeply stylized `.ass` scripts directly utilizing `QColor` matrices.
- **`AudioProcessor`**: Manages FFT buffering, stem extraction, and vocal rendering.

---

## 4. GUI Rendering

Located in `src/gui/`, the UI utilizes Qt's `QWidget` event loops for complex timeline manipulations. Standard layout systems are augmented with highly optimized `paintEvent` overrides.

### The Editor Workspace (`EditorMode`):
Constructed entirely using nested `QSplitter` layouts, allowing robust resizing:
- **`AudioPlayer`**: Controls global playback state and triggers rapid `QTimer` invalidations (refreshing at ~60Hz).
- **`SyllableEditor`**: Manages fine-tuning the active syllable's word-spacing via native font metric calculation.
- **`TimelineWidget`**: Responsible for the top-level composition view of multitracked clips.
- **`WaveformWidget`**: Employs Level-of-Detail (LOD) downsampling; waveforms are generated sequentially and cached natively to avoid blocking the main thread during scrolling.
- **`KaraokePreview`**: A robust layout container rendering real-time representations of the `.ass` generator output atop a video background.

To prevent signal-flooding, UI interactions are debounced leveraging `QTimer::singleShot` proxies before committing large block mutations to `UndoManager`.

---

## 5. Threading & Concurrency

The separation of GUI and Processing relies extensively on an asynchronous event model.

- **Audio Extraction**: `VocalRemover` operates within a `std::thread` worker pool (or QThread/QRunnable). Results are dispatched back to the main thread via standard Qt `signals`.
- **State Mutation**: The `TimelineData` tree is strictly accessible by the GUI thread unless explicitly mutexed. Long-running structural mutations clone the project state entirely.
- **Undo / Redo Paradigm**: Deep copies (handled natively via `nlohmann_json` stringification or clone constructors) exist isolated from the `QObject` lifecycles.

---

## 6. Plugin & Component Management

The `PluginManager` allows for independent shared-object (`.dll`/`.so`) execution at runtime. This modular approach is currently employed by individual audio DSP effects (Reverbs, EQs) and visual shader implementations parsed natively from YAML logic constraints.

---

*For detailed API reference, Doxygen comments are embedded throughout all `.h` interfaces.*
