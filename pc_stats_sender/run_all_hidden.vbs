Set WshShell = CreateObject("WScript.Shell")
Set fso = CreateObject("Scripting.FileSystemObject")
strPath = fso.GetParentFolderName(WScript.ScriptFullName)

' Start Screen Stream Server
WshShell.Run "pythonw """ & strPath & "\screen_stream.py""", 0, False

' Wait a moment
WScript.Sleep 500

' Start PC Stats Sender
WshShell.Run "pythonw """ & strPath & "\pc_stats.py""", 0, False
