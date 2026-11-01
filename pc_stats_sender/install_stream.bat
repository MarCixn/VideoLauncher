@echo off
title Screen Stream Installer
cd /d "%~dp0"

echo ============================================
echo   Screen Stream Server Installer
echo ============================================
echo.

:: Check Python
echo Checking Python...
python --version >nul 2>&1
if errorlevel 1 (
    echo ERROR: Python not found! Please install Python 3.10+
    pause
    exit /b 1
)
echo Python OK

:: Install Python dependencies
echo.
echo Installing Python packages...
pip install mss pillow >nul 2>&1
echo Done

:: Check/Install FFmpeg
echo.
echo Checking FFmpeg...
ffmpeg -version >nul 2>&1
if errorlevel 1 (
    echo FFmpeg not found in PATH.
    echo.
    echo You need to install FFmpeg manually:
    echo   1. Download from: https://github.com/BtbN/FFmpeg-Builds/releases
    echo   2. Extract ffmpeg.exe to: %~dp0tools\
    echo   OR add FFmpeg to system PATH
    echo.

    :: Create tools folder
    if not exist "tools" mkdir tools

    :: Try to download FFmpeg automatically
    echo Attempting automatic download...
    powershell -Command "& {$ProgressPreference='SilentlyContinue'; try { Invoke-WebRequest -Uri 'https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-master-latest-win64-gpl.zip' -OutFile 'tools\ffmpeg.zip' -TimeoutSec 60 } catch { exit 1 }}"

    if exist "tools\ffmpeg.zip" (
        echo Extracting FFmpeg...
        powershell -Command "Expand-Archive -Path 'tools\ffmpeg.zip' -DestinationPath 'tools\ffmpeg_temp' -Force"

        :: Find and move ffmpeg.exe
        for /r "tools\ffmpeg_temp" %%f in (ffmpeg.exe) do (
            copy "%%f" "tools\ffmpeg.exe" >nul
        )

        :: Cleanup
        rmdir /s /q "tools\ffmpeg_temp" 2>nul
        del "tools\ffmpeg.zip" 2>nul

        if exist "tools\ffmpeg.exe" (
            echo FFmpeg installed successfully!
        ) else (
            echo Failed to extract FFmpeg. Please install manually.
        )
    ) else (
        echo Automatic download failed. Please install FFmpeg manually.
    )
) else (
    echo FFmpeg OK
)

echo.
echo ============================================
echo   Installation complete!
echo ============================================
echo.
echo To start the stream server:
echo   - Visible:  run_stream.bat
echo   - Hidden:   run_stream_hidden.vbs
echo.
echo The server will wait for commands from the Android app.
echo.

pause
