# Changelog

All notable changes to NC-KTV will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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
- **Timeline Toggle**: "📊 Timeline View" button in editor to show/hide timeline panel
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
