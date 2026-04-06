# Implementation Plan

- [x] 1. Write bug condition exploration test
  - **Property 1: Bug Condition** - UI Clutter Structural Analysis
  - **CRITICAL**: This test MUST FAIL on unfixed code - failure confirms the clutter bugs exist
  - **DO NOT attempt to fix the test or the code when it fails**
  - **NOTE**: This test encodes the expected clean structure - it will validate the fix when it passes after implementation
  - **GOAL**: Surface counterexamples that demonstrate the structural clutter exists
  - **Scoped PBT Approach**: Scope the property to concrete failing cases — inspect the widget tree of `EditorMode` and `WizardMode` after construction
  - Test 1: Assert no child widget of `EditorMode` is a `QMainWindow` instance (from Bug Condition in design — embedded QMainWindow)
  - Test 2: Assert `EditorMode` has exactly one header-height widget (no duplicate header bars)
  - Test 3: Assert no `QPushButton` child of `EditorMode` or `WizardMode` has a `styleSheet()` containing `border-radius` values inconsistent with the global QSS (8px for buttons)
  - Test 4: Assert the sidebar audio monitor widget either has visible child content or does not exist
  - Test 5: Assert `WizardMode` hero section contains no stat card widgets (no "LATENCY", "RESOLUTION", "FFMPEG" labels)
  - Run tests on UNFIXED code
  - **EXPECTED OUTCOME**: Tests FAIL (this is correct - it proves the clutter bugs exist)
  - Document counterexamples found (e.g., "EditorMode contains QMainWindow child", "3 QPushButtons with border-radius:10px vs global 8px")
  - Mark task complete when tests are written, run, and failures are documented
  - _Requirements: 1.1, 1.2, 1.5, 1.6, 1.8_

- [x] 2. Write preservation property tests (BEFORE implementing fix)
  - **Property 2: Preservation** - Functional Behavior Unchanged
  - **IMPORTANT**: Follow observation-first methodology
  - Observe: File menu button emits `requestNew` / `requestOpen` / `requestPreferences` signals correctly on unfixed code
  - Observe: Sidebar mode buttons correctly set `m_viewStack->currentIndex()` to 0, 1, 2 on unfixed code
  - Observe: `TimelineWidget::seekRequested` fires with correct time value when clicking the ruler on unfixed code
  - Observe: `WizardMode::projectReady` signal fires after `onSeparationFinished` + `onTranscriptionFinished` on unfixed code
  - Write property-based test: for all mode button clicks, `m_viewStack->currentIndex()` equals the expected index (from Preservation Requirements in design)
  - Write property-based test: for all timeline click positions, `seekRequested` value equals `(clickX + scrollOffset) / pixelsPerSecond`
  - Verify tests pass on UNFIXED code
  - _Requirements: 3.1, 3.2, 3.3, 3.4, 3.5, 3.6, 3.7_

- [x] 3. Fix UI/UX clutter across EditorMode, WizardMode, and dark_theme.qss

  - [x] 3.1 Replace embedded QMainWindow in EditorMode with flat layout structure
    - In `editor_mode.cpp` `setupUi()`: remove `QMainWindow* rightContainer` and its dock widget setup
    - Replace with a `QSplitter(Qt::Vertical)` containing the workspace area (top) and sync queue panel (bottom)
    - Remove `QDockWidget` wrappers for "Synchronization Queue" and "Properties" panels
    - Integrate the sync queue as a plain `QWidget` section below the view stack with a clean section header
    - Integrate the properties panel as a fixed-width right column using `QSplitter(Qt::Horizontal)`
    - _Bug_Condition: isBugCondition(context) where hasEmbeddedQMainWindow(context) is true_
    - _Expected_Behavior: noEmbeddedQMainWindow(result) AND noUnwantedDockChrome(result)_
    - _Preservation: All dock content (sync table, properties panel) must remain visible and functional_
    - _Requirements: 2.2, 3.2, 3.6_

  - [x] 3.2 Consolidate to a single header bar in EditorMode
    - Simplify `appHeader` to a slim 48px bar containing only: app title (left), menu buttons (center-left), status badge (right)
    - Remove the competing visual weight from the sidebar project card — keep it as a data display card, not a header
    - Ensure consistent 48px height and `border-bottom: 1px solid rgba(255,255,255,0.05)` matching the wizard nav bar
    - _Bug_Condition: isBugCondition(context) where hasDuplicateHeaderBars(context) is true_
    - _Expected_Behavior: singleHeaderBar(result) with clear visual hierarchy_
    - _Requirements: 2.1, 2.7_

  - [x] 3.3 Remove inline setStyleSheet() calls and centralize styling in dark_theme.qss
    - In `editor_mode.cpp`: replace all inline `setStyleSheet()` calls with `setObjectName()` assignments
    - In `wizard_mode.cpp`: replace all inline `setStyleSheet()` calls with `setObjectName()` assignments
    - Add corresponding named selectors to `dark_theme.qss`:
      - `QWidget#sidebar` — sidebar background and border
      - `QPushButton#navBtn` — navigation menu buttons (File, Edit, etc.)
      - `QPushButton#startEngineBtn` — the gradient start button
      - `QPushButton#primaryAction` — gradient primary action buttons
      - `QWidget#pipelineCard` — the wizard pipeline card
      - `QWidget#projectCard` — the editor sidebar project card
      - `QWidget#syncQueuePanel` — the sync queue container
    - Standardize border-radius: buttons=8px, cards=12px, inputs=8px throughout QSS
    - _Bug_Condition: isBugCondition(context) where inconsistentWithGlobalTheme(context.styleValue) is true_
    - _Expected_Behavior: allStylesFromGlobalQSS(result) AND consistentBorderRadius(result)_
    - _Preservation: Visual appearance of all widgets must remain equivalent — only the source of the style changes_
    - _Requirements: 2.5, 3.1, 3.2, 3.3_

  - [x] 3.4 Clean up Lyrics Editor view — separate AI tools from editing surface
    - Move the Gemini API key `QLineEdit` and "Transcribe with Gemini" button into a collapsible `QFrame` or secondary toolbar row above the `QPlainTextEdit`
    - Keep the "📋 Paste & Sync" button visible but move it to the toolbar row
    - Give the `QPlainTextEdit` (`m_sourceLyricsEdit`) the full remaining height of the view
    - _Bug_Condition: isBugCondition(context) where lyricsEditorViewCramped(context) is true_
    - _Expected_Behavior: primaryEditingSurfaceUnobstructed(result)_
    - _Requirements: 2.3_

  - [x] 3.5 Remove non-actionable stat cards from WizardMode hero section
    - In `wizard_mode.cpp` `createWelcomePage()`: delete the `statsWidget` and its three `createStatCard()` calls ("0ms LATENCY", "4K RESOLUTION", "FFMPEG NATIVE CORE")
    - Replace with a simple `QLabel` showing a brief tagline or leave the hero layout cleaner with just the title and description
    - _Bug_Condition: isBugCondition(context) where hasNonActionableStatCards(context) is true_
    - _Expected_Behavior: noStatCards(result) AND cleanerHeroSection(result)_
    - _Requirements: 2.6_

  - [x] 3.6 Remove or populate the empty audio monitor placeholder in the sidebar
    - In `editor_mode.cpp` `setupUi()`: either remove the `audioMonWg` placeholder entirely, or replace it with a `QLabel` showing current playback time that updates via a timer
    - If keeping it: wire it to display `formatTimeMMSS(m_currentTime)` updated on a 100ms `QTimer`
    - _Bug_Condition: isBugCondition(context) where hasEmptyAudioMonitorPlaceholder(context) is true_
    - _Expected_Behavior: noEmptyPlaceholders(result)_
    - _Requirements: 2.8_

  - [x] 3.7 Verify bug condition exploration test now passes
    - **Property 1: Expected Behavior** - UI Clutter Structural Analysis
    - **IMPORTANT**: Re-run the SAME test from task 1 - do NOT write a new test
    - The test from task 1 encodes the expected clean structure
    - When this test passes, it confirms the structural clutter is resolved
    - Run bug condition exploration test from step 1
    - **EXPECTED OUTCOME**: Test PASSES (confirms clutter is fixed)
    - _Requirements: 2.1, 2.2, 2.5, 2.6, 2.8_

  - [x] 3.8 Verify preservation tests still pass
    - **Property 2: Preservation** - Functional Behavior Unchanged
    - **IMPORTANT**: Re-run the SAME tests from task 2 - do NOT write new tests
    - Run preservation property tests from step 2
    - **EXPECTED OUTCOME**: Tests PASS (confirms no functional regressions)
    - Confirm all menu actions, mode switches, timeline interactions, and project save/load work correctly after the layout refactor

- [x] 4. Checkpoint - Ensure all tests pass
  - Build the project and verify no compile errors from the layout refactor
  - Run all tests from tasks 1 and 2 — all must pass
  - Do a visual review: open the app, check wizard page (no stat cards, clean hero), open editor (single header, no dock chrome, clean lyrics view, no empty audio monitor box)
  - Verify the global QSS is the sole source of styling — no inline `setStyleSheet()` calls remain in wizard_mode.cpp or editor_mode.cpp
  - Ask the user if any visual issues remain before marking complete
