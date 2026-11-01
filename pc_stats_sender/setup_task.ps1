# Remove startup shortcut
$shortcutPath = "$env:APPDATA\Microsoft\Windows\Start Menu\Programs\Startup\PC_Stats_Sender.lnk"
if (Test-Path $shortcutPath) {
    Remove-Item $shortcutPath -Force
    Write-Host "Removed startup shortcut"
}

# Remove old task if exists
Unregister-ScheduledTask -TaskName "PC_Stats_Sender" -Confirm:$false -ErrorAction SilentlyContinue

# Create scheduled task that runs hidden at logon
$action = New-ScheduledTaskAction -Execute "wscript.exe" -Argument "`"C:\Users\MarCixn\VideoLauncher\pc_stats_sender\run_hidden.vbs`""
$trigger = New-ScheduledTaskTrigger -AtLogon
$settings = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -Hidden
$principal = New-ScheduledTaskPrincipal -UserId "$env:USERNAME" -RunLevel Limited

Register-ScheduledTask -TaskName "PC_Stats_Sender" -Action $action -Trigger $trigger -Settings $settings -Principal $principal

Write-Host "Scheduled task created - PC_Stats_Sender"
