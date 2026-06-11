# NC-KTV V2 - Premium Karaoke Maker and Non-Linear Editor

NC-KTV V2 is a professional, high-fidelity Karaoke Maker and Non-Linear Editor (NLE) designed to build high-quality karaoke video files. It features an advanced multi-track timeline, hardware-accelerated audio rendering, and AI-driven stem separation using ONNX Runtime.

---

## Core Capabilities

* **Multi-Track Timeline**: Synchronous editing of Audio (backing and vocal tracks), Video (guide tracks), and Lyrics tracks with real-time waveform visualization.
* **Syllable-Level Timing and Tuning**: Interactive adjustments of syllable start times, duration, and boundaries with frame accuracy.
* **Live Master Monitor and Real-Time Preview**: Glassmorphic preview panel anchored directly on the Program Monitor with interactive toggles to preview Background Modes (Video + Lyrics vs. Lyrics Only) and Lyrics Layouts (Bottom Overlay vs. Center Queue).
* **Performance Optimizations**: Advanced O(1) playhead cache validation. Skips redundant track/clip nested loops on playhead ticks, ensuring lag-free video rendering and smooth playback with no dropped frames during timed lyrics sweeps.
* **Timed Lyrics Fetching**: Integrated in-app timed lyrics search utilizing the public LRCLIB database. Query songs by track name and artist, browse matching results, preview synced lyric lines, and import them directly onto the timeline with a single click.
* **YouTube Discovery and Downloader**: In-app YouTube discovery module utilizing yt-dlp. Search for background videos or backing tracks, select results, and download Audio (WAV format) or Video (MP4 format) asynchronously with real-time progress indicators, cancellation, and direct timeline imports.
* **AI Stem Separation**: Advanced audio stem separation powered by ONNX Runtime with a high-fidelity Mid-Side DSP fallback mode.
* **Muxing Render Engine**: Fully integrated FFmpeg H.264/AAC muxer for direct project rendering, supporting bidirectional synchronization with the Live Master Monitor.
* **Drag-and-Drop NLE Editing**: Drag media files from Windows Explorer onto the application window to import them to the project pool, or drop them directly onto the timeline lanes to create timed clips. Supports dragging internally imported Media Library assets onto the tracks for seamless NLE editing workflows.
* **Smart Decoding Engine Fallback**: Safe Software Decoding fallback option to easily bypass hardware accelerated decoding freezes, black screens, or texture allocation failures (e.g., when playing heavy 4K AV1 videos on unsupported platforms). Switch modes and restart with a single click.
* **Waveform Cache Persistence**: Writes precomputed audio peaks to `.pk` files to instantly render audio waveforms on project reload without waiting for FFmpeg decoding.
* **Syllable Timing Curves**: Custom Bezier timing curves with an interactive visual editor and presets (Ease In, Ease Out, Hold, etc.) to control non-linear syllable sweeps.
* **Vocal Alignment Helpers**: Automatically aligns recorded vocals or audio tracks with reference guide tracks using a sliding envelope-based cross-correlation algorithm.
* **Native QML Splash Screen Window**: Custom borderless loading window on startup rendering gradient progress animations, pulsing neon glow spots, and floating brand icon, initializing editor assets in the background.

---

## Installation and Build Guide

### Prerequisites
Ensure the following system dependencies are installed and registered on your system PATH:
* **C++ Compiler**: A compiler supporting C++20 (e.g., MSVC 2022, GCC 13+).
* **CMake**: Version 3.25 or higher.
* **Qt 6 Framework**: Version 6.10.2 including Core, Gui, Widgets, Network, Multimedia, Qml, Quick, and QuickControls2.
* **Python**: Required for executing downloader dependencies.
* **yt-dlp**: Required for video discovery and download. Install it via pip:
  ```cmd
  pip install yt-dlp
  ```

### Building the Project
To compile the targets, navigate to the build directory and run the compilation script:
```cmd
cd build
build.bat
```

---

## Operation Instructions

### Launch the Application
Execute the following commands in PowerShell to run the primary GUI editor:
```powershell
$env:PATH += ";D:\ProgramData\Qt\6.10.2\msvc2022_64\bin"
./build/NC-KTV_V2.exe
```

### Search and Sync Synced Lyrics
1. Select a Lyrics Track in the timeline or let the app auto-create one.
2. Click on the LYRICS FINDER tab in the left-hand panel.
3. Enter the Song Title and Artist Name, then click SEARCH SYNCHRONIZED LYRICS.
4. Select a search result from the list to preview the timed LRC file.
5. Click IMPORT LYRICS INTO SUBTITLE TRACK to instantly generate perfectly aligned timed subtitle clips.

### Download YouTube Assets
1. Click the YT DISCOVER tab in the left panel.
2. Search for any song (e.g. Queen bohemian rhapsody instrumental).
3. Select the target video from the search list.
4. Click DOWNLOAD AUDIO BACKING (WAV) or DOWNLOAD VIDEO GUIDE (MP4).
5. Once completed, the file will be saved in your workspace video directory, registered in your Media Library, and automatically loaded on the active timeline playhead.

---

## Automated Verification Tests

The project includes a comprehensive automated unit and integration test suite to verify the C++ core timeline and audio processing libraries.

### Running the Tests
To compile and execute the test suite on Windows, run the helper scripts in the `build` directory:
1. Compile the project targets (compiles both the GUI editor and the test suite):
   ```cmd
   cd build
   build.bat
   ```
2. Run the automated test suite:
   ```cmd
   run_tests.bat
   ```
This will run the tests in offscreen mode and redirect the standard output to `build/test_out.txt`.

### Test Coverage
The automated test suite verifies:
* **Timecode Conversions**: Microsecond/frame translation and formatting.
* **Snapping Engine**: Magnetic snapping to playhead, clips, and zero.
* **Clip Splitting**: Splitting timeline clips with sub-millisecond precision.
* **Project Serialization**: Save, load, and clear operations using `.nctv` JSON schema.
* **Source Trimming and Offsets**: Custom source inpoints and playback offsets.
* **Syllable LRC Parser**: Timed syllable-level tag parsing.
* **LoD Peak Generation**: Waveform generation for different zoom levels.
* **Audio Gain Ramping**: Smooth volume changes and offline mixdowns.
* **Resampler & Windowing Math**: High-fidelity audio resampling and DSP window overlaps.
* **Stem Separator Fallback**: Software CPU DSP fallback modes when ONNX/GPU is unavailable.
* **Mid-Side DSP Separation**: Math verification of software fallback algorithms (vocals = (L+R)/2, instrumental = (L-R)/2) using synthetic interleaved stereo signals.
* **Markers & Subtitle Styles**: Persistent lyric text fonts, styles, and sequence markers.
* **Romanization**: Korean, Japanese, and mixed text conversions.
* **Lyrics Import**: Timed LRC/SRT subtitle importing.
* **Lyric Engine**: Syllable tracking, active word indices, and lookahead lines.
* **Filename Parser & Metadata Guesser**: Capitalizing and auto-extracting artist/title from media filenames.
* **HW/SW Decoding Preferences**: QSettings-backed hardware/software decoding preferences.
* **Unified Event Propagation**: Propagating `timelineChanged()` signal across all timeline mutations.

---

## Technical Dependencies

The application relies on the following static and dynamic components:
* **nlohmann_json**: Git Tag v3.11.3 (fetched via CMake FetchContent).
* **ONNX Runtime (GPU/CPU)**: Version 1.18.0 (fetched via CMake).
* **FFmpeg Libraries**: avcodec, avformat, avfilter, avutil, swresample, swscale (latest GPL shared builds fetched via CMake).
* **yt-dlp**: Command-line YouTube media extractor.

---

## Project Roadmap and Todo List

### Immediate Priorities (Todo List)
- [ ] **Expanded Audio Filters**: Add basic equalizer, reverb, and delay DSP filters to the tracks.

### Mid-Term Goals
- [ ] **Multi-Format Video Rendering**: Add container selections for export (e.g., MKV, WebM, AVI) and dedicated MP3/WAV audio mixdowns.
- [ ] **Custom Swipe Layout Templates**: Allow saving preset layouts for custom font packages, sweep speeds, and bouncing balls.
- [ ] **Dynamic Video Transcoding**: Automatically transcode heavy, hardware-unfriendly video formats during import to improve runtime performance.

### Long-Term Roadmap
- [ ] **Cloud Collaboration Support**: Collaborative project sync for multi-user editing teams.
- [ ] **Extension SDK**: Create a plugin system for custom transition filters and audio effects.

---

## Changelog

### Version 2.0.0-Professional (Unreleased Updates)

#### Added
- **Timeline Vertical Zoom**: Continuous vertical zooming (40px to 200px) with dedicated toolbar sliders and keyboard/mouse wheel shortcuts (Ctrl / Shift + Wheel).
- **New Project Utility**: Real-time project resets with warnings to prevent losing unsaved changes.
- **Filename Parser (Metadata Guesser)**: Automated cleanup and capitalization parsing on imported files to resolve Song Title and Artist metadata automatically.
- **C++ Unified Event Propagation**: Single-gateway QML notification system (timelineChanged()) for all clip and track edits.
- **LRCLIB Lyrics Database**: Direct in-app retrieval, previewing, and importing of synced lyrics.
- **Asynchronous YouTube Downloader**: Embedded YT discover browser with direct import and background queue downloads.
- **YouTube Search Pagination**: Paginated search results loading (endless scroll) using optimized range query parameters in `yt-dlp` and a custom-styled "Load More Results" UI button.
- **Safe Decoding Toggle**: One-click software decoding mode option to bypass hardware acceleration rendering freezes.
- **Mid-Side DSP Verification Test**: Comprehensive automated math verification tests for CPU fallback audio separating algorithms.
- **Waveform Peak Cache (.pk)**: Real-time serialization and verification of precomputed audio peak cache files to accelerate waveform loading times.
- **Syllable Bezier Curve Sweeps**: Interactive graphical control points editor, presets, and Newton-Raphson solvers for customizable lyric swipe transitions.
- **Envelope Cross-Correlation Alignment**: Automatic synchronization of target audio clips against reference guide tracks.
- **Native QML Loading Splash Window**: Asynchronously-loaded borderless splash window showing pulsing neon glows and active asset preparation to guarantee zero-lag transition to the maximized main window.

#### Changed
- **Video Sync Gating**: Seek drift threshold increased to 1000.0ms for stutter-free playback.
- **Cleaned Properties Panel Spacing**: Legible layouts for coordinate timecodes, nudge controls, and syllable durations.
- **GPU-Texture Caching**: Overhauled karaoke text rendering using O(1) texture copy routines.

#### Fixed
- **Left/Right Panels Stretching**: Resolved panel collapsing in horizontal SplitView by binding heights to parent.height.
- **WAV Buffer Swap**: Fixed vocal/instrumental audio stem separations writing to opposite tracks in stem_separator.cpp.
- **Intro Splash Sync**: Resolved title/artist rendering delays during startup intro animations.
- **Track Selection Shadows**: Prevented drag events from being captured by the outer window's drop zone.
