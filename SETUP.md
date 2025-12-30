# NC-KTV Setup Guide

Quick setup guide for NC-KTV development environment using Python 3.10.11 embedded.

## Prerequisites

1. **FFmpeg** - Download from https://ffmpeg.org/download.html
   - Add to system PATH
   - Verify: `ffmpeg -version`

2. **Python 3.10.11 Embedded** - Already downloaded
   - Extract to `python_embed/` directory

3. **UVR Models** - Download UVR models
   - Download from: https://github.com/TRvlvr/model_repo/releases
   - Recommended: `UVR_MDXNET_KARA_2.onnx`
   - Place in `models/` directory

## Setup Steps

### 1. Extract Python Embedded

```powershell
# Extract to project root
python_embed/
```

### 2. Enable pip

Edit `python_embed/python310._pth`:
```
python310.zip
.
import site  # <- UNCOMMENT THIS LINE
```

Download and install pip:
```powershell
cd python_embed
Invoke-WebRequest -Uri "https://bootstrap.pypa.io/get-pip.py" -OutFile "get-pip.py"
.\python.exe get-pip.py
```

### 3. Install Dependencies

```powershell
# Install PyTorch with CUDA 11.8 (for GTX 1060)
.\python.exe -m pip install torch==2.1.0+cu118 torchaudio==2.1.0+cu118 --index-url https://download.pytorch.org/whl/cu118

# Install project dependencies
.\python.exe -m pip install -r requirements.txt
```

### 4. Verify Installation

```powershell
# Test CUDA
.\python.exe -c "import torch; print('CUDA:', torch.cuda.is_available())"

# Test audio-separator
.\python.exe -c "import audio_separator; print('audio-separator OK')"

# Test PyQt6
.\python.exe -c "import PyQt6; print('PyQt6 OK')"
```

### 5. Download UVR Models

Place model files in `models/` directory:
- `UVR_MDXNET_KARA_2.onnx` (recommended)
- `UVR-MDX-NET-Inst_HQ_3.onnx`

### 6. Run Application

```powershell
cd ..
python_embed\python.exe main.py
```

## Troubleshooting

**Issue: CUDA not detected**
- Ensure NVIDIA drivers are updated
- Verify CUDA installation with PyTorch test

**Issue: FFmpeg not found**
- Add FFmpeg bin folder to system PATH
- Restart terminal after PATH change

**Issue: Module not found**
- Verify all dependencies installed successfully
- Check `python310._pth` has `import site` uncommented

## Development Workflow

1. Edit code in `src/`
2. Test with: `python_embed\python.exe main.py`
3. Commit changes: `git add . && git commit -m "message"`
4. Push to Stable: `git push`

## Next Steps

- Download test audio/video files
- Test vocal removal with UVR models
- Proceed to Phase 3 (Lyrics Processing)
