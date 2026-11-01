@echo off
taskkill /f /im pythonw.exe 2>nul
taskkill /f /im wscript.exe 2>nul
timeout /t 2 /nobreak >nul
start "" /B "C:\Users\MarCixn\AppData\Local\Programs\Python\Python311\pythonw.exe" "C:\Users\MarCixn\VideoLauncher\pc_stats_sender\pc_stats.py"
