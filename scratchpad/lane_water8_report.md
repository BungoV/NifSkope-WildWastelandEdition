# Lane WATER8 -- the Water tool moves to the LOD Generation PANEL, on the right

Tree `E:\Projects\NifskopeWildWastelandEdition`, `main`, nothing committed.
Written incrementally; section 0 was written BEFORE a line of code, per
CONSTITUTION rule 1 ("the gates are pre-registered in the brief, BEFORE the
agent starts").

bungo's ruling, verbatim, on seeing lane WATER7's Water tab in the LEFT strip:

> **"What? I wanted it in that right panel though"**

and, earlier: *"They should be in the LOD gen workspace"*, then, over a
screenshot of the `Header | Blocks | Files` strip, *"You'd access them like
this"*. Read together: **the strip was the STYLE, the LOD Generation panel is
the PLACE.** WATER7 read it as the place, and that is the director's recorded
misread; this lane undoes it.

---

## 0. Where the LOD Generation panel is, with file:line

Found before any gate was registered, because the gates are about it.

| what | file:line | note |
|---|---|---|
| the panel widget | `src/lodgenmanager.cpp:355` -- `class LodgenPanel final : public QWidget` | object name `LodgenPanel`; three bands (scrolling settings, splitter map, pinned action bar) |
| the dock that holds it | `src/lodgenmanager.cpp:2381` -- `tlCreateLodGenerationDock()` | object name `LodGenerationDock`, `Qt::RightDockWidgetArea`, `dock->setWidget( panel )` |
| where it is created | `src/nifskope_ui.cpp:27067-27068` | inside `NifSkope::initDockWidgets`, i.e. BEFORE `waterMarkInstall()` (`src/nifskope.cpp:2833`) and before the bar-row block in `restoreUi()` |

**So the panel is NOT built inside `src/nifskope_ui.cpp`.** It is a file this
lane owns outright (`src/lodgenmanager.cpp`), and no other live lane is in it
(UI4 holds `res/style.qss`, `src/wateruitest.cpp` and a hook-up into
`src/nifskope_ui.cpp`). The tab strip therefore goes in directly, and only the
four shared files go through the refusing script.

The left strip it is copied from, for style parity:
`src/nifskope_ui.cpp:24402-24429` -- a `QTabBar` named `LeftColumnModeSelector`
with `documentMode`, `drawBase false`, `expanding true`, `usesScrollButtons
false`, `wwSegmentedTabBarQss()`, `tabData` carrying the page index, above a
`QStackedWidget` named `LeftColumnStack` in a zero-margin `QVBoxLayout`.

---

## 0.1 Pre-registered gates (written before a line of code)

Every number below is a PREDICTION until a built exe produces it. UI4 holds the
build slot (`scratchpad/ui4_20260910/BUILDING`, checked at the top of this
lane); whatever is not run is named in section 4 and again in `PENDING.md`.

The new checks live in **`src/wateruitest_lod.cpp`**, a new translation unit,
because `src/wateruitest.cpp` is UI4's until `scratchpad/ui4_20260910/DONE`
exists (`ww-test-harness-add` section 1, the translation-unit rule). They are
called from `wateruitest.cpp`'s existing `WW_WATERUI_TEST` run as **group L**,
which REPLACES group T -- so `tests/spells/water_ui.sh` stays the one spell and
its count does not drop.

**Group L -- the LOD Generation panel's own strip**

| id | check | floor on the other side |
|---|---|---|
| L1 | the LOD dock holds a `QTabBar` named `LodPanelModeSelector` with exactly **2** tabs, `LOD` then `Water` | the same search for a tab named `Watre` finds none, so it is a real search |
| L2 | its `tabData` values are the stack indices 0 and 1, and `LodPanelStack` has exactly 2 pages: `LodgenPanel` at 0, `WaterMarkPanel` at 1 | a page count of 1 is a red that names the missing page |
| L3 | selecting `Water` shows page 1 -- **the marking rows and the flow-window button are on screen** (`WaterMarkPanel` visible, its `WaterMarkBodyBox`, its per-body rows, `WaterMarkWindowButton` visible) | selecting `LOD` puts `LodgenPanel` back and `LodgenGenerateButton` visible -- the same assertion on the tab that was always there |
| L4 | the strip's height == `wwBarRowHeight()` (predicted **35**) within 1 px, in main-window coordinates | the same measurement on the LEFT strip, printed beside it; both rects non-degenerate |
| L5 | the LOD strip's stylesheet is **byte-identical** to the left strip's -- same QSS, reported as a length + FNV-1a hash of both | the hash of `wwSegmentedTabBarQss()` with NO row (the compact default) differs from both, so the comparison is seen to be able to differ |
| L6 | the LEFT strip carries exactly **3** tabs (`Header`, `Blocks`, `Files`) and `LeftColumnStack` exactly **3** pages, with the LOD workspace OPEN and again with it CLOSED | both states asserted in one run; a strip that is 3 only while the workspace is shut, or only while it is open, goes red |
| L7 | there is no `WaterMarkDock`, and the Workspaces menu offers no `Water Marking` and no `Water window` | the same scans DO find `LodGenerationDock` and `LOD Generation`, so they can find something |
| L8 | with `UI/SegmentedStripAir` at its shipped 4, the LOD strip's two painted segments are 4 px clear of the row top and bottom and of one another -- the same pixel measurement UI4 registered for the left strip | the same measurement at air 0 reads 0; run in the same session, so the instrument is seen to move |

**Predicted numbers.** Row **35** (UI3 measured it; a 2-tab `QTabBar`'s natural
height is ~26, so it cannot be the tallest bar). **THE REFUTER, registered
here:** `wwAlignBarRow` takes the TALLEST natural height among its bars, so if
the LOD strip's natural height comes back ABOVE 35 the whole row grows and every
bar in the window gets taller -- the opposite of what shipped. In that case the
lane reports the number and does NOT ship the strip in the row (it keeps
`wwSegmentedTabBarQss()`'s compact default instead). L4 prints
`wwBarRowHeight()` and both strips' rectangles, so the first run answers it.

**Count.** BUILD/UI3's `water_ui.sh` ran **37 checks / 0 failures**. Group T
(18 checks) is retired and group L (>= 20 checks, listed above) replaces it, so
the pre-registered floor is **37**, and the spell refuses below it. UI4 is
adding group S checks to the same spell; whatever it lands, the floor is the
larger of 37 and UI4's own.

**One floor is proved to fire, live, in the same run** (CONSTITUTION 4): L2's
predicate is asked a second time against a deliberately wrong page index taken
from the LEFT stack, and must go red; the log prints both halves.

## 0.2 Pictures (through the render hook / the in-app grab, never a desktop capture)

`scratchpad/water8_20260910/images/lodtab_lod.png` -- the LOD Generation
workspace with the **LOD** tab selected; `lodtab_water.png` -- the same
workspace, same crop, with **Water** selected. Both are grabs of
`LodGenerationDock` from inside the application through `water_ui.sh`'s
`WW_WATERUI_LODSHOT` / `WW_WATERUI_LODSHOT2`. Opened and described in two
sentences each before this report says anything about them.

## 0.3 What this lane may NOT do

`src/nifskope_ui.cpp`, `src/nifskope.h`, `src/nifskope.cpp` and `NifSkope.pro`
are touched ONLY through `scratchpad/water8_20260910/hookup.py` (`--check` the
default, exact-once anchors carrying the file's own line ending, CR byte
assert). `src/wateruitest.cpp` and `res/style.qss` are not touched until
`scratchpad/ui4_20260910/DONE` exists. `WW_CHANGES.md` is not edited: the entry
text goes to `scratchpad/water8_20260910/WW_CHANGES_ENTRY.md` for the director.

---

## 1. On disk before gating (lane WATER8-GATE, 2026-09-11)

Lane WATER8 died at its account limit after `build.sh` returned `CHAIN-RC=0`,
leaving `BUILDING` up, no `DONE`, no report section past 0.3, no documents and
an empty `images/`. This continuation gates the exe that lane built. **No
source, QSS or `.pro` file is touched here, and nothing is built.**

Game/instance check before anything: `tasklist | grep -i -E "Fallout4|NifSkope"`
-> `rc=1` (nothing running).

### 1.1 One mtime table (CONSTITUTION 4: four clocks, one table)

| file | size (B) | mtime 2026-09-10 |
|---|---|---|
| `release/NifSkope.exe` | 20,830,208 | **21:02:12** |
| `release/style.qss` | 11,097 | 21:02:12 |
| `res/style.qss` | 11,097 | 09-06 01:35:14 (UI4 changed the SKIN, not the sheet) |
| `Makefile.Release` | 282,781 | 20:59:53 |
| `NifSkope.pro` | 20,235 | 20:59:18 |
| `src/nifskope.h` | 49,810 | 20:59:18 |
| `src/nifskope_ui.cpp` | 1,503,872 | 20:59:18 |
| `src/nifskope.cpp` | 442,590 | 17:03:15 (untouched by WATER8) |
| `src/lodgenmanager.cpp` | 112,166 | 20:48:57 |
| `src/wateruitest.cpp` | 36,863 | 20:59:18 |
| `src/wateruitest_lod.cpp` | 23,785 | 20:52:46 |
| `GeneratedFiles/.obj/wateruitest_lod.o` | 56,607 | 21:00:12 (newer than its source) |
| `GeneratedFiles/.obj/wateruitest.o` | 107,784 | 21:00:11 |

`cmp res/style.qss release/style.qss` -> identical ("sheet in step").

### 1.2 Markers and the hook-up (counts RE-DERIVED, not accepted)

`grep -c "lane WATER8" <file>`:

| file | count | where |
|---|---|---|
| `src/nifskope_ui.cpp` | **3** | :24314 (left strip clamp back to LeftHeader), :29972 + :30005 (the LOD strip joins the shared bar row) |
| `src/nifskope.h` | **1** | :503 (three left modes, no water page) |
| `src/lodgenmanager.cpp` | **1** | :2390 (the panel's own `LOD | Water` strip) |
| `src/wateruitest.cpp` | **1** | :750 (group L called from the existing run) |
| `src/wateruitest_lod.cpp` | **1** | :1 (the new translation unit) |
| `src/nifskope.cpp` | 0 | not a WATER8 file |
| `NifSkope.pro` | 0 | the hook-up adds a SOURCES line, not a comment marker |

`python scratchpad/water8_20260910/hookup.py --check`:

```
E1  src/nifskope.h        REFUSED: the anchor does not match exactly once | marker 'lane WATER8' x1
E2  src/nifskope_ui.cpp   REFUSED: the anchor does not match exactly once | marker 'lane WATER8: back to LeftHeader' x1
E3  src/nifskope_ui.cpp   after anchor LF 1 | marker 'LodPanelModeSelector' x1
E4  src/nifskope_ui.cpp   after anchor LF 1 | marker 'lodPanelSelector->setStyleSheet' x1
E5  NifSkope.pro          after anchor LF 1 | marker 'src/wateruitest_lod.cpp' x1
3 of 5 anchors match exactly once;  CR 0 in all three files
```

Read correctly: **all five are APPLIED.** E1/E2 are *replace* edits -- once
applied, the old anchor text is gone, so "does not match exactly once" is the
expected state and the marker being present x1 is the proof. E3/E4/E5 are
*after* edits, whose anchor survives insertion, so they still match once AND
carry their marker once. No file has any CR byte, matching the LF-only rule for
`src/`.

Build wiring: `NifSkope.pro:357` names `src/wateruitest_lod.cpp`;
`grep -c wateruitest_lod Makefile.Release` = **7** (qmake did regenerate with
the new source); `wateruitest_lod.o` 21:00:12 is newer than
`wateruitest_lod.cpp` 20:52:46.

### 1.3 Header staleness for `src/nifskope.h` (build-verify: "a successful build is not a consistent one")

`src/nifskope.h` changed 20:59:18. Every translation unit in `src/` that
includes it -- **35 objects** -- was checked against that time:

| result | count | range of object mtimes |
|---|---|---|
| ok (object newer than the header) | **35 / 35** | 20:59:57 .. 21:02:08 |
| STALE | **0** | -- |
| missing object | **0** | -- |

The earliest is `glscene.o` 20:59:57, the latest `nifskope_ui.o` 21:02:08; the
header predates all of them. **Nothing is stale, no relink is owed, and the
class-layout segfault class the skill warns about is not in play.**

### 1.4 Exe-newer sweep (gate G4)

`git status --porcelain -- src res tools tests` yields **116** existing files;
each was tested `release/NifSkope.exe -nt <file>`.

```
swept=116 stale=0
```

No `STALE` line. G4 is green.

## 2. Gates

All on `release/NifSkope.exe` **21:02:12, 20,830,208 B**, proven newer than
every changed file (1.4). One sequential chain,
`scratchpad/water8_20260910/gates.sh`, one NifSkope instance at a time, every
env assignment on the CHILD (`run <label> <secs> env VAR=... bash ...`), which
is the leak this skill's section 11 was written about. Logs:
`scratchpad/water8_20260910/logs/`, summary `logs/SUMMARY.txt`.

**The spell's real env names are `LODSHOT` / `LODSHOT2`**, not the
`WW_WATERUI_LODSHOT` names WATER8 registered as intent -- those are the
variables `tests/spells/water_ui.sh` sets on the child process from them
(`src/wateruitest.cpp:776`). Passing the registered names would have produced a
green gate and no pictures.

| harness | this run | baseline (BUILD/UI4, 2026-09-10) | log | delta |
|---|---|---|---|---|
| `water_ui.sh` | **59 checks, 0 failures, 0 skips, PASS** | 48 / 0 (floor 41) | `logs/water_ui.log` | **+11, explained below** |
| `ui_align.sh` | **11 / 0, PASS** | 11 / 0 | `logs/ui_align.log` | 0 |
| `top_bar.sh` | **43 / 5, FAIL** | 43 / 5 | `logs/top_bar.log` | 0, the same five |
| `files_tab.sh` | **28 / 2, FAIL** | 28 / 2 | `logs/files_tab.log` | 0, the same two |
| `animws.sh` | **57 / 0, 1 skip, PASS** | 57 / 0, 1 skip | `logs/animws.log` | 0 |
| `water_mark.sh` | dock **20 / 0 PASS**, model self-test **FAIL**, spell FAIL (2) | dock 20 / 0, body 3, 2 FAIL | `logs/water_mark.log` | 0, the same red |
| `water_window.sh` | **46 / 0, PASS** | 46 / 0 | `logs/water_window.log` | 0 |
| `lodl_water.sh` | **33 ok lines, 0 FAIL, RESULT PASS** (prints no count -- counted by hand, resume-pending s5) | 33 / 0 | `logs/lodl_water.log` | 0 |
| `loaded_nifs.sh` | **166 / 0, PASS** | 166 / **2** | `logs/loaded_nifs.log` | **two reds went green, named below** |

### The +11 on `water_ui.sh`, by arithmetic and by name

Group T (15 checks, the Water tab in the LEFT strip) is retired; group L (29
checks, the LOD panel's own strip) replaces it. `48 - 15 + 29 = 62`, and the
run reads 59 because the S group's three picture checks are only taken when
their `SHOT`/`TABSHOT`/`STRIPSHOT` are given and this chain gave neither
(it gave `LODSHOT`/`LODSHOT2`, whose two "(shot) ... was written" checks ARE
in the 29). Counted out of the log: R 19 + S 10 + L 29 + "the document loaded"
1 = **59**. The spell's own floor is 48 and it was not tripped.

**Every group-L gate is green, with its floor:**

| id | the number the log printed |
|---|---|
| L1 | 2 tabs, `"LOD"` at 0 then `"Water"` at 1; floor: the same search finds no `"Watre"` |
| L2 | stack = generator page at 0, water page at 1, nothing else; each tab's data IS its index (0->0, 1->1); floor: the same predicate at the wrong index is false |
| L3 | selecting Water shows page 1, tool box + Solve + Save + the flow-window button all visible; floor: selecting LOD puts page 0 and Generate back |
| L4 | LOD strip **35** px = `wwBarRowHeight()` **35** = left strip **35**; floor: both rectangles non-degenerate |
| L5 | the two strips' stylesheets are byte-identical; floor: the compact default (504 chars) differs from the row sheet (580 chars) |
| L6 | left strip **3** tabs / **3** pages with the LOD workspace OPEN *and* CLOSED, no tab named "Water"; floor: the three are Header, Blocks, Files |
| L7 | Workspaces menu: "Water Marking" 0, "Water window" 0, no `WaterMarkDock`; floor: the same scans do find "LOD Generation" (1) and the LOD dock |
| L8 | air 4; painted segment 0 of the LOD strip **top 4, bottom 4** in a row of 35 (segment h 27); floor: the LEFT strip reads the same two numbers |

### Gate L8 is NARROWER than it was registered -- read this before UI6

Report 0.1 registers L8 as *"4 px clear of the row top and bottom **and of one
another**"*. **The shipped L8 (`src/wateruitest_lod.cpp:485-517`) measures only
the row's top and bottom clearance; there is no check anywhere for the gap
BETWEEN the two segments of the LOD strip.** It printed
`LOD strip top 4 bottom 30 (h 27) ... in a row of 35` -- vertical only. So L8
is green, and green here says nothing at all about bungo's *"Why are they
separated?"*.

The inter-segment gap IS measured, but on the LEFT strip, by UI4's group S:
**`(S5) every pair of segments is 4..4 px apart (want 4)`**, green. **That 4 is
the number lane UI6 has to drive to 0**, and when it does, S5 and the "want 4"
in S3/S4 go red until they are re-registered. L8 will not notice either way
unless UI6 adds the missing half. Neither the gate nor the QSS was touched here
(the brief forbids it, and changing a gate needs a build).

### The reds, each by name (none is new, none is this lane's)

* `top_bar.sh` **5** -- `and the panel toggles it absorbed`, then
  `Panels lists the Block List dock`, `... Block Details dock`, `... Header
  dock`, `... NIF Browser dock`. The harness expects a View menu listing docks
  that were merged into one "Left Editor" entry long before this session
  (resume-pending s10 names this exact set).
* `files_tab.sh` **2** -- `(5) every tool button explains itself (0 without)`
  and `(4) unload restores the bind pose BYTE-identically`. Both are BUILD9's
  same two.
* `water_mark.sh` **2** -- the dock half is 20 / 0 PASS; the model self-test
  fails on the single pre-registered red **X2b**: *"the pin adds **0.407** of
  net outward flux per texel through a ring at two pin widths (647 texels; gate
  > 0.5)"* on body 3. That is BUILD12's number to three digits (0.407), so it
  has not moved. Its own refuter X2a is green (with the pin removed, 0 texels
  move), and X1c, X2, X3c are green.
* `animws.sh` **1 skip** -- `(i) 10mmPistol.nif has no NiControllerSequence to
  test with`. Same skip as UI4's run; a skip is not a pass and it is named here.

### `loaded_nifs.sh` 166 / 2 -> 166 / 0: the two, by name, measured not guessed

BUILD12's handoff says plainly that nobody could name the check that turned
green there, "because BUILD9 recorded only the count". That is fixed here. The
kept rollback rung `release/NifSkope.before_ui4.exe` (**18:25:20**, lane UI3's
exe, which still carried lane WATER7's FOURTH left tab) was run as a CONTROL --
deliberately an OLD exe, which is the only way to photograph the old state:

```
EXE=release/NifSkope.before_ui4.exe PORT=45898 bash tests/spells/loaded_nifs.sh
  -> 166 checks, 2 failures
  FAIL the top selector orders Header, Blocks and NIFs without remapping modes
  FAIL NIF Browser is above Loaded NIFs in its own mode
```
(`logs/loaded_nifs_CONTROL_ui3exe.log`)

On the 21:02:12 exe both of those lines read `ok`. Both are checks about the
LEFT strip having exactly three modes and its pages not being remapped -- which
is precisely what this lane put back (`src/nifskope.h:503`,
`src/nifskope_ui.cpp:24314`). **The two reds were WATER7's fourth tab, and
removing it cured them.** No other check moved: 166 in both runs.

### Nothing was left running (G5)

`tasklist | grep -i -E "Fallout4|NifSkope"` -> `rc=1` before the chain, after
the chain and after the control run. `release/NifSkope.exe` is still 21:02:12,
20,830,208 B -- no build, no relink, no source touched.

## 3. Pictures

All three are IN-APPLICATION grabs, written by the harness from inside the
running window (never a desktop capture, CONSTITUTION 5). Each was opened and
looked at before this report says anything about it.

| file | size | pixels | written by |
|---|---|---|---|
| `scratchpad/water8_20260910/images/lodtab_lod.png` | 30,994 B | 499 x 741 | `water_ui.sh`, `LODSHOT=` |
| `scratchpad/water8_20260910/images/lodtab_water.png` | 27,860 B | 499 x 741 | `water_ui.sh`, `LODSHOT2=` |
| `scratchpad/water8_20260910/images/toprow_after.png` | 5,615 B | 602 x 82 | `ui_align.sh`, `SHOT=` |

All three carry a valid PNG signature and are non-empty. G3 green.

### `lodtab_lod.png` -- described first, cited after

A grab of the whole `LodGenerationDock` (title bar "LOD Generation") with a
two-segment strip directly under the title: a highlighted blue **LOD** plate on
the left and a dark **Water** plate on the right, the two plates side by side
across the dock's full width. Below the strip is the generator exactly as it
has always been -- Source / Plugins / Resources / Worldspace / Output mod, the
192 x 192-cell progress square, "Show chunks in the viewport as they finish",
and the pinned action bar with **Generate** and **Cancel** at the bottom right.

**What it proves:** the LOD Generation panel on the right now carries its own
segmented tab strip, and the generator is page 0 behind the LOD tab, unmoved
and complete. **What it does not prove:** anything about where the strip's
pixels sit -- that is L4/L8, measured, not looked at.

### `lodtab_water.png` -- described first, cited after

The same dock, same crop, same size, with the **Water** plate now highlighted
blue and **LOD** dark. The body is the water tool: Landscape file (File /
Browse, Show = Body ID), Marking (Tool = Stroke, Speed 0.250, Width 4096, Dye
colour Choose, Dye fade 8192), Selected body (Class = Automatic), Water form
(Colour override, Flow "Still water", Dye "Dye at mouth", Name), a folded
**Bake** section with "Flow samples per cell 32", the refusal line *"No
landscape file is open. Choose a version 3 .lodl to mark its water."*, and the
action bar **Water window | Reload | Solve | Save**.

**What it proves:** bungo's *"I wanted it in that right panel though"* is
satisfied -- the whole water tool, marking rows and the full-screen flow
window's button included, is the second tab of the right-hand panel, and the
old dock is gone. **What it does not prove:** that the tool still SOLVES
correctly; that is `water_mark.sh` / `water_window.sh` / `lodl_water.sh` in
section 2, not this picture.

### `toprow_after.png` -- described first, cited after

The top-left corner of the window at 602 x 82: three separate rounded plates
reading **Header | Blocks | Files** (Files selected, blue) with an even band of
background between and around them, then the Object Mode toolbar with its
dropdown arrow, then Select / Add / Object; under the strip the "Search files..."
row with its star, box, folder and refresh icons.

**What it proves:** the LEFT strip is back to exactly three tabs -- the Water
tab that lane WATER7 put there is gone (L6 confirms 3 tabs and 3 pages, with
the LOD workspace open AND closed). **What it does not prove:** that the
segments are joined -- they are visibly separated by UI4's 4 px, which is the
state bungo objected to and which lane UI6 changes, not this lane.

## 4. Verdict and what is owed

### Green, with the number

* The feature bungo asked for is **built, on disk and measured**: the Water tool
  is the second tab of a two-segment strip inside the LOD Generation panel on
  the RIGHT (`lodtab_water.png`), the LOD generator is the first
  (`lodtab_lod.png`), and the LEFT strip is back to Header | Blocks | Files
  (`toprow_after.png`, and L6's 3 tabs / 3 pages in both workspace states).
* `water_ui.sh` **59 checks, 0 failures, 0 skips, PASS**, floor 48. All 29
  group-L checks green, each with a floor that was asked in the same run.
* The old Water dock and both Workspaces-menu entries are gone (L7: 0 and 0,
  with the floor finding "LOD Generation" so the scan can find something).
* Nothing regressed: `ui_align` 11/0, `top_bar` 43/5, `files_tab` 28/2,
  `animws` 57/0, `water_mark` dock 20/0, `water_window` 46/0, `lodl_water`
  33/0 -- every one exactly at its baseline.
* `loaded_nifs.sh` **improved** 166/2 -> 166/0, and the two are named above
  against a control run of the 18:25:20 rung.
* The build is consistent: 35 of 35 objects that include `src/nifskope.h` are
  newer than it; 116 of 116 changed files are older than the exe.

### Red, with the number

1. **`water_mark.sh` X2b, 0.407** (gate > 0.5) on body 3 -- unchanged from
   BUILD12, a known instrument problem (the same ring metric reads 0.595 on
   body 2), not a WATER8 regression. Owed to the director/bungo since BUILD12.
2. **`top_bar.sh` 5** and **`files_tab.sh` 2** -- pre-existing, named above,
   neither reaches this change.
3. **Gate L8 is half the gate it was registered as** (see section 2): it never
   measures the gap between segments. This is a gate defect, not a feature
   defect, and repairing it needs a rebuild, so under the brief's rule it is
   MEASURED AND REPORTED, not fixed.

### Not measured, and why

* Every suite the change does not reach: lodgen / terrain / impostor / gltf /
  hkx* / collision / block / water solve / water flow / water weights /
  `skeleton_overlay.sh` (flaky by BUILD11's own four-run measurement).
* `lodl_open.sh` -- its fixture is bungo's own installed
  `mods/FO4CS/Terrain/Commonwealth.lodl` and nothing here renames or rewrites
  it; `lodl_water.sh` covers the same reader on a repo fixture and is green.
* Whether bungo LIKES the two-tab strip. That is his to say; no harness has an
  opinion.
* Whether the strip's segments should touch -- deliberately NOT changed here.

### What lane UI6 changes next

bungo, verbatim 21:0x: **"Why are they separated?"** -> segments touch, only
the strip's outer box keeps 4 px. Concretely, against numbers now on record:

* `(S5) every pair of segments is 4..4 px apart` must become 0. S1/S2 (row top
  and bottom) stay 4; S3 (window edge) and S4 (toolbar) stay 4.
* Each segment is 27 px tall inside the 35 px row today; joined, they keep 27.
* **L8 needs its missing half**: the same 0-px assertion for the LOD panel's two
  segments, or the LOD strip will silently keep whatever gap it has.
* Both strips share one stylesheet byte for byte (L5), so one change moves both
  -- which is what makes L5 worth keeping green.

### Handed over

* `scratchpad/water8_20260910/DONE` written, `BUILDING` removed -> lanes UI5
  and UI6 are unblocked.
* `scratchpad/water8_20260910/WW_CHANGES_ENTRY.md` and `HANDOFF_BLOCK.md` are
  TEXT for the director to splice. `WW_CHANGES.md`, `HANDOFF.md` and
  `MISTAKES.md` were NOT edited by this lane (CONSTITUTION 8).
* Lane **UI5 is alive in the tree right now** (`scratchpad/ui5_20260910/probe.cpp`
  and `release/ui5_probe.exe` both stamped 2026-09-11 05:22). It has touched no
  file under `src/`, `res/`, `tests/` or `tools/` (checked: nothing there is
  newer than 2026-09-11 00:00), so the exe-newer sweep above still holds. Its
  standalone probe is not NifSkope and did not break the one-instance rule.

## 5. Mistakes

Both entries are also in `scratchpad/water8_20260910/MISTAKES_ENTRIES.md`, as
text for the director to append to `MISTAKES.md` (this lane does not edit that
file).

## 2026-09-11 -- lane WATER8: a finished build left BUILDING up and DONE unwritten

**What was done.** Lane WATER8 ran its chain to `CHAIN-RC=0`
(`release/NifSkope.exe` 21:02:12, 20,830,208 B) and then died at its account
limit. It left `scratchpad/water8_20260910/BUILDING` in place, no `DONE`, no
report section after 0.3, no `WW_CHANGES_ENTRY.md`, no `HANDOFF_BLOCK.md` and
an empty `images/`.

**What was true instead.** A built exe nobody has gated is indistinguishable,
from outside, from a lane that died mid-compile -- and `BUILDING` is the flag
two other lanes (UI5, UI6) wait on. The build was finished at 21:02:12; the
marker said "in progress" for eight hours.

**How it was found.** The continuation lane listed the directory before
believing anything: `BUILDING` present, `DONE` absent, `images/` empty, and
`find src res tests NifSkope.pro -newer release/NifSkope.exe` printing nothing.

**The rule that prevents it.** CONSTITUTION 1b/1c already say a lane past half
its window writes its report and resume FIRST. Extended by this: the build
markers are part of that write, not part of the report. **The moment a chain
returns, the marker is settled -- `DONE` if it linked, `PENDING.md` if it did
not -- before the gates are run and before anything else is written.** A lane
that cannot afford to run its gates can still afford one line of text, and a
`DONE` beside a red gate is a valid, unblocking verdict.

## 2026-09-11 -- lane WATER8-GATE: two copies of the gate chain ran at once, and the logs were rubbish

**What was done.** The chain script was launched three times in ninety seconds.
The first launch redirected its stdout INTO the directory the script itself
creates (`.../logs/chain_stdout.log`), so the shell's redirect failed before
the script ran and it looked like nothing had started. The second launch was
believed dead for the same reason -- its own redirect and `logs/SUMMARY.txt`
were both checked too early and both were empty -- so a third was started. Two
`gates.sh` processes then ran concurrently, each launching NifSkope.

**What was true instead.** Both were alive. `SUMMARY.txt` interleaved two runs
(`### water_ui start 05:23:53` next to `### files_tab rc=1 05:23:55`) and the
harnesses collided on their fixed ports: `animws.sh` reported "the harness
wrote no log (did the app exit before it ran, or is port 42317 bound?)" and
`water_mark.sh` "no dock log -- did the app exit before the harness ran?". Two
NifSkope instances were up at the same time, which CONSTITUTION 6 forbids
outright.

**How it was found.** The out-of-order timestamps in the lane's own summary
file, then `Get-CimInstance Win32_Process` showing two `gates.sh` shells
(pids 50100 and 16408) and two `-no-gui` NifSkope processes.

**Cost and damage.** Nine harness logs thrown away and the chain re-run; about
four minutes. Nothing was built, deployed or written outside the lane's
scratchpad; the void run is kept as
`scratchpad/water8_20260910/logs_void_doublechain/` and
`images_void_doublechain/` rather than deleted, so the collision stays
readable. No process of bungo's was touched (both NifSkope instances carried
`-no-gui` command lines belonging to this lane's own spells).

**The rules that prevent it.** Three, in order of how much they would have
saved:

1. **Before launching a chain, prove no copy is already running**, by command
   line, not by an empty log: the `Get-CimInstance Win32_Process` filter from
   `nifskope-ww-build-verify` ("Whose NifSkope is that?") applied to `bash.exe`
   with the script's own name. An empty `SUMMARY.txt` means "has not written
   yet", never "did not start"; a process list is the only honest answer.
2. **Never redirect a launcher's output into a directory the launched script
   creates.** `mkdir -p` the log directory in the SAME shell, before the
   redirect is parsed -- the redirect is opened before the command runs, so
   `script.sh > newdir/log` fails no matter what the script would have done.
3. **The chain script refuses a second copy itself.** A lock (`mkdir
   $OUT/.lock` or a pid file) makes the double launch impossible instead of
   merely unlikely, and it costs three lines. `gates.sh` for this lane did not
   have one; the next chain script written in this tree should.

## 6. Finished-work skill review (CONSTITUTION 1a)

### Skills loaded, and what each one actually saved

* **`nifskope-ww-resume-pending`** -- the whole shape of this lane. Section 4
  (sweep EVERY changed file, not the one you edited) is what produced the
  116-file check; section 5 (one sequential chain, echo the summary, some
  harnesses print no count) is why `lodl_water.sh`'s 33 was counted by hand
  instead of reported blank; **section 11 is the one that paid for itself
  twice**: it named the `VAR=x run_helper` leak, so every assignment in
  `gates.sh` went on the child (`run <label> <secs> env LODSHOT=... bash ...`),
  and it said a pending lane's NEW harness cannot photograph the OLD state --
  which is exactly why the `loaded_nifs` control was run against a kept rung
  rather than reasoned about.
* **`nifskope-ww-build-verify`** -- "a successful build is not a consistent
  one" (the 35-object header sweep), the `-nt` rule, the link-time stylesheet
  copy (`cmp res/style.qss release/style.qss`), and "Whose NifSkope is that?",
  whose `Get-CimInstance Win32_Process` command line filter is what identified
  the double chain and told me both NifSkope processes were mine, not bungo's.
* **`nifskope-ww-render-shot`** -- read only for its one-instance /
  second-monitor / invisible-headless rules, as the brief said. No headless
  render was needed; all three pictures are in-app dock grabs.
* **`ww-test-harness-add`** -- consulted when L8 turned out to be narrower than
  registered. Its rule ("a check that cannot fail on its input is not a check")
  is what made me look at what L8 asserts rather than accept its `ok`.
* **`ww-anchored-hookup`** -- for the shape of the skill-amendment script
  below: exact-once anchor, CR byte assert, `--check` that writes nothing.

### The skill that should have existed -- written

The double gate-chain launch (section 5, second entry) cost four minutes and
nine rubbish logs, and NOTHING in either skill said how to prove a chain is not
already running. That procedure is now written into the skill that owns the
chain:

**`.claude/skills/nifskope-ww-resume-pending/SKILL.md`**, new subsection of
section 5, **"Exactly one copy of the chain, proved by the process list"**
(12,810 -> **14,560 bytes**, CR 0 -> 0, marker present x1). It carries: an empty
log is not "did not start", so ask the process list by command line; never
redirect a launcher into a directory the launched script creates; give the chain
script a three-line `mkdir`-lock so a second copy refuses itself; and keep the
ruined run beside the good one as `logs_void_<reason>/`.

Applied by the refusing script `scratchpad/water8_20260910/skill_amend.py`
(`--check` first: anchor x1, CR 0, would grow 12,810 -> 14,560; then applied,
and it refuses on a second run because the marker is already there).

**DIRECTOR: the two skill trees drift and nothing syncs them.** The amendment
is in the REPO tree only. The live copy
`E:\Projects\Claude\.claude\skills\nifskope-ww-resume-pending\SKILL.md` is still
the old 12,810 bytes (17:57:07) and needs the same file mirrored.

### Declined, with the reason

* A skill for "gate a finished-but-ungated build" -- declined: that is
  `nifskope-ww-resume-pending` already, and this lane is the proof it works.
  The only gap was the one above, and it was filled in place rather than forked.
* A skill for reading `water_ui.sh`'s group letters -- declined: the arithmetic
  is written in the spell's own comment block, re-counted there by two lanes,
  and it will change again the moment a group is retired. A skill would date
  faster than the file.

---

## Addendum, 05:32 -- bungo opened the window himself

Written after section 6. At the final process check, `tasklist` showed one
`NifSkope.exe` (pid **8428**, created **2026-09-11 05:32:30**). Its command line
is `"E:\Projects\NifskopeWildWastelandEdition\release\NifSkope.exe"` with **no
`--port`**, which by `nifskope-ww-build-verify`'s discriminator makes it bungo's
own interactive window, not a harness leftover. It was not touched.

Two consequences:

* **No restart is owed.** 05:32:30 is after the 21:02:12 link, so the window in
  front of him already carries the Water tab in the right-hand panel. The
  HANDOFF block says so.
* **That window holds `release/NifSkope.exe`.** The next build -- lanes UI5 and
  UI6, which this lane's `DONE` has just unblocked -- must rename the running
  copy aside immediately before the link (`NifSkope_inuse_8428.exe`), never kill
  it, and must do the rename in the same shell as the link rather than at the
  top of the chain. That is the 20:45:04 incident already in `MISTAKES.md`, and
  the conditions for repeating it are live right now.

G5 as the brief worded it ("`tasklist` shows no NifSkope when you finish --
harness instances only; if a `--port`-less window appears it is bungo's, never
touch it") is therefore met: zero harness instances, one window of his, not
touched.
