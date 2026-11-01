$ws = New-Object -ComObject WScript.Shell
$shortcutPath = "$env:APPDATA\Microsoft\Windows\Start Menu\Programs\Startup\PC_Stats_Sender.lnk"
$s = $ws.CreateShortcut($shortcutPath)
$s.TargetPath = "wscript.exe"
$s.Arguments = "`"C:\Users\MarCixn\VideoLauncher\pc_stats_sender\run_hidden.vbs`""
$s.WindowStyle = 7
$s.Save()
Write-Host "Shortcut created: $shortcutPath"
