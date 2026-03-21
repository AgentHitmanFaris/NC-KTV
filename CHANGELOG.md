# Changelog

All notable changes to NC-KTV will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [1.3.1] - 2026-03-22

### Fixed
- **FFmpeg Integration**: Fixed a critical issue where audio separation and metadata reading (via `audio-separator` and `pydub`) failed to process MP4/audio files due to incomplete FFmpeg environments. The Python bridge now explicitly requires and injects a standalone `ffmpeg/bin` directory (containing both `ffmpeg.exe` and `ffprobe.exe`) into the global `PATH`, completely replacing the unreliable `imageio-ffmpeg` hack.

---

## [1.3.0] - 2026-03-18

### Added
- **Hardware-Accelerated Export**: Integrated GPU encoding support for NVIDIA (NVENC), Intel (QSV), and AMD (AMF) via `GpuDetector`. Automatically selects the best available hardware encoder for significantly faster video rendering.
- **Export Resolution Scaling**: Added a resolution selector to the Export dialog, allowing users to downscale videos (1080p, 720p, 480p, 360p) for smaller file sizes.
- **Precision Timing Buttons**: Added "Set Start" and "Set End" buttons to the transport bar. These allow users to instantly stamp the current playhead position as the start or end time for the selected lyric line.

### Fixed
- **ASS Render Scaling**: Fixed a discrepancy where rendered ASS subtitles appeared smaller than the live preview by standardizing the default font size (60px).
- **Inline Edit Audio Jump**: Prevented the audio player from reset-seeking to 0:00 when double-clicking the "Text" column in the synchronization table.
- **Preview Stutter & Flickering**: Optimized the A/V synchronization frequency during playback to prevent aggressive decoder flushing, resulting in much smoother video previews.

---

## [1.2.0] - 2026-03-18

### Added
- **Gemini AI Web Integration**: New "Transcribe with Gemini" button in the Source Lyrics tab. Automatically compresses the active audio track to a compact MP3 file (via ExportWorker/ffmpeg) and opens the Gemini browser at the configured URL. A companion Explorer window highlights the output file for easy drag-and-drop upload.
- **Customizable Gemini URL**: A persistent text field allows the user to change their Gemini Gem link at any time without a rebuild.
- **Paste & Sync Button**: A dedicated clipboard-import button parses Gemini transcription output (supports range-format LRC: `[00:15.15 - 00:19.30]`) and directly populates the Synchronization Queue.
- **Dockable Panels**: The Synchronization Queue and Properties panels are now full `QDockWidget` instances. Users can tear them off, float them, and re-dock them anywhere on screen for a fully customized workspace.
- **GPU Acceleration Bundling**: `build_portable_release.ps1` now auto-detects CUDA/cuDNN DLLs in `models/whisper/cudn12/bin/` and bundles them into the portable package via PyInstaller's `--add-binary` flag. This enables GPU-accelerated Whisper inference (ONNXRuntime CUDA provider) without a system-wide CUDA installation.
- **Editable Source Lyrics**: The Source Lyrics view is now a fully editable `QPlainTextEdit` that highlights the currently playing line in real-time.

### Fixed
- **Lyrics Too Fast / Laggy Sync**: `TeleprompterView::updateTime()` replaced a linear scan with a **binary search** (`O(log n)`). The teleprompter now holds on the last finished line during inter-line gaps, and only previews the next line within a **0.8-second** lead-in (down from 2 seconds).
- **LRC Timestamp Parsing Failure**: `SubtitleParser` regex `[-\u2013\u2014]` was replaced with `(?:-|–|—)` to fix range-format detection across all dash variants.
- **Timing Sync button did nothing**: Wired up `connect()` handlers for `m_modeLyricsBtn` and `m_modeTimingBtn` — switching modes now correctly updates `m_viewStack`.
- **`m_consoleBtn` null crash**: The Debug Log button was declared but never initialized. Fixed by constructing and adding it to the sidebar layout.
- **`syncGridParent` stale lambda captures**: Renamed all captures to `updateDockVisibility`, matching the actual function name, preventing linker errors.
- **`durationChanged` malformed lambda**: Extracted the initial `updateDockVisibility(0, 0)` call from inside the lambda into the proper `setupUi()` scope end.
- **Namespace mismatch in Paste & Sync**: `ncktv::LyricsData` returned by `SubtitleParser` was being directly assigned to `m_project->lyrics` (type `core::LyricsData`). Fixed with explicit field-by-field conversion loop.

### Changed
- **Karaoke Preview Wipe Effect**: Upgraded from a binary color swap to a smooth **horizontal linear wipe** per word using QPainter clip rects and a cyan→blue gradient overlay.
- **Build Script**: Portable build now reports `>>> Bundling local CUDA/cuDNN DLLs for GPU Acceleration...` when GPU libraries are detected.

---

## [1.1.0] - 2026-03-17

### Added
- **Native Waveform Generation**: Replaced passive waveform display with a dedicated `WaveformWorker` that uses bundled FFmpeg to fast-decode and generate audio peaks in the background.
- **Dynamic Waveform Sync**: Waveform now automatically updates when switching between Original, Instrumental, and Vocal tracks in the editor.
- **Wizard Mode Header Menus**: Integrated "File", "Edit", and "Project" menus directly into the Wizard Mode splash screen for consistent project management from launch.
- **Console Access**: Connected the "Console" button to instantly open the `debug.log` file via system default editor.
- **Advanced Build Pipeline**: Integrated Nuitka compilation into `build_portable_release.ps1` for professional Python-to-C++ obfuscation of the AI bridge, effectively hiding the source code within a standalone executable.

### Fixed
- **"No Waveform" Issue**: Resolved the critical bug where the waveform display remained empty by implementing a native C++ background worker for audio peak calculation.
- **AI Model Obfuscation**: Updated the Python bridge to correctly resolve `.dat` model extensions, allowing PyTorch models to be renamed and secured in portable builds.
- **Build Stalling**: Enhanced the portable build script with verbose logging (`--verbose`, `--show-progress`) and non-interactive flags (`--assume-yes-for-downloads`) to prevent silent stalls during complex compilation phases.
- **Path Resolution**: Fixed build script failures by using absolute path resolution for Python interpreters and source files.

### Changed
- **Nuitka Environment**: Build script now prioritizes the bundled `python_embed` environment for Nuitka compilation to ensure dependency consistency across different development machines.
- **Project Structure Signals**: Added `requestOpen`, `requestNew`, and `requestPreferences` signals to `WizardMode` to unify project handling between the splash screen and main application.

---

## [1.0.0] - 2026-03-15

### Added
- **C++ Native Engine**: Complete engine rewrite from Python to C++17 for immense performance gains.
- **Hardware-Accelerated Timeline**: Rebuilt the Editor mode completely. Features ultra-smooth 60fps playhead tracking, down-sampled waveform rendering, and drag-and-drop subtitle chunks. 
- **Lyrical Pro Mode**: A brand-new vertical teleprompter UI, providing a focused environment identical to *Karaoke Builder Studio*. Syllables stack alongside a frozen waveform, enabling millisecond-accurate editing via a unified word map and isolated segment playback.
- **Native GUI-to-Core Bridge**: Implemented a dual-state `LyricsData` architecture allowing high-performance standard library operations (`std::string`/`std::vector`) inside the audio/DSP routines, while the Editor seamlessly updates via Qt-based structures (`QString`).
- **Universal Subtitle Importer**: Drag and configure `.srt`, `.lrc`, `.txt`, and Whisper `.json` payloads flawlessly aligning against the core timeline.
- **AssGenerator Overload**: Native support for `core::LyricsData` in `AssGenerator::generate`, allowing direct export from high-performance C++ core structures without extra serialization steps.
- **Whisper & LRCLib Integration**: Built a flexible subprocess Python bridge for AI auto-transcription (`--model turbo` resolving to `large-v3-turbo`) and connected LRCLib APIs for instantaneous cloud lyric fetching.
- **Model Manager**: Custom settings dialog to effortlessly manage UVR separation arrays and designate transcription languages.

### Fixed
- **Timeline/GUI Conflict**: Resolved severe architecture mismatch where the `SubtitleParser` passed GUI-level Qt objects directly into the `core::Project` struct. Standardized the ingest process bridging `QString` arrays to standard string arrays dynamically inside `EditorMode`.
- **ConfigManager Template**: Fixed C++ template syntax error in `ConfigManager::get` by adding the `template` keyword for dependent names, resolving MinGW/GCC compilation failures.
- **Vocal Separator Stability**: Transitioned the power-of-2 restricted FFT with a Mixed-Radix FFT implementation in DSP core, removing high-pitched distortion anomalies on isolated stems. Also resolved a crash transitioning from Wizard to Separator. 
- **Video Playback Parsing**: Corrected URL mappings for YouTube dependencies, ensuring `video` and `youtubeUrl` JSON keys properly feed the FFmpeg extraction endpoints.
- **MOP4/MP4 Export Error**: Verified FFmpeg parameter matches in `export_worker.cpp` establishing clear mapping for `.ass` subtitle burn-in.

### Changed
- **Portable Release Environment**: Perfected `build_portable_release.ps1` to actively stage `onnxruntime`, `ffmpeg`, custom FFmpeg libraries, and translation `.qm` bundles guaranteeing a zero-install C++ executable state.
- **Transcription Worker**: Migrated local operations back to an encapsulated Python executable state for Whisper CLI, avoiding `ggml.c` mingw linkage faults while maintaining native C++ UX.
- **AssGenerator Overload**: Added native support for `core::LyricsData` in `AssGenerator::generate`, allowing direct export from high-performance C++ core structures.
- **Whisper Configuration**: Added Whisper model selection to Preferences with settings persistence in `config.ini`.

### Fixed
- **ConfigManager Template**: Fixed C++ template syntax error in `ConfigManager::get` by adding the `template` keyword for dependent names, resolving MinGW/GCC compilation failures.
- **Editor Mode Compilation**: Fixed multiple missing header errors in `editor_mode.cpp` for `ExportDialog`, `PreferencesDialog`, and `AssGenerator`.
- **Preferences Dialog**: Fixed missing `QCoreApplication` include causing build failure in `preferences_dialog.cpp`.
- **Vocal Separator Stability**: Resolved a critical crash occurring in Wizard Mode when transitioning to Vocal Separation.
- **Video Export Sync**: Fixed FFmpeg parameter mismatch in `export_worker.cpp` that prevented successful burning of synchronized lyrics into video exports.

### Changed
- **Portable Release**: Optimized the `build_portable_release.ps1` script to ensure all necessary runtime DLLs and Python bridge files are correctly staged in the final output.

---

## [1.0.0-rc3] - 2026-03-14

### Fixed
- **Stem Separation Quality**: Replaced power-of-2 restricted FFT with a Mixed-Radix FFT implementation in `ncktv_mdx_dsp.hpp`. This fixes the "high-pitched / distorted" sound issue when using MDX-Net models with non-power-of-2 window sizes (e.g. 6144, 7680).
- **Qt Deprecation Warning**: Fixed deprecated `QMouseEvent` constructor in `word_editor.cpp` by using the modern Qt 6 constructor with `position()` and `globalPosition()`.
- **Memory Leak**: Fixed heap-allocated `QMouseEvent` in `WordCanvas::mouseReleaseEvent` that was never freed; replaced with stack-allocated event.

### Changed
- **Transcription Worker**: Restored the Python subprocess bridge for Whisper transcription (`python -m whisper`) with robust Python path detection (bundled portable, local venv, system fallback). The native C++ engine remains disabled on MinGW due to compiler limitations.
- **DSP Engine**: The `rfft` and `ifft_full` functions now operate directly on the original window size without zero-padding, ensuring spectral bin accuracy matches model expectations.

---

## [1.0.0-rc2] - 2026-03-06

### Added
- **New Project Dialog**: Full project creation UI with name, source file browser, UVR model selection, and GPU toggle.
- **Import Dialog**: Subtitle/lyrics file browser with auto format detection (LRC/SRT/VTT/ASS/TTML/JSON/TXT) and file preview.
- **Export Dialog**: 4 export presets (Karaoke Video, Lyrics Video, Audio Only, Subtitles Only), format/resolution/codec selection, lyrics style picker.
- **Video Options Dialog**: Resolution presets + custom, codec (H.264/H.265/VP9/AV1), FPS, bitrate slider, hardware encoding toggle.
- **Precision Mode Rebuild**: UI overhauled to strictly mirror the *Karaoke Builder Studio* vertical authoring experience.
  - **Vertical Word Canvas**: Syllables now stack vertically descending alongside a fixed left-aligned waveform visualization.
  - **Lyrics Map Table**: Right-sided `QTableWidget` to instantly select, edit, and navigate syllable text synchronizing perfectly with the canvas.
  - **Play Segment**: Added an auto-isolating playback button that seeks and automatically stops exactly on word boundaries, compensating for QMediaPlayer async lag.
  - **60fps Real-Time Sync**: Rewritten AudioPlayer utilizing `QTimer` at 16ms to power smooth auto-scrolling and frame-perfect Video Preview Picture-in-Picture sync.
- **Preferences Dialog**: 4-tab layout (General, Audio, AI Models, Paths) with start mode, auto-save, sample rate, GPU, model, and language config.
- **Shortcuts Dialog**: Comprehensive keyboard shortcut reference table with 17 entries and styled key highlighting.
- **Model Manager Dialog/Widget**: Whisper model list with install status, download/remove buttons, progress bar, and local directory scanning.
- **Plugin Manager Dialog**: Split-panel layout with plugin list, detail view (name/author/version/description), enable/disable toggle, install from `.nckplugin`, uninstall.
- **Theme Manager Dialog**: Split-panel with theme list, preview area, apply/install/uninstall controls, built-in theme protection.
- **Export Credits Dialog**: Song info (title/artist), custom credits text, countdown toggle, intro duration spinner.
- **Curve Editor Component**: Visual Bézier curve editor with grid, draggable control points, preset easing curves, and smooth cubic rendering.
- **Effect Panel Component**: Effect list with 9 effect types, start/duration/easing controls, integrated CurveEditor widget, bidirectional model sync.
- **Timing Calibration Component**: ±5s master offset slider (0.001s precision), large numeric display, per-source latency compensation (UVR/transcription/playback).

### Changed
- **Export Worker**: Full FFmpeg QProcess pipeline with subtitle burn-in, multi-codec support (H.264/H.265/VP9), stderr progress parsing.
- **Online Search Worker**: LRCLIB API integration via `LrcLibClient`, JSON result serialization, progress and error signals.
- **Processing Worker**: 2-phase pipeline orchestration (UVR separation 0-50% → Whisper transcription 50-100%) with graceful transcription fallback.
- **Main Window**: Wired Preferences, Theme Manager, Plugin Manager, and Shortcuts menus to actual dialog classes (removed placeholder QMessageBox stubs).
- **Root Path Detection**: Replaced hardcoded `D:/Document/NC-KTV` path in `main.cpp` with dynamic resolution — walks up from executable directory looking for `assets/logo.png` marker.
- **AudioClock Drift**: Implemented sample-rate-based drift calculation with latency correction summation in `getDriftAtTime()`.

### Fixed
- Missing `#include <QSpinBox>` in `export_credits_dialog.h` causing MSVC C2143 syntax error.

---

## [1.0.0-rc1] - 2026-03-05

### Added
- **C++ Rebirth**: Complete port of the NC-KTV engine to modern C++17 and Qt 6.10.2.
- **Logo & Splash**: Proper integration of application icon and splash screen.
- **Portable FFmpeg**: Automatic runtime path injection for bundled FFmpeg/FFprobe in `python_embed/Scripts`.
- **In-Editor Whisper AI**: Local AI auto-transcription directly in the Editor canvas with multistory language support (en, ms, id, ja, ko, zh).
- **Universal Subtitle Importer**: Added a dedicated native importer block for `.lrc`, `.srt`, `.txt`, and Whisper `.json` payloads syncing perfectly into visual representation.
- **Word-Level Subtitle Editing**: `TimelineWidget` now parses internal `LyricWord` structs from Whisper JSON and paints draggable word-edges within the main lyrics block.
- **Dimming AI Modal**: Replaced standard progress bars with a `ProcessingOverlay` modal that blocks interactions and dims the canvas exclusively while Whisper computes.

### Changed
- **Build System**: Refined CMake configuration to treat OpenSSL and zstd as optional with plain-JSON fallbacks.
- **Performance Optimizations**: 
  - Debounced the real-time playback cursor syncing for both the `TimelineWidget` and `WaveformWidget` (bypassing render operations until mathematically necessary).
  - Built a per-pixel aggregate bucketing algorithm in `WaveformWidget::drawWaveform` to compress millions of audio data points into single display columns.
- **GUI Guidance**: Replaced solid red hover-cursors on the waveform and timeline panels with translucent white alignment guides (`rgba(255, 255, 255, 60)`) to cleanly distinguish them from the primary Playhead.

### Fixed
- **Transition Crash**: Fixed critical "use-after-free" segmentation fault when switching from Wizard to Editor mode by implementing deferred deletion (`deleteLater()`).
- **Rendering Crash**: Resolved uninitialized pointer access in `TimelineWidget` paint events.
- **Separation Model Fallback**: Switched default UVR model to `6_HP-Karaoke-UVR.pth` to resolve GitHub connectivity / DNS issues during first run.
- **Path Sensitivity**: Hardcoded absolute root path detection for core assets to prevent CWD-related loading failures in terminal environments.

---

## [0.11.0] - 2026-02-04

### Added
- **Optimization**: Significant Syllable Editor performance boost (85-96% draw call reduction) using viewport culling.
- **OpenAI Model Support**: Native support for standard OpenAI `.pt` models in `models/whisper`.
- **Uninstall Button**: Added ability to uninstall `faster-whisper` models to free up disk space.

### Fixed
- **Syllable Editor Lag**: Fixed severe UI freeze on long videos (2+ mins) by only rendering visible syllables/rows.
- **Interaction Crash**: Fixed "prencede" crash when clicking the syllable canvas by adding robust error handling.
- **Video Sync**: Fixed playback drift, stop state handling, and optimized sync checks with frame-skipping.

### Changed
- **Model Paths**: Standardized local model folder names for `faster-whisper` (e.g., `faster-whisper-medium`).
- **Model Selection UI**: now clearly distinguishes between "Faster-Whisper" and "OpenAI Original" models.

## [0.10.3] - 2026-01-09

### Added
- **Fine-Tune Syllable Editor**: Dedicated tab for precise word-level lyric timing adjustment
- **Waveform Reference**: Full audio waveform displayed in the timeline and syllable editor
- **Interactive Timeline**: Click-to-scrub, auto-scroll, and zoom functionality
- **Dynamic Canvas**: Syllable editor automatically resizes to fit content length
- **Zero-Duration Fallback**: Untimed lyrics are assigned a default duration for immediate visibility

### Fixed
- **Canvas Rendering**: Fixed crash where editor failed to draw due to missing method
- **Invisible Lyrics**: Fixed issue where untimed lyrics had 0 duration and were hidden
- **Line 4+ Visibility**: Fixed canvas not expanding vertically to show later lines
- **Waveform Visibility**: Increased waveform opacity and detail for better reference

## [0.10.2] - 2026-01-09

### Added
- **Intro Mode (Pre-roll)**: Option to play credits *before* the song starts (concatenates intro + main video)
- **Countdown Feature**: Automatic "3, 2, 1, GO" count-in display if there is a >4s instrumental gap
- **Global Timing Offset in Export**: Now correctly applies the user-configured timing offset to exported ASS subtitles

### Fixed
- **Export Crash**: Fixed `AttributeError` in `ASSGenerator` when generating styles (color format helper issue)
- **Intro Mode Logic**: Fixed bug where intro mode was only applied if "Mixed" audio source was selected
- **Upcoming Lyrics in Export**: Fixed styling to correctly dim upcoming lyric lines in exported video

### Changed
- **ASS Color Handling**: Refactored color conversion logic in `ASSGenerator` for better stability

## [0.10.1] - 2026-01-07

### Added
- **OpenAI Model Support**: Explicitly lists `.pt` files as "(OpenAI)" in model selector
- **Polished Model List**: Cleaner names for local and cached models (e.g. "medium [Installed]")

### Fixed
- **Startup Crash**: Fixed `TypeError` in project initialization when starting in Editor mode
- **Runtime Crash**: Fixed Access Violation (0xC0000005) by removing conflicting CuDNN DLL loading
- **Model Redownload**: Fixed bug where internal path construction caused local models to be ignored
- **Startup Logic**: Fixed "Project creation cancelled" message appearing erroneously

### Changed
- **Default DLL Loading**: Reverted to standard embedded Python library loading for maximum stability

## [0.10.0] - 2026-01-07

### Added

#### Phase 7: Community Themes and Plugins
- **Plugin Architecture**: Extensible plugin system for custom effects and export templates
- **Plugin Manager**: GUI for installing, enabling, and configuring plugins
- **Plugin API**: Safe API for plugins to access NC-KTV features
- **Theme System**: YAML-based theme files for UI and karaoke styling
- **Theme Manager**: Visual theme browser with live preview and import/export
- **Built-in Themes**: Dark and Light themes with customizable color palettes
- **Example Plugin**: "Hello World" effect plugin with full documentation
- **Plugin Types**: Support for Effect, Export Template, and UI Extension plugins
- **Package Format**: `.nckplugin` and `.ncktheme` packages for easy distribution

#### Developer Tools
- **Plugin Development Guide**: Comprehensive documentation with examples
- **Theme Creation Guide**: Complete guide to creating custom themes
- **Plugin Manifest Format**: JSON-based plugin metadata with permissions system
- **Hot Reloading**: Reload plugins without restarting application

### Changed
- **Configuration System**: Added `plugins.enabled` array to track enabled plugins
- **Main Window**: Added "Themes" and "Plugins" menu items in Settings menu
- **Default Theme**: Changed from 'dark' to 'builtin-dark' for new theme system

### Technical
- Created `src/core/plugin_base.py` (350 lines) - Plugin base classes and interfaces
- Created `src/core/plugin_manager.py` (450 lines) - Plugin discovery and lifecycle
- Created `src/utils/theme_manager.py` (400 lines) - Theme loading and application
- Created `src/gui/dialogs/theme_manager_dialog.py` (300 lines) - Theme management UI
- Created `src/gui/dialogs/plugin_manager_dialog.py` (350 lines) - Plugin management UI
- Created `plugins/examples/hello_effect/` - Example effect plugin
- Created `PLUGIN_DEVELOPMENT.md` - Plugin developer documentation
- Created `THEME_CREATION.md` - Theme creation guide
- Enhanced `src/utils/config.py` - Added plugins configuration section
- Enhanced `src/gui/main_window.py` - Integrated plugin and theme managers

---

## [0.9.0] - 2026-01-06

### Added

#### AI Transcription Upgrades
- **Faster-Whisper Integration**: CTranslate2-based engine for 4x faster transcription
- **Hybrid Model Loading**: Supports both `.pt` (OpenAI format) and optimized Faster-Whisper models
- **Local Model Detection**: Automatically scans `models/whisper` for installed models
- **Smart GPU Fallback**: Attempts GPU (float16) → GPU (int8) → CPU for maximum compatibility
- **Custom DLL Loading**: Recursively searches for cuDNN/cuBLAS libraries in model folders

#### Export Enhancements
- **"Match Preview" Style**: Export videos with exact colors and styling from the karaoke preview widget
- **Custom Color Support**: Pass active, inactive, and outline colors from preview to ASS generator
- **Filtered Model List**: Model selection dialog now shows only locally available models

### Changed
- **CUDA Requirements**: Updated to CUDA 12.x (with automatic fallback for compatibility)
- **Model Selection UI**: Removed download-only models from the selection list for cleaner UX
- **Transcription Worker**: Refactored to intelligently switch between `whisper` and `faster-whisper` engines

### Technical
- Enhanced `src/utils/config.py` - Added `get_available_models()` with recursive `.bin` file search
- Enhanced `src/gui/editor/editor_mode.py` - Model path resolution and display name mapping
- Enhanced `src/workers/transcription_worker.py` - Dual-engine support with DLL path management
- Enhanced `src/utils/ass_generator.py` - "Match Preview" style generation from custom colors
- Enhanced `src/gui/dialogs/export_dialog.py` - Added "Match Preview" to lyrics style dropdown

### Fixed
- Fixed model loading for local `.pt` files by resolving full paths
- Fixed GPU initialization errors with robust try/catch fallback logic
- Fixed cuDNN DLL discovery in deeply nested installer directories

---

## [0.8.0] - 2026-01-03

### Added

#### Timeline & Effects (Phase 6.2)
- **Drag-and-Drop Clips**: Move clips on timeline with mouse, respects snap-to-grid
- **Clip Resizing**: Drag clip edges to trim duration, minimum 100ms
- **Clip Splitting**: Split clips at playhead with Ctrl+B, preserves effects
- **Clip Deletion**: Delete selected clips with Delete key
- **Effect Panel Dialog**: UI for adding/editing 8 effect types
- **Animation Curve Editor**: Visual Bezier curve editing with presets

#### Timing Synchronization (Phase 6.3)
- **AudioClock System**: Sample-accurate master timing reference
- **Sample Rate Validation**: Detects mismatches across audio files
- **Timing Calibration Dialog**: Visual tool for drift correction
- **Global Timing Offset**: ±5s adjustment with 0.001s precision
- **Latency Compensation**: Automatic corrections for UVR/transcription/playback

#### Universal Subtitle Support
- **SRT (SubRip)**: Full import/export with HH:MM:SS,mmm timestamps
- **LRC (Lyrics)**: Karaoke format with [MM:SS.xx] timestamps
- **VTT (WebVTT)**: HTML5 standard subtitle format
- **TTML/DFXP**: XML-based format (YouTube, Netflix)
- **ASS/SSA**: Advanced SubStation Alpha format
- **Unified Parser**: Auto-detection and seamless import

#### Flexible Export System
- **4 Export Presets**: Karaoke Video, Lyrics Video, Karaoke (No Video), Lyrics (No Video)
- **Custom Options**: Video source, audio track, background color selection
- **Export Validation**: Ensures required files are available

### Changed
- **Timeline Interaction**: Click empty space to seek, click clips to select
- **Import Dialog**: Unified file filter for all subtitle formats
- **Text Editor Sync**: Imported lyrics sync with editor to prevent timestamp reset

### Technical
- Created `src/utils/subtitle_parser.py` (400+ lines) - Universal subtitle parser
- Created `src/utils/srt_parser.py` - SRT format parser
- Created `src/utils/lrc_parser.py` - LRC format parser
- Enhanced `src/sync/sync_data.py` - Added duration, romanized_text, import_from_text
- Enhanced `src/core/audio_clock.py` - Sample-accurate timing system
- Enhanced `src/gui/dialogs/export_dialog.py` - Flexible export options

### Fixed
- **Subtitle Parsing**: Fixed Windows line ending handling (CRLF)
- **Timestamp Reset Bug**: Fixed lyrics import wiping timestamps when switching tabs
- **LyricLine Compatibility**: Added missing duration and romanized_text properties
- **Transcription Completion**: Fixed add_line() signature mismatch
- **K Key Shortcut**: Uses correct toggle_playback() method

---

## [0.7.0] - 2026-01-02

### Added
- **Phase 6.1: Core Timeline System**: Multi-track timeline editor with visual timeline view
- **Timeline Widget**: Graphical timeline with ruler, zoom controls (10-200px/s), and snap-to-grid
- **Multi-Track Support**: Color-coded tracks for Audio (blue), Video (pink), Effects (green), Lyrics (gold)
- **Timeline Data Model**: Track, Clip, and Effect classes with full serialization support
- **Playhead Synchronization**: Real-time playhead visualization synced with audio playback
- **Click-to-Seek**: Click timeline ruler to jump to any position
- **Timeline Toggle**: "Timeline View" button in editor to show/hide timeline panel
- **Auto-Population**: Timeline automatically populated with audio, video, and lyrics tracks on project load
- **Lyrics Timeline Clips**: Each lyric line appears as a clip on the timeline showing the actual lyrics text
- **J/K/L Shortcuts**: Professional playback control - J (rewind 5s), K (pause/play), L (forward 5s)

### Changed
- **Project Format**: Bumped to v0.7 to support timeline data (backward compatible with v0.6)
- **LyricsLine**: Extended with `effects`, `animation_curve`, and `custom_curve_points` fields
- **Editor Layout**: Timeline widget added in vertical splitter below sync table
- **Timeline Clips**: Display lyrics text instead of generic clip IDs for better readability

### Technical
- Created `src/core/timeline_data.py` (307 lines) - Timeline data structures
- Created `src/gui/components/timeline_widget.py` (402 lines) - Timeline UI widget
- Auto-migration from v0.6 projects (creates empty timeline if missing)
- Timeline auto-initializes with default tracks (instrumental, vocals, video, lyrics)

---

## [0.6.1] - 2026-01-01

### Added
- **Global Timing Offset**: Adjust all lyrics timing to compensate for AI transcription delay
- **Adjustable Preview Lead Time**: Configure how early upcoming lyrics appear in karaoke mode

### Changed
- **Improved Transcription Source Priority**: Now uses original audio by default for better accuracy
- **Enhanced Karaoke Preview Logic**: Better handling of current/upcoming line transitions
- **Refined Timing Synchronization**: Eliminated perceived playback delay in lyrics display

### Fixed
- Fixed typo in CHANGELOG.md (keepachanglog → keepachangelog)
- Fixed typo in wiki documentation (Using-the-Lyrics-Editor.md)
- Fixed lyrics display timing to match actual audio playback accurately

---

## [0.6.0] - 2026-01-01

### Added
- **Lyrics File Import**: Import from .txt or .lrc files with timestamp parsing
- **Playback Speed Control**: Adjust speed (0.5x - 2.0x) for easier synchronization
- **Cross-Project Import**: Import lyrics, audio, or metadata from other .nctv files
- **Model Manager**: One-click Whisper model downloads with progress tracking
- **Encrypted .nctv Format**: Secure binary project files with AES-256-GCM encryption
- **Save Prompts**: Warns before closing with unsaved changes

### Changed
- Improved dirty state tracking across all edit operations
- Enhanced project file format with chunked streaming for large files
- Updated preferences dialog with model management tab

### Fixed
- Fixed AI transcription crash (missing `clear()` method in sync_data.py)
- Fixed import paths for ModelManagerWidget

---

## [0.5.0] - 2025-12-31

### Added
- **Multiple Animation Types**: Choose from 5 karaoke animations:
  - Linear Wipe (classic left-to-right fill)
  - Syllable Step (instant word fill)
  - Glow Pulse (pulsing glow effect)
  - Fade In (words fade from transparent)
  - Bouncing Ball (classic karaoke ball)
- **Keyboard Shortcuts Help**: Press F1 to see all shortcuts
- **Undo/Redo System**: Ctrl+Z/Y to undo/redo lyrics changes
- **Help Button**: Quick access to shortcuts in editor toolbar

### Changed
- Improved audio track switching (seamless position preservation)
- Streamlined documentation (combined into README, CHANGELOG, DOCS)
- Polished .gitignore with better organization

### Fixed
- Fixed audio not playing when switching tracks while playing

---

## [0.4.0] - 2025-12-31

### Added
- **Karaoke Video Export**: Generate MP4 videos with burned-in karaoke lyrics
- **Video Styles**: "Neon Gold", "Classic Blue", "Modern Clean", "Fire Red"
- **Smart Lyric Preview**: Accurate karaoke-style fills with active/upcoming lines
- **Word-Level Editing**: "Edit Words" dialog for precise word timings

### Changed
- Added official NC-KTV app icon
- Optimized preview rendering for smoother playback
- Improved Lyrics Editor layout

### Fixed
- Fixed crash in "Edit Words" dialog
- Fixed "Upcoming Lyrics" disappearing during preview
- Fixed `RecursionError` in Auto-Transcribe
- Fixed table edits not updating preview in real-time

---

## [0.3.0] - 2025-12-30

### Added
- **Lyrics Editor Mode**: Professional synchronization interface
- **AI Auto-Transcription**: Whisper-powered automatic lyric generation
- **Tap-to-Sync**: Spacebar-based timestamp marking
- **Waveform Visualization**: Visual audio representation
- **Click-to-Jump**: Click table rows to seek playback

### Changed
- Redesigned main interface with Wizard → Editor flow
- Enhanced project structure for better organization

---

## [0.2.0] - 2025-12-30

### Added
- **Vocal Separation**: UVR-powered instrumental/vocal isolation
- **GPU Acceleration**: CUDA support for faster processing
- **Model Selection**: Support for multiple UVR models

---

## [0.1.0] - 2025-12-30

### Added
- Initial project setup
- Basic wizard interface
- Audio file support (MP3, WAV, MP4)

---

## [0.0.0] - 2025-12-29

### Project Initialization
- Repository created
- Initial project structure

---

[0.0.0]: https://github.com/AgentHitmanFaris/NC-KTV/releases/tag/v0.0.0
