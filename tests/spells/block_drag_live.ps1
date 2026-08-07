# Drive a REAL mouse drag at NifSkope's block list and read what it resolved to.
#
# WHY THIS EXISTS
#
# A native drag is the one path no harness can enter. QApplication::notify routes
# drag and drop through the drag manager, so a synthetic drag event reaches
# neither the widget's event() nor any event filter -- measured at zero, sent to
# the view and to the viewport both. Everything below that boundary is covered by
# block_dragdrop.sh. Nothing above it was covered by anything.
#
# Four fixes for "I cannot drop between two rows" were made by reading code, none
# of them right, while block_dragdrop.sh sat at 44 green checks. This found it in
# one run: the log showed ONE DragEnter and then silence. Ignoring a drag event
# ends the drag over the widget, and a drag begins ON the row being dragged --
# whose neighbouring gaps are exactly the positions that refuse as no-ops -- so
# the first event was a refusal, the refusal killed the event stream, and the
# gesture was dead before it began.
#
# WHAT IT DOES
#
# Opens a fixture with four mesh children, has the program dump every row's global
# rectangle (wwLogBlockListRowGeometry), then drives the physical mouse from the
# first child to the gap above the last one and reads the drag log. A working drag
# ends with a DROP line carrying a position, and "moved 1".
#
# IT TAKES OVER THE MOUSE for about three seconds. The window goes on the SECOND
# monitor (WW_WINDOW_AT), so it never covers the primary screen, but the pointer
# does move -- do not run it while typing somewhere else.
#
# FIXTURE: four BSTriShape children of one root, built with the CLI:
#   NifSkope.exe -no-gui new -o E:/dragfx0.nif
#   NifSkope.exe -no-gui cast E:/dragfx0.nif -s "Block/Duplicate Branch" -b 1 -o E:/dragfx1.nif
#   ...repeated to E:/dragfx.nif
#
# USAGE
#   powershell -File tests/spells/block_drag_live.ps1
$ErrorActionPreference = 'Stop'

Add-Type @"
using System;using System.Runtime.InteropServices;
public class M {
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x,int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f,uint x,uint y,uint d,IntPtr e);
  public const uint DOWN=0x0002, UP=0x0004;
}
"@

$exe = 'E:\Projects\NifskopeWildWastelandEdition\release\NifSkope.exe'
$log = 'E:\drag.txt'
$fix = 'E:/dragfx.nif'
Remove-Item $log -ErrorAction SilentlyContinue

$env:WW_DRAG_LOG  = $log
$env:WW_WINDOW_AT = '1960,40'
$p = Start-Process -FilePath $exe -ArgumentList '--port','45911',$fix -PassThru

# wait for the row geometry dump
$sw = [Diagnostics.Stopwatch]::StartNew()
while ($sw.Elapsed.TotalSeconds -lt 40) {
  if ((Test-Path $log) -and ((Get-Content $log -Raw) -match '--- end rows ---')) { break }
  Start-Sleep -Milliseconds 300
}
if (-not (Test-Path $log)) { Write-Output 'NO LOG WRITTEN'; $p.CloseMainWindow() | Out-Null; exit 1 }

$rows = @{}
foreach ($line in Get-Content $log) {
  if ($line -match '^row block (\d+) \((\S+)\) global (-?\d+),(-?\d+) (\d+)x(\d+)$') {
    $rows[[int]$Matches[1]] = @{ x=[int]$Matches[3]; y=[int]$Matches[4]; w=[int]$Matches[5]; h=[int]$Matches[6]; type=$Matches[2] }
  }
}
Write-Output "rows found: $($rows.Keys | Sort-Object)"
if (-not ($rows.ContainsKey(1) -and $rows.ContainsKey(10))) {
  Write-Output 'missing the rows to drag between'; $p.CloseMainWindow() | Out-Null; exit 1
}

# from the middle of block 1's row, to the TOP portion of block 10's row --
# which is the gap between 7 and 10, position 3 in a [1,4,7,10] Children array
$from = @{ x = $rows[1].x + [int]($rows[1].w/3); y = $rows[1].y + [int]($rows[1].h/2) }
$to   = @{ x = $rows[10].x + [int]($rows[10].w/3); y = $rows[10].y + 3 }
Write-Output "drag from ($($from.x),$($from.y)) to ($($to.x),$($to.y))"

# click it first: the payload is the selection
[M]::SetCursorPos($from.x,$from.y); Start-Sleep -Milliseconds 400
[M]::mouse_event([M]::DOWN,0,0,0,[IntPtr]::Zero); Start-Sleep -Milliseconds 120
[M]::mouse_event([M]::UP,0,0,0,[IntPtr]::Zero);   Start-Sleep -Milliseconds 500

# now the drag: press, walk down in steps so the loop sees motion, release
[M]::SetCursorPos($from.x,$from.y); Start-Sleep -Milliseconds 250
[M]::mouse_event([M]::DOWN,0,0,0,[IntPtr]::Zero); Start-Sleep -Milliseconds 150
for ($i=1; $i -le 14; $i++) {
  $x = [int]($from.x + ($to.x - $from.x) * $i / 14)
  $y = [int]($from.y + ($to.y - $from.y) * $i / 14)
  [M]::SetCursorPos($x,$y); Start-Sleep -Milliseconds 60
}
# settle exactly on the target so the last DragMove is the one that matters
for ($i=0; $i -lt 5; $i++) { [M]::SetCursorPos($to.x,$to.y + ($i % 2)); Start-Sleep -Milliseconds 80 }
[M]::mouse_event([M]::UP,0,0,0,[IntPtr]::Zero)
Start-Sleep -Milliseconds 1200

$p.CloseMainWindow() | Out-Null
Start-Sleep -Seconds 2
if (-not $p.HasExited) { $p.Kill() }

Write-Output '================ LOG ================'
Get-Content $log
