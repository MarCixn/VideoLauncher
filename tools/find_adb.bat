@echo off
where adb 2>nul
if errorlevel 1 (
    echo ADB_NOT_IN_PATH
    dir /s /b "C:\Program Files\Android\*\adb.exe" 2>nul
    dir /s /b "C:\Users\MarCixn\AppData\Local\Android\*\adb.exe" 2>nul
    dir /s /b "C:\Android\*\adb.exe" 2>nul
)
