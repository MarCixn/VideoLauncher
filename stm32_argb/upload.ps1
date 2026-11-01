$arduinoCli = "C:\Users\MarCixn\AppData\Local\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"
$sketchPath = "C:\Users\MarCixn\VideoLauncher\stm32_argb\ARGB_Controller"
$fqbn = "STMicroelectronics:stm32:GenF1:pnum=BLUEPILL_F103C8,upload_method=DFUMethod"

Write-Host "Kompilacja..."
& $arduinoCli compile --fqbn $fqbn $sketchPath

if ($LASTEXITCODE -eq 0) {
    Write-Host "Wgrywanie na STM32..."
    & $arduinoCli upload --fqbn $fqbn -p COM3 $sketchPath
} else {
    Write-Host "Blad kompilacji!"
}
