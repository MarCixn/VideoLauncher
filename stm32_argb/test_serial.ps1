$port = new-Object System.IO.Ports.SerialPort COM3,115200,None,8,one
$port.ReadTimeout = 1000
try {
    $port.Open()
    $port.WriteLine("STATUS")
    Start-Sleep -Milliseconds 500
    $response = $port.ReadExisting()
    Write-Host "Response: $response"
    $port.Close()
} catch {
    Write-Host "Error: $($_.Exception.Message)"
}
