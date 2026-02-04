# NC-KTV Test Suite Runner
# Comprehensive testing script for all test categories

$ErrorActionPreference = "Stop"

# Detect Python
if (Test-Path "$PSScriptRoot\python_embed\python.exe") {
    $PYTHON = "$PSScriptRoot\python_embed\python.exe"
    Write-Host "Using embedded Python: $PYTHON" -ForegroundColor Green
} else {
    $PYTHON = "python"
}

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  NC-KTV Comprehensive Test Suite" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Check if pytest is installed
try {
    & $PYTHON -m pytest --version | Out-Null
} catch {
    Write-Host "ERROR: pytest not found. Installing test dependencies..." -ForegroundColor Red
    & $PYTHON -m pip install -r tests/requirements.txt
}

Write-Host "Test Categories:" -ForegroundColor Yellow
Write-Host "  [1] Unit Tests" -ForegroundColor White
Write-Host "  [2] Integration Tests" -ForegroundColor White
Write-Host "  [3] Security Tests" -ForegroundColor White
Write-Host "  [4] Stress Tests" -ForegroundColor White
Write-Host "  [5] All Tests" -ForegroundColor Green
Write-Host "  [6] Quick Tests (Unit + Security)" -ForegroundColor Green
Write-Host "  [7] Security Scan (Bandit)" -ForegroundColor Yellow
Write-Host "  [8] Coverage Report" -ForegroundColor Cyan
Write-Host ""

$choice = Read-Host "Select test category (1-8)"

function Run-Tests {
    param(
        [string]$TestPath,
        [string]$Description,
        [string[]]$ExtraArgs = @()
    )
    
    Write-Host ""
    Write-Host "Running: $Description" -ForegroundColor Cyan
    Write-Host "----------------------------------------" -ForegroundColor Gray
    
    $allArgs = @("-m", "pytest", "-v", "--tb=short") + $ExtraArgs + @($TestPath)
    
    & $PYTHON $allArgs
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "✓ $Description PASSED" -ForegroundColor Green
        return 0
    } else {
        Write-Host "✗ $Description FAILED" -ForegroundColor Red
        return 1
    }
}

switch ($choice) {
    '1' {
        Run-Tests "tests/unit/" "Unit Tests"
    }
    '2' {
        Run-Tests "tests/integration/" "Integration Tests"
    }
    '3' {
        Run-Tests "tests/security/" "Security Tests"
    }
    '4' {
        Run-Tests "tests/stress/" "Stress Tests" @("-m", "not slow")
    }
    '5' {
        Write-Host ""
        Write-Host "Running ALL tests..." -ForegroundColor Yellow
        Write-Host ""
        
        $results = @()
        $results += Run-Tests "tests/unit/" "Unit Tests"
        $results += Run-Tests "tests/integration/" "Integration Tests"
        $results += Run-Tests "tests/security/" "Security Tests"
        $results += Run-Tests "tests/stress/" "Stress Tests" @("-m", "not slow")
        
        Write-Host ""
        Write-Host "========================================" -ForegroundColor Cyan
        Write-Host "  Test Summary" -ForegroundColor Cyan
        Write-Host "========================================" -ForegroundColor Cyan
        
        $passed = ($results | Where-Object { $_ -eq 0 }).Count
        $failed = ($results | Where-Object { $_ -ne 0 }).Count
        
        Write-Host "Passed: $passed / $($results.Count)" -ForegroundColor Green
        Write-Host "Failed: $failed / $($results.Count)" -ForegroundColor $(if ($failed -gt 0) { "Red" } else { "Green" })
    }
    '6' {
        Write-Host ""
        Write-Host "Running Quick Tests..." -ForegroundColor Yellow
        
        $results = @()
        $results += Run-Tests "tests/unit/" "Unit Tests"
        $results += Run-Tests "tests/security/" "Security Tests"
        
        $passed = ($results | Where-Object { $_ -eq 0 }).Count
        Write-Host ""
        Write-Host "Quick Tests: $passed / $($results.Count) passed" -ForegroundColor $(if ($passed -eq $results.Count) { "Green" } else { "Yellow" })
    }
    '7' {
        Write-Host ""
        Write-Host "Running Security Scan (Bandit)..." -ForegroundColor Yellow
        Write-Host "----------------------------------------" -ForegroundColor Gray
        
        try {
            & $PYTHON -m bandit -r src/ -f text
            Write-Host ""
            Write-Host "✓ Security scan complete" -ForegroundColor Green
        } catch {
            Write-Host "ERROR: Bandit execution failed." -ForegroundColor Red
        }
    }
    '8' {
        Write-Host ""
        Write-Host "Generating Coverage Report..." -ForegroundColor Cyan
        Write-Host "----------------------------------------" -ForegroundColor Gray
        
        & $PYTHON -m pytest --cov=src --cov-report=html --cov-report=term tests/
        
        if ($LASTEXITCODE -eq 0) {
            Write-Host ""
            Write-Host "Coverage report generated: htmlcov/index.html" -ForegroundColor Green
            
            $openReport = Read-Host "Open coverage report in browser? (y/n)"
            if ($openReport -eq "y") {
                Start-Process "htmlcov/index.html"
            }
        }
    }
    default {
        Write-Host "Invalid choice. Exiting." -ForegroundColor Red
        exit 1
    }
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  Testing Complete" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
