# Live view of the glove's sensor readings from the ESP32's USB port.
# Usage:  & .\tools\live.ps1            (uses COM7)
#         & .\tools\live.ps1 -Port COM5
# Ctrl+C to stop.
param([string]$Port = 'COM7')

$names = 'thumb', 'index', 'middle', 'ring', 'pinky'
$lo = @(4095, 4095, 4095, 4095, 4095); $hi = @(0, 0, 0, 0, 0)
$status = @{}
$sp = New-Object System.IO.Ports.SerialPort $Port, 115200
$sp.ReadTimeout = 1000
$lastLine = Get-Date
Write-Host "Reading $Port. Bend one finger at a time. Ctrl+C to stop.`n"
try {
  while ($true) {
    # Reconnect by itself if the USB cable was unplugged or the board restarted.
    if (-not $sp.IsOpen -or ((Get-Date) - $lastLine).TotalSeconds -gt 3) {
      try { if ($sp.IsOpen) { $sp.Close() }; $sp.Open(); $lastLine = Get-Date }
      catch { Clear-Host; Write-Host "Waiting for the ESP32 on $Port (plug it in)…" -ForegroundColor Yellow; Start-Sleep -Milliseconds 800; continue }
    }
    try { $line = $sp.ReadLine(); $lastLine = Get-Date } catch { continue }
    if ($line -notmatch '^thumb') { if ($line -match 'I2C|MPU') { $status[$line.Substring(0, 3)] = $line.Trim() }; continue }
    $n = [regex]::Matches($line, '-?\d+') | ForEach-Object { [int]$_.Value }
    $out = @()
    for ($i = 0; $i -lt 5; $i++) {
      if ($n[$i] -lt $lo[$i]) { $lo[$i] = $n[$i] }
      if ($n[$i] -gt $hi[$i]) { $hi[$i] = $n[$i] }
      $bar = '#' * [math]::Round($n[$i] / 4095 * 20)
      $range = $hi[$i] - $lo[$i]
      $verdict = if ($range -ge 80) { 'MOVES' } elseif ($n[$i] -eq 0 -or $n[$i] -eq 4095) { 'NOT CONNECTED?' } else { 'not bent yet' }
      $out += ('{0,-6}{1,5} {2,-20}  lowest {3,5}  highest {4,5}  range {5,5}  {6}' -f $names[$i], $n[$i], $bar, $lo[$i], $hi[$i], $range, $verdict)
    }
    Clear-Host
    Write-Host "Glove readings on $Port (0-4095). Bend each finger fully. Ctrl+C to stop.`n"
    $out | ForEach-Object { Write-Host $_ }
    Write-Host ("`nmotion: acc {0} {1} {2}   gyro {3} {4} {5}" -f $n[5], $n[6], $n[7], $n[8], $n[9], $n[10])
    if ($status.Count) { Write-Host ''; $status.Values | ForEach-Object { Write-Host $_ -ForegroundColor Yellow } }
    else { Write-Host "`n(press EN on the ESP32 to re-check the MPU6050)" }
  }
} finally { $sp.Close() }
