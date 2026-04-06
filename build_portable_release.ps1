$ErrorActionPreference = "Stop"

# Configuration
$ProcCount = $env:NUMBER_OF_PROCESSORS
$QtBin = "D:\ProgramData\Qt\6.10.2\mingw_64\bin"
$CMake = "D:\ProgramData\Qt\Tools\CMake_64\bin\cmake.exe"
$PythonExe = (Resolve-Path ".\python_embed\python.exe").Path
$BridgeFile = (Resolve-Path ".\python_bridge.py").Path
$TargetDir = ".\NC-KTV-Portable"

# ==============================================================================
# 1. C++ INCREMENTAL BUILD
# ==============================================================================
Write-Host ">>> Configuring and Building C++ Project..." -ForegroundColor Cyan
Set-Location cpp

& "$CMake" --preset windows-release
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }

& "$CMake" --build --preset windows-release --parallel $ProcCount
if ($LASTEXITCODE -ne 0) { throw "CMake build failed." }
Set-Location ..

# ==============================================================================
# 2. PYINSTALLER FAST PACKAGING (ONEDIR MODE)
# ==============================================================================
Write-Host ">>> Packaging Python AI Bridge with PyInstaller (Folder Mode)..." -ForegroundColor Cyan
$OutputDir = Join-Path (Get-Location) "build_pyinstaller"
$WorkDir = Join-Path (Get-Location) "build_pyinstaller_work"

# --onedir: Creates a folder with the .exe and DLLs (INSTANT STARTUP)
$pyinstallerArgs = @(
    "-m", "PyInstaller",
    "--noconfirm",
    "--onedir", 
    "--noconsole",
    "--clean",
    "--distpath", $OutputDir,
    "--workpath", $WorkDir,
    $BridgeFile
)

if (Test-Path ".\models\whisper\cudn12\bin") {
    Write-Host ">>> Bundling local CUDA/cuDNN DLLs for GPU Acceleration..." -ForegroundColor Cyan
    $pyinstallerArgs += "--add-binary"
    $pyinstallerArgs += ".\models\whisper\cudn12\bin\*.dll;."
}

& "$PythonExe" $pyinstallerArgs
if ($LASTEXITCODE -ne 0) { throw "PyInstaller failed." }

# ==============================================================================
# 3. SMART ASSEMBLY & PACKAGING
# ==============================================================================
Write-Host ">>> Assembling Portable Directory..." -ForegroundColor Cyan

if ($TargetDir -ne "." -and -Not (Test-Path $TargetDir)) {
    New-Item -ItemType Directory -Path $TargetDir | Out-Null
}

$ExePath = ".\cpp\out\build\windows-release\src\gui\ncktv.exe"
Copy-Item $ExePath -Destination $TargetDir -Force

Write-Host ">>> Running windeployqt..."
$env:PATH = "$QtBin;" + $env:PATH
& "$QtBin\windeployqt.exe" "$TargetDir\ncktv.exe" --no-translations --no-opengl-sw --compiler-runtime --dir $TargetDir
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed." }

Write-Host ">>> Updating Assets and DLLs..." -ForegroundColor Green

$OrtLib = ".\cpp\out\build\windows-release\_deps\onnxruntime-src\lib"
if (Test-Path $OrtLib) {
    Get-ChildItem -Path $OrtLib -Filter "*.dll" | Copy-Item -Destination $TargetDir -Force
}

if ($TargetDir -ne ".") {
    @("assets", "ffmpeg", "plugins", "themes") | ForEach-Object {
        if (Test-Path ".\$_") { Copy-Item -Recurse -Force ".\$_" -Destination "$TargetDir\" }
    }

    if (Test-Path ".\models") {
        $ModelTarget = New-Item -ItemType Directory -Path "$TargetDir\models" -Force
        Get-ChildItem ".\models" | ForEach-Object {
            $ext = if ($_.Extension -match "pth") { ".dat" } else { $_.Extension }
            Copy-Item $_.FullName -Destination "$ModelTarget\$($_.BaseName)$ext" -Force
        }
    }

    if (Test-Path ".\config.yaml") { Copy-Item ".\config.yaml" -Destination $TargetDir -Force }
}

Write-Host ">>> Copying Compiled Python Bridge Folder..." -ForegroundColor Cyan
# Copy the entire generated folder into the portable directory
$CompiledFolder = "$OutputDir\python_bridge"
if (Test-Path $CompiledFolder) {
    Copy-Item -Recurse -Force $CompiledFolder -Destination "$TargetDir\"
} else {
    throw "PyInstaller output folder not found!"
}

Write-Host "`n>>> Incremental build completed successfully with PyInstaller (Fast Startup Mode)!" -ForegroundColor Green