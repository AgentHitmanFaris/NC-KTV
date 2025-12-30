# Manual PyTorch Installation Script
# Use this if you have downloaded PyTorch wheel files manually

Write-Host "=== Manual PyTorch Installation ===" -ForegroundColor Cyan
Write-Host ""

$pythonExe = "python_embed\python.exe"

# Check if Python exists
if (-not (Test-Path $pythonExe)) {
    Write-Host "[ERROR] Python embedded not found!" -ForegroundColor Red
    Write-Host "Run setup_python.ps1 first (it will fail at PyTorch, that's OK)" -ForegroundColor Yellow
    exit 1
}

Write-Host "Place your downloaded .whl files in this directory:" -ForegroundColor Yellow
Write-Host "  - torch-2.1.0+cu118-cp310-cp310-win_amd64.whl" -ForegroundColor White
Write-Host "  - torchaudio-2.1.0+cu118-cp310-cp310-win_amd64.whl" -ForegroundColor White
Write-Host ""

# Find .whl files
$whlFiles = Get-ChildItem -Path . -Filter "*.whl" -File

if ($whlFiles.Count -eq 0) {
    Write-Host "[ERROR] No .whl files found in current directory!" -ForegroundColor Red
    Write-Host ""
    Write-Host "Download from:" -ForegroundColor Yellow
    Write-Host "https://download.pytorch.org/whl/cu118/torch-2.1.0%2Bcu118-cp310-cp310-win_amd64.whl"
    Write-Host "https://download.pytorch.org/whl/cu118/torchaudio-2.1.0%2Bcu118-cp310-cp310-win_amd64.whl"
    exit 1
}

Write-Host "Found $($whlFiles.Count) wheel file(s):" -ForegroundColor Green
foreach ($whl in $whlFiles) {
    $sizeMB = [math]::Round($whl.Length / 1MB, 2)
    Write-Host "  - $($whl.Name) ($sizeMB MB)" -ForegroundColor White
}
Write-Host ""

# Install each wheel
Write-Host "Installing wheel files..." -ForegroundColor Cyan

foreach ($whl in $whlFiles) {
    Write-Host "Installing $($whl.Name)..." -ForegroundColor Yellow
    & $pythonExe -m pip install $whl.FullName
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "[OK] $($whl.Name) installed" -ForegroundColor Green
    } else {
        Write-Host "[ERROR] Failed to install $($whl.Name)" -ForegroundColor Red
    }
}

Write-Host ""
Write-Host "=== Verifying PyTorch Installation ===" -ForegroundColor Cyan

$testScript = @"
import torch
print('PyTorch version:', torch.__version__)
print('CUDA available:', torch.cuda.is_available())
if torch.cuda.is_available():
    print('GPU:', torch.cuda.get_device_name(0))
else:
    print('No GPU detected - will use CPU')
"@

& $pythonExe -c $testScript

Write-Host ""
Write-Host "Next: Continue with the rest of setup" -ForegroundColor Cyan
Write-Host "Run: .\setup_python.ps1" -ForegroundColor Yellow
Write-Host "(PyTorch installation will be skipped automatically)" -ForegroundColor Gray
Write-Host ""
