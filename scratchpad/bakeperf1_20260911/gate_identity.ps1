param([switch]$SkipBakes)

. "$PSScriptRoot\no_crash_dialog.ps1"   # BAKEPERF1: no crash dialog on his desktop

# BAKEPERF1 gates P2 / P3 / P4: four bakes on the new exe, four comparisons.
#
#   base_rN          the RUNG exe (13:24:12), inherently serial   [already taken]
#   new_rN_t1        the new exe, SHIPPED DEFAULT (chunk queue serial)
#   new_rN_tN        the new exe, --chunk-threads 16 (the opt-in fan-out)
#
# Region 1 = Sanctuary, cells -20 24 -9 35     (9 chunks at dim 4)
# Region 2 = downtown Boston, cells 0 -12 19 7 (25 chunks at dim 4)
$S = "E:\Projects\NifskopeWildWastelandEdition\scratchpad\bakeperf1_20260911"
$NEW = "E:\Projects\NifskopeWildWastelandEdition\release\NifSkope.exe"
$R1 = "-20 24 -9 35"
$R2 = "0 -12 19 7"

if (-not $SkipBakes) {
  foreach ($r in @(@('r1', $R1), @('r2', $R2))) {
    foreach ($ct in @(1, 16)) {
      $tag = if ($ct -eq 1) { 't1' } else { 'tN' }
      $out = "$S\new_$($r[0])_$tag"
      Write-Output "=== bake $($r[0]) chunk-threads=$ct ==="
      & "$S\bake_run.ps1" -Exe $NEW -Out $out -Region $r[1] -ChunkThreads $ct |
        Tee-Object -FilePath "$S\new_$($r[0])_$tag.txt"
    }
  }
}

Write-Output ""
Write-Output "================ IDENTITY ================"
foreach ($r in @('r1', 'r2')) {
  & python "$S\bake_diff.py" "$S\base_$r" "$S\new_$($r)_t1" --label "P4 $r  rung exe  vs  new exe, shipped default"
  & python "$S\bake_diff.py" "$S\new_$($r)_t1" "$S\new_$($r)_tN" --label "P2 $r  serial chunk queue  vs  --chunk-threads 16"
}
