# Download Portable FFmpeg
# This script downloads FFmpeg and places it in the NC-KTV folder for portability

Write-Host "=== Downloading Portable FFmpeg ===" -ForegroundColor Cyan
Write-Host ""

$ffmpegDir = "ffmpeg"
$ffmpegUrl = "https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-master-latest-win64-gpl.zip"
$zipFile = "ffmpeg.zip"

# Create directory
if (-not (Test-Path $ffmpegDir)) {
    New-Item -ItemType Directory -Path $ffmpegDir | Out-Null
}

# Download FFmpeg
Write-Host "Downloading FFmpeg (~200MB)..." -ForegroundColor Yellow
Invoke-WebRequest -Uri $ffmpegUrl -OutFile $zipFile

# Extract
Write-Host "Extracting..." -ForegroundColor Yellow
Expand-Archive -Path $zipFile -DestinationPath $ffmpegDir -Force

# Move binaries
$extracted = Get-ChildItem "$ffmpegDir\ffmpeg-*" -Directory | Select-Object -First 1
if ($extracted) {
    Move-Item "$($extracted.FullName)\bin\*" $ffmpegDir -Force
    Remove-Item $extracted.FullName -Recurse -Force
}

# Cleanup
Remove-Item $zipFile -Force

Write-Host ""
Write-Host "[OK] FFmpeg installed to ffmpeg\" -ForegroundColor Green
Write-Host ""
Write-Host "FFmpeg is now portable in your NC-KTV folder!" -ForegroundColor Cyan
Write-Host "The app will automatically find it." -ForegroundColor White
Write-Host ""
