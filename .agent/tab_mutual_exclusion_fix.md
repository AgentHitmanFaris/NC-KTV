# Tab Mutual Exclusion Fix

> **Note:** This document describes the Python-era (pre-v1.0.0) implementation. The C++ rewrite uses `QStackedWidget` / `QButtonGroup` in `cpp/src/gui/editor/editor_mode.cpp` for proper mutual exclusion.

## Date: 2026-02-04

## Problem

Multiple tabs/views were active simultaneously, causing UI confusion:
- Text Input tab + Timeline View both highlighted
- Sync Mode + Fine-Tune Timing both active
- Multiple editors overlapping

## Root Cause

Each view toggle operated independently without checking or clearing other active views.

---

## Solution: Mutual Exclusion

Implemented proper mutual exclusion so **only one view can be active at any time**.

###Rules:

1. **Text Input tab** → Deactivates Sync, Timeline, Fine-Tune
2. **Sync Mode tab** → Deactivates Input, Timeline, Fine-Tune
3. **Timeline View** → Deactivates Input, Sync, Fine-Tune
4. **Fine-Tune Timing** → Deactivates Input, Sync, Timeline

---

## Implementation

### 1. **_switch_tab() Function**

When switching to Input or Sync tabs:

```python
def _switch_tab(self, mode: str):
    # MUTUAL EXCLUSION: Uncheck special views
    self.btn_timeline.setChecked(False)
    self.btn_fine_tune.setChecked(False)
    self.timeline_widget.hide()
    self.syllable_editor.hide()
    
    # Update regular tab buttons
    for m, btn in self.mode_btns.items():
        btn.setChecked(m == mode)
    
    # Show appropriate view
    if mode == 'input':
        self.text_editor.show()
        self.sync_table.hide()
    else:  # 'sync'
        self.text_editor.hide()
        self.sync_table.show()
```

---

### 2. **_toggle_timeline_view() Function**

When activating Timeline View:

```python
def _toggle_timeline_view(self):
    if self.btn_timeline.isChecked():
        # MUTUAL EXCLUSION: Uncheck other views
        self.btn_fine_tune.setChecked(False)
        for mode, btn in self.mode_btns.items():
            btn.setChecked(False)
        
        # Hide other views
        self.text_editor.hide()
        self.sync_table.hide()
        self.syllable_editor.hide()
        
        # Show timeline
        self.timeline_widget.show()
```

---

### 3. **_toggle_fine_tune_view() Function**

When activating Fine-Tune Timing:

```python
def _toggle_fine_tune_view(self):
    if self.btn_fine_tune.isChecked():
        # MUTUAL EXCLUSION: Uncheck other views
        self.btn_timeline.setChecked(False)
        for mode, btn in self.mode_btns.items():
            btn.setChecked(False)
        
        # Hide other views
        self.text_editor.hide()
        self.sync_table.hide()
        self.timeline_widget.hide()
        
        # Show syllable editor
        self.syllable_editor.show()
```

---

## Behavior Matrix

| Action | Text Input | Sync Mode | Timeline | Fine-Tune |
|--------|------------|-----------|----------|-----------|
| **Click "Text Input"** | ✅ Active | ❌ Inactive | ❌ Inactive | ❌ Inactive |
| **Click "Sync Mode"** | ❌ Inactive | ✅ Active | ❌ Inactive | ❌ Inactive |
| **Click "Timeline View"** | ❌ Inactive | ❌ Inactive | ✅ Active | ❌ Inactive |
| **Click "Fine-Tune Timing"** | ❌ Inactive | ❌ Inactive | ❌ Inactive | ✅ Active |

**Result:** Only ONE view active at any time!

---

## User Experience

### Before (Broken):
```
[Text Input ✓] [Import...] [Sync Mode ✓] [Auto-Transcribe] [Timeline ✓] [Fine-Tune ✓]
                     ↑ Multiple tabs highlighted!
```

**Problems:**
- Confusing visual state
- Multiple editors overlapping
- Unclear which view is active
- Difficult to navigate

---

### After (Fixed):
```
[Text Input] [Import...] [Sync Mode ✓] [Auto-Transcribe] [Timeline] [Fine-Tune]
                                ↑ Only one tab highlighted!
```

**Benefits:**
- ✅ Clear visual state
- ✅ Only one editor visible
- ✅ Obvious which view is active
- ✅ Easy to switch between views

---

## Edge Cases Handled

### 1. **Clicking Active Tab Again**
- Unchecks it (returns to default view)
- Works for Timeline and Fine-Tune

### 2. **Rapid Clicking**
- Each click properly clears previous state
- No race conditions

### 3. **Programmatic Tab Switch**
- `_switch_tab()` properly clears special views
- Ensures consistency

---

## Testing

### Test Scenarios:

1. ✅ **Input → Sync** - Input unchecks, Sync checks
2. ✅ **Sync → Timeline** - Sync unchecks, Timeline checks
3. ✅ **Timeline → Fine-Tune** - Timeline unchecks, Fine-Tune checks
4. ✅ **Fine-Tune → Input** - Fine-Tune unchecks, Input checks
5. ✅ **Timeline → Timeline** - Timeline unchecks (toggles off)
6. ✅ **Any → Sync → Timeline** - Proper transition

**Result:** Perfect mutual exclusion in all scenarios!

---

## Code Changes

**File:** `src/gui/editor/editor_mode.py`

### Modified Functions:
1. **Line 606-628:** `_switch_tab()` - Added mutual exclusion for regular tabs
2. **Line 2224-2248:** `_toggle_fine_tune_view()` - Added mutual exclusion
3. **Line 2257-2275:** `_toggle_timeline_view()` - Added mutual exclusion

**Total Lines Changed:** ~25 lines

---

## Benefits

### 1. **Clear UI State**
- Users always know which view is active
- No ambiguity

### 2. **Single Responsibility**
- Each view has exclusive control when active
- No overlapping editors

### 3. **Predictable Behavior**
- Clicking a tab always produces expected result
- Consistent across all views

### 4. **Better UX**
- Professional, polished feel
- Matches standard tab behavior

---

## Summary

**Problem:** Multiple tabs active simultaneously

**Solution:** Mutual exclusion - only one tab can be active

**Implementation:** Each toggle function unchecks all others

**Result:** Clean, professional tab switching! ✨

---

**Users can now navigate between views with confidence, knowing exactly which editor is active!** 🎯✅
