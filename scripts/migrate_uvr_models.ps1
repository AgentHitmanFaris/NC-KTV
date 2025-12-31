# Copy UVR Models to NC-KTV
# This script copies only the model files we need from UVR installation

Write-Host "=== NC-KTV Model Migration Script ===" -ForegroundColor Cyan
Write-Host ""

# Paths
$uvrModelsPath = "d:\Document\NC-KTV\UVR\models"
$ncKtvModelsPath = "d:\Document\NC-KTV\models"

# Create models directory if it doesn't exist
if (-not (Test-Path $ncKtvModelsPath)) {
    Write-Host "Creating models directory..." -ForegroundColor Yellow
    New-Item -ItemType Directory -Path $ncKtvModelsPath | Out-Null
}

# Function to copy models
function Copy-Models {
    param (
        [string]$SourceDir,
        [string]$Pattern,
        [string]$Description
    )
    
    $files = Get-ChildItem -Path $SourceDir -Filter $Pattern -Recurse -File -ErrorAction SilentlyContinue
    
    if ($files.Count -gt 0) {
        Write-Host "`nCopying $Description models..." -ForegroundColor Green
        
        foreach ($file in $files) {
            $destPath = Join-Path $ncKtvModelsPath $file.Name
            
            if (Test-Path $destPath) {
                Write-Host "  [SKIP] $($file.Name) (already exists)" -ForegroundColor Gray
            } else {
                Copy-Item $file.FullName -Destination $destPath
                $sizeMB = [math]::Round($file.Length / 1MB, 2)
                Write-Host "  [COPY] $($file.Name) ($sizeMB MB)" -ForegroundColor Green
            }
        }
    } else {
        Write-Host "`nNo $Description models found" -ForegroundColor Yellow
    }
}

# Copy different model types
Write-Host "Scanning UVR models directory..." -ForegroundColor Cyan

Copy-Models -SourceDir $uvrModelsPath -Pattern "*.onnx" -Description "ONNX (MDX-Net)"
Copy-Models -SourceDir $uvrModelsPath -Pattern "*.pth" -Description "PyTorch (VR)"
Copy-Models -SourceDir $uvrModelsPath -Pattern "*.pt" -Description "PyTorch"

# List what was copied
Write-Host "`n=== Models in NC-KTV ===" -ForegroundColor Cyan
$copiedFiles = Get-ChildItem -Path $ncKtvModelsPath -File

if ($copiedFiles.Count -gt 0) {
    $totalSize = ($copiedFiles | Measure-Object -Property Length -Sum).Sum
    $totalSizeMB = [math]::Round($totalSize / 1MB, 2)
    
    Write-Host "Total models: $($copiedFiles.Count)" -ForegroundColor Green
    Write-Host "Total size: $totalSizeMB MB" -ForegroundColor Green
    Write-Host ""
    
    foreach ($file in $copiedFiles) {
        $sizeMB = [math]::Round($file.Length / 1MB, 2)
        Write-Host "  - $($file.Name) ($sizeMB MB)" -ForegroundColor White
    }
} else {
    Write-Host "No models found!" -ForegroundColor Red
}

Write-Host "`n=== Recommended Models for Karaoke ===" -ForegroundColor Cyan
$recommendedModels = @(
    "UVR_MDXNET_KARA_2.onnx",
    "UVR-MDX-NET-Inst_HQ_3.onnx",
    "UVR-MDX-NET-Voc_FT.onnx"
)

foreach ($model in $recommendedModels) {
    $exists = Test-Path (Join-Path $ncKtvModelsPath $model)
    if ($exists) {
        Write-Host "  OK $model" -ForegroundColor Green
    } else {
        Write-Host "  -- $model (not found)" -ForegroundColor Yellow
    }
}

Write-Host "`n=== Next Steps ===" -ForegroundColor Cyan
Write-Host "1. Verify models copied correctly above"
Write-Host "2. Test NC-KTV with: python_embed\python.exe main.py"
Write-Host "3. If working, remove UVR folder to save space:"
Write-Host "   Remove-Item -Recurse -Force 'd:\Document\NC-KTV\UVR'" -ForegroundColor Yellow
Write-Host ""
Write-Host "Done!" -ForegroundColor Green
