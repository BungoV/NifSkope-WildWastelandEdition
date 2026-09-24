# RESUME3 gate R3, the half that is not the 20x loops:
#   * the SERIAL bake of each region, kept for the identity comparison
#   * the stage-time table, medians of 3, ALTERNATING serial / 16 threads
#   * peak working set on both sides
#
# The 16-thread trees are the ones loop20.ps1 kept (`loop_r1_keep`, `loop_r2_keep`).
# BAKEPERF1 measured a 4.5 s within-exe spread on a 16 s region, so a single
# pair of numbers cannot answer "is it faster"; everything below is a median.
$ErrorActionPreference = 'Continue'
$R = "E:\Projects\NifskopeWildWastelandEdition"
$B = "$R\scratchpad\bakeperf1_20260911"
$O = "$R\scratchpad\resume3_20260911"
. "$B\no_crash_dialog.ps1"
$EXE = "$R\release\NifSkope.exe"

function Med($a) { $s = @($a | Sort-Object); return $s[[int]([math]::Floor($s.Count / 2))] }

function Run($out, $region, $ct) {
  $r = & "$B\bake_run.ps1" -Exe $EXE -Out $out -Region $region -ChunkThreads $ct
  $rc = [int](($r | Select-String 'RC=(-?\d+)').Matches.Groups[1].Value)
  $w  = [int](($r | Select-String 'WALL_MS=(\d+)').Matches.Groups[1].Value)
  $pk = [double](($r | Select-String 'PEAK_WS_MB=([0-9.]+)').Matches.Groups[1].Value)
  $nf = [int](($r | Select-String 'FILES=(\d+)').Matches.Groups[1].Value)
  $line = ($r | Select-String 'stage times:')
  $me = 0.0; $te = 0.0
  if ($line) { $s = $line.ToString()
    $me = [double]([regex]::Match($s, 'meshes ([0-9.]+) s').Groups[1].Value)
    $te = [double]([regex]::Match($s, 'textures ([0-9.]+) s').Groups[1].Value) }
  return [pscustomobject]@{ rc=$rc; wall=$w; peak=$pk; files=$nf; meshes=$me; tex=$te }
}

$regions = @{ 'sanctuary' = '-20 24 -9 35'; 'boston' = '0 -12 19 7' }

foreach ($name in @('sanctuary','boston')) {
  $reg = $regions[$name]
  "=== $name  $(Get-Date -Format HH:mm:ss) ==="

  # the SERIAL tree the identity gate compares against, kept
  $keep = "$O\serial_$name"
  if (Test-Path $keep) { Remove-Item -Recurse -Force $keep }
  $r = Run $keep $reg 1
  "serial KEEP   rc=$($r.rc) wall=$($r.wall) ms peak=$($r.peak) MB files=$($r.files) meshes=$($r.meshes) tex=$($r.tex)"

  # the timing table: 3 more of each, alternating so drift falls on both sides
  $sw=@(); $sp=@(); $sm=@(); $st=@(); $tw=@(); $tp=@(); $tm=@(); $tt=@()
  foreach ($i in 1..3) {
    $o1 = "$O\t_${name}_s$i"; $x = Run $o1 $reg 1
    "  serial  run $i  wall=$($x.wall) ms peak=$($x.peak) MB meshes=$($x.meshes) tex=$($x.tex) rc=$($x.rc)"
    $sw+=$x.wall; $sp+=$x.peak; $sm+=$x.meshes; $st+=$x.tex
    Remove-Item -Recurse -Force $o1 -ErrorAction SilentlyContinue
    $o2 = "$O\t_${name}_t$i"; $y = Run $o2 $reg 16
    "  16thr   run $i  wall=$($y.wall) ms peak=$($y.peak) MB meshes=$($y.meshes) tex=$($y.tex) rc=$($y.rc)"
    $tw+=$y.wall; $tp+=$y.peak; $tm+=$y.meshes; $tt+=$y.tex
    Remove-Item -Recurse -Force $o2 -ErrorAction SilentlyContinue
  }
  "MEDIAN $name serial : wall $(Med $sw) ms  meshes $(Med $sm) s  textures $(Med $st) s  peak $(Med $sp) MB"
  "MEDIAN $name 16thr  : wall $(Med $tw) ms  meshes $(Med $tm) s  textures $(Med $tt) s  peak $(Med $tp) MB"
}
Get-Date -Format HH:mm:ss
"R3-REST-DONE"
