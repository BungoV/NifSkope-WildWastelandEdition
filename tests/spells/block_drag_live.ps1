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
# Seven drags across a scene with three roots and nodes nested two deep, each one
# read back out of the program's own drag log:
#
#   1. into a node that is SHUT -- the block goes inside it, not beside it
#   2. into a row that only this drag's auto-unfold revealed: it rests on a
#      collapsed node until it opens, then steps into what appeared. The check is
#      that the row it landed on was not in the dump this drag started with
#   3. into a second ROOT, so the block leaves the first root's tree entirely
#   4. a root made a child of another root
#   5. out to the blank space, which means no parent at all
#   6. onto a MESH row, which is all gap: it reorders among the mesh's siblings
#   7. the root onto its own descendant, which is refused
#
# TWO THINGS THIS RUN CORRECTED IN ITSELF, both of which had it reporting the
# program as broken when it was not:
#
# "payload [N]" in the log is the block COUNT, not a block number. Reading it as
# the identity of what was picked up made every scenario in a run claim it had
# grabbed block 1 -- which is just how many blocks a one-block drag carries. The
# identity is in the "=== drag start ... first N ===" header.
#
# A REFUSED TARGET NEVER RECEIVES A DROP EVENT. The handler answers a refusal
# with Qt::IgnoreAction and Qt then does not deliver the QDropEvent at all, so
# "no DROP reached the list" is the CORRECT outcome for scenario 7 rather than
# the failure it reads as. What is checked there is the last DragMove -- the
# pointer must have been resting on the illegal target -- and that nothing moved.
#
# GEOMETRY IS READ PER DRAG, MID-DRAG. Every drag begins by dumping every visible
# row's global rectangle, and every drag changes the tree -- so the target is
# computed from the dump this drag just wrote, not from one taken before the
# first. Press, let the dump land, read it, then move. Reading it once at the
# start is how a sweep like this quietly starts aiming at where rows used to be.
#
# IT TAKES OVER THE MOUSE, for about a minute across seven drags. The window goes
# on the SECOND monitor (WW_WINDOW_AT) so it never covers the primary screen --
# but the pointer is not per-monitor, and whatever is under it wherever you are
# working will receive clicks and drags. Do not run it unattended.
#
# FIXTURE: built here, by the program, through WW_DRAGFIXTURE -- three roots
# (Scene Root, SecondRoot, ThirdRoot), Alpha/Beta/Gamma under the first, Beta
# holding BetaChild holding BetaDeep, and one mesh. It cannot be built from the
# CLI: Node/Attach Node and Node/Attach Parent Node both want a QWidget and die
# headless. It used to be a file somebody had made by hand at E:\dragfx.nif, and
# the next session to reach for this script found the file gone and the run dead
# before it started.
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

function ParseRows {
  $rows = @{}
  if (-not (Test-Path $log)) { return $rows }
  foreach ($line in Get-Content $log) {
    if ($line -match '^row block (\d+) \((\S+)\) global (-?\d+),(-?\d+) (\d+)x(\d+)$') {
      $rows[[int]$Matches[1]] = @{ x=[int]$Matches[3]; y=[int]$Matches[4]; w=[int]$Matches[5]; h=[int]$Matches[6]; type=$Matches[2] }
    }
  }
  return $rows
}
function ViewportBottom {
  $v = 0
  foreach ($line in Get-Content $log) {
    if ($line -match '^viewport global (-?\d+),(-?\d+) (\d+)x(\d+)$') { $v = [int]$Matches[2] + [int]$Matches[4] - 1 }
  }
  return $v
}
function Mid($r)  { @{ x = $r.x + [int]($r.w/3); y = $r.y + [int]($r.h/2) } }
function Top($r)  { @{ x = $r.x + [int]($r.w/3); y = $r.y + 3 } }

$exe = 'E:\Projects\NifskopeWildWastelandEdition\release\NifSkope.exe'
$log = Join-Path $env:TEMP 'ww_drag_live.txt'
$fix = (Join-Path $env:TEMP 'ww_dragfx.nif') -replace '\\', '/'
$seed = (Join-Path $env:TEMP 'ww_dragfx_seed.nif') -replace '\\', '/'
Remove-Item $log -ErrorAction SilentlyContinue

# The fixture, built by the program itself so this script depends on nothing that
# has to survive between sessions.
& $exe -no-gui new -o $seed | Out-Null
$env:WW_DRAGFIXTURE = $fix
$env:WW_WINDOW_AT = '1960,40'
$b = Start-Process -FilePath $exe -ArgumentList '--port', '45913', $seed -PassThru -Wait
Remove-Item Env:\WW_DRAGFIXTURE
$fixWin = $fix -replace '/', '\'
if (-not (Test-Path $fixWin)) { Write-Output 'COULD NOT BUILD THE FIXTURE'; exit 1 }
$note = $fixWin + '.txt'
if (Test-Path $note) { Write-Host ("fixture: " + ((Get-Content $note | Select-Object -First 1))) }

$env:WW_DRAG_LOG = $log
$p = Start-Process -FilePath $exe -ArgumentList '--port','45911',$fix -PassThru

# The program dumps the rows once the file is open as well as at every drag
# start, so there is something to aim the FIRST drag at. Wait for that one.
$sw = [Diagnostics.Stopwatch]::StartNew()
while ($sw.Elapsed.TotalSeconds -lt 40) {
  if ((Test-Path $log) -and ((Get-Content $log -Raw) -match '--- end rows ---')) { break }
  Start-Sleep -Milliseconds 300
}
if (-not (Test-Path $log)) { Write-Output 'NO LOG WRITTEN'; $p.CloseMainWindow() | Out-Null; exit 1 }
$start = ParseRows
foreach ($need in 0,1,4,5,8,9,11) {
  if (-not $start.ContainsKey($need)) {
    Write-Output "missing row $need - the fixture is not the one this expects"
    $p.CloseMainWindow() | Out-Null; exit 1
  }
}
Write-Host ("rows visible at the start: " + (($start.Keys | Sort-Object) -join ' '))


# A DRAG STARTED AND THEN CANCELLED, purely to make the program dump the rows.
#
# The dump is written at drag start, so the newest one on disk describes the tree
# as it was BEFORE the last drag moved anything -- and pressing on a row whose
# position is one move out of date grabs the wrong block. Escape cancels a Qt drag
# without a drop, so this costs a second and changes nothing. Where it presses
# does not matter; that it dumps does.
function RefreshRows($anchor) {
  if (-not $anchor) { return ParseRows }
  [M]::SetCursorPos($anchor.x,$anchor.y) | Out-Null; Start-Sleep -Milliseconds 200
  [M]::mouse_event([M]::DOWN,0,0,0,[IntPtr]::Zero); Start-Sleep -Milliseconds 120
  for ($i=1; $i -le 4; $i++) { [M]::SetCursorPos($anchor.x, $anchor.y + $i*5) | Out-Null; Start-Sleep -Milliseconds 60 }
  Start-Sleep -Milliseconds 400
  [M]::keybd_event(0x1B,0,0,[IntPtr]::Zero); Start-Sleep -Milliseconds 60   # VK_ESCAPE
  [M]::keybd_event(0x1B,0,[M]::KEYUP,[IntPtr]::Zero); Start-Sleep -Milliseconds 200
  [M]::mouse_event([M]::UP,0,0,0,[IntPtr]::Zero); Start-Sleep -Milliseconds 400
  return ParseRows
}

# SHUT A BRANCH, so there is something for the drag to unfold.
#
# The tree is fully open when the file loads -- every one of the twelve blocks has
# a row -- so a hover over a node reveals nothing and the auto-unfold scenario
# would pass while measuring an unfold that never had to happen. Click the row,
# then Left, which is how a QTreeView collapses from the keyboard.
function Collapse($rows, $block) {
  if (-not $rows.ContainsKey($block)) { return }
  $at = Mid $rows[$block]
  [M]::SetCursorPos($at.x,$at.y) | Out-Null; Start-Sleep -Milliseconds 250
  [M]::mouse_event([M]::DOWN,0,0,0,[IntPtr]::Zero); Start-Sleep -Milliseconds 100
  [M]::mouse_event([M]::UP,0,0,0,[IntPtr]::Zero);   Start-Sleep -Milliseconds 300
  [M]::keybd_event(0x25,0,0,[IntPtr]::Zero); Start-Sleep -Milliseconds 60        # VK_LEFT
  [M]::keybd_event(0x25,0,[M]::KEYUP,[IntPtr]::Zero); Start-Sleep -Milliseconds 400
}

# Refresh, press on the source, let this drag's own dump land, then let the caller
# work out where to go from THAT. $plan is handed the fresh rows and returns
# @{ point = ... } or @{ hover = ...; point = ... } to rest somewhere on the way
# — which is how auto-unfold is reached, since it needs the pointer to stay put.
function DoDrag($sourceBlock, $plan, $vk) {
  $known = ParseRows
  $anchor = $null
  if ($known.ContainsKey(0)) { $anchor = Mid $known[0] }
  $rows = RefreshRows $anchor
  if (-not $rows.ContainsKey($sourceBlock)) { return $null }
  $from = Mid $rows[$sourceBlock]

  <# CLICK FIRST -- the payload is the selection -- AND THEN WAIT OUT THE
     DOUBLE-CLICK TIME. This used to wait 450 ms, which is inside Windows' 500 ms
     default, so the click and the drag's own press were delivered as a DOUBLE
     CLICK: that opens the inline rename editor, the row was never selected, and
     the drag carried whatever had been selected before. Every drag in the run
     picked up block 1 and the script had no way to tell, because it never
     checked what it had picked up. Moving away and back defeats the other half
     of the test, which is that both clicks land in the same small rectangle. #>
  [M]::SetCursorPos($from.x,$from.y) | Out-Null; Start-Sleep -Milliseconds 350
  [M]::mouse_event([M]::DOWN,0,0,0,[IntPtr]::Zero); Start-Sleep -Milliseconds 120
  [M]::mouse_event([M]::UP,0,0,0,[IntPtr]::Zero);   Start-Sleep -Milliseconds 250
  [M]::SetCursorPos($from.x + 60,$from.y) | Out-Null; Start-Sleep -Milliseconds 400
  [M]::SetCursorPos($from.x,$from.y) | Out-Null;      Start-Sleep -Milliseconds 250

  if ($vk) { [M]::keybd_event($vk,0,0,[IntPtr]::Zero); Start-Sleep -Milliseconds 80 }
  [M]::SetCursorPos($from.x,$from.y) | Out-Null; Start-Sleep -Milliseconds 200
  [M]::mouse_event([M]::DOWN,0,0,0,[IntPtr]::Zero); Start-Sleep -Milliseconds 150
  # past the drag threshold, which is what makes the program dump the rows
  for ($i=1; $i -le 4; $i++) { [M]::SetCursorPos($from.x, $from.y + $i*4) | Out-Null; Start-Sleep -Milliseconds 60 }
  Start-Sleep -Milliseconds 400

  $fresh = ParseRows
  $step = & $plan $fresh
  if (-not $step) {
    # nowhere to aim: release where we are and let the verdict say so
    [M]::mouse_event([M]::UP,0,0,0,[IntPtr]::Zero)
    if ($vk) { [M]::keybd_event($vk,0,[M]::KEYUP,[IntPtr]::Zero) }
    Start-Sleep -Milliseconds 800
    return $fresh
  }

  $waypoints = @()
  if ($step.ContainsKey('hover')) { $waypoints += ,@($step.hover, 1100) }
  $waypoints += ,@($step.point, 0)

  $cur = @{ x = $from.x; y = $from.y + 16 }
  foreach ($w in $waypoints) {
    $to = $w[0]
    for ($i=1; $i -le 10; $i++) {
      $x = [int]($cur.x + ($to.x - $cur.x) * $i / 10)
      $y = [int]($cur.y + ($to.y - $cur.y) * $i / 10)
      [M]::SetCursorPos($x,$y) | Out-Null; Start-Sleep -Milliseconds 45
    }
    # settle, so the LAST DragMove is the one that matters -- and so a hover
    # waypoint rests long enough for auto-unfold to fire
    $dwell = $w[1]
    if ($dwell -gt 0) {
      $spent = 0
      while ($spent -lt $dwell) { [M]::SetCursorPos($to.x, $to.y + ($spent % 2)) | Out-Null; Start-Sleep -Milliseconds 90; $spent += 90 }
    } else {
      for ($i=0; $i -lt 5; $i++) { [M]::SetCursorPos($to.x,$to.y + ($i % 2)) | Out-Null; Start-Sleep -Milliseconds 80 }
    }
    $cur = $to
  }
  [M]::mouse_event([M]::UP,0,0,0,[IntPtr]::Zero)
  if ($vk) { [M]::keybd_event($vk,0,[M]::KEYUP,[IntPtr]::Zero) }
  Start-Sleep -Milliseconds 1000
  return $fresh
}

# SAY IT WITH Write-Host, RETURN ONLY THE VERDICT.
#
# This used to Write-OUTPUT the line and then return the bool, and every caller
# is `if (-not (Verdict '...')) { $fails++ }` -- which captures the function's
# whole output as the condition. A two-element array (the message, then the bool)
# is truthy whatever the bool says, so `-not` was always false, $fails could never
# increment, and the script printed PASS no matter what happened. The messages
# never reached the console either, having been eaten by the condition. The one
# test that reaches above the native-drag boundary could not fail.
function Verdict($name, $expect) {
  $txt = Get-Content $log -Raw
  $lines = $txt -split "`n"
  $drop  = $lines | Where-Object { $_ -match '^DROP ' } | Select-Object -Last 1
  $moved = $lines | Where-Object { $_ -match '^\s+-> ' } | Select-Object -Last 1
  $moves = ($lines | Where-Object { $_ -match '^move at' }).Count
  <# A REFUSED TARGET NEVER GETS A DROP EVENT AT ALL.
     The drop handler answers a refusal with Qt::IgnoreAction, and Qt does not
     deliver a QDropEvent when the release lands on a target that last reported
     that -- the drag simply ends. So "no DROP" is the CORRECT outcome for a
     refusal, and this used to read it as the failure it looks like. What is
     checked instead is the last DragMove: it must have resolved to the illegal
     target, so the pointer really was over it, and nothing may have moved. #>
  if ($expect.ContainsKey('noDrop')) {
    $last = $lines | Where-Object { $_ -match '^move at' } | Select-Object -Last 1
    $lastSpot = -99
    if ($last -match 'spot (-?\d+) pos') { $lastSpot = [int]$Matches[1] }
    if ($drop)  { Write-Host ("  FAIL {0}: a refused target still took the drop | {1}" -f $name,$drop.Trim()); return $false }
    if ($moved) { Write-Host ("  FAIL {0}: nothing should have been written | {1}" -f $name,$moved.Trim()); return $false }
    if ($lastSpot -ne $expect.noDrop) {
      Write-Host ("  FAIL {0}: the pointer ended over {1}, not {2} | {3}" -f $name,$lastSpot,$expect.noDrop,$last.Trim()); return $false
    }
    Write-Host ("  ok   {0}: {1} moves | rested on {2}, no drop delivered" -f $name,$moves,$lastSpot)
    return $true
  }

  if (-not $drop)  { Write-Host ("  FAIL {0}: no DROP reached the list ({1} moves)" -f $name,$moves); return $false }
  if (-not $moved) { Write-Host ("  FAIL {0}: the drop reached the list but nothing was written" -f $name); return $false }

  $spot = -99; $pos = -99; $payload = -99
  if ($drop -match 'spot (-?\d+) pos (-?\d+)') { $spot = [int]$Matches[1]; $pos = [int]$Matches[2] }
  # WHICH BLOCK WAS ACTUALLY PICKED UP. Rows move as the tree changes, so a press
  # aimed from stale geometry grabs the wrong one -- and every check below would
  # then be measuring a scenario nobody wrote.
  #
  # NOT from "payload [N]" on the DROP line: that is the block COUNT, and reading
  # it as a block number made every scenario in a run report picking up block 1,
  # which is simply how many blocks a single-block drag carries. The identity is
  # in the header the drag writes when it starts.
  $head = $lines | Where-Object { $_ -match '^=== drag start' } | Select-Object -Last 1
  if ($head -match 'first (\d+)') { $payload = [int]$Matches[1] }
  $count = 0
  if ($moved -match 'moved (\d+)') { $count = [int]$Matches[1] }
  $refused = ($moved -notmatch 'refusals: none')

  $why = $null
  if ($expect.ContainsKey('payload') -and $payload -ne $expect.payload) { $why = "picked up block $payload, not $($expect.payload)" }
  if ($expect.ContainsKey('spot')    -and $spot -ne $expect.spot)   { $why = "landed on $spot, wanted $($expect.spot)" }
  if ($expect.ContainsKey('moved')   -and $count -ne $expect.moved) { $why = "moved $count, wanted $($expect.moved)" }
  if ($expect.ContainsKey('refused') -and $refused -ne $expect.refused) {
    $why = $(if ($expect.refused) { "was allowed, should have been refused" } else { "was refused: $($moved.Trim())" })
  }
  if ($expect.ContainsKey('posAtLeast') -and $pos -lt $expect.posAtLeast) { $why = "pos $pos, wanted an insertion position" }
  # "only the drag's own unfold could have shown me this row": the block it landed
  # on was not in the dump this drag started with
  if ($expect.ContainsKey('revealed') -and $expect.revealed -and $expect.rows.ContainsKey($spot)) {
    $why = "block $spot was already visible, so nothing had to unfold"
  }

  if ($why) { Write-Host ("  FAIL {0}: {1} | {2}" -f $name,$why,$drop.Trim()); return $false }
  Write-Host ("  ok   {0}: {1} moves | spot {2} pos {3} | {4}" -f $name,$moves,$spot,$pos,$moved.Trim())
  return $true
}

$fails = 0
$did = 0

# 1. INTO A COLLAPSED NODE. Beta holds two levels; shut it first, then dropping
#    on its row must put the block inside it rather than beside it.
Collapse $start 5
$rows = DoDrag 4 { param($r) if ($r.ContainsKey(5)) { @{ point = (Mid $r[5]) } } } $null
if (-not (Verdict 'Alpha into the collapsed Beta' @{ payload = 4; spot = 5; moved = 1; refused = $false })) { $fails++ }; $did++

# 2. INTO A ROW THE DRAG ITSELF REVEALED. Shut Beta again -- scenario 1's drop
#    opened it to show where the block landed -- then hover it until auto-unfold
#    opens it and step one row down into what appeared. The check is that the
#    block it landed on was NOT in the dump this drag started with, so an unfold
#    is the only way the pointer could have reached it.
$rows = RefreshRows (Mid $rows[0])
Collapse $rows 5
$rows = DoDrag 8 {
  param($r)
  if (-not $r.ContainsKey(5)) { return $null }
  $beta = $r[5]
  @{ hover = (Mid $beta); point = @{ x = $beta.x + 40; y = $beta.y + $beta.h + [int]($beta.h/2) } }
} $null
if (-not (Verdict 'Gamma into a row the unfold revealed' @{ payload = 8; moved = 1; refused = $false; revealed = $true; rows = $rows })) { $fails++ }; $did++

# 3. BETWEEN TWO ROOTS. SecondRoot is a root of its own; a block dragged into it
#    leaves the first root's tree entirely.
$rows = DoDrag 4 { param($r) if ($r.ContainsKey(9)) { @{ point = (Mid $r[9]) } } } $null
if (-not (Verdict 'a node into the second root' @{ payload = 4; spot = 9; moved = 1; refused = $false })) { $fails++ }; $did++

# 4. A ROOT BECOMES A CHILD. ThirdRoot has no parent at all until this.
$rows = DoDrag 11 { param($r) if ($r.ContainsKey(0)) { @{ point = (Mid $r[0]) } } } $null
if (-not (Verdict 'ThirdRoot made a child of Scene Root' @{ payload = 11; spot = 0; moved = 1; refused = $false })) { $fails++ }; $did++

# 5. OUT. The blank space below every row means no parent at all.
$rows = DoDrag 5 { param($r) @{ point = @{ x = $r[0].x + 20; y = (ViewportBottom) - 6 } } } $null
if (-not (Verdict 'Beta dragged out to blank space' @{ payload = 5; spot = -1; moved = 1; refused = $false })) { $fails++ }; $did++

# 6. A MESH ROW IS ALL GAP. There is nothing to drop INSIDE a mesh, so its whole
#    height reorders among its siblings rather than offering itself as a parent.
#    Written first as "dropping on a mesh is refused", which is the wrong idea of
#    the feature: the refusal exists, but no point on that row can reach it.
$rows = DoDrag 8 { param($r) if ($r.ContainsKey(1)) { @{ point = (Mid $r[1]) } } } $null
if (-not (Verdict 'a drop on a mesh row reorders instead' @{ payload = 8; moved = 1; refused = $false; posAtLeast = 0 })) { $fails++ }; $did++

# 7. REFUSED: onto its own descendant, which would cut the branch out of the
#    file. Scene Root and the block it adopted in scenario 4 are both always on
#    screen, which the first choice of source could not promise -- it aimed at a
#    row an earlier scenario had moved, the release landed outside the list, and
#    no drop happened at all.
$rows = DoDrag 0 { param($r) if ($r.ContainsKey(11)) { @{ point = (Mid $r[11]) } } } $null
if (-not (Verdict 'the root onto its own descendant is refused' @{ noDrop = 11 })) { $fails++ }; $did++

$p.CloseMainWindow() | Out-Null
Start-Sleep -Seconds 2
if (-not $p.HasExited) { $p.Kill() }

Write-Output ("scenarios run: {0}" -f $did)
if ($did -lt 7) { Write-Output "NOT EVERY SCENARIO RAN"; exit 1 }
if ($fails -gt 0) { Write-Output "FAILED ($fails)"; exit 1 }
Write-Output 'PASS'
exit 0
