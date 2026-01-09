@echo off
setlocal

:: Get the directory of the batch file to handle relative paths correctly
set "ROOT_DIR=%~dp0"
cd /d "%ROOT_DIR%"

echo Starting NC-KTV

:: Run the embedded python with the script
".\python_embed\python.exe" "main.py"

:: If the script fails, this keeps the terminal open so you can read the error
if %errorlevel% neq 0 (
    echo.
    echo Script failed with error code %errorlevel%.
    pause
)

endlocal