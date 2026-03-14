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

Write-Host ">>> Copying Assets and Resources..."
$FoldersToCopy = @("assets", "ffmpeg", "models", "plugins", "themes", "python_embed")
foreach ($folder in $FoldersToCopy) {
    if (Test-Path ".\$folder") {
        Copy-Item -Recurse -Force ".\$folder" -Destination "$TargetDir\"
        Write-Host "Copied $folder/"
    }
}

if (Test-Path ".\config.yaml") {
    Copy-Item ".\config.yaml" -Destination $TargetDir
}

Write-Host ">>> Copying Python Bridge Files..."
Copy-Item ".\python_bridge.py" -Destination $TargetDir
Copy-Item ".\requirements.txt" -Destination $TargetDir

Write-Host ">>> Release build completed successfully in portable directory!"

