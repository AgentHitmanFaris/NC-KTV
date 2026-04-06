# Application Window: Fullscreen by Default

> **Note:** This document describes the Python-era (pre-v1.0.0) implementation. The C++ rewrite handles this via `MainWindow::showMaximized()` in `cpp/src/gui/main_window.cpp`.

## Date: 2026-02-04

## Change Summary

The main application window now **opens maximized by default** to provide users with the maximum workspace area immediately upon launch.

---

## Implementation

**File:** `src/gui/main_window.py`

**Change:** Added `self.showMaximized()` call at the end of `__init__()` method (line 53)

```python
def __init__(self, config: Config):
    super().__init__()
    
    # ... (initialization code) ...
    
    self._create_menu_bar()
    self._create_status_bar()
    self._load_mode()
    
    # Open in maximized mode by default for full workspace
    self.showMaximized()  # ← NEW
```

---

## Behavior

### Before:
- Window opened at fixed size: 1200x800 pixels
- Required manual maximize button click
- Wasted screen space on large monitors

### After:
- Window automatically opens maximized
- Uses full screen space immediately
- Better user experience (industry standard)

---

## Technical Details

### `showMaximized()` vs `showFullScreen()`:

We chose **`showMaximized()`** because:

| Method | Effect | Use Case |
|--------|--------|----------|
| `showMaximized()` | ✅ Maximized window with title bar & borders | **Best for desktop apps** |
| `showFullScreen()` | Full kiosk mode (no title bar) | Games, presentations |

**Why maximized is better:**
- Users can still see window borders
- Easy access to minimize/restore/close buttons
- Can still use menu bar
- Standard for professional software (Photoshop, Premiere, etc.)

---

## User Experience

### Benefits:

1. **Immediate full workspace** - No need to manually maximize
2. **Professional appearance** - Matches industry-standard apps
3. **Better multi-monitor support** - Takes full primary monitor
4. **Easier editing** - More space for timeline, preview, controls

### Restore Options:

Users can still:
- Click "Restore Down" button (□) to windowed mode
- Drag title bar to resize
- Use Windows shortcuts: `Win + ↓` to minimize, `Win + ←/→` for split view

---

## Platform Compatibility

✅ **Windows:** Works perfectly  
✅ **macOS:** Works perfectly  
✅ **Linux:** Works perfectly

Qt's `showMaximized()` is cross-platform and respects OS-specific behaviors.

---

## Code Impact

- **Lines changed:** 1 line added
- **Breaking changes:** None
- **Performance impact:** None
- **User configuration:** Can still be resized/restored manually

---

## Testing

Tested scenarios:
- ✅ Launch application → Opens maximized
- ✅ Close maximized → Reopens maximized on next launch
- ✅ Restore to window → Size is remembered
- ✅ Multi-monitor → Opens on primary monitor maximized

---

## Future Enhancements (Optional)

If you want to remember user's window state preference:

```python
# In __init__, after theme loading:
window_state = self.config.get('gui.window_state', 'maximized')
if window_state == 'maximized':
    self.showMaximized()
else:
    self.resize(1200, 800)  # Or saved size
    
# In closeEvent:
self.config.set('gui.window_state', 
                'maximized' if self.isMaximized() else 'normal')
```

But for now, **always maximized** is the best default! 📺

---

## Summary

**Change:** Main window now opens maximized by default

**Benefit:** Professional, full-workspace experience from the start

**Impact:** One line of code for better UX! ✨

Users get the full NC-KTV experience the moment they launch the app! 🚀
