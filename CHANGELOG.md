# Changelog

All notable changes to NC-KTV will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [1.6.0] - 2025-12-31

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

## [1.5.0] - 2025-12-31

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

## [0.1.0-alpha] - 2025-12-30

### Initial Alpha Release
First development release with core backend functionality.

#### Features
- **Vocal Removal**: UVR 5 integration with GPU acceleration
- **Wizard Mode**: Step-by-step file processing
- **Project Management**: JSON-based .nctv project files
- **Progress Tracking**: Real-time progress with ETA

#### Technical
- Python 3.10.11 embedded
- PyQt6 6.6+ GUI
- PyTorch 2.1.0 with CUDA 11.8
- FFmpeg integration

---

## [0.0.0] - 2025-12-29

### Project Initialization
- Repository created
- Initial project structure

---

[1.6.0]: https://github.com/AgentHitmanFaris/NC-KTV/compare/v1.5.0...v1.6.0
[1.5.0]: https://github.com/AgentHitmanFaris/NC-KTV/compare/v0.1.0-alpha...v1.5.0
[0.1.0-alpha]: https://github.com/AgentHitmanFaris/NC-KTV/releases/tag/v0.1.0-alpha
[0.0.0]: https://github.com/AgentHitmanFaris/NC-KTV/releases/tag/v0.0.0
