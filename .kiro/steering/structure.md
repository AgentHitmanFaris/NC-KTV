# Project Structure

```
NC-KTV/
├── cpp/                        # C++ application (primary codebase)
│   ├── CMakeLists.txt          # Root CMake — finds Qt, fetches dependencies
│   ├── CMakePresets.json       # Build presets (windows-release, windows-debug)
│   ├── src/
│   │   ├── core/               # ncktv_core — static library, no GUI dependencies
│   │   │   ├── audio/          # AudioClock, DSP engine interfaces
│   │   │   ├── config/         # ConfigManager (YAML/INI read/write)
│   │   │   ├── effects/        # EffectCompositor
│   │   │   ├── lyrics/         # LyricsData structs
│   │   │   ├── net/            # LrcLibClient (HTTP)
│   │   │   ├── nlohmann/       # Vendored json.hpp
│   │   │   ├── parsers/        # LRC, SRT, ASS parsers + generator
│   │   │   ├── plugins/        # Plugin base + manager
│   │   │   ├── project/        # Project I/O, .nctv format
│   │   │   ├── system/         # GpuDetector
│   │   │   ├── text/           # Romanizer
│   │   │   ├── themes/         # ThemeManager
│   │   │   └── timeline/       # TimelineData structs
│   │   └── gui/                # ncktv — WIN32 executable
│   │       ├── main.cpp
│   │       ├── main_window.cpp/h
│   │       ├── wizard/         # WizardMode — project creation splash
│   │       ├── editor/         # EditorMode — main timeline editor
│   │       ├── precision/      # PrecisionMode — syllable-level editor
│   │       ├── components/     # Reusable widgets (waveform, timeline, preview, etc.)
│   │       ├── dialogs/        # All QDialog subclasses
│   │       ├── workers/        # QThread workers (transcription, export, waveform, etc.)
│   │       └── resources/      # .qrc, .rc, icons, QSS stylesheets
│   └── tests/                  # GoogleTest suites
│       ├── test_*.cpp          # Core unit tests → ncktv_tests executable
│       └── test_ui_*.cpp       # GUI/widget tests → ncktv_gui_tests executable
├── python_bridge.py            # Python AI bridge entry point
├── python_embed/               # Embedded Python runtime (portable)
├── build_pyinstaller/          # PyInstaller output (python_bridge folder)
├── build_pyinstaller_work/     # PyInstaller work/cache directory
├── build_portable_release.ps1  # Full portable build script
├── assets/                     # App assets (logo, etc.)
├── models/                     # AI models (whisper, UVR) — not in source control
├── themes/                     # Theme files (.ncktheme)
├── plugins/                    # Plugin packages (.nckplugin)
├── NC-KTV-Portable/            # Portable build output (generated, not in source control)
├── config.yaml                 # Default app configuration
├── config.ini                  # Runtime config (persisted preferences)
└── .kiro/
    ├── specs/                  # Feature/bugfix specs
    └── steering/               # AI steering rules (this directory)
```

## Architecture Patterns

### Core / GUI Separation
`ncktv_core` is a pure static library with no GUI code. GUI targets link against it. Never add Qt Widgets/Multimedia to core.

### Dual Data Model
Two parallel data representations exist:
- `core::LyricsData` / `ncktv::LyricsData` — std-based, used in core/DSP
- Qt-based structures in the GUI layer

When passing data between layers, convert explicitly (field-by-field). Do not assign across namespaces directly.

### Worker Pattern
All long-running operations (transcription, export, waveform generation, vocal separation) use dedicated `QThread`-based worker classes in `cpp/src/gui/workers/`. Workers emit signals for progress and completion; never block the GUI thread.

### AI Operations
Never call Python/AI functionality directly from C++. All AI work goes through the Python subprocess bridge. Workers in `workers/transcription_worker.cpp` and `workers/vocal_separator_worker.cpp` manage the subprocess lifecycle.

### CMake Conventions
- AUTOMOC/AUTORCC/AUTOUIC are enabled **after** all `FetchContent_MakeAvailable()` calls to prevent Qt's MOC from processing third-party headers.
- Feature flags use compile definitions: `NCKTV_HAS_ONNX`, `NCKTV_HAS_OPENSSL`, `NCKTV_HAS_ZSTD`, `NCKTV_HAS_VULKAN`, `NCKTV_HAS_ICU`.
- Optional dependencies degrade gracefully — always check the flag before using the feature.
- Qt, CMake, Ninja, and MinGW are installed system-wide at `D:\ProgramData\Qt\` — do not reference a local `qt/` or `bin/` folder.
