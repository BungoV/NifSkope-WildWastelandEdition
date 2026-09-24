# RESUME3 gate R4: the tiling change, measured on the REAL bake.
#
#   tiling_gates.ps1
#
# Six bakes, sequential, one NifSkope instance at a time, each into this lane's
# OWN out-dir. Never his installed Data\Terrain, never the full Commonwealth:
# two single dim-4 chunks, (-20,24) and (-20,20), which are the two tiles
# SPLAT1 measured offline.
#
#   rung/<tile>      the rung exe, shipped defaults        <- S3's reference
#   back/<tile>      the new exe, --land-tiling 2048       <- must equal rung
#   new/<tile>       the new exe, default 341.3333         <- the measurement
param(
  [string]$NewExe  = "E:\Projects\NifskopeWildWastelandEdition\release\NifSkope.exe",
  [string]$RungExe = "E:\Projects\NifskopeWildWastelandEdition\release\NifSkope.before_resume3.exe"
)

$ErrorActionPreference = 'Continue'
$R   = "E:\Projects\NifskopeWildWastelandEdition"
$OUT = "$R\scratchpad\resume3_20260911\tiling"
$ESM = "X:\Programs\Steam\steamapps\common\Fallout 4\Data\Fallout4.esm"
. "$R\scratchpad\bakeperf1_20260911\no_crash_dialog.ps1"

$tiles = @{ 't2024' = @('-20','24','-17','27'); 't2020' = @('-20','20','-17','23') }

function Gate {
  $p = Get-Process -Name Fallout4, NifSkope -ErrorAction SilentlyContinue
  if ($p) { Write-Output "REFUSED: a Fallout4 or NifSkope process is up"; exit 2 }
}

function Bake($exe, $variant, $tile, $extra) {
  $o = "$OUT\$variant\$tile"
  if (Test-Path $o) { Remove-Item -Recurse -Force $o }
  New-Item -ItemType Directory -Force -Path "$o\obj" | Out-Null
  New-Item -ItemType Directory -Force -Path "$o\tex" | Out-Null
  Gate
  $r = $tiles[$tile]
  $argv = @('-no-gui','lodgen',$ESM,'--worldspace','3C',
            '--terrain-region',$r[0],$r[1],$r[2],$r[3],'--dim','4',
            '--out-dir',"$o\obj",'--tex-dir',"$o\tex")
  if ($extra) { $argv += $extra }
  $quoted = $argv | ForEach-Object { if ($_ -match '\s') { '"' + $_ + '"' } else { $_ } }
  $sw = [System.Diagnostics.Stopwatch]::StartNew()
  $p = Start-Process -FilePath $exe -ArgumentList $quoted -NoNewWindow -PassThru `
         -RedirectStandardOutput "$o\bake.log" -RedirectStandardError "$o\bake.log.err"
  $p.WaitForExit(); $sw.Stop()
  $n = (Get-ChildItem -Recurse -File "$o\tex").Count
  "BAKE {0,-5} {1,-6} rc={2} wall={3} ms  texfiles={4}" -f $variant, $tile, $p.ExitCode, $sw.ElapsedMilliseconds, $n
}

Get-Date -Format HH:mm:ss
"rung exe : $((Get-Item $RungExe).LastWriteTime.ToString('yyyy-MM-dd HH:mm:ss'))  $((Get-Item $RungExe).Length) B"
"new  exe : $((Get-Item $NewExe ).LastWriteTime.ToString('yyyy-MM-dd HH:mm:ss'))  $((Get-Item $NewExe ).Length) B"

foreach ($t in @('t2024','t2020')) {
  Bake $RungExe 'rung' $t $null
  Bake $NewExe  'back' $t @('--land-tiling','2048')
  Bake $NewExe  'new'  $t $null
}
Gate
Get-Date -Format HH:mm:ss
"BAKES-DONE"
