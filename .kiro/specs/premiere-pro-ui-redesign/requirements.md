# Requirements Document

## Introduction

NC-KTV Pro is undergoing a full UI overhaul to match the visual language and panel-based layout of Adobe Premiere Pro. The goal is a professional, dark, dockable-panel workspace that feels immediately familiar to video editors — with a native Qt 6.10.2 (Widgets) implementation on Windows. This covers all three application modes: WizardMode (project creation), EditorMode (main timeline editor), and PrecisionMode (syllable editor). The previous ui-ux-clutter-fix spec cleaned up structural issues; this spec defines the complete visual and layout redesign on top of that clean foundation.

The target aesthetic is: `#1e1e1e` / `#252525` / `#2d2d2d` panel backgrounds, `#3a3a3a` borders, `#e0e0e0` primary text, `#8a8a8a` secondary text, `#4a9eff` accent (Premiere's blue), and a compact 28–32px panel header height — identical in feel to Premiere Pro 2024.

## Glossary

- **App**: The NC-KTV Pro desktop application (Qt 6.10.2, C++20, Windows).
- **EditorMode**: The main editing workspace (`cpp/src/gui/editor/editor_mode.cpp`), active after a project is loaded.
- **WizardMode**: The project creation splash screen (`cpp/src/gui/wizard/wizard_mode.cpp`), shown on launch and for new projects.
- **PrecisionMode**: The syllable-level timing editor (`cpp/src/gui/precision/`).
- **MainWindow**: The top-level `QMainWindow` (`cpp/src/gui/main_window.cpp`) that hosts all modes.
- **Panel**: A discrete, bordered UI region with a compact header bar (28–32px) containing a title label and optional action icons — analogous to a Premiere Pro panel (Source Monitor, Timeline, etc.).
- **Panel_Header**: The 28–32px title bar at the top of each Panel, styled with `#2d2d2d` background, `#3a3a3a` bottom border, `#8a8a8a` uppercase label, and optional icon buttons.
- **Menu_Bar**: The application-level menu bar rendered by `QMainWindow`, containing File / Edit / Sequence / Clip / Window / Help menus — styled to match Premiere Pro's compact dark menu bar.
- **Transport_Bar**: The playback control strip at the bottom of EditorMode, containing play/pause, timecode display, jog/shuttle controls, and audio track selector.
- **Timeline_Panel**: The panel containing the waveform widget and timeline widget, occupying the bottom third of EditorMode.
- **Source_Panel**: The panel containing the karaoke preview widget (video monitor), occupying the upper-left area of EditorMode.
- **Program_Panel**: The panel containing the live karaoke output preview, occupying the upper-right area of EditorMode.
- **Captions_Panel**: The panel containing the subtitle/lyrics table (`m_syncTable`), occupying the left column of EditorMode.
- **Tools_Panel**: The narrow vertical icon strip on the far left of EditorMode, analogous to Premiere's Tools panel.
- **dark_theme.qss**: The global Qt stylesheet at `cpp/src/gui/resources/styles/dark_theme.qss` — the single source of truth for all visual styling.
- **Accent_Color**: `#4a9eff` — the primary interactive highlight color (Premiere Pro blue), used for selected tabs, active playhead, focus rings, and primary action buttons.
- **Panel_BG**: `#1e1e1e` — the default background for panel content areas.
- **Header_BG**: `#2d2d2d` — the background for Panel_Header bars and the Menu_Bar.
- **Border_Color**: `#3a3a3a` — the color for all panel borders, separators, and dividers.
- **Primary_Text**: `#e0e0e0` — default label and content text color.
- **Secondary_Text**: `#8a8a8a` — dimmed text for panel titles, column headers, and inactive tabs.
- **QSS_Selector**: A CSS-like rule in `dark_theme.qss` targeting a widget by class name or `objectName`.

---

## Requirements

### Requirement 1: Global Color Palette and Typography

**User Story:** As a video editor, I want the entire application to use the exact Premiere Pro dark color palette, so that the tool feels immediately familiar and professional.

#### Acceptance Criteria

1. THE App SHALL apply `#1e1e1e` as the background color for all Panel content areas via the `QWidget` base rule in `dark_theme.qss`.
2. THE App SHALL apply `#2d2d2d` as the background color for all Panel_Header bars, the Menu_Bar, and the Transport_Bar via named QSS_Selectors.
3. THE App SHALL apply `#3a3a3a` as the border color for all panel edges, splitter handles, and separator lines via QSS_Selectors.
4. THE App SHALL apply `#e0e0e0` as the default text color for all content labels and editable fields.
5. THE App SHALL apply `#8a8a8a` as the text color for Panel_Header titles, column headers, inactive tab labels, and placeholder text.
6. THE App SHALL apply `#4a9eff` as the Accent_Color for selected tab underlines, active playhead, focus borders on input fields, and primary action button backgrounds.
7. THE App SHALL use "Segoe UI" at 11px for Panel_Header labels and column headers, and 12px for content text, with no bold weight on secondary labels.
8. WHEN a `QPushButton` is in its default (non-primary) state, THE App SHALL render it with `#2d2d2d` background, `#3a3a3a` border, `#c8c8c8` text, and 4px border-radius.
9. WHEN a `QPushButton` is hovered, THE App SHALL render it with `#383838` background and `#4a9eff` border at 40% opacity.
10. WHEN a `QPushButton` is pressed, THE App SHALL render it with `#1a1a1a` background.

---

### Requirement 2: Application Menu Bar (Premiere-Style)

**User Story:** As a video editor, I want a native Qt menu bar styled exactly like Premiere Pro's compact dark menu bar, so that I can access all application commands from a familiar location.

#### Acceptance Criteria

1. THE MainWindow SHALL expose a visible `QMenuBar` containing menus: File, Edit, Sequence, Clip, Window, Help.
2. THE Menu_Bar SHALL have `#2d2d2d` background, `#3a3a3a` bottom border, 28px fixed height, and `#8a8a8a` item text color via `dark_theme.qss`.
3. WHEN a menu item is hovered or selected, THE Menu_Bar SHALL highlight it with `#383838` background and `#e0e0e0` text.
4. THE File menu SHALL contain: New Project, Open Project, Save Project, Save Project As, Import (Lyrics/Subtitle), Export Video, Preferences, and Exit.
5. THE Edit menu SHALL contain: Undo, Redo, and a separator before Preferences.
6. THE Sequence menu SHALL contain: Add Subtitle Line, Remove Selected Line, and Split at Playhead.
7. THE Clip menu SHALL contain: Set In Point, Set Out Point, and Clear In/Out.
8. THE Window menu SHALL contain: toggle entries for each Panel (Captions, Source Monitor, Program Monitor, Timeline, Tools).
9. THE Help menu SHALL contain: Keyboard Shortcuts and About NC-KTV Pro.
10. IF the App is in WizardMode, THEN THE Menu_Bar SHALL disable Sequence, Clip, and Window menus and show them as grayed out.

---

### Requirement 3: EditorMode — Premiere Pro Panel Layout

**User Story:** As a video editor, I want the EditorMode workspace to use a Premiere Pro-style panel grid layout, so that I can work with a familiar spatial arrangement of tools, monitors, captions, and timeline.

#### Acceptance Criteria

1. THE EditorMode SHALL arrange panels in a three-row layout: (top) a workspace row containing Tools_Panel + Source_Panel + Captions_Panel + Program_Panel; (middle) a tab bar row for view switching; (bottom) Timeline_Panel + Transport_Bar.
2. THE EditorMode SHALL use `QSplitter` widgets (horizontal and vertical) to divide all panel regions, allowing the user to resize panels by dragging splitter handles.
3. WHEN the EditorMode is first opened, THE EditorMode SHALL initialize panel proportions to: Tools_Panel 32px fixed width; Source_Panel and Program_Panel each ~35% of remaining width; Captions_Panel ~30% of remaining width; Timeline_Panel occupying the full bottom row at ~30% of total window height.
4. THE EditorMode SHALL persist panel size proportions across sessions using `ConfigManager`.
5. WHEN a splitter handle is hovered, THE App SHALL render it with `#4a9eff` color at 60% opacity.
6. THE EditorMode SHALL NOT use a `QMainWindow` embedded inside the widget (no dock widgets, no dock chrome).

---

### Requirement 4: Panel Headers

**User Story:** As a video editor, I want every panel to have a compact Premiere Pro-style header bar with a title and optional action icons, so that I can identify panels at a glance and access panel-specific actions.

#### Acceptance Criteria

1. EVERY Panel in EditorMode SHALL have a Panel_Header widget with fixed height of 28px, `#2d2d2d` background, and a `#3a3a3a` bottom border.
2. THE Panel_Header SHALL display the panel title as an uppercase label in `#8a8a8a` at 10px with 1px letter-spacing, left-aligned with 10px left padding.
3. WHERE a Panel supports panel-specific actions (e.g., Captions_Panel "Add Line" button, Source_Panel "Settings" icon), THE Panel_Header SHALL display those action icons/buttons right-aligned within the 28px header, using 16px icon buttons with transparent background and `#8a8a8a` color that brightens to `#e0e0e0` on hover.
4. THE Panel_Header SHALL NOT display close or float buttons (no dock widget chrome).
5. WHEN a Panel is the active focus target, THE Panel_Header SHALL render its bottom border in `#4a9eff` instead of `#3a3a3a`.

---

### Requirement 5: Tools Panel

**User Story:** As a video editor, I want a narrow vertical tools strip on the far left of the editor, so that I can access editing mode tools (select, razor, slip) in the same location as Premiere Pro.

#### Acceptance Criteria

1. THE Tools_Panel SHALL be a 32px-wide vertical strip on the far left edge of EditorMode, with `#252525` background and a `#3a3a3a` right border.
2. THE Tools_Panel SHALL contain icon buttons for: Selection Tool (arrow), Razor Tool (blade), and Slip Tool (double-arrow), stacked vertically with 4px spacing.
3. WHEN a tool button is active/selected, THE Tools_Panel SHALL render it with `#4a9eff` background at 20% opacity and `#4a9eff` icon color.
4. WHEN a tool button is hovered, THE Tools_Panel SHALL render it with `#383838` background.
5. THE Tools_Panel SHALL default to the Selection Tool active on EditorMode initialization.

---

### Requirement 6: Source Monitor Panel (Karaoke Preview)

**User Story:** As a video editor, I want the karaoke preview to be displayed in a Source Monitor-style panel with a dark frame and panel header, so that it looks like a professional video monitor.

#### Acceptance Criteria

1. THE Source_Panel SHALL display the `KaraokePreview` widget as its content area, filling the panel below the Panel_Header.
2. THE Source_Panel Panel_Header SHALL display the title "Source Monitor".
3. THE Source_Panel content area SHALL have `#0a0a0a` background (near-black, matching Premiere's monitor background).
4. THE Source_Panel SHALL display a timecode label in the bottom-left of the content area, styled in `#e0e0e0` monospace font at 11px, showing the current playback position as `HH:MM:SS:FF`.
5. WHEN no project is loaded, THE Source_Panel SHALL display a centered `#2d2d2d` placeholder with "No Source" text in `#555555`.

---

### Requirement 7: Program Monitor Panel

**User Story:** As a video editor, I want a Program Monitor panel showing the live karaoke output preview, so that I can see the final rendered look while editing.

#### Acceptance Criteria

1. THE Program_Panel SHALL occupy the upper-right area of EditorMode, mirroring the Source_Panel layout.
2. THE Program_Panel Panel_Header SHALL display the title "Program Monitor".
3. THE Program_Panel content area SHALL have `#0a0a0a` background.
4. THE Program_Panel SHALL display the same `KaraokePreview` output as the Source_Panel (shared model, same playback position).
5. WHEN no project is loaded, THE Program_Panel SHALL display a centered placeholder with "No Program" text in `#555555`.

---

### Requirement 8: Captions Panel (Subtitle Table)

**User Story:** As a video editor, I want the subtitle/lyrics table to be displayed in a Captions-style panel with a Premiere Pro-styled table, so that I can manage subtitle lines in a familiar interface.

#### Acceptance Criteria

1. THE Captions_Panel SHALL display `m_syncTable` as its content area, filling the panel below the Panel_Header.
2. THE Captions_Panel Panel_Header SHALL display the title "Captions" and right-aligned action buttons: "+ Add" and "AI Sync".
3. THE Captions_Panel table SHALL have no visible grid lines, `#1e1e1e` background, alternating row color `#222222`, and row height of 28px.
4. THE Captions_Panel table column headers SHALL use `#2d2d2d` background, `#8a8a8a` text at 10px uppercase, and a `#3a3a3a` bottom border.
5. WHEN a table row is selected, THE Captions_Panel SHALL highlight it with `#4a9eff` at 15% opacity background and a 2px `#4a9eff` left border on the row.
6. WHEN a table row is hovered, THE Captions_Panel SHALL highlight it with `#ffffff` at 4% opacity background.
7. THE Captions_Panel table SHALL display columns: # (28px), Start (72px), End (72px), Content (stretch), Duration (52px) — matching the existing `m_syncTable` column structure.

---

### Requirement 9: Timeline Panel

**User Story:** As a video editor, I want the timeline and waveform to be displayed in a Premiere Pro-style Timeline panel at the bottom of the editor, so that I can work with a familiar timeline layout.

#### Acceptance Criteria

1. THE Timeline_Panel SHALL occupy the full-width bottom row of EditorMode, below the workspace row.
2. THE Timeline_Panel Panel_Header SHALL display the title "Timeline" and right-aligned controls: zoom slider and timecode display.
3. THE Timeline_Panel SHALL contain the `WaveformWidget` and `TimelineWidget` stacked vertically, with the waveform at ~60px height and the timeline filling remaining space.
4. THE Timeline_Panel track header area (left ~120px) SHALL have `#252525` background with track name labels in `#c8c8c8` at 11px.
5. THE Timeline_Panel playhead SHALL be rendered as a 1px `#4a9eff` vertical line with a downward-pointing triangle handle at the top, 8px wide.
6. WHEN the playhead is dragged, THE Timeline_Panel SHALL update the playback position in real time and move the playhead line accordingly.
7. THE Timeline_Panel time ruler SHALL have `#2d2d2d` background, `#8a8a8a` tick marks, and `#c8c8c8` time labels at 10px.

---

### Requirement 10: Transport Bar

**User Story:** As a video editor, I want a Premiere Pro-style transport bar at the bottom of the editor with playback controls, timecode, and audio track selector, so that I can control playback from a familiar location.

#### Acceptance Criteria

1. THE Transport_Bar SHALL be a fixed 40px-height strip at the very bottom of EditorMode, with `#1e1e1e` background and a `#3a3a3a` top border.
2. THE Transport_Bar SHALL contain, left to right: Go to In Point button, Step Back button, Play/Pause button, Step Forward button, Go to Out Point button — all using 20px icon buttons with `#c8c8c8` color.
3. THE Transport_Bar Play/Pause button SHALL be 28px × 28px with `#e0e0e0` background, `#1a1a1a` icon color, and 14px border-radius (circular).
4. THE Transport_Bar SHALL display a timecode label in `#e0e0e0` monospace font at 13px, showing `HH:MM:SS:FF`, positioned to the right of the playback buttons.
5. THE Transport_Bar SHALL display the audio track selector `QComboBox` to the right of the timecode, with `#2d2d2d` background and `#3a3a3a` border.
6. THE Transport_Bar SHALL display the `WaveformWidget` as a mini waveform strip between the track selector and the right edge, filling remaining horizontal space.
7. WHEN the Play/Pause button is clicked, THE Transport_Bar SHALL toggle the playback state and update the button icon between ▶ and ⏸.

---

### Requirement 11: View Tab Bar (Lyrics / Timing / Render)

**User Story:** As a video editor, I want the Lyrics Editor / Timing Sync / Video Render view switcher to look like Premiere Pro's workspace tab bar, so that switching between editing modes feels native to the NLE paradigm.

#### Acceptance Criteria

1. THE EditorMode SHALL display a tab bar row between the workspace row and the Timeline_Panel, with `#252525` background and a `#3a3a3a` bottom border.
2. THE tab bar SHALL contain three tab buttons: "Lyrics Editor", "Timing Sync", "Video Render" — left-aligned with no gap between tabs.
3. WHEN a tab is inactive, THE tab bar SHALL render it with transparent background, `#8a8a8a` text at 12px, and no underline.
4. WHEN a tab is active, THE tab bar SHALL render it with transparent background, `#e0e0e0` text at 12px, and a 2px `#4a9eff` bottom underline.
5. WHEN a tab is hovered, THE tab bar SHALL render it with `#ffffff` at 5% opacity background and `#c0c0c0` text.
6. THE tab bar right side SHALL contain: a "Save" primary action button and a "Log" utility button, right-aligned.

---

### Requirement 12: WizardMode — Premiere Pro Start Screen

**User Story:** As a video editor, I want the project creation screen to look like Premiere Pro's "New Project" / Home screen, so that the onboarding experience matches the professional aesthetic.

#### Acceptance Criteria

1. THE WizardMode welcome page SHALL use `#1a1a1a` as the full-page background color.
2. THE WizardMode SHALL display a left sidebar (~260px) with `#252525` background and `#3a3a3a` right border, containing: the "NC-KTV PRO" branding, a "New Project" button, an "Open Project" button, and a "Recent Projects" section.
3. THE WizardMode "New Project" and "Open Project" buttons SHALL be styled as full-width `#2d2d2d` buttons with `#3a3a3a` border, `#c8c8c8` text, and a left-aligned icon.
4. THE WizardMode SHALL display a main content area to the right of the sidebar, containing the media pipeline card (drop zone, processing profile, start button).
5. THE WizardMode media pipeline card SHALL use `#252525` background, `#3a3a3a` border, 6px border-radius, and no drop shadow.
6. THE WizardMode drop zone SHALL use `#1e1e1e` background, `2px dashed #3a3a3a` border, and 4px border-radius — brightening the border to `#4a9eff` when a file is dragged over it.
7. WHEN a file is selected, THE WizardMode drop zone SHALL update its border to `2px solid #4a9eff` at 40% opacity and display the filename in `#c8c8c8`.
8. THE WizardMode "Start Engine" button SHALL be styled as a full-width `#4a9eff` button with `#ffffff` text, 4px border-radius, and 40px height.
9. WHEN the "Start Engine" button is disabled, THE WizardMode SHALL render it with `#2d2d2d` background and `#555555` text.
10. THE WizardMode SHALL NOT display stat cards, hero text, or marketing copy — only functional project creation controls.
11. THE WizardMode processing page SHALL display a centered progress card with `#252525` background, `#3a3a3a` border, task title in `#e0e0e0` at 18px, and a progress bar using `#4a9eff` fill.

---

### Requirement 13: PrecisionMode — Syllable Editor Styling

**User Story:** As a video editor, I want the syllable-level timing editor to use the same Premiere Pro panel aesthetic as the rest of the app, so that the experience is visually consistent.

#### Acceptance Criteria

1. THE PrecisionMode SHALL use `#1e1e1e` as its full-area background color.
2. THE PrecisionMode SHALL display a Panel_Header at the top with title "Precision Editor" styled per Requirement 4.
3. THE PrecisionMode syllable grid SHALL use `#252525` cell backgrounds, `#3a3a3a` cell borders, `#e0e0e0` syllable text at 13px, and `#4a9eff` highlight color for the active/selected syllable.
4. THE PrecisionMode transport controls SHALL match the Transport_Bar styling defined in Requirement 10.
5. WHEN a syllable is being timed (in-progress), THE PrecisionMode SHALL render it with `#4a9eff` at 30% opacity background and a 1px `#4a9eff` border.

---

### Requirement 14: Scrollbars, Splitters, and Interactive Chrome

**User Story:** As a video editor, I want all scrollbars, splitter handles, and interactive chrome to match Premiere Pro's minimal, dark styling, so that the UI feels cohesive and unobtrusive.

#### Acceptance Criteria

1. THE App SHALL render all `QScrollBar` handles with `#3a3a3a` background, 3px border-radius, and no arrow buttons — matching the existing dark_theme.qss scrollbar style but with handle width reduced to 5px.
2. WHEN a scrollbar handle is hovered, THE App SHALL render it with `#4a9eff` background.
3. THE App SHALL render all `QSplitter` handles as 1px `#3a3a3a` lines with no visible grip texture.
4. WHEN a splitter handle is hovered, THE App SHALL render it with `#4a9eff` color.
5. THE App SHALL render all `QComboBox` dropdowns with `#252525` background, `#3a3a3a` border, `#c8c8c8` text, and a `▾` indicator in `#8a8a8a`.
6. THE App SHALL render all `QLineEdit` and `QPlainTextEdit` fields with `#1a1a1a` background, `#3a3a3a` border, `#e0e0e0` text, and 4px border-radius.
7. WHEN a `QLineEdit` or `QPlainTextEdit` is focused, THE App SHALL render its border in `#4a9eff`.

---

### Requirement 15: QSS Stylesheet Consolidation

**User Story:** As a developer, I want all visual styling to be defined exclusively in `dark_theme.qss` using named object selectors, so that the codebase has a single source of truth for appearance and inline `setStyleSheet()` calls are eliminated.

#### Acceptance Criteria

1. THE App SHALL define all visual styles in `dark_theme.qss` using `QWidget`, class-level, and `objectName`-based QSS_Selectors — no inline `setStyleSheet()` calls SHALL remain in `editor_mode.cpp`, `wizard_mode.cpp`, or `main_window.cpp` after the redesign.
2. THE dark_theme.qss SHALL define named selectors for every structural widget: `QWidget#panelHeader`, `QWidget#toolsPanel`, `QWidget#sourcePanel`, `QWidget#programPanel`, `QWidget#captionsPanel`, `QWidget#timelinePanel`, `QWidget#transportBar`, `QWidget#tabBar`, `QWidget#topBar`.
3. THE dark_theme.qss SHALL define named selectors for all interactive button variants: `QPushButton#primaryAction`, `QPushButton#tabBtn`, `QPushButton#navBtn`, `QPushButton#toolBtn`, `QPushButton#stampBtn`, `QPushButton#playBtn`.
4. THE dark_theme.qss SHALL use CSS custom-property-style comments to document each color value with its semantic name (e.g., `/* Panel_BG: #1e1e1e */`) at the top of the file.
5. WHERE a widget requires a style that cannot be expressed in QSS (e.g., custom painting in `paintEvent`), THE App SHALL use `QPalette` or direct `QPainter` calls in the widget's own `paintEvent` — not inline `setStyleSheet()` on parent widgets.

---

### Requirement 16: Dialogs and Overlays

**User Story:** As a video editor, I want all dialogs (Export, Preferences, Import, Model Manager) and processing overlays to use the Premiere Pro dark aesthetic, so that modal interactions feel consistent with the main workspace.

#### Acceptance Criteria

1. ALL `QDialog` subclasses SHALL use `#1e1e1e` background, `#3a3a3a` border, and no title bar gradient.
2. ALL `QDialog` subclasses SHALL display a compact 32px header area with the dialog title in `#e0e0e0` at 13px bold, and a close button (`✕`) right-aligned in `#8a8a8a`.
3. THE processing overlay (`ProcessingOverlay`) SHALL use `rgba(0, 0, 0, 0.75)` full-screen backdrop and a centered `#252525` card with `#3a3a3a` border and 8px border-radius.
4. THE processing overlay progress bar SHALL use `#4a9eff` fill color and `#2d2d2d` track background, with 4px height and no text.
5. ALL `QMessageBox` instances SHALL be styled via `dark_theme.qss` to use `#1e1e1e` background, `#e0e0e0` text, and `#4a9eff` default button.
