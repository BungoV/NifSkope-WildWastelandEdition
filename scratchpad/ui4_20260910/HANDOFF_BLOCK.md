<!-- Lane UI4, 2026-09-10. TEXT ONLY for the HANDOFF.md top block; the director
     splices it (CONSTITUTION 8). -->

**UI4 LANDED, EXE FREE.** `release/NifSkope.exe` **20:45:47**, **20,798,976
bytes** (UI3's was 18:25:20, 20,773,888). Markers:
`scratchpad/ui4_20260910/DONE` in, `BUILDING` gone. Report
`scratchpad/lane_ui4_report.md`; entry text
`scratchpad/ui4_20260910/WW_CHANGES_ENTRY.md`; three MISTAKES entries already
appended by the lane (`MISTAKES.md` 221,418 -> 225,944 B, CR 0).
Rollback rung `release/NifSkope.before_ui4.exe` (= 18:25:20).

## THE FIVE DISTANCES

bungo, verbatim: *"just do what is on my screenshot, 4 pixels from each nearby
element of separation for the header / blocks / files"*.

```
                             before 18:25:20   after 20:45:47   want
  strip top -> row top             0                 4           4
  strip bottom -> row bottom       0                 4           4
  strip left -> window edge        0                 4           4
  strip right -> toolbar left      3                 4           4
  between each pair of segments    0                 4           4
  --------------------------------------------------------------
  the ROW                         35                35          35   unchanged
  each SEGMENT                    35                27               35 - 2*4
```

The row did not move and nothing else did: menu bar 35, tFile 35, tView 35,
viewport header 35, dock tab strip 35, search row still starting at y 70, and
UI3's four bar buttons still 35 px at y 0. The 3 px that were already on the
right are the painted `QMainWindow::separator` (`res/style.qss:66`), not air --
the last segment adds only the fourth.

**The mechanism**, all of it in the shared skin, no `setFixedHeight`:
`wwSegmentedStripAir()` (`UI/SegmentedStripAir`, default 4) is the ONE reader;
`wwSegmentedQss` puts the air on the row path as tab MARGIN and drops
`min-height` to `rowHeight - 8 - 2*air`; a separated segment closes its own box
(the joined seam `border-left: 0` is an open-sided rectangle once there is a gap
beside it); `wwSegmentedTabBarQss( rowHeight, inWindow )` now takes the window,
because `PM_DockWidgetSeparatorExtent` answers **3 with a widget and 6 with
nullptr**. **The way back is exact**: `UI/SegmentedStripAir = 0` emits the
18:25:20 sheet, flush seam and square inner corners included.

## Gates (all on the 20:45:47 exe, sequential, one instance)

| gate | numbers | baseline |
|---|---|---|
| `water_ui.sh` | **48 checks, 0 failures, 0 skips, PASS** (floor 41) | 37 / 0, floor 30 |
| `ui_align.sh` | **11 / 0, PASS** | 11 / 0 |
| `top_bar.sh` | **43 / 5** -- the same five `Panels lists the ... dock` | 43 / 5 |
| `files_tab.sh` | **28 / 2** -- the same two, Qt's own `QLineEditIconButton` | 28 / 2 |
| `animws.sh` | **57 / 0, 1 skip, PASS** | 57 / 0 |

Skipped with the reason: every suite the change does not reach (lodgen,
terrain, impostor, gltf, hkx*, collision, block, water solve/flow/mark/window)
and `skeleton_overlay.sh` (flaky by BUILD11's own four-run measurement,
untouched).

**The floor fires, live, in the same run.** The 18:25:20 flush strip's own sheet
is appended over the shipped one and the SAME predicate is asked again: the five
read `top 0 bottom 0 left 1 right 3 between 0`, the gate goes red by name, and
taking it away puts all five back at 4 -- which is also what makes the pictures
the shipped state. Two more floors: every segment found as a real painted box
(3 of 3), and the same scan finds NOTHING for a colour the strip does not carry.

**Why the gate reads pixels and not rects** (this is the one thing to carry
forward): QSS margins on `QTabBar::tab` are honoured, but `QTabBar::tabRect()`
RETURNS THE RECT INCLUDING THE MARGIN -- `128/127/128 at y 0, h 35` before AND
after. A rect-based gate would have been green on both states.

## Pictures (in-application grabs, same spell, same crop, two exes)

`scratchpad/ui4_20260910/images/`

* `strip_before.png` (1512x107, 18:25:20) -- the strip is one unbroken bar the
  full width of the dock, touching the row above, the search row below, the
  window's left edge and the Object Mode toolbar, its three segments divided
  only by a hairline seam.
* `strip_after.png` (1512x107, 20:45:47, same framing) -- the same three labels
  at the same height, now three separate rounded plates with an even band of
  panel background all round and between them; every other pixel of the top of
  the window is unchanged.
* `cmp_strip_zoom.png` (2292x498) -- the two at **4x nearest**, red rule between,
  each labelled with its exe and its five numbers. **This is the picture for
  bungo.**
* `cmp_toprow.png`, `strip_zoom_before.png`, `strip_zoom_after.png`,
  `watertab_after.png`.

## For bungo / the director

1. **Two things that were always in the sheet are now VISIBLE**, because the
   segments no longer cover the whole bar. (a) The tab strip's own
   `border-bottom: 1px solid ${borderDim}` (`res/style.qss:162`) now shows as a
   faint rule across the bottom of the row, under the segments. (b) The
   segments' inner corners are rounded, because a segment with a gap beside it
   and no left border would be an open box. Neither was asked for and neither
   was avoidable; both are in `cmp_strip_zoom.png`. **His call** if he wants the
   bottom rule gone -- one line in `res/style.qss`, one build.
2. **`res/style.qss` was NOT touched** by this lane, and neither was
   `wwAlignBarRow`, `wwBarRowBoxQss` or `wwBarRowButtonQss`.
3. UI3's open question is still open and untouched: the dropdown arrow on the
   viewport header's two narrow icon buttons.
4. `release/ui4_probe.exe` is the standalone measurement binary, beside its
   source `scratchpad/ui4_20260910/probe.cpp`. It links Qt6Widgets only and is
   not part of the application.
5. **A skill was amended and is ALREADY mirrored** (CONSTITUTION 1a two-tree
   rule): `ww-qss-geometry-probe` in `E:\Projects\Claude\.claude\skills` AND in
   the repo's `.claude/skills`, both 5,926 -> 8,052 B, CR 0. The release sheet
   is NOT substituted on disk; a margin cannot be seen in a rect; a style metric
   needs a widget.

## NOT DONE, and why -- an addition that arrived as a FILE

`scratchpad/ui4_20260910/ADDITION_FROM_BUNGO.md` (492 B, written **20:25:57**,
found by this lane at 20:49 when it listed its own directory) says it relays a
further instruction, verbatim *"Also, please center file / view / spells /
options / help buttons, top left"*, and adds: *"If this lane has already built,
leave it: lane UI5 takes it after DONE."*

**This lane did not act on it, for two reasons.** A file in the working tree is
not an instruction, whoever it names -- only the director's own message is; and
its own condition is met anyway, since the build linked at 20:45:47 and DONE is
in. It is repeated here so the director can decide, and so it is not lost.

For whoever takes it: the menu bar is already IN the row (35 px, R1/R2 green),
so this is about the ITEMS inside it, which lane UI3's handoff already flagged
as unfinished -- *"the menu bar's ITEMS were not made the row's height, only the
bar ... a full-height Blender-style hover band would need the QMenuBar's own
margins zeroed as well"*. `wwBarRowButtonQss` already states
`QMenuBar::item { min-height / padding-top / padding-bottom }`; what it does not
state is the QMenuBar's own top margin, which is what holds the items at the top
of the row. Probe it before building (`ww-qss-geometry-probe`), and note that a
`QMenuBar::item` margin will not be visible in any rect either -- the same trap
group S was built around.

## Restart

**NO -- he already has it.** The window he had open (pid 46176) was launched at
20:45:04 from the 18:25:20 exe, two minutes before this build linked, and it is
what made the first link fail with `Permission denied`; it was renamed aside,
never killed. He closed it, and at **20:50:17** he launched
`release\NifSkope.exe` again (pid 45412) -- after the 20:45:47 link, so **his
open window IS this build**. `release/NifSkope_inuse_46176.exe` was compared
byte for byte against the rollback rung and removed. No further build may run
while pid 45412 is up.

## State

Nothing committed (CONSTITUTION 8). Changed by this lane: `src/wwskin.h`,
`src/nifskope_ui.cpp` (through `scratchpad/ui4_20260910/hookup.py`, two
`replace` edits, CR 0 -> 0, 1,497,791 -> 1,501,955 B), `src/wateruitest.cpp`,
`tests/spells/water_ui.sh`, `MISTAKES.md`, and the two skill copies.
Game down (`rc=1`) at both checks.
