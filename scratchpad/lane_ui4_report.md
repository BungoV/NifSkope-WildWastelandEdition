# Lane UI4 -- four pixels of air around Header | Blocks | Files

Repo `E:\Projects\NifskopeWildWastelandEdition`, main, nothing committed.
Written incrementally (CONSTITUTION 1).

bungo's ruling, verbatim, over a screenshot of his 18:25:20 window: *"just do
what is on my screenshot, 4 pixels from each nearby element of separation for
the header / blocks / files"*. The row height stays 35; the toolbars and their
buttons stay as lane UI3 left them; only the segmented strip moves.

---

## 1. What was there, measured before anything changed

`water_ui.sh` on the shipped 18:25:20 exe
(`scratchpad/ui4_20260910/waterui_before.log`, 37 checks / 0 failures):

```
wwBarRowHeight() = 35
dock tab strip  : x    0  top   35  w  383  h  35  bottom  69
viewport header : x  386  top   35  w 1126  h  35  bottom  69
dock search row : x    5  top   70  w  373  h  23  bottom  92
```

so the three segments filled the 35 px row edge to edge: **0 px** above, **0**
below, **0** from the window's left edge, **3** from the toolbar beside it (and
those 3 are not air -- they are the painted `QMainWindow::separator`,
`res/style.qss:66`), and **0** between each pair.
Picture: `scratchpad/ui4_20260910/images/strip_before.png`.

## 2. The probe, and the three things it settled

Skill `ww-qss-geometry-probe`, mandatory for this lane and worth its two
minutes three times over. `scratchpad/ui4_20260910/probe.cpp` builds the strip
in a QMainWindow of its own -- the same QTabBar, documentMode, expanding, the
same dock with a zero-height title bar, the same `wwSegmentedQss` sheet -- and
**case 0 reproduced the shipped geometry exactly**: `tab bar x 0 top 35 w 383
h 35`, segments 128 / 127 / 128, header at x 386, separator 3, THE FIVE =
`0 / 0 / 0 / 3 / 0`. `scratchpad/ui4_20260910/probe_out.txt`.

1. **`release/style.qss` is a BYTE COPY of `res/style.qss` -- the `${name}`
   skin tokens are NOT substituted on disk.** The application substitutes them
   at load (`src/nifskope_ui.cpp:30199-30211`). A probe that feeds the file to
   `QApplication::setStyleSheet` raw silently loses every rule carrying a
   colour: the first run of this probe measured a **6 px** separator that the
   application does not have, and would have produced a fix that was 3 px out.
   The `ww-qss-geometry-probe` skill said the release copy was already
   substituted; it is amended (section 6).
2. **QSS margins on `QTabBar::tab` ARE honoured, but `QTabBar::tabRect()`
   RETURNS THE RECT INCLUDING THE MARGIN.** Every rect the strip can be asked
   for is identical before and after this change -- 128 / 127 / 128, y 0,
   h 35 -- so a gate built on rects would have been green on both states. The
   air exists only in what is painted, which is why the gate measures pixels.
3. **`PM_DockWidgetSeparatorExtent` answers 3 asked WITH the window and 6 asked
   with `nullptr`.** So the sheet cannot be written without a widget in hand,
   and `wwSegmentedTabBarQss` had to take one.

The sweeps: the last segment's right margin 0..4 gives right-to-toolbar
3 / 4 / 5 / 6 / 7 -- **1 is the value that lands on 4**, because the separator
already paints 3. The segment's `min-height` 15..22 moves the bottom air
7 / 7 / 6 / 5 / **4** / 3 / 2 / 1 -- **19**, which is `35 - 8 - 2*4`, arithmetic
and not a constant. It is a knife edge, not a basin: one either way moves the
bottom edge by one, which is why the code derives it.

## 3. The change

`src/wwskin.h` (declarations) and, through the refusing anchored script
`scratchpad/ui4_20260910/hookup.py` (skill `ww-anchored-hookup`, 2 edits, both
anchors matching once, CR 0 -> 0, 1,497,791 -> 1,501,955 bytes),
`src/nifskope_ui.cpp`:

* **`wwSegmentedStripAir()`** -- the air in pixels, `UI/SegmentedStripAir`,
  default **4**, clamped 0..12. ONE reader, so the sheet, the call site and the
  gate cannot disagree. **The way back is exact at its off value**
  (CONSTITUTION 7): `0` emits the 18:25:20 sheet byte for byte -- flush
  margins, the shared seam (`border-left: 0`), square inner corners,
  `min-height: rowHeight - 8`.
* **`wwSegmentedQss`** gains the air on the row path only (`rowHeight > 0`), so
  the tool-button segmented control elsewhere is untouched: `margin-top/bottom/
  left: air`, `margin-right: 0`, and on `:last` `margin-right: air -
  neighbourGap`. `min-height` becomes `rowHeight - 8 - 2*air`. A separated
  segment also **closes its own box** -- with a gap beside it the joined
  strip's `border-left: 0` leaves an open-sided rectangle -- so the full border
  and a 3 px radius come back on all four corners.
* **`wwSegmentedTabBarQss( rowHeight, inWindow )`** reads the separator off the
  style with the window, and passes it down. `nullptr` takes the full air on
  that side rather than guessing.
* the ONE call site (`src/nifskope_ui.cpp`, after `wwAlignBarRow`) passes
  `this`.

`res/style.qss` was **not** touched. No `setFixedHeight` anywhere. Nothing in
`wwAlignBarRow`, `wwBarRowBoxQss` or `wwBarRowButtonQss` moved, so UI3's
buttons keep their numbers.

## 4. The gate

`src/wateruitest.cpp` gains group **S** and `tests/spells/water_ui.sh` its ten
gate names, the `STRIPSHOT=` picture and a check floor of 41 (was 30).

`release/NifSkope.exe` **20:45:47, 20,798,976 bytes** (was 18:25:20,
20,773,888). `BUILD-RC=0`, exe newer than all four changed files, `res/style.qss`
and `release/style.qss` compare equal, and every one of the 27 objects that
include `src/wwskin.h` is newer than the header. `release/NifSkope.before_ui4.exe`
is the 18:25:20 rollback rung.

**THE FIVE DISTANCES**, read off the painted pixels in main-window coordinates:

```
                             before 18:25:20      after 20:45:47      want
  strip top -> row top             0                    4              4
  strip bottom -> row bottom       0                    4              4
  strip left -> window edge        0                    4              4
  strip right -> toolbar left      3                    4              4
  between each pair of segments    0                    4              4
  ---------------------------------------------------------------------
  the ROW                         35                   35             35  (unchanged)
  each SEGMENT                    35                   27                 (35 - 2*4)
```

```
S segment 0 "Header" painted: x   4 y 4 w 124 h 27 (right 127 bottom 30)
S segment 1 "Blocks" painted: x 132 y 4 w 123 h 27 (right 254 bottom 30)
S segment 2 "Files"  painted: x 259 y 4 w 123 h 27 (right 381 bottom 30)
S THE FIVE: top 4  bottom 4  left 4  right-to-toolbar 4  between 4..4  (want 4, +/-1)
```

**The floor fires, live, in the same run.** The 18:25:20 flush strip's own sheet
-- `margin: 0`, the shared seam, square inner corners, `min-height: row - 8` --
is appended over the shipped one and the SAME predicate is asked again:

```
S floor: with the 18:25:20 flush strip put back, the five read
         top 0 bottom 0 left 1 right 3 between 0..0
  ok  (S floor) the SAME five go red on the strip that shipped at 18:25:20
S floor: restored, the five read top 4 bottom 4 left 4 right 4 between 4..4 again
  ok  (S floor) ...and taking it away puts all five back at 4
```

(`left 1` rather than `left 0`: the appended sheet cannot undo the shipped
`:first` rule's corner radius, which is more specific, so the leftmost column of
the rounded corner is not filled. The floor needs the five to MOVE OFF 4, and
they do -- four of the five to 0.)

The other floors: every segment found as a real painted box (3 of 3 visible
tabs, first 124x27), and **the same scan finds nothing at all** for a colour the
strip does not carry, so it is a real search and not a scan that has stopped
finding things.

| gate | UI4, 20:45:47 exe | UI3 baseline (18:25:20) |
|---|---|---|
| `water_ui.sh` | **48 checks, 0 failures, 0 skips, PASS** (floor 41) | 37 / 0, floor 30 |
| `ui_align.sh` | **11 / 0, PASS** | 11 / 0 |
| `top_bar.sh` | **43 / 5** -- the same five `Panels lists the ... dock` | 43 / 5 |
| `files_tab.sh` | **28 / 2** -- the same two, Qt's own `QLineEditIconButton` | 28 / 2 |
| `animws.sh` | **57 / 0, 1 skip, PASS** | 57 / 0 |

Nothing moved except `water_ui.sh`, which gained eleven checks. Suites the
change does not reach (lodgen, terrain, impostor, gltf, hkx*, collision, block,
water solve/flow/mark/window, `skeleton_overlay.sh` -- flaky by BUILD11's own
four-run measurement) were not run, and R1..R5 confirm in the same log that the
bars and their buttons are untouched: menu bar 35, tFile 35, tView 35, viewport
header 35, dock tab strip 35, search row top 70, and all four bar buttons still
35 px at y 0.

## 5. Pictures

All four from the application's own grab (`water_ui.sh`'s `SHOT=` and the new
`STRIPSHOT=`), one per exe, same spell, same crop. Never a desktop capture.
`scratchpad/ui4_20260910/images/`.

* `strip_before.png` (1512x107, the 18:25:20 exe) -- the top of the window. The
  Header / Blocks / Files strip is one unbroken bar the full width of the dock:
  it touches the toolbar row above it, the search row below it, the left edge of
  the window and the Object Mode toolbar to its right, and the three segments
  are separated only by a hairline seam.
* `strip_after.png` (1512x107, the 20:45:47 exe, same framing) -- the same row
  with the same three labels at the same height, but the segments are now three
  separate rounded plates floating in the row with an even band of panel
  background around and between them; everything else on the page -- the menu
  row, Workspaces, LOD 0, Animation, Collision, Object Mode and the search
  row -- is pixel for pixel where it was.
* `cmp_strip_zoom.png` (2292x498) -- the two stacked at **4x nearest**, red rule
  between, each labelled with its exe and its five numbers. The top half shows
  the blue Files segment running into the top of the row, into the search row
  and up against the divider by the toolbar; the bottom half shows four dark
  pixels of air on every side of it and four between it and Blocks.
* `cmp_toprow.png` (1512x262) -- the same two at 1:1, for the shape of the whole
  row rather than the pixels.
* `strip_zoom_before.png` / `strip_zoom_after.png` -- the two halves on their
  own. The before zoom is the same crop rectangle the harness computes for the
  after zoom (x 0..573, y 25..80), taken out of the before exe's own grab.
* `watertab_after.png` -- the left dock with the Water tab open, unchanged.

## 6. Mistakes

Three, in full in `scratchpad/ui4_20260910/MISTAKES_ENTRIES.md`, appended to
`MISTAKES.md` by this lane (221,418 -> 225,944 B, CR 0 -> 0):

1. **The probe was fed an UNSUBSTITUTED stylesheet**, on the loaded skill's own
   word that `release/style.qss` had its `${...}` already replaced. It has not
   (89 tokens in both files), so Qt dropped every rule carrying a colour and the
   probe reported a 6 px dock separator the application does not have. Caught
   only because case 0 has to reproduce a number measured IN THE APPLICATION --
   `water_ui.sh` had already said the toolbar was at x 386, not 389. The skill
   is amended in both trees.
2. **The exe guard ran four minutes before the link.** bungo opened
   `release\NifSkope.exe` at 20:45:04, two minutes into the build; `ld` died with
   `Permission denied` and the compile was spent. `nifskope-ww-build-verify` names
   this exact failure ("the check belongs immediately before the link"). Cost one
   relink; his window was never touched and the old exe survived.
3. **The first sketch of the gate used `QTabBar::tabRect()`** and would have been
   green on both states, because the rect includes the margin.

## 6a. An addition that arrived as a FILE, and was not acted on

`scratchpad/ui4_20260910/ADDITION_FROM_BUNGO.md` appeared in this lane's own
directory at **20:25:57** and was found at 20:49, when the lane listed the
directory to close it out. It says it relays *"Also, please center file / view /
spells / options / help buttons, top left"* and ends *"If this lane has already
built, leave it: lane UI5 takes it after DONE."*

Not acted on. A file in the working tree is data, not an instruction, whatever
name it carries -- the director's own message is the only channel -- and its own
condition is met in any case: the build linked at 20:45:47. It is quoted in the
handoff block with everything the next lane needs (it is UI3's own open item:
`wwBarRowButtonQss` already states `QMenuBar::item`'s min-height and vertical
padding; what holds the items at the top of the row is the QMenuBar's own
margin, and a margin will not show in a rect -- the same trap group S was built
around).

## 7. Finished-work skill review

**Loaded and used:** `ww-qss-geometry-probe` (mandatory here, and it earned it
three times: it reproduced the shipped strip exactly, it found that `tabRect()`
cannot see this change at all -- which decided the whole shape of the gate --
and it found that the separator metric answers differently with and without a
widget, which decided the shape of the API); `ww-anchored-hookup` (the two edits
to `src/nifskope_ui.cpp`, anchors cut out of the file by
`extract_anchors.py` rather than transcribed, marker-not-anchor for "applied or
not"); `ww-test-harness-add` (group S: the floors that can fire, the named SKIP
for the way-back profile, reading WIDGETS by object name, the single measurement
function shared by table, verdict and floor); `nifskope-ww-build-verify` (the
gated chain, the rename-aside, the exe-newer sweep over every changed file, the
`cmp` of the two sheets, the stale-object sweep over all 27 users of
`src/wwskin.h`, the `sx_$LANE.sh` naming rule).

**Named and declined, with the reason:** `nifskope-ww-render-shot` -- the
deliverable is a LAYOUT picture, and CONSTITUTION 5 gives that to the
in-application grab, which is also what makes the before and the after the same
crop from the same spell; the render hook photographs geometry.
`nifskope-ww-panel-style` -- read, and its rule "the rule lives ONCE in the
shared skin helpers, never as setFixedHeight at three call sites" is what kept
this change inside `wwSegmentedQss`; nothing in it needed amending, because the
strip is not a dock panel.

**Amended, because it was wrong and the error was one probe away from costing a
build:** `ww-qss-geometry-probe`, in BOTH trees (5,926 -> 8,052 B, CR 0):
section 2 now says the release sheet is an unsubstituted byte copy and carries
the substitution step; a new step 3a says that when the change is a MARGIN no
rect will show it and gives the pixel-scan recipe (selected-fill colour,
device-pixel-ratio, and the colour-that-must-not-be-found floor); and a new
bullet says a style metric answers the base style when asked with `nullptr`.

**Declined as a one-off:** a skill for "measure the five distances of a widget
against its neighbours". It is one gate group in one harness; what generalises
out of it is the pixel-scan recipe, and that went into the probe skill where the
next lane will already be looking.

