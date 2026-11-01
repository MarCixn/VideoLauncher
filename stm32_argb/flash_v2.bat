@echo off
echo ============================================
echo   STM32 ARGB Controller v2 - Flash ST-Link
echo ============================================
echo.

set SCRIPT_DIR=C:\Users\MarCixn\VideoLauncher\stm32_argb\
set CLI_EXE=%SCRIPT_DIR%arduino-cli\arduino-cli.exe
set OPENOCD_EXE=%SCRIPT_DIR%tools\xpack-openocd\bin\openocd.exe
set SKETCH_DIR=%SCRIPT_DIR%ARGB_Controller_v2

echo Kompilacja...
"%CLI_EXE%" compile --fqbn STMicroelectronics:stm32:GenF1:pnum=BLUEPILL_F103C8,upload_method=swdMethod --output-dir "%SKETCH_DIR%\build" "%SKETCH_DIR%"

if errorlevel 1 (
    echo.
    echo BLAD kompilacji!
    pause
    exit /b 1
)

echo.
echo Wgrywanie przez ST-Link...
set BIN_FILE=%SKETCH_DIR%\build\ARGB_Controller_v2.ino.bin
set SCRIPTS=%SCRIPT_DIR%tools\xpack-openocd\openocd\scripts

"%OPENOCD_EXE%" -f "%SCRIPTS%\interface\stlink.cfg" -f "%SCRIPTS%\target\stm32f1x.cfg" -c "program {%BIN_FILE%} 0x08000000 verify reset exit"

if errorlevel 1 (
    echo.
    echo BLAD wgrywania! Sprawdz:
    echo - ST-Link podlaczony?
    echo - SWDIO-PA13, SWCLK-PA14, GND-GND
) else (
    echo.
    echo ============================================
    echo   SUKCES! Firmware wgrany!
    echo ============================================
)

pause
