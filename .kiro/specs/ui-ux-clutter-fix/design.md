# UI/UX Clutter Fix — Bugfix Design

## Overview

NC-KTV Pro's UI suffers from visual clutter and instability caused by structural layout issues, inconsistent inline styling, redundant UI elements, and misused Qt layout primitives (a `QMainWindow` embedded inside a widget). The fix targets these root causes systematically: flatten the layout structure, centralize all styling into the QSS theme, remove non-actionable UI noise, and establish a clear visual hierarchy. No functional behavior changes — only visual and structural improvements.

## Glossary

- **Bug_Condition (C)**: Any UI render context where the clutter conditions are triggered — editor open, wizard displayed, or any widget styled via inline `setStyleSheet()` with values inconsistent with the global theme.
- **Property (P)**: The desired visual state — a single unified header, flat layout structure, consistent border-radius/spacing, no empty placeholder sections, and no redundant navigation elements.
- **Preservation**: All functional behaviors (menu actions, mode switching, timeline interaction, project save/load, karaoke preview rendering) must remain identical after the fix.
- **EditorMode**: The `EditorMode` class in `cpp/src/gui/editor/editor_mode.cpp` that manages the main editing workspace.
- **WizardMode**: The `WizardMode` class in `cpp/src/gui/wizard/wizard_mode.cpp` that manages the onboarding/processing flow.
- **dark_theme.qss**: The global Qt stylesheet at `cpp/src/gui/resources/styles/dark_theme.qss` — the single source of truth for all visual styling.
- **inline setStyleSheet()**: Per-widget style overrides applied in C++ code that bypass the global theme and cause visual inconsistency.

## Bug Details

### Bug Condition

The clutter manifests across multiple render contexts. The `EditorMode::setupUi()` function embeds a `QMainWindow` as a child widget, which introduces dock widget chrome and layout instability. Both `WizardMode` and `EditorMode` define their own navigation menus independently, creating duplication. Hundreds of inline `setStyleSheet()` calls across both files use inconsistent values (border-radius ranging from 4px to 20px, varying padding, different color literals) that conflict with the global QSS theme.

**Formal Specification:**
```
FUNCTION isBugCondition(context)
  INPUT: context of type UIRenderContext
  OUTPUT: boolean

  RETURN (context.isEditorOpen AND hasEmbeddedQMainWindow(context))
      OR (context.hasInlineStyleSheet AND inconsistentWithGlobalTheme(context.styleValue))
      OR (context.isWizardOpen AND hasNonActionableStatCards(context))
      OR (context.isSidebarRendered AND hasEmptyAudioMonitorPlaceholder(context))
      OR (context.isEditorOpen AND hasDuplicateHeaderBars(context))
END FUNCTION
```

### Examples

- Editor opens → two header bars visible simultaneously (appHeader + sidebar project card area acting as a second header zone)
- Inline style on "Start Engine" button uses `border-radius: 10px`; global QSS uses `8px`; preferences dialog uses `4px` — three different values for the same element type
- Wizard hero section shows "0ms LATENCY" stat card — no user action possible, pure visual noise
- Sidebar audio monitor: a 70px dark rectangle with zero content, looks like a broken widget
- `QMainWindow` embedded inside `EditorMode`'s right container produces dock title bars with close/float buttons that users cannot dismiss cleanly

## Expected Behavior

### Preservation Requirements

**Unchanged Behaviors:**
- All menu actions (File, Edit, Project, Export) must continue to work with identical behavior
- Mode switching (Lyrics Editor / Timing Sync / Video Render) via sidebar buttons must continue to work
- Timeline drag, resize, seek, zoom, and context menu interactions must be unaffected
- Syllable editor scroll, zoom, and playhead tracking must be unaffected
- Karaoke preview wipe animation, gradient rendering, and video frame display must be unaffected
- Wizard vocal separation → transcription → editor transition flow must be unaffected
- Project save/load serialization must be unaffected
- Synchronization Queue table display and interaction must be unaffected

**Scope:**
All inputs that do NOT involve the visual rendering of layout structure, inline style values, or non-functional placeholder widgets are completely unaffected. This includes all signal/slot connections, worker threads, data models, and file I/O.

## Hypothesized Root Cause

Based on code analysis, the root causes are:

1. **Embedded QMainWindow**: `EditorMode::setupUi()` creates a `QMainWindow* rightContainer` as a child widget and adds dock widgets to it. `QMainWindow` is designed to be a top-level window — embedding it produces unexpected chrome (dock title bars with float/close buttons) and layout instability. Fix: replace with a `QSplitter` or plain `QVBoxLayout`/`QHBoxLayout` structure.

2. **Duplicate Navigation Menus**: Both `WizardMode::createWelcomePage()` and `EditorMode::setupUi()` independently construct File/Edit/Project menus with inline `QMenu` + `QPushButton` patterns. This duplicates code and creates visual inconsistency between the two modes. Fix: consolidate into a shared nav component or rely on the `QMainWindow` menu bar in `MainWindow`.

3. **Pervasive Inline setStyleSheet() Calls**: Both `wizard_mode.cpp` and `editor_mode.cpp` contain hundreds of inline style strings with hardcoded color values, border-radius values, and padding that differ from `dark_theme.qss`. This is the primary cause of visual incoherence. Fix: remove inline styles and use object names + QSS selectors from the global theme.

4. **Non-Actionable UI Elements**: The wizard's stat cards ("0ms LATENCY", "4K RESOLUTION", "FFMPEG NATIVE CORE") and the sidebar's empty audio monitor box add visual weight without providing user value. Fix: remove stat cards; replace audio monitor placeholder with actual content or remove it.

5. **Dual Header Bars in Editor**: The `appHeader` widget (64px, "NC-KTV PRO" title + menus + SYNCED badge) sits inside the workspace, while the sidebar has its own project card that visually competes as a secondary header. Fix: establish a single top bar with clear hierarchy.

## Correctness Properties

Property 1: Bug Condition - Visual Clutter Elimination

_For any_ UI render context where the bug condition holds (isBugCondition returns true — editor open with embedded QMainWindow, inline styles inconsistent with global theme, non-actionable stat cards visible, empty audio monitor placeholder rendered, or duplicate header bars present), the fixed UI SHALL render a single unified header, use a flat layout structure without embedded QMainWindow dock chrome, apply styles exclusively from the global QSS theme with consistent values, and display no empty placeholder sections or non-actionable stat cards.

**Validates: Requirements 2.1, 2.2, 2.3, 2.4, 2.5, 2.6, 2.7, 2.8**

Property 2: Preservation - Functional Behavior Unchanged

_For any_ user interaction that does NOT involve the visual layout structure or inline style values (menu clicks, mode switches, timeline interactions, project save/load, karaoke preview rendering, worker thread operations), the fixed code SHALL produce exactly the same behavior as the original code, preserving all existing functional correctness.

**Validates: Requirements 3.1, 3.2, 3.3, 3.4, 3.5, 3.6, 3.7**

## Fix Implementation

### Changes Required

Assuming our root cause analysis is correct:

**File 1**: `cpp/src/gui/editor/editor_mode.cpp`

**Specific Changes**:
1. **Replace embedded QMainWindow**: Remove `QMainWindow* rightContainer` and replace with a `QSplitter` (vertical) containing the workspace area and the sync queue panel. This eliminates dock widget chrome.
2. **Remove duplicate header bar**: Remove or simplify `appHeader` — the main window title bar already provides branding context. Keep only a slim toolbar row for menus and status badge.
3. **Remove inline setStyleSheet() calls**: Replace all per-widget inline styles with `setObjectName()` calls that map to selectors in `dark_theme.qss`. For unique cases, add new named selectors to the QSS file.
4. **Clean up Lyrics Editor view**: Move the Gemini API key input and paste button into a collapsible `QToolBar` or a secondary row that can be hidden, keeping the `QPlainTextEdit` as the dominant element.
5. **Remove or populate audio monitor**: Either remove the empty 70px audio monitor box from the sidebar, or wire it to display a simple playback time label.

**File 2**: `cpp/src/gui/wizard/wizard_mode.cpp`

**Specific Changes**:
1. **Remove stat cards**: Delete the `statsWidget` and its three stat cards from `createWelcomePage()`. Replace with a simple recent files hint or leave the hero section cleaner.
2. **Remove inline setStyleSheet() calls**: Replace inline styles with object names mapped to QSS selectors.

**File 3**: `cpp/src/gui/resources/styles/dark_theme.qss`

**Specific Changes**:
1. **Add named selectors**: Add `QWidget#sidebar`, `QWidget#appHeader`, `QPushButton#navBtn`, `QPushButton#startEngineBtn`, `QWidget#pipelineCard`, `QWidget#projectCard` selectors to cover the main structural widgets.
2. **Standardize border-radius**: Enforce a single border-radius value per widget type across the entire QSS file (buttons: 8px, cards: 12px, inputs: 8px).

## Testing Strategy

### Validation Approach

The testing strategy follows a two-phase approach: first, surface counterexamples that demonstrate the clutter on the current (unfixed) code by visual inspection and structural analysis, then verify the fix produces a clean layout and preserves all functional behavior.

### Exploratory Bug Condition Checking

**Goal**: Surface counterexamples that demonstrate the clutter exists BEFORE implementing the fix. Confirm or refute the root cause analysis.

**Test Plan**: Write tests that inspect the widget tree of `EditorMode` and `WizardMode` after construction, asserting structural properties (no embedded `QMainWindow`, no duplicate header widgets, no empty placeholder boxes). Run these tests on UNFIXED code to observe failures.

**Test Cases**:
1. **Embedded QMainWindow Test**: Assert that no child of `EditorMode` is a `QMainWindow` instance (will fail on unfixed code)
2. **Duplicate Header Test**: Assert that `EditorMode` contains exactly one widget with `fixedHeight(64)` or similar header-height constraint (will fail on unfixed code — finds both appHeader and sidebar project card)
3. **Inline Style Consistency Test**: Assert that no `QPushButton` child of `EditorMode` has a `styleSheet()` containing `border-radius` values other than those defined in the global QSS (will fail on unfixed code)
4. **Empty Placeholder Test**: Assert that the audio monitor widget either has children with content or does not exist (will fail on unfixed code)

**Expected Counterexamples**:
- `EditorMode` widget tree contains a `QMainWindow` child → confirms embedded QMainWindow bug
- Multiple widgets with header-like fixed heights → confirms duplicate header bug
- `QPushButton::styleSheet()` returns non-empty strings with inconsistent border-radius → confirms inline style inconsistency

### Fix Checking

**Goal**: Verify that for all inputs where the bug condition holds, the fixed UI produces the expected clean visual structure.

**Pseudocode:**
```
FOR ALL context WHERE isBugCondition(context) DO
  result := renderUI_fixed(context)
  ASSERT noEmbeddedQMainWindow(result)
       AND singleHeaderBar(result)
       AND noInlineStyleInconsistencies(result)
       AND noEmptyPlaceholders(result)
       AND noNonActionableStatCards(result)
END FOR
```

### Preservation Checking

**Goal**: Verify that for all inputs where the bug condition does NOT hold (functional interactions), the fixed code produces the same result as the original.

**Pseudocode:**
```
FOR ALL interaction WHERE NOT isBugCondition(interaction) DO
  ASSERT originalBehavior(interaction) = fixedBehavior(interaction)
END FOR
```

**Testing Approach**: Property-based testing is recommended for preservation checking because:
- It generates many interaction sequences automatically
- It catches regressions in menu actions, mode switches, and data flow that manual tests might miss
- It provides strong guarantees that functional behavior is unchanged

**Test Plan**: Observe behavior on UNFIXED code for all functional interactions, then write property-based tests capturing those behaviors.

**Test Cases**:
1. **Menu Action Preservation**: Verify File/Edit/Project/Export menus emit correct signals after layout refactor
2. **Mode Switch Preservation**: Verify sidebar mode buttons correctly switch `m_viewStack` index after removing embedded QMainWindow
3. **Timeline Interaction Preservation**: Verify seek, drag, zoom interactions on `TimelineWidget` are unaffected
4. **Project Save/Load Preservation**: Verify serialization round-trip produces identical data after UI changes

### Unit Tests

- Test `EditorMode` widget tree: no `QMainWindow` child, single header widget, no empty placeholder boxes
- Test `WizardMode` widget tree: no stat cards, pipeline card present, start button disabled by default
- Test that all menu actions are connected to correct slots after refactor

### Property-Based Tests

- Generate random sequences of mode switches and verify `m_viewStack->currentIndex()` matches expected state
- Generate random project data and verify save/load round-trip is identical before and after UI changes
- Generate random timeline seek positions and verify `seekRequested` signal fires with correct values

### Integration Tests

- Full wizard → editor transition: load a file, complete processing, verify editor opens with correct layout
- Full editor session: switch modes, edit lyrics, save project, reload — verify no visual regressions
- Karaoke preview: load lyrics data, advance time, verify wipe animation renders correctly
