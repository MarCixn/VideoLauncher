@echo off
echo ============================================
echo   STM32 ARGB - Flash przez ST-Link V2
echo ============================================
echo.

set SCRIPT_DIR=C:\Users\MarCixn\VideoLauncher\stm32_argb\
set TOOLS_DIR=%SCRIPT_DIR%tools
set OPENOCD_EXE=%TOOLS_DIR%\xpack-openocd\bin\openocd.exe
set CLI_EXE=%TOOLS_DIR%\arduino-cli\arduino-cli.exe
set SKETCH_DIR=%SCRIPT_DIR%ARGB_Controller

echo Sprawdzam narzedzia...

if not exist "%OPENOCD_EXE%" (
    echo Brak OpenOCD - pobieram...
    mkdir "%TOOLS_DIR%" 2>nul
    powershell -Command "[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; Invoke-WebRequest -Uri 'https://github.com/xpack-dev-tools/openocd-xpack/releases/download/v0.12.0-3/xpack-openocd-0.12.0-3-win32-x64.zip' -OutFile '%TOOLS_DIR%\openocd.zip'"
    powershell -Command "Expand-Archive -Path '%TOOLS_DIR%\openocd.zip' -DestinationPath '%TOOLS_DIR%' -Force"
    ren "%TOOLS_DIR%\xpack-openocd-0.12.0-3" "xpack-openocd"
    del "%TOOLS_DIR%\openocd.zip"
)

if not exist "%CLI_EXE%" (
    echo Brak Arduino CLI - pobieram...
    mkdir "%TOOLS_DIR%\arduino-cli" 2>nul
    powershell -Command "[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; Invoke-WebRequest -Uri 'https://github.com/arduino/arduino-cli/releases/download/v1.0.4/arduino-cli_1.0.4_Windows_64bit.zip' -OutFile '%TOOLS_DIR%\cli.zip'"
    powershell -Command "Expand-Archive -Path '%TOOLS_DIR%\cli.zip' -DestinationPath '%TOOLS_DIR%\arduino-cli' -Force"
    del "%TOOLS_DIR%\cli.zip"

    echo Konfiguracja...
    "%CLI_EXE%" config init --overwrite
    "%CLI_EXE%" config add board_manager.additional_urls https://github.com/stm32duino/BoardManagerFiles/raw/main/package_stmicroelectronics_index.json
    "%CLI_EXE%" core update-index
    echo Instalowanie STM32 (2-3 min)...
    "%CLI_EXE%" core install STMicroelectronics:stm32
    "%CLI_EXE%" lib install FastLED
)

echo.
echo Kompilacja...
mkdir "%SKETCH_DIR%" 2>nul
copy /y "%SCRIPT_DIR%src\main.cpp" "%SKETCH_DIR%\ARGB_Controller.ino" >nul

"%CLI_EXE%" compile --fqbn STMicroelectronics:stm32:GenF1:pnum=BLUEPILL_F103C8 --output-dir "%SKETCH_DIR%\build" "%SKETCH_DIR%"
if errorlevel 1 (
    echo BLAD kompilacji!
    pause
    exit /b 1
)

echo.
echo Wgrywanie na STM32...
set BIN_FILE=%SKETCH_DIR%\build\ARGB_Controller.ino.bin
set SCRIPTS=%TOOLS_DIR%\xpack-openocd\openocd\scripts

"%OPENOCD_EXE%" -f "%SCRIPTS%\interface\stlink.cfg" -f "%SCRIPTS%\target\stm32f1x.cfg" -c "program {%BIN_FILE%} 0x08000000 verify reset exit"

if errorlevel 1 (
    echo.
    echo BLAD! Sprawdz:
    echo - ST-Link podlaczony?
    echo - Kable: SWDIO-PA13, SWCLK-PA14, GND-GND
    echo - Sterowniki: https://www.st.com/en/development-tools/stsw-link009.html
) else (
    echo.
    echo ============================================
    echo   SUKCES!
    echo ============================================
    echo Podlacz LED do PA7, STM32 do radia USB
)

pause
