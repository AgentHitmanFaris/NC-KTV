# Changelog

All notable changes to this project will be documented in this file.

## [Unreleased]

### Performance
- Overhauled C++ `KaraokeLyricRenderer` to pre-render styled text lines into `QImage` textures and cache font-specific layouts. Real-time playhead updates now perform simple O(1) texture blits instead of CPU-intensive path vector calculations and rasterization (`QPainterPath::addText`, `strokePath`, `fillPath`) on the GUI thread, resolving video playback lag.
- Eliminated infinite `SequentialAnimation` loop on the branding dot indicator that ran 24/7 consuming animation frames even when idle.
- Removed `layer.enabled: true` from the intro splash breathing circle, which forced unnecessary offscreen GPU texture allocation.
- Replaced `Math.random()` calls in the program monitor and source monitor Canvas visualizers with deterministic sine-wave patterns, eliminating forced GPU repaints every frame.
- Reduced source monitor visualizer timer interval from 33ms (30fps) to 50ms (20fps) and gated it behind visibility check.
- Optimized ClipItem `sweepProgress` calculation to short-circuit when the playhead is outside the clip's time range, reducing per-frame work from O(clips × syllables) to O(1) for inactive clips.
- Removed redundant overlapping MouseArea in ClipItem that duplicated selection handling already performed by the drag MouseArea.
- Replaced N per-delegate `isActive` syllable bindings in PropertiesPanel with a single centrally-computed `activeSylIdx` property, reducing binding evaluations from O(N) to O(1) per playhead tick.
- Added zoom-change repaint trigger to the timeline ruler Canvas to prevent stale tick marks after zooming.

### Added
- Internal Media Library drag-and-drop support: you can now drag-and-drop media assets directly from the Media Browser list onto the timeline tracks to insert them.
- Global persistent software decoding preference setting (`disable_hw_decoding` in `QSettings`) that disables hardware acceleration for the Qt Multimedia FFmpeg backend on startup to resolve device/texture allocation failures (like `8007000e`) for 4K and AV1 files.
- One-click recovery toggle button ("Switch to Software Decoding & Restart") directly inside the playback error/warning dialog to allow instant switching to software mode when a video fails to play.
- "SYSTEM DECODING PREFERENCES" card in the global metadata properties panel to view the active decoding mode and manually switch between Hardware (Fast) and Software (Safe) modes with a restart prompt.
- `disableHwDecoding` and `setDisableHwDecoding` settings controls and a `restartApplication` helper to the C++ `TimelineManager` backend.
- Syllable-level timing and tuning methods to the C++ core backend (`Clip::updateSyllable`).
- Interactive Syllable Tuning Inspector in the clip properties panel supporting 50ms nudging controls.
- Live audio visualizer canvas rendering a spectrum sine wave inside the preview monitor.
- Lookahead lyrics overlay display in the preview monitor, showing the active cue and upcoming lyrics line.
- Search and filter layout in the Media Library panel.
- Stationary track headers layout in the timeline grid, ensuring that track titles, volumes, and controls remain pinned to the left edge of the viewport during horizontal scrolling.
- Horizontal time ruler cover to block scrolling ticks behind the stationary header block.
- Playhead and clip guide visual tooltip displaying current timeline timecodes during drag operations.
- Master volume control slider inside the transport deck.
- Shortcut Guide helper dialog mapping core keyboard shortcuts.
- Project Save and Load UI buttons and status text alerts.
- Magnetic snapping to exactly 0s in the timeline snapping engine.
- Real-time progress percentage updates and status text in both the Media Library panel and the Properties Panel during stem separation.
- Automated auto-put logic to instantly place imported media files into compatible tracks at the current playhead time on the timeline.
- Fully functional "+ VIDEO TRACK" creation button in the timeline control header.
- Custom orange color accents and layout styling tags for Video Tracks inside the timeline lane headers.
- Horizontal playback auto-scrolling viewport follow in TimelineView.qml, automatically keeping the playhead visible and centered at 25% of the tracks area during active playback.
- Premiere Pro-style continuous vertical track zooming from 40px to 200px in the timeline tracks area, with toolbar zoom controls (slider and increment buttons) and mouse wheel shortcuts (`Ctrl / Shift + Wheel` to zoom vertically, `Alt + Wheel` to zoom horizontally).
- "New Project" button (`btnNew`) and action flow in the main header toolbar that resets the workspace tracks layout, timeline manager, active metadata, and selection, with dialog prompts to prevent losing unsaved changes.
- C++ `TimelineManager::timelineChanged()` signal that acts as a single gateway to propagate track `clipsChanged`, clip `durationChanged`, and clip `lyricTextChanged` events to QML.
- Automatic metadata guesser in C++ `TimelineManager` that parses imported media filenames to extract and capitalize clean Song Title and Artist Name metadata.
- Automated unit tests in `timeline_test.cpp` for Metadata Guesser/Filename Parser, Hardware/Software decoding preferences, and QML event propagation (`timelineChanged()` signal).
- Comprehensive automated verification tests documentation in `README.md` outlining execution steps and test coverage details.
- Automated unit test `testMidSideDspSeparation()` in `timeline_test.cpp` to verify software fallback Mid-Side DSP stem separation logic with synthetic stereo signals.
- YouTube Discovery pagination features, implementing `searchMore()` in `YoutubeManager` C++ backend and a paginated "Load More Results" footer button in `MediaBrowser.qml`.
- Waveform cache persistence using `.pk` files to instantly render visual audio waveforms on startup.
- Non-linear subtitle/lyric sweeps with customizable syllable Bezier curves, visual editor handles, and Newton-Raphson solvers.
- Vocal Alignment tool performing sliding amplitude envelope cross-correlations to synchronize clips to guide tracks.
- Variable playback speed (`playbackRate`) control in C++ `AudioEngine` and interactive tuning toolbar ComboBox in `LyricTunerWindow.qml` with automated loop triggers on syllable highlight.
- Native QML/C++ borderless Loading Splash Screen Window showing real-time progress and spinning animation, while asynchronously compiling and preparing workspace components on startup.

### Changed
- Re-anchored the root window's global `DropArea` and glassmorphic overlay to exclude the timeline editor area. This prevents the global drop area from shadowing/intercepting drop events, allowing files dragged directly onto the timeline tracks to be correctly added as clips and snapped to the drop point.
- Refactored C++ `StemSeparator` spectrogram representations from pointer-chasing 3D vectors to flat 1D contiguous vectors to improve cache locality.
- Pre-allocated STFT and ISTFT temporary frame/complex vectors outside of processing loops to eliminate over 120,000 dynamic heap memory operations.
- Optimized fallback Mid-Side DSP separation loop by removing thread sleeping (`msleep`) and loop boundary branch checks, accelerating fallback execution by up to 100x.
- Decoupled active clip dragging and trimming coordinates to use native QML drag targets, deferring C++ model writes and expensive waveform rebuilds to mouse release for smooth 60fps movement.
- Exposed horizontal scroll coordinate `scrollX` from `TimelineView` to delegate `TrackLane` templates to resolve undeclared warning outputs.
- Qualified the `snappingEnabled` and `compactTracks` properties inside `TimelineView.qml` with the root ID `timelineRoot` to resolve QML ReferenceError console warnings.
- Gated the video preview seek synchronization behind a drift threshold of 1000.0ms (up from 250ms) to ensure smooth playback without constant seeking and stuttering.
- Scaled and fixed layout size dimensions and button bounds (timecodes, nudge buttons, offset and duration columns) in the Properties Panel to prevent text wrapping or clipping.

### Fixed
- Fixed playback compatibility issues for high-resolution (2K/4K) and AV1-encoded videos by providing a software decoding fallback mechanism.
- Fixed QML TypeErrors on animation state changes, specifically resolving the missing property alias for `introAnimationSequence` on the intro splash screen.
- QML double-to-qlonglong property assignment crash by wrapping calculations in `Math.round`.
- Fixed playhead scrubbing lockups by directly writing to properties instead of using missing C++ setters as functions.
- Corrected alignment of timeline ruler ticks and playhead lines by offsetting coordinates with the 180px track header boundary.
- Fixed stem separation track duplication by searching for and reusing existing audio tracks.
- Resolved video clip placement bugs where Video files incorrectly mapped to Audio tracks and could not be loaded into Video lanes.
- Fixed playhead jumping when clicking on track headers in TimelineView.qml by adding a bounds check to the ruler MouseArea.
- Fixed ONNX Runtime GPU execution provider loader failure by updating the CMake post-build script to copy all runtime library DLLs (including cuda, tensorrt, and shared providers) to the executable directory, allowing CUDA GPU-accelerated stem separation.
- Resolved vocal/instrumental audio channel swap by correcting C++ WAV writing buffer paths in `stem_separator.cpp`.
- Fixed the song title and artist metadata synchronization bug on intro splash launch in `main.qml`.
- Fixed vertical panel collapsing of `MediaBrowser` (left pane) and `PropertiesPanel` (right pane) by binding their heights explicitly to `parent.height`.
- Resolved application freezes during bulk lyric/track replacements by adding `beginResetModel()` and `endResetModel()` blocks to `ClipListModel::handleClipsChanged()`.
- Fixed QML TypeError when calling `autoGenerateSyllables()` on `Clip` by adding `Q_INVOKABLE` macro declaration in C++ to expose the method to QML.

