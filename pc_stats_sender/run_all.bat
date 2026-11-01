@echo off
title VideoLauncher PC Services
cd /d "%~dp0"

echo ============================================
echo   VideoLauncher PC Services
echo ============================================
echo.
echo Starting PC Stats Sender and Screen Stream Server...
echo.

:: Start screen stream server in background
start /b pythonw screen_stream.py

:: Wait a moment
timeout /t 1 /nobreak >nul

:: Run pc_stats in foreground (shows output)
python pc_stats.py
