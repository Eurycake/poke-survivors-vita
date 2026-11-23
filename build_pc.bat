@echo off
setlocal
cd /d "%~dp0"

REM ---- One-click: if the ROM data has not been extracted yet, do it first ----
if not exist "src\maps_data.c" (
    echo [INFO] First run: extracting maps and audio from your Pokemon Emerald ROM...
    where python >nul 2>&1
    if errorlevel 1 (
        echo [ERROR] Python 3 is not installed or not in PATH.
        echo         Download it from https://www.python.org/downloads/ and tick Add Python to PATH.
        pause
        exit /b 1
    )
    python -c "import PIL" >nul 2>&1
    if errorlevel 1 (
        echo [INFO] Installing Pillow, needed for ROM asset extraction...
        python -m pip install -q pillow
        if errorlevel 1 (
            echo [ERROR] Failed to install Pillow. Run: pip install pillow
            pause
            exit /b 1
        )
    )
    if "%~1"=="" (
        python compile.py --target extract-only
    ) else (
        python compile.py --rom "%~1" --target extract-only
    )
    if errorlevel 1 (
        echo [FAILED] ROM extraction failed.
        pause
        exit /b 1
    )
)

taskkill /F /IM "poke-survivors vita.exe" >nul 2>&1
taskkill /F /IM poke-survivor-win32.exe >nul 2>&1
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
cd /d "%~dp0"
cl /nologo /O2 /W3 /D_CRT_SECURE_NO_WARNINGS /Isrc src\main.c src\game.c src\player.c src\enemies.c src\moves.c src\font.c src\save_system.c src\audio_pc.c src\renderer_vita3d.c src\g3d.c src\maps_data.c user32.lib gdi32.lib winmm.lib opengl32.lib /Fe:"poke-survivors vita.exe"
if %ERRORLEVEL% EQU 0 (
    del /q *.obj >nul 2>&1
    echo [SUCCESS] PC executable built: poke-survivors vita.exe
) else (
    echo [FAILED] Compilation error.
)

REM Keep the window open only when double-clicked, not when called from compile.py
echo %cmdcmdline% | find /i "%~f0" >nul
if not errorlevel 1 pause
