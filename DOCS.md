# NC-KTV Documentation

This document contains setup instructions, development guidelines, and troubleshooting information.

---

## Quick Setup

### Prerequisites
- Windows 10/11 (64-bit)
- FFmpeg in system PATH (`ffmpeg -version` to verify)
- Python 3.10.11 embedded (included in `python_embed/`)
- UVR models in `models/` folder

### Installation

1. **Enable pip** - Edit `python_embed/python310._pth`, uncomment `import site`

2. **Install dependencies**:
   ```powershell
   # PyTorch with CUDA 11.8
   .\python_embed\python.exe -m pip install torch==2.1.0+cu118 torchaudio==2.1.0+cu118 --index-url https://download.pytorch.org/whl/cu118
   
   # Other dependencies
   .\python_embed\python.exe -m pip install -r requirements.txt
   ```

3. **Run application**:
   ```powershell
   .\python_embed\python.exe main.py
   ```

---

## Manual PyTorch Installation (Offline)

If online installation fails, download these wheels manually:

| Package | Size | URL |
|---------|------|-----|
| torch-2.1.0+cu118 | 2.7 GB | https://download.pytorch.org/whl/cu118/torch-2.1.0%2Bcu118-cp310-cp310-win_amd64.whl |
| torchaudio-2.1.0+cu118 | 3.6 MB | https://download.pytorch.org/whl/cu118/torchaudio-2.1.0%2Bcu118-cp310-cp310-win_amd64.whl |

Install with:
```powershell
.\python_embed\python.exe -m pip install torch-2.1.0+cu118-cp310-cp310-win_amd64.whl
.\python_embed\python.exe -m pip install torchaudio-2.1.0+cu118-cp310-cp310-win_amd64.whl
```

---

## Manual Whisper Model Installation

For offline AI transcription, download a model:

| Size | Parameters | English-only | Multilingual | VRAM | Speed | Download |
|:----:|:----------:|:------------:|:------------:|:----:|:-----:|:--------:|
| tiny | ~39 M | [tiny.en](https://openaipublic.azureedge.net/main/whisper/models/d3dd57d32accea0b295c96e26691aa14d8822fac7d9d27d5dc00b4ca2826dd03/tiny.en.pt) | [tiny](https://openaipublic.azureedge.net/main/whisper/models/65147644a518d12f04e32d6f3b26facc3f8dd46e5390956a9424a650c0ce22b9/tiny.pt) | ~1 GB | ~10x | ✓ |
| base | ~74 M | [base.en](https://openaipublic.azureedge.net/main/whisper/models/25a8566e1d0c1e2231d1c762132cd20e0f96a85d16145c3a00adf5d1ac670ead/base.en.pt) | [base](https://openaipublic.azureedge.net/main/whisper/models/ed3a0b6b1c0edf879ad9b11b1af5a0e6ab5db9205f891f668f8b0e6c6326e34e/base.pt) | ~1 GB | ~7x | ✓ |
| **small** | ~244 M | [small.en](https://openaipublic.azureedge.net/main/whisper/models/f953ad0fd29cacd07d5a9eda5624af0f6bcf2258be67c92b79389873d91e0872/small.en.pt) | [small](https://openaipublic.azureedge.net/main/whisper/models/9ecf779972d90ba49c06d968637d720dd632c55bbf19d441fb42bf17a411e794/small.pt) | ~2 GB | ~4x | ✓ |
| medium | ~769 M | [medium.en](https://openaipublic.azureedge.net/main/whisper/models/d7440d1dc186f76616474e0ff0b3b6b879abc9d1a4926b7adfa41db2d497ab4f/medium.en.pt) | [medium](https://openaipublic.azureedge.net/main/whisper/models/345ae4da62f9b3d59415adc60127b97c714f32e89e936602e85993674d08dcb1/medium.pt) | ~5 GB | ~2x | ✓ |
| large-v1 | ~1.6GB | N/A | [large-v1](https://openaipublic.azureedge.net/main/whisper/models/e4b87e7e0bf463eb8e6956e646f1e277e901512310def2c24bf0e11bd3c28e9a/large.pt) | ~10 GB | 1x | ✓ |
| large-v2 | ~2.9GB | N/A | [large-v2](https://openaipublic.azureedge.net/main/whisper/models/81f7c96c852ee8fc832187b0132e569d6c3065a3252ed18e56effd0b6a73e524/large-v2.pt) | ~10 GB | 1x | ✓ |
| large-v3 | ~1.6GB | N/A | [large-v3-turbo](https://openaipublic.azureedge.net/main/whisper/models/aff26ae408abcba5fbf8813c21e62b0941638c5f6eebfb145be0c9839262a19a/large-v3-turbo.pt) | ~10 GB | 1x | ✓ |

**Recommended:** `small` or `small.en`

Place in: `models/whisper/small.pt`

---

## Troubleshooting

| Issue | Solution |
|-------|----------|
| CUDA not detected | Update NVIDIA drivers, verify with `python -c "import torch; print(torch.cuda.is_available())"` |
| FFmpeg not found | Add FFmpeg bin folder to PATH, restart terminal |
| Module not found | Check `python310._pth` has `import site` uncommented |

---

## Contributing

### Commit Format
```
<type>(<scope>): <subject>
```
Types: `feat`, `fix`, `docs`, `style`, `refactor`, `perf`, `test`, `chore`

### Code Style
- Follow PEP 8
- Use type hints
- 4-space indentation
- Docstrings for public functions

### Pull Request Checklist
- [ ] Code follows style guidelines
- [ ] Tests pass
- [ ] Documentation updated
- [ ] No merge conflicts

---

## License

NC-KTV is licensed under the MIT License. See [LICENSE](LICENSE) for details.
