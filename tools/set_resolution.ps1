# Set virtual monitor to lower resolution for bigger text
# Change resolution to 800x480 or 1024x576

Add-Type @"
using System;
using System.Runtime.InteropServices;

public class Display {
    [DllImport("user32.dll")]
    public static extern int EnumDisplaySettings(string deviceName, int modeNum, ref DEVMODE devMode);

    [DllImport("user32.dll")]
    public static extern int ChangeDisplaySettingsEx(string deviceName, ref DEVMODE devMode, IntPtr hwnd, uint flags, IntPtr lParam);

    [StructLayout(LayoutKind.Sequential)]
    public struct DEVMODE {
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)]
        public string dmDeviceName;
        public short dmSpecVersion;
        public short dmDriverVersion;
        public short dmSize;
        public short dmDriverExtra;
        public int dmFields;
        public int dmPositionX;
        public int dmPositionY;
        public int dmDisplayOrientation;
        public int dmDisplayFixedOutput;
        public short dmColor;
        public short dmDuplex;
        public short dmYResolution;
        public short dmTTOption;
        public short dmCollate;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)]
        public string dmFormName;
        public short dmLogPixels;
        public int dmBitsPerPel;
        public int dmPelsWidth;
        public int dmPelsHeight;
        public int dmDisplayFlags;
        public int dmDisplayFrequency;
    }
}
"@

# List all displays
Write-Host "Available displays:"
$i = 0
while ($true) {
    $device = "\\.\DISPLAY$i"
    $devMode = New-Object Display+DEVMODE
    $devMode.dmSize = [System.Runtime.InteropServices.Marshal]::SizeOf($devMode)

    $result = [Display]::EnumDisplaySettings($device, -1, [ref]$devMode)
    if ($result -eq 0) { break }

    Write-Host "  DISPLAY$i : $($devMode.dmPelsWidth)x$($devMode.dmPelsHeight)"
    $i++
}

Write-Host ""
Write-Host "To change resolution, use Windows Settings > Display > select monitor 3 > change resolution to lower value"
