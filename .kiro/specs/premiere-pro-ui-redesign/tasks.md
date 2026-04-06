# Implementation Plan: Premiere Pro UI Redesign

## Overview

Incrementally transform NC-KTV Pro's three application modes into a full Premiere Pro-style dark panel workspace. Each task builds on the previous: QSS palette first, then structural layout, then per-panel details, then menu bar, then WizardMode, then PrecisionMode, then tests.

## Tasks

- [x] 1. Update `dark_theme.qss` — Premiere Pro color palette and named selectors
  - Replace all existing color values with the Premiere palette: `#1e1e1e` Panel_BG, `#252525` secondary BG, `#2d2d2d` Header_BG, `#3a3a3a` Border_Color, `#e0e0e0` Primary_Text, `#8a8a8a` Secondary_Text, `#4a9eff` Accent_Color
  - Add semantic color comments at the top of the file (e.g., `/* Panel_BG: #1e1e1e */`)
  - Add or update named selectors: `QWidget#panelHeader`, `QWidget#toolsPanel`, `QWidget#sourcePanel`, `QWidget#programPanel`, `QWidget#captionsPanel`, `QWidget#timelinePanel`, `QWidget#transportBar`, `QWidget#tabBar`, `QWidget#topBar`, `QWidget#wizardSidebar`, `QWidget#dropZone`, `QWidget#progressCard`, `QWidget#monitorPlaceholder`
  - Add or update button selectors: `QPushButton#primaryAction`, `QPushButton#tabBtn`, `QPushButton#navBtn`, `QPushButton#toolBtn`, `QPushButton#stampBtn`, `QPushButton#playBtn`, `QPushButton#startEngineBtn`, `QPushButton#transportBtn`
  - Update `QPushButton` base rule: `#2d2d2d` bg, `#3a3a3a` border, `#c8c8c8` text, 4px radius; hover: `#383838` bg, `#4a9eff` border at 40% opacity; pressed: `#1a1a1a` bg
  - Update `QMenuBar` rule: `#2d2d2d` bg, `#3a3a3a` bottom border, 28px height, `#8a8a8a` item text; hover/selected: `#383838` bg, `#e0e0e0` text
  - Update `QScrollBar` handle width to 5px, hover color `#4a9eff`
  - Update `QSplitter::handle` to 1px `#3a3a3a`, hover `#4a9eff`
  - Update `QComboBox` rule: `#252525` bg, `#3a3a3a` border, `#c8c8c8` text, `▾` in `#8a8a8a`
  - Update `QLineEdit`/`QPlainTextEdit` rule: `#1a1a1a` bg, `#3a3a3a` border, `#e0e0e0` text, 4px radius; focus border `#4a9eff`
  - Add `QWidget#panelHeader` rule: `#2d2d2d` bg, `#3a3a3a` bottom border, 28px height; `QLabel#panelTitle` inside: `#8a8a8a`, 10px, uppercase, 1px letter-spacing
  - Add `QPushButton#toolBtn` rule: transparent bg, `#8a8a8a` color; `:checked` state: `rgba(74,158,255,0.2)` bg, `#4a9eff` color; hover: `#383838` bg
  - Add `QPushButton#transportBtn` rule: transparent bg, `#c8c8c8` color, 20px icon size
  - Add `QLabel#timecodeLabel` rule: monospace font, `#e0e0e0`, 13px
  - Add `QWidget#monitorPlaceholder` rule: `#2d2d2d` bg, centered `#555555` text
  - Add `QDialog` base rule: `#1e1e1e` bg, `#3a3a3a` border, no gradient
  - _Requirements: 1.1–1.10, 2.2–2.3, 4.1–4.2, 5.3–5.4, 8.3–8.6, 10.1–10.3, 11.3–11.5, 14.1–14.7, 15.1–15.4, 16.1–16.5_

- [x] 2. Add `makePanelHeader()` factory and `formatTimecode()` utility to `editor_mode.cpp`
  - [x] 2.1 Implement `static QWidget* makePanelHeader(QWidget* parent, const QString& title, QList<QWidget*> rightActions = {})` in `editor_mode.cpp`
    - Create a `QWidget` with `objectName("panelHeader")` and `fixedHeight(28)`
    - Add a `QHBoxLayout` with 10px left padding, 6px right padding, 0 spacing
    - Add a `QLabel` with `objectName("panelTitle")` displaying `title.toUpper()`
    - Add a stretch, then append each widget from `rightActions` right-aligned
    - Return the constructed widget
    - _Requirements: 4.1–4.4_

  - [x] 2.2 Implement `static QString formatTimecode(double seconds, int fps = 30)` in `editor_mode.cpp`
    - Compute `totalFrames = static_cast<int>(seconds * fps)`
    - Extract `ff = totalFrames % fps`, `ss = (totalFrames/fps) % 60`, `mm = (totalFrames/fps/60) % 60`, `hh = totalFrames/fps/3600`
    - Return `QString("%1:%2:%3:%4").arg(hh,2,10,QChar('0'))...` zero-padded to 2 digits each
    - _Requirements: 6.4, 10.4_

  - [ ]* 2.3 Write property test for `formatTimecode()` (Property 3)
    - **Property 3: Timecode Formatting Correctness**
    - Add `TEST_P(TimecodeFormatTest, MatchesPattern)` in `cpp/tests/test_ui_premiere.cpp`
    - Generate 100 random positions in [0.0, 86400.0) using `std::mt19937`
    - Assert output matches `\d{2}:\d{2}:\d{2}:\d{2}` via `QRegularExpression`
    - Assert FF component (last segment) is in [0, 29]
    - Use `INSTANTIATE_TEST_SUITE_P` with `ValuesIn(generateRandomPositions(100))`
    - **Validates: Requirements 6.4, 10.4**

- [x] 3. Rebuild `EditorMode::setupUi()` — panel grid with QSplitter layout
  - [x] 3.1 Replace the existing `setupUi()` body in `editor_mode.cpp` with the new panel grid
    - Root `QVBoxLayout` (0 margins, 0 spacing) on `this`
    - Row 0: Tab bar `QWidget#tabBar` (36px fixed height) with three `QPushButton#tabBtn` (checkable, auto-exclusive): "Lyrics Editor", "Timing Sync", "Video Render"; right side: `QPushButton#primaryAction` "Save" + `QPushButton#navBtn` "Log"
    - Row 1: Workspace `QSplitter(Qt::Horizontal)` — first child is `QWidget#toolsPanel` (32px fixed width); second child is a nested `QSplitter(Qt::Horizontal)` containing Source Panel, Captions Panel, Program Panel (each a `QWidget` with `makePanelHeader()` + content)
    - Row 2: Timeline Panel `QWidget#timelinePanel` — `makePanelHeader("Timeline", ...)` + vertical `QSplitter` containing `WaveformWidget` (~60px) and `TimelineWidget`
    - Row 3: Transport Bar `QWidget#transportBar` (40px fixed height) — five `QPushButton#transportBtn` (⏮ ◀ ▶ ▶ ⏭), `QPushButton#playBtn` (28×28 circular), `QLabel#timecodeLabel`, `QComboBox#trackSelector`, `WaveformWidget`
    - Load splitter sizes from `ConfigManager` keys `ui.editor.workspace_splitter` and `ui.editor.timeline_height`; connect `splitterMoved` to write back immediately
    - _Requirements: 3.1–3.4, 3.6, 4.1–4.4, 9.1–9.3, 10.1–10.6_

  - [x] 3.2 Implement Tools Panel content inside `QWidget#toolsPanel`
    - `QVBoxLayout` with 4px spacing, top-aligned
    - Three `QPushButton#toolBtn` buttons: "▲" (Selection), "✂" (Razor), "↔" (Slip) — each `setCheckable(true)`, added to a `QButtonGroup` (exclusive)
    - Default: Selection button checked
    - Add `ToolMode` enum (`Selection`, `Razor`, `Slip`) to `editor_mode.h`
    - Connect `QButtonGroup::idClicked` to update `m_activeTool` and emit `toolModeChanged(ToolMode)`
    - _Requirements: 5.1–5.5_

  - [x] 3.3 Implement Source Panel and Program Panel content
    - Source Panel: `makePanelHeader("Source Monitor")` + `KaraokePreview` widget filling content area; add `QLabel#timecodeLabel` overlaid bottom-left of content area showing `formatTimecode(m_currentTime)`; add `QWidget#monitorPlaceholder` (hidden when project has source)
    - Program Panel: same structure with header "Program Monitor" and placeholder "No Program"
    - Both panels share the same `AudioPlayer` / `KaraokePreview` model instance
    - _Requirements: 6.1–6.5, 7.1–7.5_

  - [x] 3.4 Implement Captions Panel content
    - `makePanelHeader("Captions", {addBtn, aiSyncBtn})` where `addBtn` = `QPushButton#subtitleActionBtn` "+ Add" and `aiSyncBtn` = `QPushButton#subtitleActionBtnAccent` "AI Sync"
    - Move `m_syncTable` into this panel; set `objectName("subtitleTable")`, no grid lines, 28px row height, alternating row color `#222222`
    - _Requirements: 8.1–8.7_

  - [ ]* 3.5 Write property test for tool button mutual exclusion (Property 2)
    - **Property 2: Tool Button Mutual Exclusion**
    - Add `TEST_P(ToolMutualExclusionTest, ExactlyOneChecked)` in `cpp/tests/test_ui_premiere.cpp`
    - Generate 50 random click sequences over tool button indices {0, 1, 2} using `std::mt19937`
    - After each click, assert exactly one button in the `QButtonGroup` is checked
    - **Validates: Requirements 5.3, 5.5**

  - [ ]* 3.6 Write property test for tab switching view consistency (Property 5)
    - **Property 5: Tab Switching View Consistency**
    - Add `TEST_P(TabSwitchTest, ViewStackConsistency)` in `cpp/tests/test_ui_premiere.cpp`
    - Generate 50 random sequences of tab indices {0, 1, 2}
    - After each simulated tab click, assert `m_viewStack->currentIndex() == tabIdx` and `tabButton(tabIdx)->isChecked() == true`
    - **Validates: Requirements 11.1, 11.3, 11.4**

- [x] 4. Checkpoint — wire tab switching and transport bar signals
  - Connect each `QPushButton#tabBtn` `toggled` signal to update `m_viewStack->currentIndex()`
  - Connect Play/Pause `QPushButton#playBtn` `clicked` to toggle `m_audioPlayer` play state and update button text between "▶" and "⏸"
  - Connect `AudioPlayer` position signal to update `m_currentTime`, `m_playbackTimeLabel` (via `formatTimecode()`), and Source Panel timecode overlay
  - Connect `m_trackSelector` `currentIndexChanged` to switch `AudioPlayer` source
  - Connect `m_saveProjectBtn` `clicked` to emit `requestSave()`
  - Ensure all tests pass, ask the user if questions arise.

- [x] 5. Eliminate inline `setStyleSheet()` calls from `editor_mode.cpp`
  - Remove every `setStyleSheet(...)` call on structural widgets (those with an `objectName` matching a QSS selector)
  - For `ProcessingOverlay`, replace inline styles with `objectName` assignments and matching QSS rules in `dark_theme.qss`
  - For the lyrics plain-text edit, assign `objectName("lyricsEdit")` and add a `QPlainTextEdit#lyricsEdit` rule in QSS (24px font, 24px/32px padding)
  - Verify no `setStyleSheet` remains on: `topBar`, `tabBar`, `panelHeader`, `transportBar`, `subtitlePanel`, `previewPanel`, `pipelineCard`, `projectCard`, `audioMonitor`, `syncQueuePanel`
  - _Requirements: 15.1, 15.5_

  - [ ]* 5.1 Write unit test for no inline styles on structural widgets (Property 6)
    - **Property 6: No Inline Styles on Structural Widgets**
    - Add `TEST(NoInlineStylesTest, EditorModeStructuralWidgets)` in `cpp/tests/test_ui_premiere.cpp`
    - Construct `EditorMode` with a mock project and config
    - Iterate all `QObject` descendants; for each widget whose `objectName()` is in the set `{"panelHeader","transportBar","toolsPanel","tabBar","topBar","captionsPanel","sourcePanel","programPanel","timelinePanel"}`, assert `widget->styleSheet().isEmpty()`
    - **Validates: Requirements 15.1, 15.2**

- [x] 6. Rebuild `MainWindow` menu bar — Premiere-style with `setEditorMenusEnabled()`
  - [x] 6.1 Show the `QMenuBar` in `MainWindow::setupApplicationUI()` (remove `menuBar()->hide()`)
    - Populate menus: File (New, Open, Save, Save As, Import, Export Video, Preferences, Exit), Edit (Undo, Redo, separator, Preferences), Sequence (Add Subtitle Line, Remove Selected Line, Split at Playhead), Clip (Set In Point, Set Out Point, Clear In/Out), Window (toggle Captions, Source Monitor, Program Monitor, Timeline, Tools), Help (Keyboard Shortcuts, About)
    - _Requirements: 2.1, 2.4–2.9_

  - [x] 6.2 Add `setEditorMenusEnabled(bool enabled)` slot to `MainWindow`
    - Disable/enable the Sequence, Clip, and Window `QMenu` objects
    - Call `setEditorMenusEnabled(false)` when switching to WizardMode, `setEditorMenusEnabled(true)` when switching to EditorMode
    - _Requirements: 2.10_

- [x] 7. Rebuild `WizardMode::setupUi()` — Premiere Pro start screen layout
  - Replace the existing `createWelcomePage()` body with the new sidebar + content layout
  - Root `QHBoxLayout` (0 margins, 0 spacing)
  - Left: `QWidget#wizardSidebar` (260px fixed width, `#252525` bg, `#3a3a3a` right border) containing: "NC-KTV PRO" branding label, `QPushButton#navBtn` "New Project" (full-width), `QPushButton#navBtn` "Open Project" (full-width), "RECENT PROJECTS" section label, `QListWidget` for recent files
  - Right: main content area with `QWidget#pipelineCard` containing drop zone `QWidget#dropZone` (dashed `#3a3a3a` border, `#1e1e1e` bg), processing profile section, `QPushButton#startEngineBtn` "Start Engine" (full-width, 40px, disabled until file selected)
  - Drop zone drag-over: update border to `2px dashed #4a9eff`; file selected: update border to `2px solid rgba(74,158,255,0.4)`, show filename
  - Remove all inline `setStyleSheet()` calls; assign `objectName` to every structural widget and rely on QSS
  - Update `createProcessingPage()`: assign `objectName("progressCard")` to the progress card widget; remove inline styles from progress bar (use `QProgressBar#processingBar` QSS rule with `#4a9eff` fill, `#2d2d2d` track, 4px height)
  - _Requirements: 12.1–12.11, 15.1_

- [x] 8. Update `PrecisionMode` — panel header and Premiere styling
  - Add `makePanelHeader("Precision Editor")` as the first row of `PrecisionMode`'s root layout (copy the factory function or move it to a shared header `panel_utils.h` in `cpp/src/gui/editor/`)
  - Assign `objectName("precisionSubtitlePanel")` and `objectName("precisionEditorPanel")` to the two splitter children
  - Assign `objectName("transportBar")` to the bottom transport strip so it inherits the QSS transport bar rule
  - Remove inline `setStyleSheet()` calls from `precision_mode.cpp` for structural widgets
  - Assign `objectName("syllableCell")` to syllable grid cells; add `QWidget#syllableCell` QSS rule: `#252525` bg, `#3a3a3a` border, `#e0e0e0` text 13px; `:checked` / active state: `rgba(74,158,255,0.3)` bg, `1px solid #4a9eff` border
  - _Requirements: 13.1–13.5, 15.1_

- [x] 9. Add `cpp/tests/test_ui_premiere.cpp` and wire into `CMakeLists.txt`
  - [x] 9.1 Create `cpp/tests/test_ui_premiere.cpp` with all test fixtures
    - Include headers: `<gtest/gtest.h>`, `<QApplication>`, `<QRegularExpression>`, `editor/editor_mode.h`, `wizard/wizard_mode.h`, `core/config/config_manager.h`
    - Add `TEST(EditorModeLayout, NoQMainWindowChild)` — construct `EditorMode`, walk widget tree, assert no `QMainWindow` descendant
    - Add `TEST(EditorModeLayout, TransportBarHeight)` — assert `findChild<QWidget*>("transportBar")->height() == 40`
    - Add `TEST(EditorModeLayout, TabBarHasThreeTabBtns)` — find `QWidget#tabBar`, count `QPushButton#tabBtn` children, assert count == 3 and all are checkable
    - Add `TEST(EditorModeLayout, ToolsPanelWidth)` — assert `findChild<QWidget*>("toolsPanel")->width() == 32`
    - Add `TEST(WizardModeLayout, SidebarWidth)` — assert `findChild<QWidget*>("wizardSidebar")->width() == 260`
    - Add `TEST(WizardModeLayout, StartBtnDisabledOnConstruct)` — assert `findChild<QPushButton*>("startEngineBtn")->isEnabled() == false`
    - Add `TEST(QSSSelectors, AllRequiredSelectorsPresent)` — load `dark_theme.qss` content, assert it contains each of the 9 required `objectName`-based selectors
    - _Requirements: 3.1, 3.3, 4.1, 5.1, 12.2, 12.8, 15.2_

  - [x] 9.2 Add `test_ui_premiere.cpp` to `TEST_SOURCES` (or the `ncktv_gui_tests` source list) in `cpp/tests/CMakeLists.txt`
    - Add the file to the `add_executable(ncktv_gui_tests ...)` source list alongside `test_ui_clutter.cpp`

  - [ ]* 9.3 Write property test for panel size persistence round-trip (Property 1)
    - **Property 1: Panel Size Persistence Round-Trip**
    - Add `TEST_P(PanelSizePersistenceTest, RoundTrip)` in `test_ui_premiere.cpp`
    - Generate 100 random valid splitter size sets (2–4 positive ints, each in [50, 800]) using `std::mt19937`
    - For each set: write to a temp `ConfigManager` via `ui.editor.workspace_splitter`, construct `EditorMode`, assert `workspaceSplitter()->sizes()` matches the written values
    - Expose `workspaceSplitter()` as a `const` accessor on `EditorMode` (or use `findChild<QSplitter*>`)
    - **Validates: Requirements 3.4**

  - [ ]* 9.4 Write property test for play/pause toggle (Property 4)
    - **Property 4: Play/Pause State Toggle**
    - Add `TEST_P(PlayPauseToggleTest, TogglesState)` in `test_ui_premiere.cpp`
    - Generate 50 random sequences of play/pause button clicks (lengths 1–20)
    - After each click, assert the playback state is the logical inverse of the state before the click
    - Use `QSignalSpy` on `AudioPlayer::playbackStateChanged` or check `m_audioPlayer->isPlaying()`
    - **Validates: Requirements 10.7**

- [x] 10. Final checkpoint — ensure all tests pass
  - Ensure all tests pass, ask the user if questions arise.
