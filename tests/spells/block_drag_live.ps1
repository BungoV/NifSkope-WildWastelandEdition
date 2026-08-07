# Drive a REAL mouse drag at NifSkope's block list and read what it resolved to.
#
#   *** THIS SEIZES THE PHYSICAL MOUSE. RUN IT YOURSELF, DELIBERATELY. ***
#
# It is not a harness you fire off while someone is at the machine. Placing the
# window on the second monitor keeps a WINDOW out of the way; this takes the
# input device, which is shared with whatever else is being done on any monitor.
# It was run once mid-task and dragged things around in the app the user was
# working in. Ask, every time, and do not re-run it "to confirm" -- if it has
# already answered the question, report the answer.
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
# IT TAKES OVER THE MOUSE, now for about fifteen seconds across four drags. The
# window goes on the SECOND monitor (WW_WINDOW_AT) so it never covers the primary
# screen -- but the pointer is not per-monitor, and whatever is under it wherever
# you are working will receive clicks and drags. Do not run it unattended.
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
  [DllImport("user32.dll")] public static extern void keybd_event(byte vk,byte scan,uint f,IntPtr e);
  public const uint DOWN=0x0002, UP=0x0004, KEYUP=0x0002;
  public const byte VK_SHIFT=0x10, VK_CONTROL=0x11;
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
foreach ($need in 0,1,4,7,10) {
  if (-not $rows.ContainsKey($need)) {
    Write-Output "missing row $need - is the fixture four meshes under one root?"
    $p.CloseMainWindow() | Out-Null; exit 1
  }
}
$viewportBottom = 0
foreach ($line in Get-Content $log) {
  if ($line -match '^viewport global (-?\d+),(-?\d+) (\d+)x(\d+)$') {
    $viewportBottom = [int]$Matches[2] + [int]$Matches[4] - 1
  }
}

function Drag($from, $to, $vk) {
  # click first: the payload is the selection
  [M]::SetCursorPos($from.x,$from.y) | Out-Null; Start-Sleep -Milliseconds 350
  [M]::mouse_event([M]::DOWN,0,0,0,[IntPtr]::Zero); Start-Sleep -Milliseconds 120
  [M]::mouse_event([M]::UP,0,0,0,[IntPtr]::Zero);   Start-Sleep -Milliseconds 450

  if ($vk) { [M]::keybd_event($vk,0,0,[IntPtr]::Zero); Start-Sleep -Milliseconds 80 }
  [M]::SetCursorPos($from.x,$from.y) | Out-Null; Start-Sleep -Milliseconds 200
  [M]::mouse_event([M]::DOWN,0,0,0,[IntPtr]::Zero); Start-Sleep -Milliseconds 150
  for ($i=1; $i -le 12; $i++) {
    $x = [int]($from.x + ($to.x - $from.x) * $i / 12)
    $y = [int]($from.y + ($to.y - $from.y) * $i / 12)
    [M]::SetCursorPos($x,$y) | Out-Null; Start-Sleep -Milliseconds 55
  }
  # settle on the target so the LAST DragMove is the one that matters
  for ($i=0; $i -lt 5; $i++) { [M]::SetCursorPos($to.x,$to.y + ($i % 2)) | Out-Null; Start-Sleep -Milliseconds 80 }
  [M]::mouse_event([M]::UP,0,0,0,[IntPtr]::Zero)
  if ($vk) { [M]::keybd_event($vk,0,[M]::KEYUP,[IntPtr]::Zero) }
  Start-Sleep -Milliseconds 1000
}

function Verdict($name) {
  # the log holds only the most recent drag, so this reads that one
  $txt = Get-Content $log -Raw
  $drop = ($txt -split "`n") | Where-Object { $_ -match '^DROP ' } | Select-Object -Last 1
  $moved = ($txt -split "`n") | Where-Object { $_ -match '^\s+-> moved' } | Select-Object -Last 1
  $moves = (($txt -split "`n") | Where-Object { $_ -match '^move at' }).Count
  if (-not $drop)  { Write-Output ("  FAIL {0}: no DROP reached the list ({1} moves)" -f $name,$moves); return $false }
  if (-not $moved) { Write-Output ("  FAIL {0}: drop reached the list but nothing was written" -f $name); return $false }
  Write-Output ("  ok   {0}: {1} moves | {2} | {3}" -f $name,$moves,$drop.Trim(),$moved.Trim())
  return $true
}

$mid  = { param($b) @{ x = $rows[$b].x + [int]($rows[$b].w/3); y = $rows[$b].y + [int]($rows[$b].h/2) } }
$top  = { param($b) @{ x = $rows[$b].x + [int]($rows[$b].w/3); y = $rows[$b].y + 3 } }
$fails = 0

# 1. reorder: block 1 into the gap above block 10
if (-not (Verdict 'setup' | Out-Null)) {}
Drag (& $mid 1) (& $top 10) $null
if (-not (Verdict 'reorder into a gap')) { $fails++ }

# 2. onto a NiNode: block 4 into the root, which is a re-parent not a reorder
Drag (& $mid 4) (& $mid 0) $null
if (-not (Verdict 'onto the root node')) { $fails++ }

# 3. Shift: the same, keeping the local transform
Drag (& $mid 7) (& $top 4) ([M]::VK_SHIFT)
if (-not (Verdict 'Shift held')) { $fails++ }

# 4. the empty space below the last row = the end of the list
$below = @{ x = $rows[1].x + 20; y = $viewportBottom - 6 }
Drag (& $mid 1) $below $null
if (-not (Verdict 'the empty space below the rows')) { $fails++ }

$p.CloseMainWindow() | Out-Null
Start-Sleep -Seconds 2
if (-not $p.HasExited) { $p.Kill() }

if ($fails -gt 0) { Write-Output "FAILED ($fails)"; exit 1 }
Write-Output 'PASS'
exit 0
