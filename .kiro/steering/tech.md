# Tech Stack

## Language & Standard
- C++20 (primary application)
- Python (AI bridge only — `python_bridge.py`)

## GUI Framework
- Qt 6.10.2 (MinGW build) — Widgets, Multimedia, MultimediaWidgets, Network
- Installed at: `D:\ProgramData\Qt\6.10.2\mingw_64`
- AUTOMOC/AUTORCC/AUTOUIC enabled (after FetchContent to avoid contaminating third-party libs)

## Build System
- CMake 3.25+ with Ninja generator
- Presets defined in `cpp/CMakePresets.json`
  - `windows-release` — MinGW Release
  - `windows-debug` — MinGW Debug
- Toolchain: MinGW 13.1 (`D:\ProgramData\Qt\Tools\mingw1310_64`)
- Qt prefix: `D:\ProgramData\Qt\6.10.2\mingw_64`
- CMake binary: `D:\ProgramData\Qt\Tools\CMake_64\bin\cmake.exe`
- Ninja binary: `D:\ProgramData\Qt\Tools\Ninja\ninja.exe`

## Key Libraries (via FetchContent)
| Library | Version | Purpose |
|---|---|---|
| nlohmann/json | v3.11.3 | JSON parsing |
| ONNX Runtime | 1.18.0 | GPU inference (UVR/MDX-Net) |
| zstd | v1.5.5 | Project file compression |
| LibreSSL | v3.9.1 | AES-256-GCM encryption for `.nctv` files |
| GoogleTest | v1.15.2 | Unit + GUI tests |

## Optional Dependencies
- Vulkan SDK — hardware rendering backend (`NCKTV_HAS_VULKAN`)
- OpenSSL / LibreSSL — encrypted project files (`NCKTV_HAS_OPENSSL`)
- ICU — romanization support (`NCKTV_HAS_ICU`)

## Python Bridge
- Entry point: `python_bridge.py` (root)
- Packaged with PyInstaller `--onedir` into `build_pyinstaller/python_bridge/`
- Key Python libs: `faster-whisper`, `audio-separator`, `pydub`, `PyInstaller`
- Embedded Python runtime: `python_embed/`

## Testing
- Framework: GoogleTest
- Two test executables:
  - `ncktv_tests` — core library unit tests (no Qt display needed)
  - `ncktv_gui_tests` — widget/UI tests (runs with `QT_QPA_PLATFORM=offscreen`)

## Common Commands

### Configure (from `cpp/` directory)
```powershell
D:\ProgramData\Qt\Tools\CMake_64\bin\cmake.exe --preset windows-release   # Release
D:\ProgramData\Qt\Tools\CMake_64\bin\cmake.exe --preset windows-debug     # Debug
```

### Build (from `cpp/` directory)
```powershell
D:\ProgramData\Qt\Tools\CMake_64\bin\cmake.exe --build --preset windows-release --parallel
D:\ProgramData\Qt\Tools\CMake_64\bin\cmake.exe --build --preset windows-debug --parallel
```

### Run Tests (from `cpp/` directory)
```powershell
D:\ProgramData\Qt\Tools\CMake_64\bin\ctest.exe --preset windows-release
D:\ProgramData\Qt\Tools\CMake_64\bin\ctest.exe --preset windows-debug
```

### Full Portable Build (from repo root)
```powershell
.\build_portable_release.ps1
```
Output: `ncktv.exe` + all runtime dependencies assembled in `NC-KTV-Portable\`.

## Output Paths
- Release binary: `cpp/out/build/windows-release/src/gui/ncktv.exe`
- Debug binary: `cpp/out/build/windows-debug/src/gui/ncktv.exe`
- Python bridge output: `build_pyinstaller/python_bridge/`
