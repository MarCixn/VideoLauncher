Get-Process -Name wscript, powershell, pythonw -ErrorAction SilentlyContinue | Format-Table Id, ProcessName -AutoSize
