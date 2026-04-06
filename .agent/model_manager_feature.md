# Vocal Removal Model Manager Feature

> **Note:** This document describes the Python-era (pre-v1.0.0) implementation. The C++ equivalent is `ModelManagerDialog` in `cpp/src/gui/dialogs/model_manager_dialog.cpp`.

## Date: 2026-02-04

## Overview

Added a comprehensive **Vocal Removal Model Manager** that allows users to browse, download, and select from **7 different UVR (Ultimate Vocal Remover) models** instead of being limited to 4 presets.

---

## Feature Highlights

### 🎨 **Model Manager Dialog**

Beautiful, professional interface for managing vocal removal models:

| Feature | Description |
|---------|-------------|
| **7 Models** | Choose from 7 different UVR models |
| **Model Info** | View quality, speed, size, and description |
| **Download** | Download models directly from GitHub |
| **Filtering** | Filter by status (Installed/Not Installed) or type (MDX-Net/VR) |
| **Model Management** | Delete unused models to free space |
| **Live Status** | See which models are installed (✅) vs not (📦) |

---

## Available Models

### 1. **UVR MDX-Net KARA 2** (Default)
```
Type: MDX-Net
Quality: ★★★★☆
Speed: ★★★★☆
Size: ~129 MB
Description: Balanced karaoke model. Good for most songs.
```

### 2. **HP Karaoke (Fast)**
```
Type: VR Architecture
Quality: ★★★☆☆
Speed: ★★★★★
Size: ~81 MB
Description: High performance, fast processing. Great for quick results.
```

### 3. **HP Karaoke Aggressive**
```
Type: VR Architecture
Quality: ★★★★★
Speed: ★★★★☆
Size: ~81 MB
Description: Aggressive vocal removal. Cleaner instrumentals.
```

### 4. **MDX-Net Instrumental HQ**
```
Type: MDX-Net
Quality: ★★★★★
Speed: ★★★☆☆
Size: ~129 MB
Description: Highest quality instrumentals. Slower but best results.
```

### 5. **MDX-Net Vocals (Fine-Tuned)**
```
Type: MDX-Net
Quality: ★★★★☆
Speed: ★★★☆☆
Size: ~129 MB
Description: Optimized for extracting clean vocals.
```

### 6. **Kim Vocal 2**
```
Type: MDX-Net
Quality: ★★★★☆
Speed: ★★★★☆
Size: ~129 MB
Description: Community favorite for vocal extraction.
```

### 7. **Reverb HQ**
```
Type: MDX-Net
Quality: ★★★★☆
Speed: ★★★☆☆
Size: ~129 MB
Description: Removes reverb from vocals/instrumentals.
```

---

## How to Use

### **Method 1: Quick Selection (Presets)**

1. Open "New Project" dialog
2. Select from 4 preset options in dropdown:
   - Standard Karaoke (Balanced)
   - High Performance (Fast)
   - Aggressive Removal (Cleaner)
   - Instrumental Only (High Quality)

### **Method 2: Advanced Model Manager**

1. Open "New Project" dialog
2. Click **"🎛️ Advanced Model Selection..."** button
3. **Model Manager** opens showing all 7 models
4. **Browse** available models (installed shown with ✅)
5. **Filter** by type or installation status
6. **Select a model** to view its details:
   - Type (MDX-Net or VR Architecture)
   - Quality rating (★★★★★)
   - Speed rating (★★★★★)
   - File size (~81-129 MB)
   - Description
7. **Download** if not installed:
   - Click "📥 Download Model"
   - Confirm download
   - Watch progress bar
   - Model installs automatically
8. **Select** and click "✅ Select Model"
9. Model is now selected for project!

---

## Model Manager Features

### **1. Model Browsing**

```
Available Models List:
┌──────────────────────────────────┐
│ ✅ UVR MDX-Net KARA 2 (Current) │
│ ✅ HP Karaoke (Fast)            │
│ ✅ HP Karaoke Aggressive         │
│ 📦 MDX-Net Instrumental HQ      │
│ 📦 MDX-Net Vocals (Fine-Tuned)  │
│ 📦 Kim Vocal 2                   │
│ 📦 Reverb HQ                     │
└──────────────────────────────────┘
```

- ✅ = Installed and ready
- 📦 = Available for download
- **(Current)** = Currently selected

---

### **2. Filtering**

Filter dropdown options:
- **All Models** - Show everything
- **Installed Only** - Show only downloaded models
- **Not Installed** - Show only models to download
- **MDX-Net** - Show only MDX-Net architecture
- **VR Architecture** - Show only VR models

**Use case:** Quickly find installed models or browse specific types.

---

### **3. Model Information Panel**

When you select a model, see detailed info:

```
━━━━━━━━━━━━━━━━━━━━━━━━━━
UVR MDX-Net KARA 2 (Current)
━━━━━━━━━━━━━━━━━━━━━━━━━━

Type: MDX-Net

Quality: ★★★★☆
Speed: ★★★★☆

Size: ~129 MB

Status: ✅ Installed

Description:
A highly optimized vocal removal
model using MDX-Net architecture,
delivering excellent separation
quality with moderate processing
speed. Ideal for most karaoke tasks.
```

**Helps you:** Make informed decisions about which model to use!

---

### **4. Model Download**

Download any model with one click:

1. Select model (shows 📦 icon)
2. Click "📥 Download Model" button
3. Confirm download dialog:
   ```
   Download UVR MDX-Net KARA 2?
   Size: ~129 MB
   [Yes] [No]
   ```
4. Progress dialog appears:
   ```
   Downloading UVR_MDXNET_KARA_2.onnx...
   [████████████████░░░░] 75%
   [Cancel]
   ```
5. Success notification!
6. Model icon changes to ✅

**Download source:** Official UVR GitHub repository (TRvlvr/model_repo)

---

### **5. Model Deletion**

Free up disk space by deleting unused models:

1. Select installed model (✅ icon)
2. Click "🗑️ Delete Model"
3. Confirm deletion:
   ```
   Delete HP Karaoke (Fast)?
   This will free up ~81 MB.
   [Yes] [No]
   ```
4. Model removed!
5. Icon changes to 📦

**Note:** Cannot delete currently selected model (safety feature).

---

## Technical Details

### **Model Types**

#### **MDX-Net (Multi-Domain Neural Network)**
- Uses hybrid time-frequency domain processing
- Higher quality, slower processing
- Best for final production
- Size: ~129 MB

#### **VR Architecture (Vocal Remover)**
- Lightweight neural network
- Faster processing, good quality
- Great for quick previews
- Size: ~81 MB

---

### **Quality vs Speed Trade-off**

| Speed | Quality | Model | Use Case |
|-------|---------|-------|----------|
| ★★★★★ | ★★★☆☆ | HP Karaoke (Fast) | Quick preview |
| ★★★★☆ | ★★★★☆ | UVR MDX-Net KARA 2 | Balanced |
| ★★★★☆ | ★★★★★ | HP Karaoke Aggressive | Clean instrumentals |
| ★★★☆☆ | ★★★★★ | MDX-Net Inst HQ | Best quality |

**Choose based on:**
- **Need speed?** → HP Karaoke (Fast)
- **Need quality?** → MDX-Net Inst HQ
- **Balanced?** → UVR MDX-Net KARA 2

---

### **Download System**

Models are downloaded from:
```
https://github.com/TRvlvr/model_repo/releases/download/all_public_uvr_models/
```

**Features:**
- Progress indication
- Resume capability (chunk-based download)
- Error handling with retries
- Automatic installation

**Storage:** Models saved to `models/` directory

---

## UI Integration

### **New Project Dialog**

**Before:**
```
Karaoke Mode:
┌────────────────────────────────┐
│ Standard Karaoke (Balanced) ▼ │
└────────────────────────────────┘
```

**After:**
```
Karaoke Mode:
┌────────────────────────────────┐
│ Standard Karaoke (Balanced) ▼ │
└────────────────────────────────┘
┌────────────────────────────────┐
│  🎛️ Advanced Model Selection... │
└────────────────────────────────┘
```

**New button opens:** Full Model Manager dialog!

---

### **Custom Model Selection**

When you select a model from Manager:

1. If it's a preset → Dropdown updates automatically
2. If it's custom → New entry added:
   ```
   ┌────────────────────────────────┐
   │ 📦 Kim Vocal 2 (Custom) ▼     │
   └────────────────────────────────┘
   ```

**Result:** Any of the 7 models can be used!

---

## Benefits

### **1. Flexibility**
- 7 models instead of 4 presets
- Choose optimal model for each song
- Download only what you need

### **2. Quality Control**
- See quality ratings before choosing
- Read descriptions to understand differences
- Make informed decisions

### **3. Space Management**
- Download models on-demand
- Delete unused models
- Save disk space (~81-129 MB per model)

### **4. Professional Workflow**
- Browse like in professional audio software
- Filter by needs
- Manage library efficiently

---

## Use Cases

### **Scenario 1: Quick Karaoke**
```
Need: Fast instrumental for practice
Solution: Download "HP Karaoke (Fast)"
Speed: ★★★★★
Result: Quick separation, good enough quality
```

### **Scenario 2: Professional Production**
```
Need: Highest quality instrumental for album
Solution: Download "MDX-Net Inst HQ"
Quality: ★★★★★
Result: Best possible separation quality
```

### **Scenario 3: Vocal Extraction**
```
Need: Clean a cappella for remix
Solution: Download "Kim Vocal 2"
Quality: ★★★★☆
Result: Clean vocals with minimal artifacts
```

### **Scenario 4: Reverb Removal**
```
Need: Remove echo from live recording
Solution: Download "Reverb HQ"
Quality: ★★★★☆
Result: Dry vocals/instrumentals
```

---

## Files Created/Modified

### **New File:**
- **`src/gui/dialogs/model_manager_dialog.py`** (400+ lines)
  - VocalRemovalModelDialog class
  - ModelDownloadWorker thread
  - Model database with 7 models
  - Full download/delete/select logic

### **Modified:**
- **`src/gui/dialogs/new_project_dialog.py`**
  - Added "Advanced Model Selection" button
  - Added `_open_model_manager()` method
  - Updated `_create_project()` to handle custom models

---

## Future Enhancements

### **Possible Additions:**

1. **Model Testing**
   - Preview separation on 30-second clip
   - Compare models side-by-side

2. **Custom Model Import**
   - Add your own .pth/.onnx files
   - Community model sharing

3. **Model Recommendations**
   - AI suggests best model for song
   - Based on genre, vocal complexity

4. **Batch Processing**
   - Apply different models to same song
   - Compare results automatically

---

## Summary

**New Feature:** Vocal Removal Model Manager

**Models Available:** 7 (up from 4 presets)

**Features:** Browse, Download, Filter, Delete, Select

**UI:** Professional model management dialog

**Benefits:** Flexibility, quality control, space management

---

**Users now have professional-grade control over vocal removal models, just like in $500+ audio software!** 🎵✨

They can choose the perfect model for each song, download on-demand, and manage their model library efficiently! 🚀💎
