# Set display scale for virtual monitor to 125%
# Find virtual display monitor ID and set DPI

$monitors = Get-ChildItem "HKCU:\Control Panel\Desktop\PerMonitorSettings"
foreach ($mon in $monitors) {
    Write-Host "Monitor: $($mon.PSChildName)"
    $props = Get-ItemProperty $mon.PSPath
    Write-Host "  Current DpiValue: $($props.DpiValue)"

    # If this looks like virtual display (you may need to identify it)
    if ($mon.PSChildName -like "*Virtual*" -or $mon.PSChildName -like "*VDD*") {
        Set-ItemProperty -Path $mon.PSPath -Name "DpiValue" -Value 1
        Write-Host "  Set to 125% (DpiValue=1)"
    }
}

Write-Host ""
Write-Host "To apply changes, sign out and sign back in, or restart."
