@echo off
setlocal enabledelayedexpansion
echo ============================================
echo   STM32 ARGB Controller - Wgrywanie
echo ============================================
echo.

set SCRIPT_DIR=%~dp0
set CLI_DIR=%SCRIPT_DIR%arduino-cli
set CLI_EXE=%CLI_DIR%\arduino-cli.exe

:: Check if Arduino CLI exists, if not download it
if not exist "%CLI_EXE%" (
    echo [1/5] Pobieranie Arduino CLI...
    mkdir "%CLI_DIR%" 2>nul

    powershell -Command "Invoke-WebRequest -Uri 'https://github.com/arduino/arduino-cli/releases/download/v1.0.4/arduino-cli_1.0.4_Windows_64bit.zip' -OutFile '%CLI_DIR%\arduino-cli.zip'"

    if errorlevel 1 (
        echo BLAD: Nie mozna pobrac Arduino CLI
        pause
        exit /b 1
    )

    echo Rozpakowywanie...
    powershell -Command "Expand-Archive -Path '%CLI_DIR%\arduino-cli.zip' -DestinationPath '%CLI_DIR%' -Force"
    del "%CLI_DIR%\arduino-cli.zip" 2>nul
)

if not exist "%CLI_EXE%" (
    echo BLAD: Arduino CLI nie zostalo zainstalowane
    pause
    exit /b 1
)

echo [2/5] Konfiguracja Arduino CLI...
"%CLI_EXE%" config init --overwrite 2>nul

:: Add STM32 board URL
"%CLI_EXE%" config add board_manager.additional_urls https://github.com/stm32duino/BoardManagerFiles/raw/main/package_stmicroelectronics_index.json

echo [3/5] Instalacja STM32 boards (moze zajac kilka minut)...
"%CLI_EXE%" core update-index
"%CLI_EXE%" core install STMicroelectronics:stm32

echo [4/5] Instalacja biblioteki FastLED...
"%CLI_EXE%" lib install FastLED

:: Create Arduino sketch from main.cpp
echo [5/5] Kompilacja i wgrywanie...

set SKETCH_DIR=%SCRIPT_DIR%ARGB_Controller
mkdir "%SKETCH_DIR%" 2>nul

:: Copy source to .ino file
copy "%SCRIPT_DIR%src\main.cpp" "%SKETCH_DIR%\ARGB_Controller.ino" >nul

:: Find COM port
echo.
echo Szukam portu COM...
for /f "tokens=*" %%i in ('powershell -Command "Get-WMIObject Win32_SerialPort | Where-Object { $_.Description -match 'CH340\|STM\|Serial' } | Select-Object -ExpandProperty DeviceID -First 1"') do set COMPORT=%%i

if "%COMPORT%"=="" (
    echo.
    echo Nie znaleziono portu COM automatycznie.
    echo.
    echo Dostepne porty:
    "%CLI_EXE%" board list
    echo.
    set /p COMPORT="Podaj port COM (np. COM3): "
)

echo.
echo Uzyje portu: %COMPORT%
echo.

:: Upload - try different upload methods
echo Wgrywam firmware (metoda: STM32CubeProgrammer/stlink)...
"%CLI_EXE%" compile --fqbn STMicroelectronics:stm32:GenF1:pnum=BLUEPILL_F103C8,upload_method=swdMethod "%SKETCH_DIR%"
if errorlevel 1 (
    echo.
    echo Proba z metoda Serial...
    "%CLI_EXE%" compile --fqbn STMicroelectronics:stm32:GenF1:pnum=BLUEPILL_F103C8,upload_method=serialMethod "%SKETCH_DIR%"
)

"%CLI_EXE%" upload -p %COMPORT% --fqbn STMicroelectronics:stm32:GenF1:pnum=BLUEPILL_F103C8 "%SKETCH_DIR%"

if errorlevel 1 (
    echo.
    echo ============================================
    echo BLAD wgrywania!
    echo.
    echo Sprawdz:
    echo 1. Czy STM32 jest podlaczony przez USB?
    echo 2. Czy masz sterowniki CH340/STLink?
    echo 3. Dla bootloadera Serial: BOOT0=1, BOOT1=0
    echo.
    echo Mozesz tez uzyc Arduino IDE reczne:
    echo - Otworz: %SKETCH_DIR%\ARGB_Controller.ino
    echo - Board: Generic STM32F1 series
    echo - Board part: BluePill F103C8
    echo ============================================
) else (
    echo.
    echo ============================================
    echo   SUKCES! Firmware wgrany!
    echo ============================================
    echo.
    echo Teraz:
    echo 1. Podlacz tasme LED do pinu PA7
    echo 2. Podlacz STM32 do radia przez USB
    echo 3. Otworz ARGB Control w launcherze
)

echo.
pause
