# Design Document — Premiere Pro UI Redesign

## Overview

This redesign transforms NC-KTV Pro's three application modes (WizardMode, EditorMode, PrecisionMode) into a full Adobe Premiere Pro-style dark panel workspace. The previous `ui-ux-clutter-fix` spec established a clean structural foundation — no embedded `QMainWindow`, no inline styles, no stat cards, a single header bar. This spec builds the complete visual and layout system on top of that foundation.

The target is a professional NLE aesthetic: `#1e1e1e`/`#252525`/`#2d2d2d` panel backgrounds, `#3a3a3a` borders, `#e0e0e0` primary text, `#8a8a8a` secondary text, `#4a9eff` accent (Premiere blue), 28–32px panel headers, `QSplitter`-based resizable panel grid, Transport Bar, Tools Panel, Source/Program/Captions/Timeline panels — all styled exclusively through `dark_theme.qss` using `objectName`-based QSS selectors.

### Key Design Decisions

- **QSS-only styling**: All visual rules live in `dark_theme.qss`. No `setStyleSheet()` calls in mode files. This is enforced by assigning `objectName` to every structural widget and writing matching QSS selectors.
- **No QDockWidget**: Premiere-style panels are plain `QWidget` containers with a hand-built 28px header row, not Qt dock widgets. This avoids dock chrome (float/close buttons) and gives full control over appearance.
- **QSplitter for resizing**: Horizontal and vertical `QSplitter` instances divide the panel grid. Splitter state is persisted via `ConfigManager`.
- **Shared transport model**: The `AudioPlayer` component is the single source of truth for playback position. Both Source and Program panels observe the same player instance.
- **Menu bar in MainWindow**: The `QMenuBar` is owned by `MainWindow` and shown/hidden based on active mode. WizardMode disables editor-specific menus. This eliminates the duplicate per-mode menu construction that existed before.

---

## Architecture

### Mode Hierarchy

```
MainWindow (QMainWindow)
├── QMenuBar                    ← single app-level menu bar
├── QStackedWidget (mainStack_)
│   ├── WizardMode (QWidget)    ← index 0: project creation
│   ├── EditorMode (QWidget)    ← index 1: main editor
│   └── PrecisionMode (QWidget) ← index 2: syllable editor
└── QStatusBar                  ← hidden in normal use
```

### EditorMode Panel Layout

```
EditorMode (QVBoxLayout)
├── [row 0] Tab Bar (QWidget#tabBar, 36px fixed)
│   └── "Lyrics Editor" | "Timing Sync" | "Video Render" tabs + Save/Log buttons
├── [row 1] Workspace Splitter (QSplitter, Horizontal, flex 1)
│   ├── Tools Panel (QWidget#toolsPanel, 32px fixed width)
│   └── Content Splitter (QSplitter, Horizontal)
│       ├── Source Panel (QWidget#sourcePanel)
│       │   ├── Panel Header (QWidget#panelHeader, 28px)
│       │   └── KaraokePreview
│       ├── Captions Panel (QWidget#captionsPanel)
│       │   ├── Panel Header (QWidget#panelHeader, 28px)
│       │   └── QTableWidget#subtitleTable
│       └── Program Panel (QWidget#programPanel)
│           ├── Panel Header (QWidget#panelHeader, 28px)
│           └── KaraokePreview (shared model)
├── [row 2] Timeline Panel (QWidget#timelinePanel)
│   ├── Panel Header (QWidget#panelHeader, 28px)
│   └── QSplitter (Vertical)
│       ├── WaveformWidget (~60px)
│       └── TimelineWidget (flex)
└── [row 3] Transport Bar (QWidget#transportBar, 40px fixed)
    └── [In | ◀ | ▶/⏸ | ▶ | Out] + timecode + track selector + mini waveform
```

### WizardMode Layout

```
WizardMode (QHBoxLayout)
├── Sidebar (QWidget#wizardSidebar, 260px fixed)
│   ├── Branding ("NC-KTV PRO")
│   ├── "New Project" button
│   ├── "Open Project" button
│   └── Recent Projects list
└── Main Content (QWidget, flex)
    ├── Pipeline Card (QWidget#pipelineCard)
    │   ├── Drop Zone (QWidget#dropZone)
    │   ├── Processing Profile section
    │   └── "Start Engine" button
    └── Processing Page (QStackedWidget page 1)
        └── Progress Card (QWidget#progressCard)
```

### PrecisionMode Layout

```
PrecisionMode (QVBoxLayout)
├── Panel Header (QWidget#panelHeader, 28px) — "Precision Editor"
├── QSplitter (Horizontal, flex)
│   ├── Subtitle List Panel (QWidget#precisionSubtitlePanel)
│   │   ├── Panel Header (28px)
│   │   └── QListWidget
│   └── Word Editor Panel (QWidget#precisionEditorPanel)
│       ├── Editor Toolbar (QWidget#panelHeader, 28px)
│       ├── WordCanvas (QScrollArea)
│       └── Bottom: AudioPlayer + KaraokePreview
└── Transport Bar (QWidget#transportBar, 40px)
```

---

## Components and Interfaces

### PanelHeader (inline factory function)

A reusable factory function `makePanelHeader(QWidget* parent, const QString& title, QList<QWidget*> rightActions)` creates the standard 28px panel header. It is not a separate class — it returns a `QWidget*` with `objectName("panelHeader")` and the correct child layout. This keeps the implementation minimal while ensuring consistent structure.

```cpp
// Signature (defined in editor_mode.cpp and reused in precision_mode.cpp)
static QWidget* makePanelHeader(QWidget* parent, const QString& title,
                                 QList<QWidget*> rightActions = {});
```

The returned widget has:
- `objectName("panelHeader")` → styled by `QWidget#panelHeader` in QSS
- A `QLabel` with `objectName("panelTitle")` for the uppercase title
- Right-aligned action widgets passed in via `rightActions`

### ToolsPanel

A 32px-wide `QWidget#toolsPanel` containing three `QPushButton#toolBtn` instances (Selection, Razor, Slip) in a `QVBoxLayout`. Buttons are `QCheckable` with `QButtonGroup` for mutual exclusion. The active button gets `objectName("toolBtnActive")` set dynamically, or a `checked` state is used with a QSS `:checked` selector.

### Transport Bar

The transport bar is a `QWidget#transportBar` with a fixed 40px height. It contains:
- Five icon `QPushButton#transportBtn` instances (Go-to-In, Step-Back, Play/Pause, Step-Forward, Go-to-Out)
- The Play/Pause button uses `objectName("playBtn")` for its distinct circular styling
- A `QLabel#timecodeLabel` in monospace font
- A `QComboBox#trackSelector`
- A `WaveformWidget` filling remaining space

The transport bar is constructed once in `EditorMode::setupUi()` and added to the root `QVBoxLayout` as the last row, making it always visible regardless of which tab is active.

### Menu Bar (MainWindow)

`MainWindow` owns the `QMenuBar` and populates it with all menus on construction. It exposes a `setEditorMenusEnabled(bool)` slot that enables/disables the Sequence, Clip, and Window menus. `WizardMode` emits no menu-related signals — `MainWindow` calls `setEditorMenusEnabled(false)` when switching to WizardMode and `setEditorMenusEnabled(true)` when switching to EditorMode.

---

## Data Models

### Panel Size Persistence

Panel splitter sizes are persisted via `ConfigManager` using dot-notation keys:

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `ui.editor.workspace_splitter` | `QString` (comma-separated ints) | `"32,600,400,600"` | Workspace horizontal splitter sizes |
| `ui.editor.timeline_height` | `int` | `220` | Timeline panel height in pixels |
| `ui.editor.captions_width` | `int` | `320` | Captions panel width in pixels |
| `ui.editor.precision_splitter` | `QString` | `"300,900"` | PrecisionMode main splitter sizes |

On `EditorMode` construction, sizes are read from `ConfigManager` and applied via `QSplitter::setSizes()`. On `QSplitter::splitterMoved` signal, sizes are written back immediately.

### Active Tool State

The Tools Panel maintains a `ToolMode` enum:

```cpp
enum class ToolMode { Selection, Razor, Slip };
```

The current tool is stored as `m_activeTool` in `EditorMode` and emitted via `toolModeChanged(ToolMode)` signal when changed. `TimelineWidget` connects to this signal to update its cursor and interaction behavior.

### Timecode Format

Playback position (in seconds, `double`) is formatted as `HH:MM:SS:FF` where FF is frame number at 30fps:

```cpp
static QString formatTimecode(double seconds, int fps = 30) {
    int totalFrames = static_cast<int>(seconds * fps);
    int ff = totalFrames % fps;
    int ss = (totalFrames / fps) % 60;
    int mm = (totalFrames / fps / 60) % 60;
    int hh = totalFrames / fps / 3600;
    return QString("%1:%2:%3:%4")
        .arg(hh, 2, 10, QChar('0'))
        .arg(mm, 2, 10, QChar('0'))
        .arg(ss, 2, 10, QChar('0'))
        .arg(ff, 2, 10, QChar('0'));
}
```

This function is used in both the Transport Bar timecode label and the Source Panel overlay timecode.

---

## Correctness Properties

*A property is a characteristic or behavior that should hold true across all valid executions of a system — essentially, a formal statement about what the system should do. Properties serve as the bridge between human-readable specifications and machine-verifiable correctness guarantees.*

### Property 1: Panel Size Persistence Round-Trip

*For any* set of valid panel splitter sizes (positive integers summing to a reasonable window width), saving those sizes via `ConfigManager` and then constructing a new `EditorMode` with the same `ConfigManager` instance SHALL restore the splitter to the saved sizes.

**Validates: Requirements 3.4**

### Property 2: Tool Button Mutual Exclusion

*For any* sequence of tool button clicks in the Tools Panel, exactly one tool button SHALL be in the active/checked state at all times — clicking a tool button activates it and deactivates all others.

**Validates: Requirements 5.3, 5.5**

### Property 3: Timecode Formatting Correctness

*For any* valid playback position in seconds (0.0 to 86400.0), the `formatTimecode()` function SHALL produce a string matching the pattern `HH:MM:SS:FF` where each component is zero-padded to 2 digits and FF is in the range [0, 29] for 30fps.

**Validates: Requirements 6.4, 10.4**

### Property 4: Play/Pause State Toggle

*For any* playback state (playing or paused), clicking the Play/Pause button in the Transport Bar SHALL toggle the state to the opposite value — playing becomes paused, paused becomes playing.

**Validates: Requirements 10.7**

### Property 5: Tab Switching View Consistency

*For any* tab button click in the EditorMode tab bar (Lyrics Editor, Timing Sync, Video Render), the clicked tab SHALL become the active (checked) tab and the `QStackedWidget` current index SHALL update to the corresponding page index — these two states are always in sync.

**Validates: Requirements 11.1, 11.3, 11.4**

### Property 6: No Inline Styles on Structural Widgets

*For any* widget constructed by `EditorMode::setupUi()`, `WizardMode::setupUi()`, or `MainWindow::setupApplicationUI()` that has a structural `objectName` (e.g., `panelHeader`, `transportBar`, `toolsPanel`, `tabBar`, `topBar`, `captionsPanel`, `sourcePanel`, `programPanel`, `timelinePanel`), that widget's own `styleSheet()` SHALL be empty — all styling comes from the global QSS cascade.

**Validates: Requirements 15.1, 15.2**

---

## Error Handling

### Missing ConfigManager Keys

When `ConfigManager` does not contain a panel size key (first launch, corrupted config), `EditorMode` falls back to hardcoded default proportions. The fallback is applied silently — no error dialog is shown.

### Invalid Splitter Sizes

If persisted splitter sizes are invalid (e.g., all zeros, negative values, or count mismatch with actual splitter children), `EditorMode` ignores the persisted value and applies defaults. Validation: `sizes.size() == splitter->count() && all sizes > 0`.

### No Project Loaded (Source/Program Panels)

When `EditorMode` is constructed with a project that has no source file, the Source Panel and Program Panel display a centered placeholder widget (`QWidget#monitorPlaceholder`) with "No Source" / "No Program" text. The `KaraokePreview` widget is still constructed but hidden until a valid source is loaded.

### WizardMode Drop Zone — Invalid File Type

If a file is dragged onto the drop zone that does not match the accepted MIME types (video/audio), the drop is rejected and the drop zone border remains unchanged. No error dialog — the visual feedback (border stays dashed `#3a3a3a`) is sufficient.

### QSS File Load Failure

`MainWindow::setupApplicationUI()` attempts to load `dark_theme.qss` from the Qt resource system (`:/styles/dark_theme.qss`) first, then falls back to a relative path. If both fail, the application runs with Qt's default style. A `qWarning()` is emitted but no crash occurs.

---

## Testing Strategy

### Unit Tests (GoogleTest, `ncktv_gui_tests`)

These run with `QT_QPA_PLATFORM=offscreen` and verify structural and behavioral correctness without visual rendering.

**Structural tests:**
- `EditorModeLayout`: Construct `EditorMode`, assert no `QMainWindow` child exists anywhere in the widget tree
- `EditorModeLayout`: Assert `QWidget#transportBar` exists with `fixedHeight(40)`
- `EditorModeLayout`: Assert `QWidget#tabBar` exists with exactly 3 checkable `QPushButton#tabBtn` children
- `EditorModeLayout`: Assert `QWidget#toolsPanel` exists with `fixedWidth(32)`
- `WizardModeLayout`: Assert `QWidget#wizardSidebar` exists with `fixedWidth(260)`
- `WizardModeLayout`: Assert `QPushButton#startEngineBtn` is disabled on construction
- `PrecisionModeLayout`: Assert `QWidget#panelHeader` exists as first child of root layout

**Behavioral tests:**
- `ToolButtonMutualExclusion`: Click each tool button in sequence, assert exactly one is checked after each click (Property 2)
- `TabSwitchConsistency`: Click each tab button, assert `m_viewStack->currentIndex()` matches expected index (Property 5)
- `PlayPauseToggle`: Simulate play/pause button click, assert playback state toggles (Property 4)
- `TimecodeFormat`: Test `formatTimecode()` with boundary values: 0.0, 3599.999, 3600.0, 86399.0 (Property 3)

**QSS consolidation tests:**
- `NoInlineStyles`: Construct `EditorMode`, iterate all children with structural `objectName` values, assert `widget->styleSheet().isEmpty()` for each (Property 6)
- `QSSNamedSelectors`: Load `dark_theme.qss` content, assert it contains all required `objectName`-based selectors: `QWidget#panelHeader`, `QWidget#toolsPanel`, `QWidget#sourcePanel`, `QWidget#programPanel`, `QWidget#captionsPanel`, `QWidget#timelinePanel`, `QWidget#transportBar`, `QWidget#tabBar`, `QWidget#topBar`

### Property-Based Tests (GoogleTest + custom generator)

Since the project uses GoogleTest (not a dedicated PBT library), property tests are implemented as parameterized tests with randomized inputs using `std::mt19937`.

**Property 1 — Panel Size Persistence:**
```cpp
// Generate N random valid splitter size sets, save each, reload, assert equality
TEST_P(PanelSizePersistenceTest, RoundTrip) {
    auto sizes = GetParam(); // random positive ints
    config->set("ui.editor.workspace_splitter", sizesToString(sizes));
    EditorMode editor(project, config);
    EXPECT_EQ(editor.workspaceSplitterSizes(), sizes);
}
INSTANTIATE_TEST_SUITE_P(..., ValuesIn(generateRandomSizeSets(100)));
```

**Property 3 — Timecode Formatting:**
```cpp
// Generate 100 random positions in [0, 86400), assert format matches regex
TEST_P(TimecodeFormatTest, MatchesPattern) {
    double pos = GetParam();
    QString tc = formatTimecode(pos);
    QRegularExpression re(R"(\d{2}:\d{2}:\d{2}:\d{2})");
    EXPECT_TRUE(re.match(tc).hasMatch());
    // Also assert FF component is in [0, 29]
    int ff = tc.split(':').last().toInt();
    EXPECT_GE(ff, 0); EXPECT_LE(ff, 29);
}
INSTANTIATE_TEST_SUITE_P(..., ValuesIn(generateRandomPositions(100)));
```

**Property 5 — Tab Switching:**
```cpp
// Generate random sequences of tab clicks, assert view stack index always matches
TEST_P(TabSwitchTest, ViewStackConsistency) {
    auto clickSequence = GetParam(); // random sequence of {0,1,2}
    EditorMode editor(project, config);
    for (int tabIdx : clickSequence) {
        editor.simulateTabClick(tabIdx);
        EXPECT_EQ(editor.viewStackIndex(), tabIdx);
        EXPECT_TRUE(editor.tabButton(tabIdx)->isChecked());
    }
}
INSTANTIATE_TEST_SUITE_P(..., ValuesIn(generateRandomTabSequences(100)));
```

### Integration Tests

- **Wizard → Editor transition**: Load a test `.nctv` project file, trigger `onProjectReady`, assert `EditorMode` is shown with correct panel structure and menu bar has editor menus enabled
- **Panel resize persistence**: Resize splitters in `EditorMode`, close and reopen, assert sizes are restored from `ConfigManager`
- **Mode switching**: Switch between WizardMode and EditorMode, assert menu bar state (editor menus disabled/enabled) is correct in each mode
- **PrecisionMode launch**: Open `PrecisionMode` from `EditorMode`, assert panel header shows "Precision Editor" and subtitle list is populated from project lyrics

### Manual Visual Verification Checklist

After implementation, verify visually against Premiere Pro 2024 screenshots:
- [ ] Panel headers are 28px, `#2d2d2d` background, uppercase `#8a8a8a` title
- [ ] Active panel header has `#4a9eff` bottom border
- [ ] Tools panel is 32px wide, tool buttons are icon-only
- [ ] Transport bar is 40px, play button is circular `#e0e0e0`
- [ ] Tab bar active tab has 2px `#4a9eff` underline
- [ ] Splitter handles are 1px `#3a3a3a`, turn `#4a9eff` on hover
- [ ] WizardMode sidebar is `#252525`, 260px wide
- [ ] Drop zone has dashed `#3a3a3a` border, turns `#4a9eff` on drag-over
- [ ] All dialogs use `#1e1e1e` background with no title bar gradient
