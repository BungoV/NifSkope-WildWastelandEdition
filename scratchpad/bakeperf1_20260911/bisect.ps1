. "$PSScriptRoot\no_crash_dialog.ps1"   # BAKEPERF1: no crash dialog on his desktop
# BAKEPERF1: which half of the chunk build corrupts the heap at 16 threads?
# Each variant drops ONE thing from the full run and reports the exit code.
# 0xC0000374 = heap corruption. Each variant is run three times: one clean run
# proves nothing about a race.
$S = "E:\Projects\NifskopeWildWastelandEdition\scratchpad\bakeperf1_20260911"
$NS = "E:\Projects\NifskopeWildWastelandEdition\release\NifSkope.exe"
$esm = "X:\Programs\Steam\steamapps\common\Fallout 4\Data\Fallout4.esm"
$W = "$S\bisect"

$variants = @{
  'full'       = @('--tex-dir', "$W\tex", '--native', $W, '--arrays', '--merge')
  'no-native'  = @('--tex-dir', "$W\tex", '--arrays', '--merge')
  'no-tex'     = @('--native', $W, '--arrays', '--merge')
  'no-ao'      = @('--tex-dir', "$W\tex", '--native', $W, '--arrays', '--merge', '--no-ao')
  'meshes-only'= @()
}

foreach ($name in @('full', 'no-native', 'no-tex', 'no-ao', 'meshes-only')) {
  $codes = @()
  foreach ($i in 1..3) {
    if (Test-Path $W) { Remove-Item -Recurse -Force $W }
    New-Item -ItemType Directory -Force -Path "$W\tex" | Out-Null
    $argv = @('-no-gui', 'lodgen', "`"$esm`"", '--worldspace', '3C',
              '--terrain-region', '-20', '24', '-9', '35', '--dim', '4',
              '--out-dir', $W, '--threads', '16') + $variants[$name]
    $q = $argv | ForEach-Object { if ($_ -match '\s' -and -not $_.StartsWith('"')) { '"' + $_ + '"' } else { $_ } }
    $p = Start-Process -FilePath $NS -ArgumentList $q -NoNewWindow -PassThru `
           -RedirectStandardOutput "$S\bisect_$name.log" -RedirectStandardError "$S\bisect_$name.err"
    $h = $p.Handle
    $p.WaitForExit()
    $codes += ('0x{0:X8}' -f [uint32]$p.ExitCode)
  }
  "$name : $($codes -join ' ')"
}
if (Test-Path $W) { Remove-Item -Recurse -Force $W }
