# Syllable Editor - Mouse Interaction Guide

> **Note:** This document describes the Python-era (pre-v1.0.0) interaction model. The C++ `SyllableEditor` in `cpp/src/gui/components/syllable_editor.cpp` implements the same Ctrl+drag / click-to-seek paradigm natively.

## New Control Scheme (Fixed!)

### 🎯 **Seeking / Playback Control (Default Mode)**

**How to use:**
- **Click anywhere** on the timeline → Playhead jumps to that position
- **Click on a syllable** → Playhead jumps to that syllable's start time
- **Mouse cursor:** 👆 Pointing hand when hovering over syllables

**Purpose:** Quick navigation and playback control

---

### ✏️ **Editing Mode (Ctrl + Click)**

**How to use:**
- **Hold Ctrl** while hovering over syllables
- **Ctrl + Click and drag** on syllable center → Move the syllable
- **Ctrl + Click and drag** on syllable left edge → Adjust start time
- **Ctrl + Click and drag** on syllable right edge → Adjust end time

**Mouse cursors when Ctrl is held:**
- ↔️ Horizontal resize cursor at edges
- ✥ Move cursor in the middle

**Purpose:** Fine-tuning syllable timing

---

## Quick Reference

| Action | Effect |
|--------|--------|
| **Click anywhere** | Seek playhead to that time |
| **Click on syllable** | Seek to syllable start |
| **Ctrl + Click syllable center** | Start moving syllable |
| **Ctrl + Click syllable left edge** | Adjust syllable start time |
| **Ctrl + Click syllable right edge** | Adjust syllable end time |

---

## Visual Feedback

### Cursor Changes:
1. **Arrow** → Default (empty space)
2. **Pointing Hand** 👆 → Hovering over syllable (click to seek)
3. **Horizontal Resize** ↔️ → Ctrl held, hovering over edge (drag to adjust timing)
4. **Move** ✥ → Ctrl held, hovering over center (drag to move)

### Playhead Behavior:
- **Click anywhere** → Playhead immediately jumps
- **Music starts playing** from the new position
- **Auto-scroll** follows the playhead

---

## Examples

### Example 1: Jump to a specific word
```
1. Locate the word "love" in the syllable editor
2. Click on the "love" syllable block
3. ✅ Playhead jumps to "love", music starts from there
```

### Example 2: Adjust timing of a word
```
1. Find the word that's out of sync
2. Hold Ctrl key
3. Click and drag the syllable left/right to adjust timing
4. Release mouse to apply changes
5. ✅ Timing is adjusted
```

### Example 3: Scrub through the song
```
1. Click on different positions in the timeline
2. Playhead follows your clicks
3. ✅ Quick navigation through the song
```

---

## What Changed?

### Before (Broken):
- ❌ Clicking on syllables didn't seek playhead
- ❌ No way to manually adjust playhead position
- ❌ Confusing interaction model

### After (Fixed):
- ✅ Click anywhere to seek (including on syllables)
- ✅ Ctrl+Click to edit syllable timing
- ✅ Clear visual feedback with cursor changes
- ✅ Intuitive: click = seek, Ctrl+drag = edit

---

## Tips

1. **Quick Playback Testing:**
   - Click on different syllables to quickly test their timing
   - No need to hold Ctrl if you just want to navigate

2. **Precise Editing:**
   - Use Ctrl+drag for fine-tuning
   - Zoom in for pixel-perfect adjustments
   - Hold Ctrl to see which edges are draggable

3. **Workflow:**
   - First pass: Click syllables to test timing
   - Second pass: Ctrl+drag to fix any issues
   - Final check: Click through again to verify

---

## Keyboard Shortcuts

| Key | Function |
|-----|----------|
| **Ctrl** | Enable drag/edit mode while held |
| (more shortcuts can be added) |

---

## Technical Notes

- Seeking is handled by `seek_requested` signal
- Playhead position updates immediately on click
- Throttling ensures smooth performance
- Dirty region updates minimize redraws

---

## Troubleshooting

**Q: Playhead doesn't move when I click?**
- Check if `seek_requested` signal is connected to audio player
- Verify the audio player is responding to seek requests

**Q: Can't drag syllables?**
- Make sure you're holding Ctrl key
- Check if cursor changes to resize/move icons

**Q: Clicking feels laggy?**
- This should be fixed with latest performance optimizations
- Check CPU usage (should be low)

---

## Summary

The syllable editor now has a **dual-mode** interaction model:

1. **Default mode:** Click to seek (navigation)
2. **Ctrl mode:** Click+drag to edit (timing adjustment)

This makes it both easy to navigate AND powerful for editing! 🎵✨
