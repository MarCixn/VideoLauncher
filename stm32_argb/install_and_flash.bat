@echo off
setlocal
echo ============================================
echo   STM32 ARGB - Wgrywanie przez ST-Link V2
echo ============================================
echo.

set SCRIPT_DIR=%~dp0
set TOOLS_DIR=%SCRIPT_DIR%tools
set OPENOCD_DIR=%TOOLS_DIR%\xpack-openocd
set OPENOCD_EXE=%OPENOCD_DIR%\bin\openocd.exe

mkdir "%TOOLS_DIR%" 2>nul

:: Download OpenOCD (xpack version - ZIP format)
if not exist "%OPENOCD_EXE%" (
    echo [1/4] Pobieranie OpenOCD...

    powershell -Command "[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; Invoke-WebRequest -Uri 'https://github.com/xpack-dev-tools/openocd-xpack/releases/download/v0.12.0-3/xpack-openocd-0.12.0-3-win32-x64.zip' -OutFile '%TOOLS_DIR%\openocd.zip'"

    if not exist "%TOOLS_DIR%\openocd.zip" (
        echo Blad pobierania OpenOCD
        pause
        exit /b 1
    )

    echo Rozpakowywanie...
    powershell -Command "Expand-Archive -Path '%TOOLS_DIR%\openocd.zip' -DestinationPath '%TOOLS_DIR%' -Force"
    del "%TOOLS_DIR%\openocd.zip" 2>nul

    :: Find and rename openocd folder
    for /d %%i in ("%TOOLS_DIR%\xpack-openocd-*") do (
        if exist "%%i" (
            rmdir /s /q "%OPENOCD_DIR%" 2>nul
            move "%%i" "%OPENOCD_DIR%" >nul
        )
    )
)

if not exist "%OPENOCD_EXE%" (
    echo BLAD: OpenOCD nie zainstalowane
    echo Sprawdz folder: %TOOLS_DIR%
    pause
    exit /b 1
)

echo [2/4] OpenOCD OK
echo.

:: Arduino CLI
set CLI_DIR=%TOOLS_DIR%\arduino-cli
set CLI_EXE=%CLI_DIR%\arduino-cli.exe

if not exist "%CLI_EXE%" (
    echo Pobieranie Arduino CLI...
    mkdir "%CLI_DIR%" 2>nul

    powershell -Command "[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; Invoke-WebRequest -Uri 'https://github.com/arduino/arduino-cli/releases/download/v1.0.4/arduino-cli_1.0.4_Windows_64bit.zip' -OutFile '%CLI_DIR%\cli.zip'"
    powershell -Command "Expand-Archive -Path '%CLI_DIR%\cli.zip' -DestinationPath '%CLI_DIR%' -Force"
    del "%CLI_DIR%\cli.zip" 2>nul

    echo Konfiguracja Arduino CLI...
    "%CLI_EXE%" config init --overwrite 2>nul
    "%CLI_EXE%" config add board_manager.additional_urls https://github.com/stm32duino/BoardManagerFiles/raw/main/package_stmicroelectronics_index.json
    "%CLI_EXE%" core update-index

    echo Instalowanie STM32 boards (moze zajac 2-3 minuty)...
    "%CLI_EXE%" core install STMicroelectronics:stm32

    echo Instalowanie FastLED...
    "%CLI_EXE%" lib install FastLED
)

if not exist "%CLI_EXE%" (
    echo BLAD: Arduino CLI nie zainstalowane
    pause
    exit /b 1
)

echo [3/4] Kompilacja firmware...

:: Create sketch
set SKETCH_DIR=%SCRIPT_DIR%ARGB_Controller
mkdir "%SKETCH_DIR%" 2>nul
copy /y "%SCRIPT_DIR%src\main.cpp" "%SKETCH_DIR%\ARGB_Controller.ino" >nul

:: Compile
"%CLI_EXE%" compile --fqbn STMicroelectronics:stm32:GenF1:pnum=BLUEPILL_F103C8 --output-dir "%SKETCH_DIR%\build" "%SKETCH_DIR%"

if errorlevel 1 (
    echo BLAD kompilacji!
    pause
    exit /b 1
)

:: Find bin file
set BIN_FILE=
for /r "%SKETCH_DIR%\build" %%f in (*.bin) do set BIN_FILE=%%f

if "%BIN_FILE%"=="" (
    echo Nie znaleziono pliku .bin!
    pause
    exit /b 1
)

echo Kompilacja OK: %BIN_FILE%
echo.

echo [4/4] Wgrywanie przez ST-Link V2...
echo.

:: Flash with OpenOCD
"%OPENOCD_EXE%" -f "%OPENOCD_DIR%\openocd\scripts\interface\stlink.cfg" -f "%OPENOCD_DIR%\openocd\scripts\target\stm32f1x.cfg" -c "program {%BIN_FILE%} 0x08000000 verify reset exit"

if errorlevel 1 (
    echo.
    echo Proba z stlink-v2.cfg...
    "%OPENOCD_EXE%" -f "%OPENOCD_DIR%\openocd\scripts\interface\stlink-v2.cfg" -f "%OPENOCD_DIR%\openocd\scripts\target\stm32f1x.cfg" -c "program {%BIN_FILE%} 0x08000000 verify reset exit"
)

if errorlevel 1 (
    echo.
    echo ============================================
    echo   Problem z ST-Link
    echo ============================================
    echo.
    echo Sprawdz:
    echo 1. Czy ST-Link jest podlaczony do USB?
    echo 2. Czy dioda na ST-Link swieci?
    echo 3. Podlaczenie kabli:
    echo    SWDIO -^> PA13
    echo    SWCLK -^> PA14
    echo    GND   -^> GND
    echo    3.3V  -^> 3.3V
    echo.
    echo 4. Zainstaluj sterowniki ST-Link:
    echo    https://www.st.com/en/development-tools/stsw-link009.html
    echo.
) else (
    echo.
    echo ============================================
    echo   SUKCES! Firmware wgrany!
    echo ============================================
    echo.
    echo Teraz:
    echo 1. Odlacz ST-Link
    echo 2. Podlacz tasme LED do pinu PA7
    echo 3. Podlacz STM32 do radia przez USB
    echo 4. Otworz ARGB Control w launcherze
    echo.
)

pause
