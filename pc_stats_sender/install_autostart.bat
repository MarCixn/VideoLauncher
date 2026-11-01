@echo off
echo ========================================
echo  PC Stats Sender - Instalacja
echo ========================================
echo.

echo [1/3] Instalowanie zaleznosci...
pip install psutil GPUtil wmi pywin32 2>nul
echo.

echo [2/3] Tworzenie skrotu w Autostart...
set STARTUP=%APPDATA%\Microsoft\Windows\Start Menu\Programs\Startup
set SCRIPT_PATH=%~dp0run_hidden.vbs

:: Create shortcut using PowerShell
powershell -Command "$ws = New-Object -ComObject WScript.Shell; $s = $ws.CreateShortcut('%STARTUP%\PC_Stats_Sender.lnk'); $s.TargetPath = '%SCRIPT_PATH%'; $s.WorkingDirectory = '%~dp0'; $s.Description = 'PC Stats for Video Launcher'; $s.Save()"

echo Skrot utworzony w: %STARTUP%
echo.

echo [3/3] Uruchamianie...
start "" "%SCRIPT_PATH%"

echo.
echo ========================================
echo  Gotowe! PC Stats bedzie uruchamiac sie
echo  automatycznie przy starcie Windows.
echo.
echo  Aby usunac autostart, usun skrot:
echo  %STARTUP%\PC_Stats_Sender.lnk
echo ========================================
echo.
pause
