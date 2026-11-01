@echo off
title Screen Stream Server
cd /d "%~dp0"

echo Starting Screen Stream Server...
echo.
echo Press Ctrl+C to stop
echo.

python screen_stream.py

pause
