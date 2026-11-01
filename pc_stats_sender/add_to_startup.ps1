$WshShell = New-Object -ComObject WScript.Shell
$Shortcut = $WshShell.CreateShortcut("$env:APPDATA\Microsoft\Windows\Start Menu\Programs\Startup\VideoLauncher_Stream.lnk")
$Shortcut.TargetPath = "C:\Users\MarCixn\VideoLauncher\pc_stats_sender\run_all_hidden.vbs"
$Shortcut.WorkingDirectory = "C:\Users\MarCixn\VideoLauncher\pc_stats_sender"
$Shortcut.Save()
Write-Host "Autostart added!"
