@echo off
echo PC Stats Sender for Video Launcher
echo ==================================
echo.
echo Installing dependencies...
pip install psutil GPUtil 2>nul
echo.
echo Starting stats sender...
python "%~dp0pc_stats.py"
pause
