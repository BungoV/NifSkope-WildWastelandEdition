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

# Run native registry operations as real child processes. PowerShell 5 turns
# some harmless native STDERR into ErrorRecords, while the former cmd wrapper
# hid the exit status completely; Process.ExitCode is the unambiguous result.
function RunReg([string]$arguments) {
  $rp = Start-Process -FilePath "$env:SystemRoot\System32\reg.exe" `
    -ArgumentList $arguments -WindowStyle Hidden -Wait -PassThru
  return $rp.ExitCode
}

$exportRc = RunReg ('export "HKCU\Software\NifTools\NifSkope 2.0" "' + $bk + '" /y')
if ($exportRc -ne 0 -or -not (Test-Path $bk)) {
  Fail "could not snapshot NifSkope settings; refusing to run"
}
try {
  # --- seed a maximised-on-second-monitor geometry -------------------------
  # QSettings stores the QByteArray as the UTF-16 string "@ByteArray(<raw>)",
  # so payload byte i sits at blob offset 22 + 2i. Layout of saveGeometry():
  # magic 0-3, major 4-5, minor 6-7, frame(x1,y1,x2,y2) 8-23,
  # normal(x1,y1,x2,y2) 24-39, screenNumber 40-43.
  $g = (Get-ItemProperty $K -EA SilentlyContinue)."Window Geometry"
  if (-not $g) { Fail "no Window Geometry to seed from - open NifSkope once first" }
  # [int] casts are NOT decoration: PowerShell's -shl on a BYTE shifts within the
  # byte's own width, so 0x01 -shl 24 is 0, not 0x01000000. Every high term fell
  # off and the magic read back as 0xCB -- the last byte alone -- which was then
  # reported as "the geometry format changed" and stopped this suite running at
  # all. The stored blob was correct the whole time.
  function GetI([byte[]]$b, [int]$i) {
    ([int]$b[22+2*$i] -shl 24) -bor ([int]$b[22+2*($i+1)] -shl 16) `
      -bor ([int]$b[22+2*($i+2)] -shl 8) -bor [int]$b[22+2*($i+3)]
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
  # GetWindowRect fills a RECT (left, top, RIGHT, BOTTOM) and this is a Rectangle
  # (X, Y, WIDTH, HEIGHT), so $r.Width is really the right edge. Kept, because the
  # centre is what this wants and that reads it correctly either way.
  #
  # THE CENTRE, NOT THE LEFT EDGE. GetWindowRect includes the invisible resize
  # border Windows puts around a window -- about 8 px -- so a window genuinely
  # maximised on a monitor starting at 1920 reports x=1912 and this refused to
  # continue. Which monitor the window is ON is a question about where its middle
  # is, and no frame inset can move that across a screen boundary.
  $cx = [int]((($r.X) + ($r.Width)) / 2)
  if ($cx -lt $SX) {
    $p.CloseMainWindow() | Out-Null; $p.WaitForExit(15000) | Out-Null
    Fail "window centre at x=$cx, on the PRIMARY monitor - seeding failed, aborted"
  }
  Write-Output "  cycle 1: window at x=$($r.X), centre $cx (off-primary, ok)"
  $p.CloseMainWindow() | Out-Null
  if (-not $p.WaitForExit(30000)) { $p.Kill(); Fail "cycle 1 would not close" }
  $ws = (Get-ItemProperty $K)."Window State"
  if (-not $ws) { Fail "cycle 1 wrote no Window State - saveUi() did not run (a WW_* var set?)" }
  Write-Output "  cycle 1: saved $($ws.Length) bytes while maximised"

  # The new left editor is outside QMainWindow's opaque dock graph: its active
  # page and both inner splitters have explicit state. Prove a normal close wrote
  # all four values, then select NIF mode for the restore cycle below.
  $LK = "$K\LeftColumn"
  $ls = Get-ItemProperty $LK -EA SilentlyContinue
  if (-not $ls -or $ls.LayoutSchema -ne 2) { Fail "cycle 1 wrote no LeftColumn schema" }
  if (-not $ls.BlockSplitter -or -not $ls.NifSplitter) { Fail "cycle 1 wrote no splitter states" }
  Set-ItemProperty -Path $LK -Name Mode -Value 1 -Type DWord
  Write-Output "  cycle 1: left-column schema and both splitter states saved"

  # --- cycle 2: THE ASSERTION. This is what faulted on the old ordering. ---
  $p = Start-Process -FilePath $EXE -PassThru
  if ($p.WaitForExit(30000)) { Fail ("cycle 2 crashed 0x{0:X8} restoring the layout cycle 1 saved" -f $p.ExitCode) }
  Write-Output "  cycle 2: restored the maximised layout without crashing"
  $p.CloseMainWindow() | Out-Null
  if (-not $p.WaitForExit(30000)) { $p.Kill() }
  $restoredMode = (Get-ItemProperty $LK).Mode
  if ($restoredMode -ne 1) { Fail "cycle 2 did not restore NIF mode (saved $restoredMode)" }
  Write-Output "  cycle 2: restored and re-saved NIF mode"
  Write-Output "PASS: 2 cycles, maximised save -> restore"
}
finally {
  # Do not let native-command stream handling decide whether the user's settings
  # were restored.  The old cmd /c wrapper swallowed both output AND the exit
  # code; a failed import therefore looked successful and the only backup was
  # deleted.  That left test-only values such as New Document Cube=0 and
  # Suppress Save Confirmation=1 in the real profile.
  #
  # Keep the backup on ANY failure and print its path so recovery remains
  # possible.
  # Import MERGES; it does not remove keys created during the test. The new
  # LeftColumn schema therefore leaked into the real profile on the first run
  # of this expanded suite. Delete exactly the backed-up product key first,
  # then import the complete snapshot so absent-before stays absent-after.
  $deleteRc = RunReg 'delete "HKCU\Software\NifTools\NifSkope 2.0" /f'
  $importRc = if ($deleteRc -eq 0) { RunReg ('import "' + $bk + '"') } else { -1 }
  if ($deleteRc -ne 0 -or $importRc -ne 0) {
    $restoreError = "could not restore the NifSkope settings snapshot (delete=$deleteRc import=$importRc)"
    Write-Output "FATAL: $restoreError"
    Write-Output "FATAL: backup retained at $bk"
    throw $restoreError
  } else {
    Remove-Item $bk -EA SilentlyContinue
  }
}
PSEOF

echo "window_state_roundtrip: maximised save -> restore, on the second monitor"
WSRT_EXE="$(cygpath -w "$EXE")" WSRT_SECOND_X="$SECOND_X" \
  powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "$(cygpath -w /tmp/wsrt.ps1)"
rc=$?
[ $rc -eq 0 ] || { echo "window_state_roundtrip FAILED"; exit 1; }
echo "window_state_roundtrip OK"
