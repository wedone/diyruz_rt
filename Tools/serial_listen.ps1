# 串口日志监听脚本
# 用法: powershell -File serial_listen.ps1 [-Port COM4] [-Baud 38400] [-Secs 60]
param(
    [string]$Port = "COM4",
    [int]$Baud   = 38400,
    [int]$Secs   = 60
)

try {
    $port = New-Object -TypeName System.IO.Ports.SerialPort -ArgumentList $Port,$Baud,[System.IO.Ports.Parity]::None,8,[System.IO.Ports.StopBits]::One
    Write-Host ("DBG type=" + $port.GetType().FullName)
    Write-Host ("DBG props=" + ($port | Get-Member -MemberType Property | Select-Object -ExpandProperty Name | Out-String))
    $port.ReadTimeout = 500
    $port.Open()
    Write-Host ("[{0}] {1} opened {2} 8N1, listening {3}s ..." -f (Get-Date -Format 'HH:mm:ss'), $Port, $Baud, $Secs)

    $deadline = (Get-Date).AddSeconds($Secs)
    while ((Get-Date) -lt $deadline) {
        try {
            $line = $port.ReadLine()
            Write-Host ("[{0}] {1}" -f (Get-Date -Format 'HH:mm:ss.fff'), $line)
        } catch [System.TimeoutException] {
            Start-Sleep -Milliseconds 30
        } catch {
            # 读到的数据没有 \n，尝试 ReadExisting
            $rest = $port.ReadExisting()
            if ($rest) { Write-Host ("[{0}] {1}" -f (Get-Date -Format 'HH:mm:ss.fff'), $rest) -NoNewline }
            Start-Sleep -Milliseconds 30
        }
    }
    $port.Close()
    Write-Host ("[{0}] close {1}" -f (Get-Date -Format 'HH:mm:ss'), $Port)
} catch {
    Write-Host ("ERROR: {0}" -f $_.Exception.Message)
    exit 1
}
