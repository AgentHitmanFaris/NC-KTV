# Bugfix Requirements Document

## Introduction

NC-KTV Pro's UI is functionally correct — no crashes, no lag — but the visual experience feels cluttered, unstable, and overwhelming. Users encounter inconsistent spacing, competing visual hierarchies, redundant UI elements, and layout instability that makes the app feel disorganized despite having straightforward controls. This document captures the defective visual behaviors and defines what a clean, polished experience should look like.

## Bug Analysis

### Current Behavior (Defect)

1.1 WHEN the editor mode is open THEN the system renders two separate header bars (the `appHeader` inside the workspace and the sidebar's project card area), creating a visually noisy top region with competing branding and redundant navigation menus duplicated between the wizard and editor.

1.2 WHEN the user views the editor layout THEN the system displays a fixed 280px sidebar alongside a `QMainWindow`-within-a-widget containing docked panels, causing the layout to feel structurally unstable and producing unexpected dock widget chrome (title bars, close buttons) that clutter the interface.

1.3 WHEN the editor's Lyrics Editor view is active THEN the system shows a Gemini API key input field, a paste button, and a plain text editor all in the same horizontal row at the top of the content area, making the primary editing surface feel cramped and the API key field feel out of place.

1.4 WHEN the user looks at the Synchronization Queue dock THEN the system renders it as a floating dock widget at the bottom with its own title bar chrome, visually disconnecting it from the main content and adding unnecessary visual weight.

1.5 WHEN inline `setStyleSheet()` calls are applied on individual widgets THEN the system produces inconsistent visual results — some buttons use `border-radius: 6px`, others use `10px`, `8px`, or `4px` — making the UI feel visually incoherent and unstable across panels.

1.6 WHEN the wizard welcome page is displayed THEN the system shows three stat cards ("0ms LATENCY", "4K RESOLUTION", "FFMPEG NATIVE CORE") that provide no actionable value to the user and add visual noise to the hero section.

1.7 WHEN the editor toolbar area is rendered THEN the system places the "● SYNCED" badge and menu buttons in the same header bar as the "NC-KTV PRO" title, creating a dense, cluttered top bar with too many competing elements at the same visual weight.

1.8 WHEN the audio monitor section in the sidebar is rendered THEN the system shows an empty 70px dark box with no content, creating a visual dead zone that wastes sidebar space and looks unfinished.

### Expected Behavior (Correct)

2.1 WHEN the editor mode is open THEN the system SHALL render a single unified top bar that consolidates branding and navigation, eliminating the duplicate header layer.

2.2 WHEN the user views the editor layout THEN the system SHALL use a stable, flat layout structure (e.g., a `QSplitter` or simple `QHBoxLayout`) without embedding a `QMainWindow` inside a widget, removing unexpected dock chrome and layout instability.

2.3 WHEN the editor's Lyrics Editor view is active THEN the system SHALL separate the AI tools (Gemini key, paste) into a collapsible or secondary toolbar area, keeping the primary lyrics editing surface clean and unobstructed.

2.4 WHEN the user looks at the Synchronization Queue THEN the system SHALL render it as an integrated panel within the main layout (not a floating dock), with a clean section header consistent with the rest of the UI.

2.5 WHEN any widget is styled THEN the system SHALL apply styles exclusively through the global `dark_theme.qss` stylesheet or a small set of named object styles, eliminating per-widget inline `setStyleSheet()` calls that cause visual inconsistency.

2.6 WHEN the wizard welcome page is displayed THEN the system SHALL replace the non-actionable stat cards with a concise recent projects list or a single clear call-to-action, reducing visual noise in the hero section.

2.7 WHEN the editor toolbar area is rendered THEN the system SHALL visually separate the app title, navigation menus, and status indicators using spacing and weight hierarchy so no single row feels overcrowded.

2.8 WHEN the audio monitor section in the sidebar is rendered THEN the system SHALL either display meaningful content (e.g., a waveform level meter or playback time) or remove the placeholder entirely to eliminate the visual dead zone.

### Unchanged Behavior (Regression Prevention)

3.1 WHEN the user clicks File, Edit, Project, or Export menu buttons THEN the system SHALL CONTINUE TO open the correct dropdown menus with all existing actions intact.

3.2 WHEN the user navigates between Lyrics Editor, Timing Sync, and Video Render modes THEN the system SHALL CONTINUE TO switch views correctly via the sidebar mode buttons.

3.3 WHEN the user interacts with the timeline, syllable editor, or karaoke preview THEN the system SHALL CONTINUE TO respond to all mouse, scroll, and keyboard interactions without regression.

3.4 WHEN the wizard processes a media file THEN the system SHALL CONTINUE TO run vocal separation and transcription workers correctly and transition to the editor on completion.

3.5 WHEN the user saves or opens a project THEN the system SHALL CONTINUE TO serialize and deserialize project data correctly.

3.6 WHEN the Synchronization Queue table is displayed THEN the system SHALL CONTINUE TO show subtitle lines with correct start/end times, content, and duration columns.

3.7 WHEN the karaoke preview renders lyrics THEN the system SHALL CONTINUE TO display the wipe animation, gradient colors, and shadow effects correctly.
