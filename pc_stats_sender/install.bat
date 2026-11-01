@echo off
chcp 65001 >nul
title PC Stats Sender - Instalator
echo.
echo ╔═══════════════════════════════════════════════════════════╗
echo ║         PC Stats Sender - Instalacja                      ║
echo ║  Wysyla statystyki CPU/GPU/RAM do aplikacji VideoLauncher ║
echo ╚═══════════════════════════════════════════════════════════╝
echo.

:: Check if Python is installed
where python >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo [BLAD] Python nie jest zainstalowany!
    echo.
    echo Pobierz Python z: https://www.python.org/downloads/
    echo Podczas instalacji zaznacz "Add Python to PATH"
    echo.
    pause
    exit /b 1
)

echo [1/4] Sprawdzanie Python...
for /f "tokens=*" %%i in ('python --version 2^>^&1') do set PYVER=%%i
echo       %PYVER%
echo.

echo [2/4] Instalowanie zaleznosci...
pip install --quiet psutil pynvml wmi pywin32 2>nul
if %ERRORLEVEL% neq 0 (
    echo       Proba z --user...
    pip install --quiet --user psutil pynvml wmi pywin32 2>nul
)
echo       OK
echo.

echo [3/4] Tworzenie autostart...
set "STARTUP=%APPDATA%\Microsoft\Windows\Start Menu\Programs\Startup"
set "SCRIPT_DIR=%~dp0"
set "VBS_PATH=%SCRIPT_DIR%run_hidden.vbs"

:: Create shortcut using PowerShell
powershell -NoProfile -Command "$ws = New-Object -ComObject WScript.Shell; $s = $ws.CreateShortcut('%STARTUP%\PC_Stats_Sender.lnk'); $s.TargetPath = 'wscript.exe'; $s.Arguments = '\"%VBS_PATH%\"'; $s.WorkingDirectory = '%SCRIPT_DIR%'; $s.Description = 'PC Stats for VideoLauncher'; $s.Save()"

if exist "%STARTUP%\PC_Stats_Sender.lnk" (
    echo       Skrot utworzony w Autostart
) else (
    echo       [!] Nie udalo sie utworzyc skrotu
)
echo.

echo [4/4] Uruchamianie...
start "" wscript.exe "%VBS_PATH%"
timeout /t 2 /nobreak >nul

:: Verify it's running
tasklist /FI "IMAGENAME eq pythonw.exe" 2>nul | find /i "pythonw" >nul
if %ERRORLEVEL% equ 0 (
    echo       PC Stats dziala w tle
) else (
    tasklist /FI "IMAGENAME eq python.exe" 2>nul | find /i "python" >nul
    if %ERRORLEVEL% equ 0 (
        echo       PC Stats dziala w tle
    ) else (
        echo       [!] Sprawdz czy Python jest poprawnie zainstalowany
    )
)

echo.
echo ════════════════════════════════════════════════════════════
echo   GOTOWE! PC Stats bedzie uruchamiac sie przy starcie Windows
echo.
echo   Aby odinstalowac, uruchom: uninstall.bat
echo ════════════════════════════════════════════════════════════
echo.
pause
