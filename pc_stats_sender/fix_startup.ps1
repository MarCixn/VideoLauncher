# Remove old shortcut
$shortcutPath = [Environment]::GetFolderPath('Startup') + "\PC_Stats_Sender.lnk"
Remove-Item $shortcutPath -Force -ErrorAction SilentlyContinue

# Create shortcut that runs pythonw directly (no VBS, no windows)
$ws = New-Object -ComObject WScript.Shell
$shortcut = $ws.CreateShortcut($shortcutPath)
$shortcut.TargetPath = "C:\Users\MarCixn\AppData\Local\Programs\Python\Python311\pythonw.exe"
$shortcut.Arguments = "`"C:\Users\MarCixn\VideoLauncher\pc_stats_sender\pc_stats.py`""
$shortcut.WindowStyle = 7
$shortcut.Save()
Write-Host "Done"
