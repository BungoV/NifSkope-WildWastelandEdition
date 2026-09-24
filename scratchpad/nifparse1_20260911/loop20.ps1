# NIFPARSE1 gate N2: TWENTY CONSECUTIVE 16-thread runs, one region, no fault.
#
#   loop20.ps1 -Region "-20 24 -9 35" -Tag r1 [-Runs 20] [-Exe <exe>]
#
# Prints each run's exit code (as an NTSTATUS when it is negative) and its wall
# time, so a red gate says AT WHICH RUN it went red rather than "it crashed".
# The first run's output tree is KEPT as the identity subject; the rest are
# deleted as they pass, because twenty 25-chunk trees is 1.5 GB.
param(
  [Parameter(Mandatory=$true)][string]$Region,
  [Parameter(Mandatory=$true)][string]$Tag,
  [int]$Runs = 20,
  [int]$ChunkThreads = 16,
  [string]$Exe = "E:\Projects\NifskopeWildWastelandEdition\release\NifSkope.exe"
)

$S = "E:\Projects\NifskopeWildWastelandEdition\scratchpad\nifparse1_20260911"
$B = "E:\Projects\NifskopeWildWastelandEdition\scratchpad\bakeperf1_20260911"
. "$B\no_crash_dialog.ps1"   # N6: no Application Error box on his desktop

function Nts($c) {
  if ($c -lt 0) { return ('0x{0:X8}' -f ([uint32]([int64]$c + 4294967296))) }
  return "$c"
}

$fails = 0
$firstOut = "$S\loop_${Tag}_keep"
if (Test-Path $firstOut) { Remove-Item -Recurse -Force $firstOut }

foreach ($i in 1..$Runs) {
  $out = if ($i -eq 1) { $firstOut } else { "$S\loop_${Tag}_$i" }
  $r = & "$B\bake_run.ps1" -Exe $Exe -Out $out -Region $Region -ChunkThreads $ChunkThreads
  $rc = [int](($r | Select-String 'RC=(-?\d+)').Matches.Groups[1].Value)
  $w  = [int](($r | Select-String 'WALL_MS=(\d+)').Matches.Groups[1].Value)
  $pk = [double](($r | Select-String 'PEAK_WS_MB=([0-9.]+)').Matches.Groups[1].Value)
  $nf = [int](($r | Select-String 'FILES=(\d+)').Matches.Groups[1].Value)
  $verdict = if ($rc -eq 0) { "ok" } else { $fails++; "FAULT" }
  "run {0,2}/{1}  rc={2,-10} wall={3,7} ms  peakWS={4,8} MB  files={5,4}  {6}" -f `
      $i, $Runs, (Nts $rc), $w, $pk, $nf, $verdict
  if ($i -ne 1) { Remove-Item -Recurse -Force $out -ErrorAction SilentlyContinue }
}

""
if ($fails -eq 0) {
  "GATE N2 $Tag : GREEN - $Runs of $Runs clean at --chunk-threads $ChunkThreads"
} else {
  "GATE N2 $Tag : RED - $fails of $Runs faulted at --chunk-threads $ChunkThreads"
}
