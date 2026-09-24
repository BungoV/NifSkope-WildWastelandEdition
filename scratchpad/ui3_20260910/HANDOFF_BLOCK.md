# UI3 handoff block (for the director to splice into HANDOFF.md)

**UI3 LANDED, EXE FREE.** `release/NifSkope.exe` **18:25:20**, **20,773,888
bytes** (BUILD12's was 17:45:29, 20,751,360). Markers:
`scratchpad/ui3_20260910/DONE` in, `BUILDING` gone. Report
`scratchpad/lane_ui3_report.md`; entry text
`scratchpad/ui3_20260910/WW_CHANGES_ENTRY.md`; three MISTAKES entries already
appended by the lane (`MISTAKES.md` 216,188 -> 219,982 B, CR 0).

## THE BUTTON NUMBER

**39 px at y 4 -> 35 px at y 0. All four. The row is 35.** BUILD12's red (a) is
closed.

```
before (17:45:29):  R3: 4 bar buttons, heights 39..39, row 35     [y 4, bottom 8 px clipped]
after  (18:25:20):  R3 button tFile/ViewWorkspacesButton: 35 px, y 0 (row 35)
                    R3 button tView/ViewLodButton:        35 px, y 0 (row 35)
                    R3 button tView/ViewAnimationButton:  35 px, y 0 (row 35)
                    R3 button tView/ViewCollisionButton:  35 px, y 0 (row 35)
                    R3: 4 at the row height, 35..35, worst offset 0
                    R3: the skin calibrated content 26, measured style overhead 9
```

**Why 39 won** (measured outside the application, `scratchpad/ui3_20260910/
probe.cpp` + `probe2.cpp` reproduce 39 exactly): (1) a widget's OWN stylesheet
outranks every ancestor's, so `wwBoxedButtonQss`'s `padding: 3px 6px`
(`src/nifskope_ui.cpp:520`, set at `:27540` and `:26929`) beat the bar's --
WATER7's `min-height` arrived, its `padding` never did; (2) QSS `min-height` on
a QToolButton is a minimum on the CONTENTS and Qt adds 3 px of its own, and
`max-height` is not consulted on that path at all; (3) a QToolBar starts its
items 4 px below its own top (`PM_ToolBarItemMargin 2 + PM_ToolBarFrameWidth 2`).
None of the three was the row height.

**The fix, through the skin, no `setFixedHeight` anywhere.** New
`wwBarRowBoxQss()` takes the bar's own box away (items at y 0, and the layout
then CLAMPS a button to the bar); `wwBarRowButtonQss( contentHeight )` states
the glyph line, the air above and below it and the menu arrow's place, nothing
horizontal; `wwAlignBarRow` appends both to the bars AND to every tool button in
them, and CALIBRATES the content from the widgets rather than typing Qt's
constants. `wwBarRowButtonContent()` / `wwBarRowButtonOverhead()` read the two
numbers back. Way back unchanged and still exact: `UI/CompactTopBars = false`.

## Gates (both on the 18:25:20 exe, sequential, one instance)

| gate | numbers | baseline |
|---|---|---|
| `water_ui.sh` | **37 checks, 0 failures, 0 skips, PASS** (floor 30) | 30 / 0, floor 24 |
| `ui_align.sh` | **11 / 0, PASS** | 11 / 0 |
| `top_bar.sh` | **43 / 5** -- the same five `Panels lists the ... dock` | 43 / 5 |
| `files_tab.sh` | **28 / 2** -- the same two, Qt's own `QLineEditIconButton` | 28 / 2 |
| `animws.sh` | **57 / 0, 1 skip, PASS** | 57 / 0 |

Skipped, with the reason: every suite the change does not reach (lodgen,
terrain, impostor, gltf, hkxfile, collision, block, water solve/flow/mark) and
`skeleton_overlay.sh` (flaky by BUILD11's own four-run measurement, untouched).

**R3 is 1 px now and its floor FIRES, live, in the same run.** BUILD12's own
arithmetic is appended over the shipped sheet and the same predicate is asked
again: `the buttons read 49..49 against a row of 35 ... 0 of 4 at the row
height`, then `restored, the buttons read 35..35 again`. Both halves are
checks, so the picture the spell writes afterwards is the shipped state. R4
pins that the skin states one min-height and one pair of vertical paddings per
box selector, nothing horizontal, the menu arrow centred, and that the bars
actually carry the bar-box rule.

## Pictures (in-application grabs, same spell, same crop, two exes)

`scratchpad/ui3_20260910/images/`

* `buttons_before.png` (1512x107) -- the 17:45:29 exe. The **Workspaces** box is
  cut off along the bottom of the row: 39 px starting 4 px down inside a 35 px
  bar, so 8 px of it are clipped away.
* `buttons_after.png` (1512x107) -- the 18:25:20 exe, same framing. Every box in
  the row is whole and ends on the row's own bottom edge.
* `cmp_toprow.png` -- the two stacked at 1.8x with a red rule between.
* `cmp_header.png`, `cmp_zoom.png` -- the viewport header, at 2x and 4x.
* `watertab_after.png` -- the four-tab left dock, unchanged.

## For bungo / the director

1. **The viewport header's Global, snap and grid buttons now SHOW their
   dropdown arrow.** Not a new arrow: those buttons always had a menu and their
   indicator lived in the 8 px the old clipping threw away. The row's sheet
   centres it (the rule `wwBoxedButtonQss` has always had, which is why the four
   boxed buttons never showed the problem). On **Global** it sits cleanly beside
   the label; on the two narrow ICON-ONLY buttons it touches the icon, because
   `res/style.qss:205` gives them `padding: 1px 1px`. `cmp_zoom.png` is the
   picture. **His call**: leave it, give the row's indicator a small right
   margin (one line in `wwBarRowButtonQss`, one build), or `UI/CompactTopBars =
   false`.
2. **The menu bar's ITEMS were not made the row's height**, only the bar. They
   take the same calibrated content number, so File / View / Spells sit on the
   row's line, but a full-height Blender-style hover band would need the
   QMenuBar's own margins zeroed as well. Not asked for; say so rather than
   claim the menu row is finished.
3. `release/ui3_probe.exe` and `release/ui3_probe2.exe` are the two standalone
   measurement binaries, left on disk beside their sources. They link
   Qt6Widgets only and are not part of the application.
4. **A new skill was written and needs mirroring** (CONSTITUTION 1a, two-tree
   rule): `E:\Projects\Claude\.claude\skills\ww-qss-geometry-probe\SKILL.md`
   -> the repo's `.claude/skills/`.

## Restart

**YES.** Whatever bungo's open window is, it predates 18:25:20. The next launch
of `release\NifSkope.exe` is the one with the buttons on the row.

## State

Nothing committed (CONSTITUTION 8). Changed by this lane: `src/wwskin.h`,
`src/nifskope_ui.cpp` (through the refusing anchored script
`scratchpad/ui3_20260910/hookup.py`, one `replace` edit, CR 0 -> 0),
`src/wateruitest.cpp`, `tests/spells/water_ui.sh`, `MISTAKES.md`. **`res/style.qss`
was NOT touched.** Game down (`rc=1`) at every check; no NifSkope was running at
either build.
