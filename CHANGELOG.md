# Changelog

All notable changes to NC-KTV will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Planned
- Manual lyrics input interface
- Whisper AI integration for auto-synchronization
- Karaoke video generation with text effects
- Advanced timeline editor
- Batch processing support

---

## [0.1.0-alpha] - 2025-12-30

### 🎉 Initial Alpha Release

First development release of NC-KTV with core backend functionality and basic GUI.

### Added

#### Core Backend (Phase 2)
- **Audio Processing System**
  - FFmpeg integration for audio extraction from video files
  - Audio format conversion (MP3, WAV, FLAC → standard 44.1kHz WAV)
  - Audio metadata extraction (duration, sample rate, channels)
  - File format validation for audio and video inputs

- **Vocal Removal Engine**
  - Integration with `audio-separator` library (UVR 5 wrapper)
  - Support for VR_Models and MDX_Net_Models
  - GPU acceleration with CUDA 11.8
  - Automatic CPU fallback when GPU unavailable
  - Progress callback system for UI integration
  - Model management and listing utilities

- **Project Management**
  - JSON-based project file format (`.nctv`)
  - Project settings persistence (UVR model, GPU, output format)
  - Lyrics data structure integration
  - File path tracking (audio, instrumental, vocals, output)
  - Temporary file management with cleanup

- **Background Processing**
  - Qt QThread-based worker for non-blocking operations
  - Multi-stage processing pipeline (extraction → vocal removal)
  - Progress reporting via Qt signals
  - User cancellation support
  - Thread-safe error handling

- **Progress Tracking**
  - Weighted stage progress calculation
  - ETA (Estimated Time of Arrival) estimation
  - Human-readable status messages
  - Real-time progress updates

#### GUI (Phase 2)
- **Wizard Mode Interface**
  - Step 1: File Selection
    - Browse dialog for file selection
    - Drag-and-drop support
    - Format validation with user-friendly error messages
    - Visual feedback for loaded files
  
  - Step 2: Vocal Removal Settings
    - UVR model selection dropdown
    - GPU toggle with availability status
    - Processing button with progress dialog
    - Real-time progress updates during processing

- **Main Window**
  - Menu bar with File, View, Settings, Help menus
  - Status bar with GPU information display
  - Mode switching (Wizard ↔ Editor) via View menu
  - High-DPI scaling support

#### Configuration
- YAML-based configuration system (`config.yaml`)
- Settings categories:
  - UVR model paths and defaults
  - Processing parameters (temp/output directories, sample rate)
  - Lyrics synchronization settings
  - Karaoke video styling options
  - GUI preferences (theme, tooltips, autosave)
  - Advanced options (batch processing, cache settings)

#### Utilities
- **GPU Detection**
  - CUDA availability checking
  - GPU information retrieval (name, memory, compute capability)
  - Recommended batch size calculation based on GPU memory
  - GPU cache clearing utilities

- **FFmpeg Utilities**
  - Media file information extraction
  - Audio extraction and conversion
  - Video/audio merging
  - Subtitle burning capabilities
  - Error handling with informative messages

#### Data Structures
- **Lyrics Data Models**
  - `LyricWord`: Individual word with timing and confidence
  - `LyricLine`: Line of lyrics with word-level timing
  - `LyricsData`: Complete lyrics with metadata
  - JSON serialization/deserialization
  - LRC and SRT export formats

#### Development Tools
- **Setup Scripts**
  - `setup_python.ps1`: Automated Python embedded environment setup
  - `install_torch_manual.ps1`: Manual PyTorch installation helper
  - `migrate_uvr_models.ps1`: UVR model migration from existing installation

- **Documentation**
  - Comprehensive README.md
  - Detailed SETUP.md with step-by-step instructions
  - PYTORCH_MANUAL_INSTALL.md for offline PyTorch setup
  - Configuration examples and explanations

### Dependencies
- Python 3.10.11 (embedded distribution)
- PyQt6 >= 6.6.0
- PyTorch 2.1.0 with CUDA 11.8
- audio-separator >= 0.18.0
- librosa >= 0.10.0
- soundfile >= 0.12.0
- ffmpeg-python >= 0.2.0
- openai-whisper >= 20231117
- pyqtgraph >= 0.13.0
- numpy >= 1.24.0
- pyyaml >= 6.0

### Technical Details
- **Architecture**: MVC pattern with Qt signals/slots
- **Threading**: Background worker threads for heavy processing
- **GPU Support**: CUDA 11.8 for NVIDIA GPUs (GTX 1060+ recommended)
- **File Formats**: 
  - Input: MP3, WAV, FLAC, M4A, MP4, AVI, MKV, MOV
  - Output: WAV (instrumental/vocals), NCTV (project)

### Known Limitations
- Wizard mode only includes Steps 1-2 (file selection and vocal removal)
- No lyrics input or synchronization yet (Phase 3)
- No karaoke video generation yet (Phase 4)
- No advanced editor mode yet (Phase 6)
- Windows-only (cross-platform support planned)

### Performance
- **With GPU (GTX 1060 6GB)**:
  - 3-minute song: ~30-60 seconds processing
  - Real-time progress tracking
  
- **CPU-only mode**:
  - 3-minute song: ~5-10 minutes processing
  - Automatic fallback when GPU unavailable

---

## Project Phases

### Phase 1: Research & Planning ✅ Completed
- UVR 5 model integration research
- Architecture design
- Technology stack selection
- Implementation planning

### Phase 2: Core Backend Development ✅ Completed
- Audio processing pipeline
- UVR integration
- Background workers
- Wizard Mode GUI (Steps 1-2)

### Phase 3: Lyrics Processing 🚧 Next
- Manual lyrics input
- Whisper AI integration
- Auto-synchronization
- Timeline data structures

### Phase 4-8: Future Development
- Video generation
- Advanced editor
- Testing and release

---

## [0.0.0] - 2025-12-29

### Project Initialization
- Repository created
- Initial project structure
- Planning documents

---

[Unreleased]: https://github.com/AgentHitmanFaris/NC-KTV/compare/v0.1.0-alpha...HEAD
[0.1.0-alpha]: https://github.com/AgentHitmanFaris/NC-KTV/releases/tag/v0.1.0-alpha
[0.0.0]: https://github.com/AgentHitmanFaris/NC-KTV/releases/tag/v0.0.0
