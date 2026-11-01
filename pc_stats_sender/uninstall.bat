@echo off
chcp 65001 >nul
title PC Stats Sender - Deinstalacja
echo.
echo ╔═══════════════════════════════════════════════════════════╗
echo ║         PC Stats Sender - Deinstalacja                    ║
echo ╚═══════════════════════════════════════════════════════════╝
echo.

echo [1/3] Zatrzymywanie procesu...
taskkill /F /IM pythonw.exe /FI "WINDOWTITLE eq *pc_stats*" 2>nul
:: Kill any python running pc_stats.py
for /f "tokens=2" %%i in ('wmic process where "commandline like '%%pc_stats.py%%'" get processid 2^>nul ^| findstr /r "[0-9]"') do (
    taskkill /F /PID %%i 2>nul
)
echo       OK
echo.

echo [2/3] Usuwanie autostart...
set "STARTUP=%APPDATA%\Microsoft\Windows\Start Menu\Programs\Startup"
if exist "%STARTUP%\PC_Stats_Sender.lnk" (
    del "%STARTUP%\PC_Stats_Sender.lnk"
    echo       Skrot usuniety
) else (
    echo       Skrot nie istnieje
)
echo.

echo [3/3] Usuwanie Scheduled Task (jesli istnieje)...
schtasks /delete /tn "PC_Stats_Sender" /f 2>nul
echo       OK
echo.

echo ════════════════════════════════════════════════════════════
echo   PC Stats Sender zostal odinstalowany.
echo   Pliki programu pozostaly w folderze (mozesz je usunac recznie)
echo ════════════════════════════════════════════════════════════
echo.
pause
