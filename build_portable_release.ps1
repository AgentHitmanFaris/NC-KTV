$ErrorActionPreference = "Stop"

# CMake Path Resolution
$CMake = "cmake"
if (Test-Path "D:\ProgramData\Qt\Tools\CMake_64\bin\cmake.exe") {
    $CMake = "D:\ProgramData\Qt\Tools\CMake_64\bin\cmake.exe"
}

Write-Host ">>> Building C++ Native Bridge using: $CMake..." -ForegroundColor Cyan
& "$CMake" --build build --config Release
if ($LASTEXITCODE -ne 0) { throw "C++ build failed." }

Write-Host ">>> Publishing WPF Application..." -ForegroundColor Cyan
$PublishDir = Resolve-Path ".\NC-KTV-Portable" -ErrorAction SilentlyContinue
if ($PublishDir) {
    Remove-Item -Path $PublishDir -Recurse -Force -ErrorAction SilentlyContinue
}
dotnet publish NCKTV.Wpf/NCKTV.Wpf.csproj -c Release -o .\NC-KTV-Portable
if ($LASTEXITCODE -ne 0) { throw "WPF publish failed." }

# Copy models folder if it exists
if (Test-Path ".\models") {
    Write-Host ">>> Copying ONNX Models..." -ForegroundColor Cyan
    $ModelTarget = New-Item -ItemType Directory -Path ".\NC-KTV-Portable\models" -Force
    Get-ChildItem ".\models" -Filter "*.onnx" | Copy-Item -Destination $ModelTarget -Force
}

# Copy assets and themes folders if they exist
foreach ($folder in @("assets", "themes")) {
    if (Test-Path ".\$folder") {
        Write-Host ">>> Copying $folder..." -ForegroundColor Cyan
        Copy-Item -Recurse -Force ".\$folder" -Destination ".\NC-KTV-Portable\"
    }
}

Write-Host "`n>>> Portable Release compiled successfully in .\NC-KTV-Portable!" -ForegroundColor Green