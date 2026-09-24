# Lane WATER7 / UI2 -- the water tool becomes a tab of the LOD Generation
# workspace; every top bar one compact height

Tree `E:\Projects\NifskopeWildWastelandEdition`, `main`, nothing committed.
Written incrementally; section 0 was written BEFORE any code, per CONSTITUTION
rule 1 ("the gates are pre-registered in the brief, BEFORE the agent starts").

---

## 0. Pre-registered gates (written before a line of code)

Every number below is a PREDICTION until a built exe produces it. The lane
cannot build while the BUILD11 slot is held; whatever is not run is named in
section 4 and again in `PENDING.md`.

### 0.1 New harness `WW_WATERUI_TEST` -- `src/wateruitest.cpp`, spell `tests/spells/water_ui.sh`

Its own translation unit (`nifskope_ui.cpp` is contended, and this lane may not
edit it except through `hookup.py`). It reads WIDGETS by object name, never
private members, and every check carries a floor that can fire.

**Group T -- the Water tab (work A)**

| id | check | floor on the other side |
|---|---|---|
| T1 | `LeftColumnModeSelector` carries a tab whose text is exactly `Water` | the same search for a tab named `Wagter` finds none, so the search is a real search |
| T2 | its `tabData` is 3, and 3 is the index the panel occupies in `LeftColumnStack` | the three existing tabs still map to 0/1/2, printed |
| T3 | `LeftColumnStack` has exactly 4 pages, the 4th being the `WaterMarkPanel` (object name `WaterMarkPanel`) | a page count of 3 is a red naming the missing page |
| T4 | selecting the Water tab makes the stack show page 3 (**this is the check that fails by name if `hookup.py` edit E2, the clamp, did not land**) | selecting the Blocks tab puts it back on page 0 -- the same assertion is shown working on a tab that always worked |
| T5 | with `LodGenerationDock` hidden the Water tab is NOT visible; with it shown, it is | both halves are asserted in one run, so a tab that is always visible and a tab that is never visible both go red |
| T6 | hiding the LOD dock while the Water tab is current leaves the strip on the Blocks tab, not on a hidden tab | the same test with the Blocks tab current leaves it on Blocks (no spurious switching) |
| T7 | `ViewWorkspacesMenu` contains **0** actions whose text is `Water Marking` or `Water window` | the same scan finds `LOD Generation` (>= 1), so the scan is seen to be able to find something |
| T8 | there is no `QDockWidget` named `WaterMarkDock` anywhere under the main window | the same search finds `LodGenerationDock`, so it is a real search |

**Group R -- the bar row (work B)**

Measured in MAIN-WINDOW coordinates (`mapTo( skope, QPoint(0,0) )`), which is
the only frame two widgets in different parents can be compared in.

| id | check | floor |
|---|---|---|
| R1 | `menubar`, `tFile`, `tLOD`, `tView`, `ViewportHeader`, `LeftColumnModeSelector` all report height == `wwBarRowHeight()` within 1 px | every rect non-degenerate (w > 0 and h > 0), printed one per line |
| R2 | the menu bar's height equals the dock strip's height within 1 px | the same comparison is shown going RED after the menu bar is grown 4 px, then restored |
| R3 | every `QToolButton` that is a descendant of `tFile` / `tLOD` / `tView` reports a height within 1 px of the others in its own bar, and >= `wwBarRowHeight() - 8` | the count of such buttons is printed and must be >= 4, so an empty bar cannot pass |
| R4 | the row's vertical padding is stated ONCE: `wwBarRowButtonQss()` is the only place a padding number is computed; the check reads the sheet the skin returned and asserts it contains exactly one `padding:` declaration per selector | a hand-written second number in the sheet makes the count != 1 |
| R5 | the search row's content top is unchanged from BUILD9's measurement, or the new number is printed and named | the number is always printed, pass or fail |

`ui_align.sh` (BUILD9's, 11 checks / 0 failures) is run unchanged as a
NEIGHBOUR: its two assertions must still hold after the menu bar joins the row.

**Predicted numbers, section 2 will replace them with measured ones**
BUILD9 measured the row at **35 px** and the menu bar was never in it. The
prediction is: row height stays **35** (the tallest natural height among the
bars is still a main toolbar, not the menu bar, whose `QMenuBar::item` padding
in `res/style.qss` is `4px 8px` and whose natural height is therefore ~23-25),
the menu bar GROWS from ~23-25 to 35, and nothing else moves. **If the measured
menu-bar natural height turns out to be ABOVE 35, the row grows instead and
every bar gets taller -- which is the opposite of the word bungo used
("compact"), and in that case the lane reports the number and does NOT ship the
change.** That refuter is registered here, before the measurement.

### 0.2 The five reds (work C)

| red | gate that proves it moved | floor |
|---|---|---|
| 2, `water_flow.sh` takes the informational line | the loop's own grep becomes `grep -aE '^  (ok\|FAIL) '` before `head -1`; `F8 the solve` stops being reported red while its `ok` line is present | a deliberately red gate must still be reported red -- the spell is run once with a seeded FAIL line in the fixture text |
| 3, floor 18 unreachable | the floor becomes **17**, and the spell prints the arithmetic `19 F-gates - 2 pre-registered red = 17` | 17 is one BELOW the count a green run produces, so a run that loses one more gate still goes red |
| 4, `water_mark.sh` runs body 2 | the headless half is pinned `WW_WATER_MARK_BODY=3`, as `water_flow.sh` already does | the spell prints the body it pinned, so a future run cannot silently drift |
| 5a, the dye pin's per-point weight is never written | `WaterCurveDoc::writeTo` writes the weights for `DyePin` as it already does for `Stroke`/`Pin`; gate: write a dye pin with weight 0.25, save, reopen, read 0.25 back | the same round trip with the fix absent reads 1.0 (the reader's default), which is the red |
| 5b, the FIRST named body reads back nameless | `encodeNames()` reserves byte 0 (a leading NUL) and `encodeTable()` starts `nameAt` at 1, so no real name can sit at the offset `LodtFile::bodyName` spells "no name"; gate: name body 1 `Charles`, save, reopen, read `Charles` back | an unedited document must still re-encode to the file's own bytes -- with no names the blob is empty and every offset is 0, unchanged |
| 1, X2b's radial cosine | replaced by the NET FLUX through a ring around the pin (`lane_water6_report.md` s6): sum of `v . n` over a ring of radius 2 pin-widths, normalised by the sum of `\|v\|` over the same ring. **Registered floor: >= 0.5.** A source has all-outward flux; curvature moves the flux vector round the ring but not its sign, which is why this instrument cannot be biased by the Charles bending inside four widths | the SAME ring on the SAME body with the pin REMOVED must read below 0.2 -- the refuter is run first, so the instrument is seen to be able to fail |

The X2b floor is registered HERE, before the instrument is coded, per the
brief's "register the floor before coding".

### 0.3 Pictures (work B)

Before/after grabs through the render hook / the in-app grab, never a desktop
capture: `scratchpad/water7_20260910/images/topbar_before.png` and
`topbar_after.png` (the whole top strip: menu row, workspace row, dock strip,
viewport header), plus `watertab.png` (the Water tab open in the LOD Generation
workspace, `SHOT=` through `water_mark.sh`). Both opened and looked at before
the report says anything about them.

### 0.4 What this lane may NOT do

`src/nifskope_ui.cpp`, `src/nifskope.cpp`, `src/nifskope.h` and `NifSkope.pro`
are touched ONLY by `scratchpad/water7_20260910/hookup.py` (`--check` default,
exact-once anchors carrying the file's own line ending, CR byte assert).
`WW_CHANGES.md` is not edited: the entry text goes to
`scratchpad/water7_20260910/WW_CHANGES_ENTRY.md`. The `.lodl` contract and the
curve json are untouched -- if either had to move the lane would stop and say
so, and it did not have to.

---

## 1. How the strip scopes tabs, and what was built

### 1.1 The answer to the brief's question, with file:line

**The strip does not scope its tabs per workspace today, at all.** All three are
added unconditionally:

* `src/nifskope_ui.cpp:24175-24186` -- `leftColumnSelector->addTab( tr( "Header" ) )`
  / `"Blocks"` / `"Files"`, each followed by `setTabData` and `setTabToolTip`,
  with no condition anywhere, and nothing in the tree ever calls `setTabVisible`
  on that bar.
* `src/nifskope_ui.cpp:24069-24090` -- `NifSkope::setLeftColumnMode` maps a mode
  to a page with `leftColumnStack->setCurrentIndex( int( mode ) )`, so **the mode
  number IS the stack page index**, and clamps anything outside
  `LeftBlocks..LeftHeader` back to `LeftBlocks`.
* `src/nifskope.h:501-503` -- the enum and `leftColumnIs()`.

The only per-workspace mechanism the window has is on the manager DOCKS:
`src/nifskope_ui.cpp:26777-26788` sets `workspaceRole = "manager"` on the ten of
them and connects an exclusive-visibility rule, and
`src/nifskope_ui.cpp:27452-27472` (`activateWorkspace`) hides them all and shows
one. "The LOD Generation workspace is active" therefore means, to every other
piece of code in this window, "`LodGenerationDock` is visible"
(`src/lodgenmanager.cpp:2383-2384` names it). **That is what the Water tab
follows** -- there was no tab-scoping convention to follow, so the dock
convention was followed instead, and this says so rather than inventing one.

### 1.2 What was built (A)

* `src/watermarkpanel.cpp` -- `waterMarkInstall()` no longer creates a
  `QDockWidget`. It finds `LeftColumnModeSelector` and `LeftColumnStack` by
  object name, adds a `WaterMarkPanel` as a stack page, **asserts the page landed
  at index 3 and refuses in words if it did not** (a tab that bounces to Blocks
  reads as a broken tool, not an absent one), adds the `Water` tab with
  `tabData` = that index, and shows or hides the tab with `LodGenerationDock`.
  When the tab is hidden while it is current, the strip is moved to the Blocks
  tab first, so the highlight and the page can never disagree. **Fallback
  floor:** with no LOD dock in the window at all, the tab stays visible rather
  than the tool disappearing.
* `src/waterwindow.cpp` -- `waterWindowInstall()` no longer adds `Water window`
  to the Workspaces menu. `waterWindowOpen()` and everything the window does are
  untouched; the one door left is the button already in the panel.
* `src/watermarkpanel.cpp` -- `runSelfTest` retargeted from the dock to the left
  dock plus the tab (show the LOD workspace, select `Water`, grab
  `LeftColumnDock`). **No check changed**, so `water_mark.sh`'s dock floor of 16
  and its 20/0 are the same floors on the same assertions.
* `hookup.py` E1 / E2 -- `LeftWater = 3`, and the clamp that lets it be reached.

### 1.3 What was built (B)

* `src/wwskin.h` -- two new declarations, `wwBarRowButtonQss( int )` and
  `wwCompactTopBars()`. **`wwAlignBarRow`'s signature is deliberately
  unchanged**: see section 5, mistake 1.
* `hookup.py` E3 -- their definitions, beside the other skin helpers. The
  vertical padding is `max( 2, ( row - 18 ) / 2 )` and the min-height
  `max( 12, row - 4 )`: both derived from the row, so no second number is typed
  anywhere, and the sheet carries exactly one `padding:` per selector -- gate R4
  counts them.
* `hookup.py` E4 -- `wwAlignBarRow` computes that sheet ONCE and **appends** it
  to each bar it aligns. Appends, never assigns: the viewport header carries a
  sheet of its own, and replacing it would take that look away while fixing the
  height.
* `hookup.py` E5 -- the row gains `ui->menubar`, behind `wwCompactTopBars()`.

## 2. The bar numbers before/after

**NOT MEASURED. Nothing in this lane has been executed by a built exe.**

| | value | source |
|---|---|---|
| main toolbars, before BUILD9 | 35 px | BUILD9's own measurement, HANDOFF |
| viewport toolbar, before BUILD9 | 33 px | same |
| dock tab strip, before BUILD9 | 26 px | same |
| all three, after BUILD9 | 35 px | same, `ui_align.sh` 11 / 0 |
| the menu bar | **never measured** | it was not in the row |

The prediction registered in section 0, before any code: the row stays **35**,
the menu bar grows from about 23-25 (its `QMenuBar::item` padding is `4px 8px` at
`res/style.qss:48`, which is where that estimate comes from -- an estimate, not a
number), and nothing else moves. **The refuter, also registered before the
code:** `wwAlignBarRow` takes the TALLEST natural height, so if the menu bar is
taller than 35 every other bar grows instead, which is the opposite of "compact".
Gate `water_ui.sh` prints `wwBarRowHeight()` and every bar's rectangle on its own
line, so the first run answers this without another round.

## 3. The five reds

**1 -- X2b read 0.371 against a registered 0.5.** Cause: the disc metric
compares the solved flow with a STRAIGHT-LINE radial over four pin widths, and
the Charles bends inside four widths; nothing about the pin moved. Change
(`src/watermark.cpp`, the X2 block): the net outward flux through a thin ring at
two pin widths, swept once with no strokes and once with the pin, over the
intersection of the two texel sets, so through-flow cancels in the difference
whatever the channel does. The pin point is now chosen BEFORE the refuter solve,
which is what lets both states be read from one solve each. Numbers: floor
**> 0.5** for the difference and **< 0.2** for the no-pin ring, both registered
in section 0 before the code was written. **Neither has been run.** The retired
disc number is still printed beside the new one, informational, so the first run
reads as a comparison of two instruments on one fixture.

**2 -- a green gate reported red.** Cause: `grep -F "$g" | head -1` took the
informational line the harness prints above the verdict. Change: a
`grep -aE '^  (ok|FAIL) '` filter before `head -1` in
`tests/spells/water_flow.sh` -- the same one-line rule `water_weights.sh`
already had.

**3 -- an unreachable floor.** Cause: 18 green F-gates demanded while 2 of the
19 are pre-registered red. Change: floor **17**, with the arithmetic
`19 - 2 = 17` printed by the spell. 17 is one below a green run, so losing
another gate still goes red.

**4 -- the gates ran on the wrong body.** Cause: the headless half took the
default, body 2, the marsh, whose dye has no mouth, while the gates were
registered on body 3. Change: `BODY="${BODY:-3}"` exported as
`WW_WATER_MARK_BODY` and PRINTED in the banner, so it cannot drift again. Four
of that spell's eight reds had this one stated cause.

**5a -- a dye pin's per-point weight never written.** Cause, and it is bigger
than the red says: `WaterCurveDoc::writeTo` wrote `extra` for `Stroke` and `Pin`
only -- **and `parseStoreExtras` never READ a dye pin's weights either**, with a
`base` offset that does not allow for the four colour bytes a dye pin's record
carries between its points and its trailing bytes. Change: both halves, in
`src/watercurves.cpp`. `SourcePin` and `OutletPin` were deliberately NOT added:
they are single points whose weight the solver never reads, so four bytes each
would move the file for no behaviour. **That is a candidate, not a fix**; the
gate that would settle it is "write a source pin with weight 0.25, save, reopen,
read it back", and it has not been run. Number: the round trip is the gate --
0.25 in, 0.25 out, where without the fix the reader's default 1.0 comes back.

**5b -- the first named body reads back nameless.** Cause: `encodeTable` started
its running name offset at **0**, and 0 is what `LodtFile::bodyName`
(`src/lodtfile.cpp:3094-3098`) and the generator (`src/lodtfile.cpp:1151`,
`table.u32( 0 ); // name offset: unnamed`) both spell "no name". Change
(`src/watermark.cpp`): `encodeNames()` reserves byte 0, written lazily on the
first non-empty name, and `encodeTable`'s offset starts at 1. Number: **a
document with no names writes no blob and every offset stays 0**, so the
unedited-file byte identity the whole encoder rests on is unchanged. The fix is
in the marking tool's own encoder and not in `src/lodtfile.cpp`, which this lane
does not own -- and it did not need to be, because the generator never names a
body.

## 4. Gates run / skipped, and the mtimes

**NONE OF THE GATES WERE RUN.** The lane could not build:
`scratchpad/build11_20260910/DONE` does not exist. The check was made once, at
17:23, and nothing was polled.

| clock | reading |
|---|---|
| `scratchpad/build11_20260910/BUILDING` | "BUILD11 holds the build slot, started 2026-09-10T14:58:02Z" |
| BUILD11's last write | `gates_summary.txt` 17:19:40, `logs/` 17:19:32 |
| this lane's check | 17:23:28 |
| the process guard | `rc=1` -- the game is NOT the blocker, the slot is |
| `release/NifSkope.exe` | **17:08:39** (BUILD11's link); `release/style.qss` 17:08:39 |
| `src/watermark.cpp` | 17:09:36 -- **newer than the exe** |
| `src/watermarkpanel.cpp` | 17:12:09 -- newer than the exe |
| `src/wateruitest.cpp` | 17:19:55 -- newer than the exe, and in no Makefile yet |

So no harness may be run on this exe, and none was.

**What WAS run, and what it proves.** `sx_WATER7.sh`, with the flags read out of
`Makefile.Release` rather than typed: **RC=0** on `src/wateruitest.cpp`,
`src/watermarkpanel.cpp`, `src/watercurves.cpp`, `src/waterwindow.cpp` and
`src/watermark.cpp`, with only the known pre-existing `qchar.h` /
`-Wsfinae-incomplete` noise. The hook-up's own inserted TEXT was proved to
compile as well, without touching the shared tree:
`scratchpad/water7_20260910/sx_overlay.py` imports `hookup.py`'s EDITS table --
never a retyped copy -- writes patched copies under `sx/`, and a syntax pass over
the patched `nifskope_ui.cpp` with that directory first on the include path
returned **RC=0**. That run is self-proving about its own overlay:
`mode > LeftWater` compiles only against the PATCHED `src/nifskope.h`, so a run
that had quietly used the one still on disk would have failed. It proves the
files compile. It proves nothing about linking, about moc, or about behaviour.

`hookup.py --check`: **7 of 7 anchors match exactly once**; all four files
LF-only, CR 0. Nothing was applied, and no `BUILDING` marker was written by this
lane.

**PICTURES: OWED, and refused rather than faked.** BUILD11 was still writing its
gate logs four minutes before the check, so launching a NifSkope instance for a
before/after grab would have broken "one instance ever" in the middle of another
lane's runs. `water_ui.sh` takes both (`SHOT=` the whole top strip, `TABSHOT=`
the left dock with the Water tab open); the resume names which exe is still a
valid BEFORE and says not to substitute a different crop silently.

## 5. Mistakes

Appended to `MISTAKES.md` by this lane, append-only: 204,423 -> 208,304 bytes,
CR 0 -> 0, the original bytes verified unchanged.

1. **A header change that would have broken the tree for every other lane.**
   `wwAlignBarRow`'s declaration in `src/wwskin.h` (this lane's file) was given a
   second parameter with a default argument, while its definition lives in
   `src/nifskope_ui.cpp` (not this lane's) and could not be changed until the
   hook-up landed. Those are two OVERLOADS, so the single call site becomes
   ambiguous and `nifskope_ui.cpp` stops compiling -- for every live lane in the
   tree, with an error pointing at a call site none of them touched. Found by
   `ww-anchored-hookup` section 2, before anything ran. **Rule:** a header a lane
   owns may only GAIN declarations while its definitions live elsewhere;
   changing an existing signature is a hook-up edit, or it is a new function. The
   way back moved inside the skin instead, which is better anyway -- one reader
   of `UI/CompactTopBars`.
2. **Fixing the writer of a pair whose reader never read it.** Red 5a names
   `writeTo`; adding `DyePin` there alone would have emitted weights that
   `parseStoreExtras` still excludes, and whose offset does not allow for a dye
   pin's four colour bytes -- so the gate would still have been red with the
   writer blamed for the reader's defect. **Rule:** a round-trip red names one
   side because that is where somebody looked; read the other side in the same
   sitting, and check the codec's per-kind offset for the kind in hand.
3. **Marker counts typed instead of derived, again.** `hookup.py` listed
   `wwBarRowButtonQss` at 3 where the truth is 2. This is the mistake
   `nifskope-ww-resume-pending` section 9 already records for lane BUILD9 (a
   resume promising 1 and 7 where the truth was 1 and 6). `hookup.py` now DERIVES
   every marker expectation from its own EDITS table. **Rule:** a number in a
   resume that can be computed from the resume's own data is computed, never
   typed.

## 6. Finished-work skill review (CONSTITUTION 1a)

**Loaded and used, before any route was chosen.**

`nifskope-ww-panel-style` -- the tab is a page of the left editor, and the
existing panel's own house-style self-test moved with it unchanged, so its counts
and floors are the same counts on the same assertions. Its 2026-09-10 section on
bars that meet along one line is what work B extends, including "grow, never
shrink" -- which is exactly why the registered refuter says not to ship if the
menu bar turns out to be the tallest bar.

`ww-anchored-hookup` -- the whole shape of `hookup.py`: one EDITS table, exact
anchors carrying the file's own line ending, `--check` as the default that writes
nothing, the COUNT printed instead of the word "ok", markers as the applied-state
signal, and section 3a's warning about a backslash-bearing anchor -- the `.pro`
line is tab-indented and ends in a backslash, and the script prints its `repr`
beside the count for that reason. Its section 2 caught mistake 1.

`ww-test-harness-add` -- `src/wateruitest.cpp` is its own translation unit; the
1.5 s timer with the reason behind it; widgets read by object name; the
stack-object log trap; the undo-stack cleanup before quit; the `^done$` poll in
the spell; floors that can actually fire; and section 2b's rule about the gate
loop's own grep, which IS red 2, and which the new spell was written to from the
start.

`nifskope-ww-build-verify` -- "When you CANNOT build": the syntax pass with the
real flags out of `Makefile.Release`, `sx_$LANE.sh` and never a fixed name, and
`${PIPESTATUS[0]}` as the gate.

`nifskope-ww-resume-pending` -- the shape of `PENDING.md`, qmake-before-make, the
object-mtime read-back, and section 9's "a PENDING's numbers are PREDICTIONS",
which is why every figure here says so.

`nifskope-ww-render-shot` -- consulted for the pictures, then NOT used, on its
own rule: one instance at a time, and another lane was running harnesses.

**The skill this lane wishes had existed, and recommends to the director:**
`ww-owned-header-foreign-definition`. One page on the seam behind mistake 1 -- a
lane owns a header whose definitions live in a file it does not own. The rules
are short and are in no existing skill: declarations may be ADDED; signatures may
not be CHANGED (two overloads, an ambiguous call site, a tree that does not build
for anybody until the hook-up lands); a behaviour that needs a new argument
becomes a new function, or a reader inside the shared code; and a setting
honoured in two places gets ONE reader exposed from the header rather than a
parameter threaded through it. `ww-anchored-hookup` section 2 covers new FILES
compiling with and without the hook-up and says nothing about an owned header,
which is the commoner case in this tree and the more dangerous one, because it
breaks other people's lanes rather than your own. It will recur: `src/wwskin.h`,
`src/watermark.h` and `src/watercurves.h` are all owned by water lanes while
parts of their implementation sit in `src/nifskope_ui.cpp`.

**Declined:** a skill for "replace a metric with a better one". The procedure --
register the floor before coding, keep the retired instrument printing beside the
new one for one run, run the refuter first -- is CONSTITUTION rule 4 plus
`ww-control-calibration`, already written, and this lane added nothing to it.

## 7. HANDOFF text (the director splices)

**Lane WATER7 / UI2 ended BUILD PENDING; `hookup.py` is NOT applied and no gate
has been run.** `scratchpad/build11_20260910/DONE` did not exist and that lane
was still writing its gate logs (17:19:40 against a 17:23 check); the game was
DOWN, so the slot and not the game is the blocker. Resume:
`scratchpad/water7_20260910/PENDING.md`. Report:
`scratchpad/lane_water7_report.md`. Changelog text, for the director to splice
(the lane did not touch `WW_CHANGES.md`):
`scratchpad/water7_20260910/WW_CHANGES_ENTRY.md`. `MISTAKES.md` was appended to
by the lane itself -- three entries, append-only, 204,423 -> 208,304, CR 0 -> 0.

Written and syntax-checked (RC=0 on all five owned translation units, and RC=0 on
a patched overlay copy of `nifskope_ui.cpp`, so the hook-up's inserted text
compiles in place): the **Water tab** of the LOD Generation workspace -- no water
dock, no Workspaces entry for either half of the tool, the tab shown while
`LodGenerationDock` is visible, and the install refusing in words if its page
does not land at stack index 3; **one compact height for the whole top of the
window**, the MENU BAR and every button in the row included, stated once in the
skin as `wwBarRowButtonQss()` and appended by `wwAlignBarRow`, with
`UI/CompactTopBars` (default true) as a way back that is exact at its off value;
and **all five of BUILD10's reds** -- X2b is a net-flux ring with its floor
registered before the code, `water_flow.sh` reads verdict lines only and its
floor is 17, `water_mark.sh` pins body 3, a dye pin's weights are written AND
read (the reader was broken too), and the first named body no longer reads back
nameless. New gate: `tests/spells/water_ui.sh` / `src/wateruitest.cpp`.

**Owed to bungo, by name:** the before/after picture of the top strip, and the
picture of the Water tab. Both were REFUSED rather than faked -- BUILD11 was
running harnesses, and one NifSkope instance is the rule. `water_ui.sh` takes
both when the build lands.

**The one thing to check before it ships**, registered before the code was
written: `wwAlignBarRow` takes the TALLEST bar. If the log's R block shows the
row above 35 px, the menu bar was the tallest and every other bar has just been
made TALLER -- the opposite of "compact these vertically". Report the number and
set `UI/CompactTopBars` false rather than shipping it.

## Build (BUILD12)

Appended by lane BUILD12, 2026-09-10. Nothing above this line was changed.

**The exe.** `release/NifSkope.exe` **17:45:29**, 20,751,360 bytes. Built from
the applied hook-up on the 17:08:39 tree. Game check `rc=1` before the build and
before every exe launch; no NifSkope was running, so no exe was renamed aside.

**The hook-up.** `--check` 7 of 7 anchors exactly once; `--apply` wrote
`src/nifskope.h` 48,915 -> 49,371, `src/nifskope_ui.cpp` 1,488,803 -> 1,492,544,
`NifSkope.pro` 20,185 -> 20,208, CR 0 -> 0 on all three. Markers read back:
`LeftWater = 3` 2, `mode > LeftWater` 1, `wwBarRowButtonQss` **3**,
`UI/CompactTopBars` **3**, `wwWaterUiHarness` 2, `src/wateruitest.cpp` 1 --
each equal to the script's own derived expectation. **Two rows of PENDING.md's
marker table were wrong** (both tabulated as 2); the DERIVATION was right, and
section 5's own mistake entry states 2 as "the truth" where the truth is 3.
That is BUILD12's first MISTAKES entry.

**Build chain.** qmake before make (RC 0, `wateruitest` 7 times in
`Makefile.Release`); no `DEFINES`/`CXXFLAGS` change, so no object deleted for a
changed flag; `make` exit 0; exe newer than all **115** changed paths (0 stale);
0 stale objects among the **27** TUs including `src/wwskin.h` and the **34**
including `src/nifskope.h`; `res/style.qss` = `release/style.qss`.

**The 35-px check: ANSWERED, and the change ships.** `wwBarRowHeight() = 35`,
`UI/CompactTopBars = true`, 5 visible bars all at the row height, menu bar 35
against the tab strip's 35. 35 is BUILD9's number; the tab strip is still at
top 35, the viewport toolbar at top 35 and the search row at top 70, exactly
where the 17:08:39 exe had them. `tMode` and `tRender` moved from 33 px at
top 36 to 35 px at top 35 -- they joined the row. **The top did not grow, the
refuter did not fire, and `UI/CompactTopBars` stays true.**

R3, the button half, is the one measured miss: the buttons are **39 px inside
the 35 px row** and the check passes because it allows 8 px where the spell's
own header promises 1. Left as a verdict for the director; second MISTAKES
entry.

**Gates** (all on the 17:45:29 exe, sequential, one instance):
`water_ui.sh` **30 / 0 / 0 skips** PASS (floor 24) -- its first run ever;
`water_weights.sh` X-gates green **17** (floor 16) PASS;
`water_flow.sh` F-gates green **17** (floor 17), spell FAIL (3) = the
selftest's exit code plus F2 island bank and F5 p99, both pre-registered red;
`water_mark.sh` dock **20 / 0** (floor 16) with `WW_WATER_MARK_BODY=3` printed,
spell FAIL (2) = the same reds through the model half;
`water_window.sh` **46 / 0**; `lodl_water.sh` 33 ok / 0 FAIL;
`lodl_open.sh` **23 / 0**. Neighbours: `ui_align.sh` **11 / 0** (unchanged),
`top_bar.sh` **43 / 5** (baseline), `loaded_nifs.sh` **166 / 2** (baseline was
166 / 3), `files_tab.sh` **28 / 2** (baseline), `hkxanim_ui.sh` **48 / 1**
(baseline), `animws.sh` **57 / 0** with 1 skip -- the number that had to hold.

**The five reds:** 2, 3, 4 and 5 green by their own printed numbers; **X2b is
half** -- 0.595 on body 2 (green, > 0.5) and **0.407 on body 3 (red)**, both
controls under 0.2. The retired disc metric beside it reads 0.742 / 0.371, so
the new ring's body-to-body disagreement is 1.46x where the old one's was
2.00x. Candidates, not a cause: the floor was registered off body 2's number,
or the ring at two pin widths clips body 3's narrower channel; the
discriminator is the same ring at one and four pin widths on both bodies.

**Pictures, both OWED items discharged** (in
`scratchpad/build12_20260910/images/`): `seam_before.png` (the 17:08:39 exe,
taken with `ui_align.sh` because `water_ui.sh`'s harness does not exist in that
exe), `seam_after.png`, `topbar_after.png` and `watertab.png`.
