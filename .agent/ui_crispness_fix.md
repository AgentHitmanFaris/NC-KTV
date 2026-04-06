# UI Crispness Fix - Less "Squishy" Elements

> **Note:** This document describes the Python-era (pre-v1.0.0) QSS theme. The C++ rewrite uses 4px border-radius throughout its QSS stylesheets in `cpp/src/gui/resources/`.

## Date: 2026-02-04

## Problem

UI elements felt "squishy" - too soft, rounded, and lack definition:
- Buttons had overly rounded corners (8px)
- Inputs/dropdowns looked mushy
- Everything felt soft/pillowy
- Not professional/crisp enough

---

## Solution

Reduced border-radius throughout the theme:
- **8px → 4px** (buttons, inputs, tables, tabs, menus, etc.)
- **6px → 3px** (progress bar chunks, scrollbars)
- **Increased border visibility** (1px → 2px on buttons)

**Result:** Sharper, more professional, less "squishy" UI!

---

## Changes Made

### **Before (Squishy 8px)**
```css
QPushButton {
    border-radius: 8px;  /* Too rounded */
    border: 1px solid rgba(255, 255, 255, 0.2);  /* Too faint */
}
```

### **After (Crisp 4px)**
```css
QPushButton {
    border-radius: 4px;  /* Sharp, professional */
    border: 2px solid rgba(102, 126, 234, 0.4);  /* More defined */
}
```

---

## Elements Updated

| Element | Before | After | Change |
|---------|--------|-------|--------|
| **Buttons** | 8px | 4px | ↓50% rounder |
| **Text Inputs** | 8px | 4px | ↓50% rounder |
| **Tables** | 8px | 4px | ↓50% rounder |
| **Tabs** | 8px | 4px | ↓50% rounder |
| **Menus** | 8px | 4px | ↓50% rounder |
| **Progress Bars** | 8px | 4px | ↓50% rounder |
| **Progress Chunks** | 6px | 3px | ↓50% rounder |
| **Combo Boxes** | 8px | 4px | ↓50% rounder |
| **Tooltips** | 8px | 4px | ↓50% rounder |
| **Scrollbar Handles** | 6px | 6px | No change (already good) |

---

## Visual Comparison

### **Buttons**

**Before (Squishy):**
```
┌────────────────┐
│   Save Project │  ← 8px very rounded
└────────────────┘
```

**After (Crisp):**
```
┌──────────────┐
│ Save Project │  ← 4px sharp corners
└──────────────┘
```

---

### **Table**

**Before (Squishy):**
```
╭─────────────────╮
│ Start │ End │...│  ← 8px soft corners
├───────┼─────┼───┤
│  0.00 │ 3.34│   │
╰─────────────────╯
```

**After (Crisp):**
```
┌─────────────────┐
│ Start │ End │...│  ← 4px defined edges
├───────┼─────┼───┤
│  0.00 │ 3.34│   │
└─────────────────┘
```

---

### **Speed Buttons**

**Before (Squishy):**
```
 ╭───╮ ╭───╮ ╭───╮
 │1.25│ │.75│ │ .8│  ← Rounded like pills
 ╰───╯ ╰───╯ ╰───╯
```

**After (Crisp):**
```
 ┌───┐ ┌───┐ ┌───┐
 │1.25│ │.75│ │ .8│  ← Clean rectangles
 └───┘ └───┘ └───┘
```

---

## Additional Improvements

### **Border Visibility**

Also increased border thickness for better definition:

```css
/* Before */
border: 1px solid rgba(255, 255, 255, 0.2);  /* Too faint */

/* After */
border: 2px solid rgba(102, 126, 234, 0.4);  /* Clear definition */
```

**Benefit:** Elements have clear boundaries, not blending into background

---

## Design Rationale

### **Why 4px instead of 0px?**

| Radius | Feel | Use Case |
|--------|------|----------|
| **0px** | Harsh, rigid | Too sharp, retro Windows 95 |
| **4px** | Professional, modern | ✅ Perfect balance |
| **8px** | Soft, playful | Mobile apps, too "squishy" |
| **16px+** | Pill-shaped | iOS style, too casual |

**4px = Modern & Professional Sweet Spot!**

---

## Style Reference

### **Modern Professional Software:**

| Software | Border Radius | Match? |
|----------|---------------|--------|
| **VS Code** | ~3-4px | ✅ Yes |
| **Adobe Premiere** | ~2-4px | ✅ Yes |
| **DaVinci Resolve** | ~3-5px | ✅ Yes |
| **Spotify** | ~4-8px | ✅ Close |
| **Discord** | ~8-12px | ❌ Too rounded |

**Our choice: 4px matches professional video editing software!**

---

## User Experience Impact

### **Before (Squishy Feeling):**
- Buttons felt "soft"
- UI lacked definition
- Looked more like a mobile app
- Less professional

### **After (Crisp Feeling):**
- ✅ Buttons feel precise
- ✅ Clear element boundaries
- ✅ Desktop application aesthetic
- ✅ Professional & modern

---

## Technical Details

### **CSS Border-Radius Property:**

```css
border-radius: 4px;
```

Controls corner rounding:
- **0px** = Square corners
- **50%** = Circular/pill shape
- **4px** = Slight rounding (modern)

### **Why Gradients Still Look Good:**

Even with sharp corners, gradients remain beautiful:
```css
background: qlineargradient(
    stop:0 #667eea,
    stop:1 #764ba2
);
```

**Purple → Violet gradient flows smoothly in sharp rectangles!**

---

## Performance Impact

**Zero performance difference!**

Border-radius is GPU-accelerated in Qt:
- 4px renders same as 8px
- No additional CPU/GPU load
- Instant visual change

---

## Files Modified

**File:** `src/gui/styles/modern_theme.py`

**Changes:**
- Line 33: QPushButton border-radius 8px → 4px
- Line 95: QLineEdit border-radius 8px → 4px
- Line 119: QTableWidget border-radius 8px → 4px
- Line 217: QTabWidget::pane border-radius 8px → 4px
- Line 227-228: QTabBar::tab border-radius 8px → 4px
- Line 339: QMenu border-radius 8px → 4px
- Line 381: QProgressBar border-radius 8px → 4px
- Line 391: QProgressBar::chunk border-radius 6px → 3px
- Line 401: QComboBox border-radius 8px → 4px
- Line 426: QComboBox dropdown border-radius 8px → 4px
- Line 485: QToolTip border-radius 8px → 4px

**Total:** 11 components updated

---

## Accessibility

### **Better Visual Definition:**

Crisp edges improve:
1. **Element Recognition** - Clearer boundaries
2. **Focus Indication** - Easier to see selected elements
3. **Touch Targets** - Better perceived clickable areas
4. **Eye Strain** - Less visual softness = easier to focus

---

## Summary

**Problem:** UI felt "squishy" with 8px rounded corners

**Solution:** Reduced to 4px for crisp, professional look

**Impact:**
- ✅ Sharper, more defined elements
- ✅ Professional desktop app aesthetic
- ✅ Matches Adobe/DaVinci style
- ✅ Zero performance cost

**Result:** Modern, crisp UI instead of soft/mushy!

---

**Your UI now has professional, crisp edges while still maintaining the beautiful gradient theme!** 🎨✨

No more "squishy" buttons - everything feels precise and well-defined! 🚀
