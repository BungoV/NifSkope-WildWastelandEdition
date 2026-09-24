. "$PSScriptRoot\no_crash_dialog.ps1"

# BAKEPERF1: the rung's own run-to-run spread was 15.3 s and 19.8 s on the SAME
# region, so a single pair of numbers cannot say whether the default got slower.
# Three runs each, alternating, and the MEDIAN is what the report quotes.
$S = "E:\Projects\NifskopeWildWastelandEdition\scratchpad\bakeperf1_20260911"
$RUNG = "E:\Projects\NifskopeWildWastelandEdition\release\NifSkope.before_bakeperf1.exe"
$NEW = "E:\Projects\NifskopeWildWastelandEdition\release\NifSkope.exe"
$R1 = "-20 24 -9 35"

function Med($a) { $s = $a | Sort-Object; return $s[[int]([math]::Floor($s.Count / 2))] }

$rungWall = @(); $rungTex = @(); $rungMesh = @()
$newWall = @();  $newTex = @();  $newMesh = @()

foreach ($i in 1..3) {
  foreach ($side in @('rung', 'new')) {
    $exe = if ($side -eq 'rung') { $RUNG } else { $NEW }
    $out = "$S\timing_${side}_$i"
    $r = & "$S\bake_run.ps1" -Exe $exe -Out $out -Region $R1
    $w = [int](($r | Select-String 'WALL_MS=(\d+)').Matches.Groups[1].Value)
    $line = ($r | Select-String 'stage times:').ToString()
    $me = [double]([regex]::Match($line, 'meshes ([0-9.]+) s').Groups[1].Value)
    $te = [double]([regex]::Match($line, 'textures ([0-9.]+) s').Groups[1].Value)
    "$side run $i : wall ${w} ms, meshes $me s, textures $te s"
    if ($side -eq 'rung') { $rungWall += $w; $rungMesh += $me; $rungTex += $te }
    else { $newWall += $w; $newMesh += $me; $newTex += $te }
    Remove-Item -Recurse -Force $out -ErrorAction SilentlyContinue
  }
}

""
"MEDIAN  rung : wall $(Med $rungWall) ms, meshes $(Med $rungMesh) s, textures $(Med $rungTex) s"
"MEDIAN  new  : wall $(Med $newWall) ms, meshes $(Med $newMesh) s, textures $(Med $newTex) s"
"SPREAD  rung : wall $($rungWall -join '/') ms, textures $($rungTex -join '/') s"
"SPREAD  new  : wall $($newWall -join '/') ms, textures $($newTex -join '/') s"
