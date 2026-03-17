$ErrorActionPreference = "Stop"

$QtBin = "D:\ProgramData\Qt\6.10.2\mingw_64\bin"
$VsVars = "D:\ProgramData\Microsoft Visual Studio\VC\Auxiliary\Build\vcvars64.bat"
$CMake = "D:\ProgramData\Qt\Tools\CMake_64\bin\cmake.exe"

Write-Host ">>> Entering cpp workspace and cleaning old config..."
Set-Location cpp
if (Test-Path "out\build\windows-release") {
    Remove-Item -Recurse -Force "out\build\windows-release"
}

Write-Host ">>> Configuring CMake (Release)..."
& "$CMake" --preset windows-release
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }

Write-Host ">>> Building Project..."
& "$CMake" --build --preset windows-release
if ($LASTEXITCODE -ne 0) { throw "CMake build failed." }

Set-Location ..

# ==============================================================================
# NEW STEP: Compile the Python AI Bridge into a standalone executable
# ==============================================================================
Write-Host ">>> Compiling Python AI Engine with Nuitka..." -ForegroundColor Cyan

# Clear any Python environment variables that might interfere with the embedded env
$env:PYTHONPATH = ""
$env:PYTHONHOME = ""

Write-Host ">>> Cleaning Nuitka leftovers..." -ForegroundColor Yellow
if (Test-Path ".\build_nuitka") { Remove-Item -Recurse -Force ".\build_nuitka" }
if (Test-Path ".\python_bridge.build") { Remove-Item -Recurse -Force ".\python_bridge.build" }
if (Test-Path ".\python_bridge.onefile-build") { Remove-Item -Recurse -Force ".\python_bridge.onefile-build" }
if (Test-Path ".\python_bridge.dist") { Remove-Item -Recurse -Force ".\python_bridge.dist" }

# Use the bundled python to ensure we have the correct dependencies (nuitka, torch, etc)
$PythonExe = Resolve-Path ".\python_embed\python.exe"
$BridgeFile = Resolve-Path ".\python_bridge.py"
$OutputDir = Resolve-Path ".\build_nuitka"

Write-Host "Using Python: $PythonExe"
Write-Host "Compiling: $BridgeFile"

# Added --show-scons and --show-code-generation to see EXACTLY what is happening
# Added --assume-yes-for-downloads to avoid stalling
cmd /c "`"$PythonExe`" -m nuitka --standalone --onefile --mingw64 --plugin-enable=torch --windows-console-mode=disable --output-dir=`"$OutputDir`" --show-progress --assume-yes-for-downloads --verbose --show-scons --show-code-generation `"$BridgeFile`""
if ($LASTEXITCODE -ne 0) { throw "Nuitka compilation failed. Please check your Python environment." }

Write-Host ">>> Assembling Portable Directory..."
$TargetDir = ".\NC-KTV-Portable"
if (Test-Path $TargetDir) {
    Remove-Item -Recurse -Force $TargetDir
}
New-Item -ItemType Directory -Path $TargetDir | Out-Null

$ExePath = ".\cpp\out\build\windows-release\src\gui\ncktv.exe"
if (-Not (Test-Path $ExePath)) { throw "Executable not found at expected path: $ExePath" }
Copy-Item $ExePath -Destination $TargetDir

Write-Host ">>> Running windeployqt..."
$env:PATH = "$QtBin;" + $env:PATH
& "$QtBin\windeployqt.exe" "$TargetDir\ncktv.exe"
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed." }

Write-Host ">>> Copying ONNX Runtime DLLs (GPU Support)..."
$OrtLib = ".\cpp\out\build\windows-release\_deps\onnxruntime-src\lib"
if (Test-Path $OrtLib) {
    $foundDlls = Get-ChildItem -Path $OrtLib -Filter "*.dll"
    foreach ($file in $foundDlls) {
        Copy-Item $file.FullName -Destination $TargetDir
        Write-Host "Copied $($file.Name)"
    }
} else {
    Write-Host "WARNING: ONNX Runtime lib dir not found at $OrtLib" -ForegroundColor Yellow
}

# Notice we removed "python_embed" and "models" from this general copy list
Write-Host ">>> Copying Assets and Resources..."
$FoldersToCopy = @("assets", "ffmpeg", "plugins", "themes")
foreach ($folder in $FoldersToCopy) {
    if (Test-Path ".\$folder") {
        Copy-Item -Recurse -Force ".\$folder" -Destination "$TargetDir\"
        Write-Host "Copied $folder/"
    }
}

# ==============================================================================
# NEW STEP: Copy and Obfuscate AI Models
# ==============================================================================
Write-Host ">>> Securing and Copying AI Models..." -ForegroundColor Cyan
if (Test-Path ".\models") {
    $ModelTarget = "$TargetDir\models"
    New-Item -ItemType Directory -Path $ModelTarget | Out-Null
    
    $models = Get-ChildItem -Path ".\models"
    foreach ($model in $models) {
        # If it's a PyTorch model, change the extension to .dat to hide it
        if ($model.Extension -match "\.pth?$") {
            $newName = $model.Name -replace "\.pth?$", ".dat"
            Copy-Item $model.FullName -Destination "$ModelTarget\$newName"
            Write-Host "Obfuscated and copied model: $newName"
        } else {
            # Copy ONNX and JSON files normally
            Copy-Item $model.FullName -Destination "$ModelTarget\"
            Write-Host "Copied model: $($model.Name)"
        }
    }
}

if (Test-Path ".\config.yaml") {
    Copy-Item ".\config.yaml" -Destination $TargetDir
}

# ==============================================================================
# NEW STEP: Copy the compiled executable instead of the raw Python scripts
# ==============================================================================
Write-Host ">>> Copying Compiled Python Bridge..." -ForegroundColor Cyan
$CompiledPythonExe = ".\build_nuitka\python_bridge.exe"
if (Test-Path $CompiledPythonExe) {
    Copy-Item $CompiledPythonExe -Destination $TargetDir
    Write-Host "Copied python_bridge.exe (Environment and code hidden successfully!)"
} else {
    throw "Compiled Python executable not found!"
}

Write-Host ">>> Release build completed successfully in portable directory!" -ForegroundColor Green