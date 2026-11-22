# Records the glove's serial output for a while and reports, per finger, how far it moved and when.
# Usage: & .\tools\bendtest.ps1 [-Seconds 60] [-Port COM7]
param([int]$Seconds = 60, [string]$Port = 'COM7')
$names = 'thumb', 'index', 'middle', 'ring'
$sp = New-Object System.IO.Ports.SerialPort $Port, 115200
$sp.ReadTimeout = 500; $sp.DtrEnable = $false; $sp.RtsEnable = $false   # don't restart the ESP32
$sp.Open()
$t0 = Get-Date; $rows = @()
while (((Get-Date) - $t0).TotalSeconds -lt $Seconds) {
  try { $line = $sp.ReadLine() } catch { continue }
  if ($line -match '^thumb') {
    $n = [regex]::Matches($line, '-?\d+') | ForEach-Object { [int]$_.Value }
    $rows += , (@([math]::Round(((Get-Date) - $t0).TotalSeconds, 1)) + $n[0..3])
  }
}
$sp.Close()
"samples: $($rows.Count) over $Seconds s"
for ($i = 0; $i -lt 4; $i++) {
  $v = $rows | ForEach-Object { $_[$i + 1] }
  $start = ($v | Select-Object -First 5 | Measure-Object -Average).Average
  $s = $v | Measure-Object -Min -Max
  $moved = $rows | Where-Object { [math]::Abs($_[$i + 1] - $start) -ge 80 } | ForEach-Object { $_[0] }
  $when = if ($moved) { "moved at {0}s to {1}s" -f ($moved | Select-Object -First 1), ($moved | Select-Object -Last 1) } else { 'never moved 80+' }
  '{0,-7} start {1,5:N0}  min {2,5}  max {3,5}  range {4,5}  {5}' -f $names[$i], $start, $s.Minimum, $s.Maximum, ($s.Maximum - $s.Minimum), $when
}
