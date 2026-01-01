# Changelog

All notable changes to NC-KTV will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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
