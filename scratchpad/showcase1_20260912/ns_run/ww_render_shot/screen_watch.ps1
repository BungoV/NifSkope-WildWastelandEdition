param([string]$Out, [string]$Stop, [int]$X, [int]$Y, [int]$W, [int]$H)
Add-Type -AssemblyName System.Drawing
$big = New-Object System.Drawing.Bitmap $W, $H
$gb  = [System.Drawing.Graphics]::FromImage($big)
$sm  = New-Object System.Drawing.Bitmap 8, 8
$gs  = [System.Drawing.Graphics]::FromImage($sm)
$gs.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBilinear
$lines = New-Object System.Collections.Generic.List[string]
while (-not (Test-Path $Stop)) {
  try { $gb.CopyFromScreen($X, $Y, 0, 0, $big.Size) } catch { break }
  $gs.DrawImage($big, 0, 0, 8, 8)
  $sum = 0.0
  for ($i = 0; $i -lt 8; $i++) {
    for ($j = 0; $j -lt 8; $j++) {
      $p = $sm.GetPixel($i, $j)
      $sum += 0.299 * $p.R + 0.587 * $p.G + 0.114 * $p.B
    }
  }
  $lines.Add(("{0:F3}" -f ($sum / 64)))
  Start-Sleep -Milliseconds 50
}
$lines | Out-File -Encoding ascii $Out
