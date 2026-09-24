<!-- Lane UI5, 2026-09-11. TEXT ONLY for the HANDOFF.md top block; the director
     splices it (CONSTITUTION 8). -->

**UI5 LANDED, EXE FREE.** `release/NifSkope.exe` **2026-09-11 05:58:21**,
**20,855,296 bytes** (WATER8's was 2026-09-10 21:02:12, 20,830,208). Markers:
`scratchpad/ui5_20260910/DONE` in, `BUILDING` gone. Report
`scratchpad/lane_ui5_report.md`; entry text
`scratchpad/ui5_20260910/WW_CHANGES_ENTRY.md`; **five MISTAKES entries NOT
appended by the lane** -- `scratchpad/ui5_20260910/MISTAKES_ENTRIES.md`, the
director splices. **There is NO pre-UI5 rollback rung on disk** (see the reds
below); the nearest earlier exe is `release/NifSkope.before_ui4.exe`
(2026-09-10 18:25:20).

## THE TITLE NUMBER

bungo, verbatim: *"Also, please center file / view / spells / options / help
buttons, top left"*.

```
                                   before 20:45:47   after 05:58:21   want
  the five titles' text, ink y        6..16 / 6..17   13..23 / 13..24
  the CAP BAND's centre               10.0            17.0             17.0
  offset from the row's centre        -7.0             0.0             0
  and the five agree, spread           -               0.0             0
  the item's painted box            y 0..19 (20 px)   y 0..34 (35)     the row
  ------------------------------------------------------------------------
  the ROW                             35              35               35  unchanged
  the menu bar's own size hint        20              35 (<= 35)
  tFile / tLOD / tView / header / tab strip           all 35           unchanged
```

The cap band is the topmost row of ink down by the font's own `capHeight()`
(8 px here). It is what a reader judges centring by, and it is NOT the whole ink
band -- that also holds the mnemonic underline under F / V / S / O / H and the
descender of the `p` in Spells and Options, which is what the first run of the
gate got wrong. The whole ink band is printed beside it in every log.

The before numbers come from the 20:45:47 exe's OWN in-application grab
(`scratchpad/ui4_20260910/images/strip_after.png`, whose first 35 rows are the
menu row) read by `scratchpad/ui5_20260910/measure_before.py`, and the gate's
own live floor reproduces them to the pixel (`File 6..16`, `Spells 6..17`).

## The mechanism, and the two things it is NOT

Measured over 30 cases outside the application BEFORE a line was written
(`scratchpad/ui5_20260910/probe.cpp`, `probe_out2..4.txt`, skill
`ww-qss-geometry-probe`; `release/ui5_probe.exe` links Qt only and is not part
of the application):

1. **`QMenuBar::item { min-height }` is not consulted on this path at all.**
   Fifteen cases, 22 to 36 px, item height 20 in every one. The row's sheet has
   stated a menu-item min-height since lane WATER7 and it has never done
   anything -- **this is why UI3's "the items take the same calibrated content
   number" did not centre them.**
2. **A `::item` margin moves the item but cannot be gated.**
   `actionGeometry()` grows with the margin, exactly as `QTabBar::tabRect()` did
   for lane UI4.
3. **The two vertical paddings are consulted and leave the rect honest.** Item
   content 16 (measured), row 35, split 9 / 10.

New `wwBarRowMenuItemQss( rowHeight, itemContent )` in the shared skin, applied
inside `wwAlignBarRow`, which MEASURES the content (row sheet applied once with
both paddings zeroed, `ensurePolished()`, read back -- no event loop, which is
probe case E and is what makes it possible during construction) and appends the
rule LAST so it outranks the min-height above it. `wwBarRowMenuItemContent()`,
`wwBarRowMenuItemPadTop()`, `wwBarRowMenuItemPadBottom()` read the numbers back.
Fallback, named: a menu bar with no titles, or items taller than the row, takes
the font's own line height. Way back unchanged and still exact:
`UI/CompactTopBars = false`, the same single reader, and M4 measures that state
live (titles back at cap centre 12.0, item 24 px).

## Gates (all on the 05:58:21 exe, sequential, one instance, `.gatelock`)

| gate | numbers | baseline |
|---|---|---|
| `water_ui.sh` | **76 checks, 0 failures, 0 skips, PASS** (floor 62) | 59 / 0, floor 48 |
| `ui_align.sh` | **11 / 0, PASS** | 11 / 0 |
| `top_bar.sh` | **43 / 5** -- the same five | 43 / 5 |
| `files_tab.sh` | **28 / 2** -- the same two | 28 / 2 |
| `animws.sh` | **57 / 0, 1 skip, PASS** | 57 / 0, 1 skip |

Logs `scratchpad/ui5_20260910/logs/`. Nothing moved from baseline but
`water_ui.sh`, which gained this lane's 14 checks plus the 3 the chain's two
LOD-tab picture arguments add. Skipped with the reason: every suite a menu-item
padding does not reach (lodgen, terrain, impostor, gltf, `hkx*`, collision,
block, the water solve/flow/mark/window suites) and `skeleton_overlay.sh`,
flaky by BUILD11's own four-run measurement and untouched.

**The floor fires, live, in the same run.** The 20:45:47 arithmetic
(`padding-top: 2px; padding-bottom: 2px`) is appended over the shipped sheet and
the SAME predicate is asked again: worst **-7.0**, red by name; taken away,
**0.0** again. Both halves are checks, so the picture is the shipped state.

## Pictures

`scratchpad/ui5_20260910/images/`

* **`cmp_menu_zoom.png`** (1080x338) -- File..Help from both exes at **4x
  nearest**, red rule between, each half labelled with its exe and its numbers.
  **This is the picture for bungo.**
* `toprow_after.png` (1512x107) -- the in-app grab of the top of the window.
* `cmp_toprow.png`, `menu_zoom_before.png`, `menu_zoom_after.png`,
  `seam_after.png`, `strip4x_after.png`, `lodtab_lod.png`, `lodtab_water.png`.

## For bungo / the director

1. **The hover band is now the height of the row.** Giving a title the row's
   padding makes its painted box the whole row, so `QMenuBar::item:selected`
   (`res/style.qss:49`) paints a 35-px highlight when a title is hovered or
   open, where it painted a 20-px one before. That is what "centred in the row"
   means geometrically and it is Blender's behaviour, but it was not separately
   asked for and no count sees it. **His call.**
2. **THREE LINKS, not one.** The application code was compiled once and never
   changed after 05:48:20; links 2 and 3 rebuilt only `src/wateruitest.cpp`
   because the first run of a gate that had never been executed found two
   defects IN THE GATE (it called the mnemonic underline "the text"; its
   way-back half set an empty stylesheet, which the application does not
   re-resolve). Both are MISTAKES entries.
3. **The rollback rung is gone**, destroyed by this lane's own `build.sh`
   copying the exe before EVERY link. Fixed (the rung is written only if none
   exists) and the two misleading copies were deleted. The exact way back for
   this change is the setting.
4. **Not measured:** any theme but dark, any device pixel ratio but 1, any font
   but this machine's; and why `setStyleSheet( QString() )` does not re-resolve
   the menu bar's items in the application when it does in a standalone rig.
5. UI3's open question is still open and untouched: the dropdown arrow on the
   viewport header's two narrow icon buttons.
6. `res/style.qss` was NOT touched, and neither was the segmented strip --
   lane UI6's, after his *"Why are they separated?"*. `water_ui.sh`'s groups S
   and L still read 4 px of air with the segments apart, which is the expected
   state for this exe and UI6's to change.

## Restart

**YES.** Whatever bungo opens next must be launched after 05:58:21. His last
window (pid 8428, launched 05:32:30 from WATER8's 21:02:12 exe) was closed by
him at 05:48 and never touched by this lane; every link after that ran with
`rc=1`.

## State

Nothing committed (CONSTITUTION 8). Changed by this lane, through the refusing
script `scratchpad/ui5_20260910/hookup.py` (9 edits, 9 of 9 anchors matched
once, CR 0 -> 0 on every file): `src/wwskin.h` (14,041 B),
`src/nifskope_ui.cpp` (1,507,501 B), `src/wateruitest.cpp` (54,229 B),
`tests/spells/water_ui.sh` (12,744 B). Three skill files in the REPO tree were
amended and need mirroring to `E:\Projects\Claude\.claude\skills`:
`nifskope-ww-build-verify` (14,403 -> 16,546 B), `ww-qss-geometry-probe`
(8,052 -> 10,395 B), `ww-test-harness-add` (11,126 -> 12,783 B), all CR 0.
Game down (`rc=1`) at every check.
