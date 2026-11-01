Set WshShell = CreateObject("WScript.Shell")
Set objWMI = GetObject("winmgmts:\\.\root\cimv2")
Set fso = CreateObject("Scripting.FileSystemObject")

' Check if pc_stats.py is already running
Set colProcesses = objWMI.ExecQuery("SELECT * FROM Win32_Process WHERE CommandLine LIKE '%pc_stats.py%'")

If colProcesses.Count > 0 Then
    WScript.Quit
End If

' Get script directory
scriptDir = fso.GetParentFolderName(WScript.ScriptFullName)
scriptPath = scriptDir & "\pc_stats.py"

' Try to find pythonw.exe
pythonPath = ""

' Try common locations
pythonLocations = Array( _
    WshShell.ExpandEnvironmentStrings("%LOCALAPPDATA%") & "\Programs\Python\Python311\pythonw.exe", _
    WshShell.ExpandEnvironmentStrings("%LOCALAPPDATA%") & "\Programs\Python\Python312\pythonw.exe", _
    WshShell.ExpandEnvironmentStrings("%LOCALAPPDATA%") & "\Programs\Python\Python310\pythonw.exe", _
    "C:\Python311\pythonw.exe", _
    "C:\Python312\pythonw.exe", _
    "C:\Python310\pythonw.exe" _
)

For Each loc In pythonLocations
    If fso.FileExists(loc) Then
        pythonPath = loc
        Exit For
    End If
Next

' If not found, try PATH
If pythonPath = "" Then
    On Error Resume Next
    Set objExec = WshShell.Exec("where pythonw.exe")
    pythonPath = Trim(objExec.StdOut.ReadLine())
    On Error GoTo 0
End If

' Final fallback - try python.exe (will show console briefly)
If pythonPath = "" Or Not fso.FileExists(pythonPath) Then
    pythonPath = "pythonw.exe"
End If

WshShell.Run """" & pythonPath & """ """ & scriptPath & """", 0, False
