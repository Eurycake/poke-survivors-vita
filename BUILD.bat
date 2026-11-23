@echo off
title poke-survivors vita - VPK & Data Builder
setlocal
cd /d "%~dp0"

echo ============================================================
echo   poke-survivors vita - 1-Click VPK ^& Data Builder
echo ============================================================
echo.

REM Check Python
where python >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Python 3 is not installed or not in PATH.
    echo         Download Python from https://www.python.org/downloads/
    echo         ^(Check "Add Python to PATH" during installation.^)
    echo.
    pause
    exit /b 1
)

REM Auto-install Pillow if missing
python -c "import PIL" >nul 2>&1
if errorlevel 1 (
    echo [INFO] Installing Pillow ^(required for ROM asset extraction^)...
    python -m pip install -q pillow
    if errorlevel 1 (
        echo [ERROR] Failed to install Pillow. Run: pip install pillow
        pause
        exit /b 1
    )
)

REM Run unified extractor & builder (accepts emerald.gba or any .gba in this folder, or dragged onto BUILD.bat)
if "%~1"=="" (
    python compile.py --target vita
) else (
    python compile.py --rom "%~1" --target vita
)
if errorlevel 1 (
    echo.
    pause
    exit /b 1
)

echo.
echo Opening dist\ folder...
if exist "dist" start "" "dist"
pause
