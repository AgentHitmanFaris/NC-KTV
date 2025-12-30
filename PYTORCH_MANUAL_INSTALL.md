# Manual PyTorch Installation Guide

## Download PyTorch Wheels

Download these two files (save to `NC-KTV` folder):

1. **torch-2.1.0+cu118-cp310-cp310-win_amd64.whl** (2.7 GB)
   - URL: https://download.pytorch.org/whl/cu118/torch-2.1.0%2Bcu118-cp310-cp310-win_amd64.whl

2. **torchaudio-2.1.0+cu118-cp310-cp310-win_amd64.whl** (3.6 MB)
   - URL: https://download.pytorch.org/whl/cu118/torchaudio-2.1.0%2Bcu118-cp310-cp310-win_amd64.whl

## Installation Steps

### Option 1: Use Helper Script (Recommended)

1. Download the .whl files to the `NC-KTV` folder
2. Run:
   ```powershell
   .\install_torch_manual.ps1
   ```

### Option 2: Manual Command

```powershell
python_embed\python.exe -m pip install torch-2.1.0+cu118-cp310-cp310-win_amd64.whl
python_embed\python.exe -m pip install torchaudio-2.1.0+cu118-cp310-cp310-win_amd64.whl
```

## After PyTorch Installation

Run the setup script again (it will skip PyTorch since it's installed):
```powershell
.\setup_python.ps1
```

This will install the remaining dependencies:
- audio-separator
- PyQt6
- librosa
- openai-whisper
- etc.

## Verify Installation

```powershell
python_embed\python.exe -c "import torch; print('PyTorch:', torch.__version__); print('CUDA:', torch.cuda.is_available())"
```

Expected output:
```
PyTorch: 2.1.0+cu118
CUDA: True
```
