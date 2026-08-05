#!/usr/bin/env bash
# Does a saved window layout survive being restored again?
#
# THE BUG THIS EXISTS FOR
#
# Upstream restoreUi() ran restoreGeometry() first and restoreState() second.
# Restoring a layout that was saved while MAXIMISED, into a window that
# restoreGeometry() had just flagged maximised but that had not been shown yet,
# faulted 0xC0000005 in QLayout::addChildWidget. Clearing UI/Window State
# "fixed" it, which framed the saved blob as corrupt when it never was:
#
#     saved blob   restored geometry   result
#     maximised    maximised           CRASH
#     maximised    normal              fine
#     normal       maximised           fine
#     normal       normal              fine
#
# Blamed on three separate sessions and twice written up as unreproducible,
# because reproducing it needs the window to be MAXIMISED at close. Once
# 648cfa2 made NifSkope open maximised, every clean close armed the next
# launch and it became a hard 100%.
#
# WHY THIS ONE CANNOT USE _harness.sh
#
# saveUi() returns early if ANY WW_* variable is set, on purpose, so that
# harness layouts never overwrite the user's. WW_WINDOW_AT is a WW_ variable.
# So the one flag that places the window off the primary monitor also disables
# the write path this test is about, and the round trip has to run as a plain
# launch.
#
# Placement is done instead by seeding UI/Window Geometry: showMaximized()
# maximises onto whichever screen the restored geometry lands on. The script
# ASSERTS it landed off-primary and aborts if it did not, because the failure
# mode is a window over someone's work.
#
# It writes to the real settings key, so it backs the key up first and puts it
# back on exit, including on failure.
#
# Run:  bash tests/spells/window_state_roundtrip.sh
set -u

EXE="$(cd "$(dirname "$0")/../.." && pwd)/release/NifSkope.exe"
[ -x "$EXE" ] || { echo "FAIL: no binary at $EXE"; exit 1; }

# Second monitor, matching _harness.sh. x1 of the primary is 0..1919.
SECOND_X=1920

ps() { powershell.exe -NoProfile -NonInteractive -Command "$1"; }

cat > /tmp/wsrt.ps1 <<'PSEOF'
$ErrorActionPreference = "Stop"
$K   = "HKCU:\Software\NifTools\NifSkope 2.0\UI"
$EXE = $env:WSRT_EXE
$SX  = [int]$env:WSRT_SECOND_X
$bk  = "$env:TEMP\wsrt_backup.reg"

Add-Type @"
using System;using System.Runtime.InteropServices;using System.Drawing;
public class RT {
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out Rectangle r);
}
"@ -ReferencedAssemblies System.Drawing

function Fail($m) { Write-Output "FAIL: $m"; exit 1 }

reg export "HKCU\Software\NifTools\NifSkope 2.0" $bk /y | Out-Null
try {
  # --- seed a maximised-on-second-monitor geometry -------------------------
  # QSettings stores the QByteArray as the UTF-16 string "@ByteArray(<raw>)",
  # so payload byte i sits at blob offset 22 + 2i. Layout of saveGeometry():
  # magic 0-3, major 4-5, minor 6-7, frame(x1,y1,x2,y2) 8-23,
  # normal(x1,y1,x2,y2) 24-39, screenNumber 40-43.
  $g = (Get-ItemProperty $K -EA SilentlyContinue)."Window Geometry"
  if (-not $g) { Fail "no Window Geometry to seed from - open NifSkope once first" }
  function GetI([byte[]]$b, [int]$i) {
    ($b[22+2*$i] -shl 24) -bor ($b[22+2*($i+1)] -shl 16) -bor ($b[22+2*($i+2)] -shl 8) -bor $b[22+2*($i+3)]
  }
  function SetI([byte[]]$b, [int]$i, [int]$v) {
    $b[22+2*$i]     = ($v -shr 24) -band 0xff; $b[22+2*($i+1)] = ($v -shr 16) -band 0xff
    $b[22+2*($i+2)] = ($v -shr 8)  -band 0xff; $b[22+2*($i+3)] = $v -band 0xff
  }
  $magic = GetI $g 0
  if ($magic -ne 0x01D9D0CB) { Fail ("geometry magic 0x{0:X} - format changed, refusing to patch" -f $magic) }
  foreach ($off in 8,16,24,32) {            # frame.x1, frame.x2, normal.x1, normal.x2
    $v = GetI $g $off
    if ($v -lt $SX) { SetI $g $off ($v + $SX) }
  }
  SetI $g 40 1                               # screenNumber -> not the primary
  Set-ItemProperty -Path $K -Name "Window Geometry" -Value $g -Type Binary
  Remove-ItemProperty -Path $K -Name "Window State" -EA SilentlyContinue

  # --- cycle 1: open, confirm placement, close cleanly so saveUi() runs ----
  $p = Start-Process -FilePath $EXE -PassThru
  $h = [IntPtr]::Zero; $sw = [Diagnostics.Stopwatch]::StartNew()
  while ($sw.Elapsed.TotalSeconds -lt 30) {
    $p.Refresh(); if ($p.HasExited) { Fail ("cycle 1 crashed 0x{0:X8}" -f $p.ExitCode) }
    if ($p.MainWindowHandle -ne [IntPtr]::Zero) { $h = $p.MainWindowHandle; break }
    Start-Sleep -Milliseconds 200
  }
  if ($h -eq [IntPtr]::Zero) { $p.Kill(); Fail "cycle 1 never showed a window" }
  $r = New-Object Drawing.Rectangle
  [RT]::GetWindowRect($h, [ref]$r) | Out-Null
  if ($r.X -lt $SX) {
    $p.CloseMainWindow() | Out-Null; $p.WaitForExit(15000) | Out-Null
    Fail "window landed at x=$($r.X), on the PRIMARY monitor - seeding failed, aborted"
  }
  Write-Output "  cycle 1: window at x=$($r.X) (off-primary, ok)"
  $p.CloseMainWindow() | Out-Null
  if (-not $p.WaitForExit(30000)) { $p.Kill(); Fail "cycle 1 would not close" }
  $ws = (Get-ItemProperty $K)."Window State"
  if (-not $ws) { Fail "cycle 1 wrote no Window State - saveUi() did not run (a WW_* var set?)" }
  Write-Output "  cycle 1: saved $($ws.Length) bytes while maximised"

  # --- cycle 2: THE ASSERTION. This is what faulted on the old ordering. ---
  $p = Start-Process -FilePath $EXE -PassThru
  if ($p.WaitForExit(30000)) { Fail ("cycle 2 crashed 0x{0:X8} restoring the layout cycle 1 saved" -f $p.ExitCode) }
  Write-Output "  cycle 2: restored the maximised layout without crashing"
  $p.CloseMainWindow() | Out-Null
  if (-not $p.WaitForExit(30000)) { $p.Kill() }
  Write-Output "PASS: 2 cycles, maximised save -> restore"
}
finally {
  reg import $bk 2>&1 | Out-Null
  Remove-Item $bk -EA SilentlyContinue
}
PSEOF

echo "window_state_roundtrip: maximised save -> restore, on the second monitor"
WSRT_EXE="$(cygpath -w "$EXE")" WSRT_SECOND_X="$SECOND_X" \
  powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "$(cygpath -w /tmp/wsrt.ps1)"
rc=$?
[ $rc -eq 0 ] || { echo "window_state_roundtrip FAILED"; exit 1; }
echo "window_state_roundtrip OK"
