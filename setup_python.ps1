# NC-KTV Python Embedded Setup Script
# Automates the setup of Python 3.10.11 embedded environment

Write-Host "=== NC-KTV Python Embedded Setup ===" -ForegroundColor Cyan
Write-Host ""

$pythonEmbedDir = "python_embed"
$pythonExe = "$pythonEmbedDir\python.exe"
$pthFile = "$pythonEmbedDir\python310._pth"

# Check if Python embedded exists
if (-not (Test-Path $pythonExe)) {
    Write-Host "ERROR: Python embedded not found!" -ForegroundColor Red
    Write-Host ""
    Write-Host "Please:" -ForegroundColor Yellow
    Write-Host "1. Download Python 3.10.11 embedded from:" -ForegroundColor Yellow
    Write-Host "   https://www.python.org/ftp/python/3.10.11/python-3.10.11-embed-amd64.zip"
    Write-Host "2. Extract to: $pythonEmbedDir\" -ForegroundColor Yellow
    Write-Host "3. Run this script again" -ForegroundColor Yellow
    exit 1
}

Write-Host "[OK] Python embedded found" -ForegroundColor Green

# Step 1: Enable pip by modifying .pth file
Write-Host "`nStep 1: Enabling pip..." -ForegroundColor Cyan

if (Test-Path $pthFile) {
    $pthContent = Get-Content $pthFile
    
    if ($pthContent -match "^#.*import site") {
        # Uncomment import site
        $newContent = $pthContent -replace "^#(import site)", '$1'
        Set-Content $pthFile $newContent
        Write-Host "[OK] Enabled 'import site' in $pthFile" -ForegroundColor Green
    } elseif ($pthContent -match "^import site") {
        Write-Host "[SKIP] 'import site' already enabled" -ForegroundColor Gray
    } else {
        # Add import site at the end
        Add-Content $pthFile "`nimport site"
        Write-Host "[OK] Added 'import site' to $pthFile" -ForegroundColor Green
    }
} else {
    Write-Host "[ERROR] $pthFile not found!" -ForegroundColor Red
    exit 1
}

# Step 2: Download and install pip
Write-Host "`nStep 2: Installing pip..." -ForegroundColor Cyan

$getPipPath = "$pythonEmbedDir\get-pip.py"

if (-not (Test-Path $getPipPath)) {
    Write-Host "Downloading get-pip.py..." -ForegroundColor Yellow
    Invoke-WebRequest -Uri "https://bootstrap.pypa.io/get-pip.py" -OutFile $getPipPath
    Write-Host "[OK] Downloaded get-pip.py" -ForegroundColor Green
}

# Install pip
Write-Host "Installing pip..." -ForegroundColor Yellow
& $pythonExe $getPipPath --quiet --no-warn-script-location

if ($LASTEXITCODE -eq 0) {
    Write-Host "[OK] Pip installed successfully" -ForegroundColor Green
} else {
    Write-Host "[ERROR] Pip installation failed" -ForegroundColor Red
    exit 1
}

# Step 3: Upgrade pip
Write-Host "`nStep 3: Upgrading pip..." -ForegroundColor Cyan
& $pythonExe -m pip install --upgrade pip --quiet
Write-Host "[OK] Pip upgraded" -ForegroundColor Green

# Step 4: Install PyTorch with CUDA 11.8
Write-Host "`nStep 4: Installing PyTorch with CUDA 11.8..." -ForegroundColor Cyan

# Check if PyTorch is already installed
$torchCheck = & $pythonExe -c "import torch; print(torch.__version__)" 2>$null

if ($LASTEXITCODE -eq 0) {
    Write-Host "[SKIP] PyTorch already installed: $torchCheck" -ForegroundColor Gray
} else {
    Write-Host "This may take a few minutes..." -ForegroundColor Yellow
    
    & $pythonExe -m pip install torch==2.1.0+cu118 torchaudio==2.1.0+cu118 --index-url https://download.pytorch.org/whl/cu118 
    if ($LASTEXITCODE -eq 0) {
        Write-Host "[OK] PyTorch with CUDA 11.8 installed" -ForegroundColor Green
    } else {
        Write-Host "[ERROR] PyTorch installation failed" -ForegroundColor Red
        Write-Host "Trying without quiet mode for debugging..." -ForegroundColor Yellow
        & $pythonExe -m pip install torch==2.1.0+cu118 torchaudio==2.1.0+cu118 --index-url https://download.pytorch.org/whl/cu118
    }
}


# Step 5: Install project dependencies
Write-Host "`nStep 5: Installing project dependencies..." -ForegroundColor Cyan
Write-Host "This may take several minutes..." -ForegroundColor Yellow

if (Test-Path "requirements.txt") {
    & $pythonExe -m pip install -r requirements.txt --quiet
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "[OK] All dependencies installed" -ForegroundColor Green
    } else {
        Write-Host "[WARNING] Some dependencies may have failed" -ForegroundColor Yellow
    }
} else {
    Write-Host "[ERROR] requirements.txt not found!" -ForegroundColor Red
}

# Step 6: Verify installation
Write-Host "`nStep 6: Verifying installation..." -ForegroundColor Cyan

# Test imports
$testScript = @"
import sys
print('Python version:', sys.version)
try:
    import torch
    print('PyTorch:', torch.__version__)
    print('CUDA available:', torch.cuda.is_available())
    if torch.cuda.is_available():
        print('GPU:', torch.cuda.get_device_name(0))
except Exception as e:
    print('PyTorch ERROR:', e)

try:
    import PyQt6
    print('PyQt6: OK')
except Exception as e:
    print('PyQt6 ERROR:', e)

try:
    import audio_separator
    print('audio-separator: OK')
except Exception as e:
    print('audio-separator ERROR:', e)

try:
    import librosa
    print('librosa: OK')
except Exception as e:
    print('librosa ERROR:', e)

try:
    import whisper
    print('openai-whisper: OK')
except Exception as e:
    print('openai-whisper ERROR:', e)
"@

Write-Host ""
& $pythonExe -c $testScript
Write-Host ""

# Step 7: Check FFmpeg
Write-Host "Step 7: Checking FFmpeg..." -ForegroundColor Cyan

try {
    $ffmpegVersion = & ffmpeg -version 2>&1 | Select-Object -First 1
    Write-Host "[OK] FFmpeg found: $ffmpegVersion" -ForegroundColor Green
} catch {
    Write-Host "[WARNING] FFmpeg not found in PATH" -ForegroundColor Yellow
    Write-Host "Please install FFmpeg and add to system PATH" -ForegroundColor Yellow
}

# Summary
Write-Host "`n=== Setup Complete! ===" -ForegroundColor Cyan
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Green
Write-Host "1. Ensure UVR models are in models/ directory" -ForegroundColor White
Write-Host "2. Run the application:" -ForegroundColor White
Write-Host "   $pythonExe main.py" -ForegroundColor Yellow
Write-Host ""
Write-Host "To test vocal removal, select an MP3/MP4 file in the app." -ForegroundColor White
Write-Host ""
