. "$PSScriptRoot\no_crash_dialog.ps1"   # BAKEPERF1: no crash dialog on his desktop
# BAKEPERF1 bisect 2: inside the terrain TEXTURE stage, what corrupts the heap?
$S = "E:\Projects\NifskopeWildWastelandEdition\scratchpad\bakeperf1_20260911"
$NS = "E:\Projects\NifskopeWildWastelandEdition\release\NifSkope.exe"
$esm = "X:\Programs\Steam\steamapps\common\Fallout 4\Data\Fallout4.esm"
$W = "$S\bisect2"

function RunIt($name, $extra, $threads, $times) {
  $codes = @()
  foreach ($i in 1..$times) {
    if (Test-Path $W) { Remove-Item -Recurse -Force $W }
    New-Item -ItemType Directory -Force -Path "$W\tex" | Out-Null
    $argv = @('-no-gui', 'lodgen', "`"$esm`"", '--worldspace', '3C',
              '--terrain-region', '-20', '24', '-9', '35', '--dim', '4',
              '--out-dir', $W, '--tex-dir', "$W\tex", '--threads', "$threads") + $extra
    $q = $argv | ForEach-Object { if ($_ -match '\s' -and -not $_.StartsWith('"')) { '"' + $_ + '"' } else { $_ } }
    $p = Start-Process -FilePath $NS -ArgumentList $q -NoNewWindow -PassThru `
           -RedirectStandardOutput "$S\b2_$name.log" -RedirectStandardError "$S\b2_$name.err"
    $h = $p.Handle
    $p.WaitForExit()
    $c = $p.ExitCode
    if ($c -lt 0) { $codes += ('0x{0:X8}' -f ([uint32]([int64]$c + 4294967296))) } else { $codes += "$c" }
  }
  "{0,-22} threads={1}  {2}" -f $name, $threads, ($codes -join ' ')
}

RunIt 'tex-only'        @()             16 3
RunIt 'tex-only-noroads' @('--no-roads') 16 3
RunIt 'tex-only'        @()              2 3
RunIt 'tex-only'        @()              4 3
if (Test-Path $W) { Remove-Item -Recurse -Force $W }
