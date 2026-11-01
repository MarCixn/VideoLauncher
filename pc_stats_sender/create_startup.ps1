$ws = New-Object -ComObject WScript.Shell
$shortcutPath = [Environment]::GetFolderPath('Startup') + "\PC_Stats_Sender.lnk"
$shortcut = $ws.CreateShortcut($shortcutPath)
$shortcut.TargetPath = "wscript.exe"
$shortcut.Arguments = "//B //Nologo `"C:\Users\MarCixn\VideoLauncher\pc_stats_sender\run_hidden.vbs`""
$shortcut.WindowStyle = 7
$shortcut.Save()
Write-Host "Created: $shortcutPath"
