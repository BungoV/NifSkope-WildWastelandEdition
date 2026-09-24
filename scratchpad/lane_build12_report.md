# Lane BUILD12 -- WATER7 / UI2 built and gated

Written incrementally. Repo `E:\Projects\NifskopeWildWastelandEdition`, branch
`main`, nothing committed.

## 1. The slot, the game, and the BEFORE picture

- `scratchpad/build12_20260910/BUILDING` written at 17:40 before anything else.
- Game check before every build and every exe launch:
  `tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?` -> **rc=1** at 17:40,
  17:41 and 17:42. No Fallout4, no NifSkope, so no exe was renamed aside and
  no window of bungo's was touched.
- The exe on disk when this lane started: `release/NifSkope.exe`
  **17:08:39**, 20,693,504 B (BUILD11's). Copied to
  `release/NifSkope.before12.exe` (17:41:07) so the old behaviour can be
  re-photographed if a gate fails -- deleted only once the replacing gate is
  green (`nifskope-ww-resume-pending` s8).
- **`water_ui.sh` cannot take a BEFORE picture**: it drives `WW_WATERUI_TEST`,
  which does not exist in the 17:08:39 exe, so it would write no log and no
  PNG. The BEFORE grab was therefore taken with BUILD9's `ui_align.sh`, the
  spell that photographs the same strip:

```
SHOT=.../images/seam_before.png bash tests/spells/ui_align.sh   # on the 17:08:39 exe
11 checks, 0 failures  PASS
```

  `scratchpad/build12_20260910/images/seam_before.png` (707x82). Bars in
  main-window coordinates on the OLD exe:

| bar | top | height |
|---|---|---|
| toolbar tFile | 0 | 35 |
| toolbar tView | 0 | 35 |
| dock tab strip (Header/Blocks/Files) | 35 | 35 |
| viewport toolbar (header) | 35 | 35 |
| toolbar tMode | 36 | 33 |
| toolbar tRender | 36 | 33 |
| dock search row | 70 | 23 |
| dock `WaterMarkDock` | 35 | 741 (present) |

  The menu bar is not in that dump -- `ui_align.sh` never listed it, which is
  exactly the gap WATER7 closes.

## 2. The hook-up

`python scratchpad/water7_20260910/hookup.py` (check, writes nothing) ->
**every one of the 7 anchors matched exactly once**, CR 0 on all three files.
`--apply` wrote:

| file | bytes before | bytes after |
|---|---|---|
| `src/nifskope.h` | 48,915 | 49,371 |
| `src/nifskope_ui.cpp` | 1,488,803 | 1,492,544 |
| `NifSkope.pro` | 20,185 | 20,208 |

CR 0 before and after on all three (LF-only files, measured by Python byte
count). Markers read back after the apply, each equal to the count the script
DERIVES from its own EDITS table:

| file | marker | applied = | read back |
|---|---|---|---|
| `src/nifskope.h` | `LeftWater = 3` | 2 | 2 |
| `src/nifskope_ui.cpp` | `mode > LeftWater` | 1 | 1 |
| `src/nifskope_ui.cpp` | `wwBarRowButtonQss` | 3 | 3 |
| `src/nifskope_ui.cpp` | `UI/CompactTopBars` | 3 | 3 |
| `src/nifskope_ui.cpp` | `wwWaterUiHarness` | 2 | 2 |
| `NifSkope.pro` | `src/wateruitest.cpp` | 1 | 1 |

**Two of PENDING.md's six marker numbers were wrong** (`wwBarRowButtonQss` and
`UI/CompactTopBars` were tabulated as 2, the script derives 3). The script's
derived numbers are the ones that matched. This is the same class of defect
WATER7 already recorded for itself -- a number typed into a resume instead of
computed -- and it survived into the resume's own table.

## 3. qmake, the flags, and the build

`NifSkope.pro`'s only WATER7 edit is one line in `SOURCES`
(`src/wateruitest.cpp`), so **qmake ran before make**:

```
QMAKE-RC=0    (scratchpad/build12_20260910/qmake.log)
grep -c wateruitest Makefile.Release            -> 7
grep -c -- -DWW_HKXANIM_UI Makefile.Release     -> 1
grep -c -- -DWW_HKXCLIP_CANON Makefile.Release  -> 1
grep -c -- -DWW_ANIMWS_HKXMODEL Makefile.Release-> 1
```

**No `DEFINES` or `CXXFLAGS` line changed**, so BUILD9's changed-flag staleness
trap does not apply and no object was deleted for it. The three WW defines
above are earlier lanes' and were already in the 17:08:39 exe.

Headers this build had to propagate: `src/wwskin.h` (17:18:45, two new
declarations, **27 translation units include it**), `src/nifskope.h` (17:41:49,
the `LeftWater` enum), `src/watermarkpanel.h` (17:12:34).

Build through `tools/ww_build.sh` -- the process guard is inside the chain,
immediately before the link, not echoed at the top of the lane.

## 4. The build

```
tools/ww_build.sh
exe not held by a window
BUILD-RC=0
release/NifSkope.exe 17:45:29   20,751,360 bytes
exe newer than the sources
copies in step            (cmp res/style.qss release/style.qss)
```

`release/style.qss` 17:45:29, byte-equal to `res/style.qss` -- the link-time
copy is in step and no sheet was edited by hand (WATER7 changed no sheet).

**Exe-newer sweep over the WHOLE working set, not one file:** 115 changed
paths under `src res tools tests NifSkope.pro`, **0 stale**.

**Object staleness after the header changes:** `src/wwskin.h` is included by
**27** translation units and `src/nifskope.h` by **34**; every one of their
objects is newer than its header -- **0 stale** in both sets.
`GeneratedFiles/.obj/wateruitest.o` exists (17:43:13, 66,584 B), so the new
translation unit really entered the build.

## 5. THE 35-PX CHECK -- the one thing that had to be answered before shipping

WATER7 registered the refuter before it wrote the code: `wwAlignBarRow` takes
the TALLEST natural height among the bars it is given, so if the MENU BAR
turned out to be the tallest, every other bar would have been made taller --
the opposite of bungo's "compact these vertically like this, the top bar and
the buttons".

**The row reads 35 px. It did not grow.** From the R block of
`release/ww_waterui_test.log`, on the 17:45:29 exe:

```
UI/CompactTopBars = true
wwBarRowHeight() = 35
menu bar                    : x   3  top   3  w  250  h 35  bottom 37
tFile (workspaces row)      : x   0  top   0  w  378  h 35  bottom 34
tLOD                        : <absent or hidden>
tView (anim/collision)      : x 378  top   0  w 1134  h 35  bottom 34
viewport header             : x 386  top  35  w 1126  h 35  bottom 69
dock tab strip              : x   0  top  35  w  383  h 35  bottom 69
dock search row             : x   5  top  70  w  373  h 23  bottom 92
R1: 5 visible bars, 5 at the row height
```

35 is the same number BUILD9 measured, and the second, third and fourth rows
land at exactly the tops the OLD exe had (`seam_before.png`'s dump: tab strip
top 35, viewport toolbar top 35, search row top 70). So the menu bar joined the
row at the row's own height and **nothing above it moved by a pixel**.
`UI/CompactTopBars` therefore stays **true**, the shipped default, and no code
was changed to force it false.

**One measured thing that is NOT what the gate's own comment promises.** R3
reads the buttons at **39 px inside a 35-px row** -- 4 px TALLER than the bar
that holds them:

```
R3: 4 bar buttons, heights 39..39, row 35
ok (R3) every button in the row is within 8 px of the row height (4 of 4)
```

The spell's header says R3 asserts that "every button in those bars takes the
row's height and agrees with its neighbours within 1 px"; the check that
actually runs allows **8**, which is why 39 against 35 is green. The 1 px is
enforced only between the buttons (`39..39`). The arithmetic behind it is
`wwBarRowButtonQss`: `min-height: rowHeight - 4` (31) plus `padding
(rowHeight-18)/2` (8) top and bottom. Not fixed here -- a build lane's product
is a verdict (`nifskope-ww-resume-pending` s6) -- but it is a red for the
director to rule on, because "the buttons" is half of what bungo asked for and
the gate as written cannot fail on 4 px.

## 6. The pictures

All three are in `scratchpad/build12_20260910/images/`. Every one was opened
and read; the descriptions are of what is in the frame, not of a cause.

**`seam_before.png`** (707x82, the 17:08:39 exe, taken BEFORE the hook-up was
applied). Three tabs -- Header, Blocks, Files -- with Files the blue one, and
to their right the viewport toolbar starting with Object Mode, then Select,
Add, Object. Under the tabs is a "Search files..." field with four small
icon buttons at its right end.

**`topbar_after.png`** (1512x107, the 17:45:29 exe). The whole top of the
window in three bands: a first row carrying File / View / Spells / Options /
Help immediately followed on the SAME line by Workspaces, LOD 0, Animation and
Collision; a second row with the Header / Blocks / Files tabs on the left and
Object Mode / Select / Add / Object / Global / Overlays on the right; and the
"Search files..." row beneath the tabs.

**`watertab.png`** (278x741, the 17:45:29 exe). The left dock with FOUR tabs --
Header, Blocks, Files, Water -- Water selected and blue, and its page showing
Landscape file (File, Show), Marking (Tool, Speed, Width, Dye colour, Dye
fade), Selected body (Class, Water form, Colour, Flow, Dye, Name), a folding
Bake section with "Flow samples per cell 32", then a grey sentence "No
landscape file is open. Choose a version 3 .lodl to mark its water." above a
pinned row of four buttons: Water window, Reload, Solve, Save.

## 7. Gates -- every one run on the 17:45:29 exe, sequentially, one instance

| gate | numbers | verdict |
|---|---|---|
| `water_ui.sh` (first run ever) | **30 checks, 0 failures, 0 skips** (floor 24) | **PASS** |
| `water_weights.sh` | WATER6 X-gates green **17** (floor 16); its selftest 64 / 8 | **PASS** |
| `water_flow.sh` | flow F-gates green **17** (floor 17); its selftest 64 / 3 | FAIL (3) |
| `water_mark.sh` | dock **20 / 0** (floor 16), `WW_WATER_MARK_BODY=3` printed; model selftest 64 / 3, exit 1 | FAIL (2) |
| `water_window.sh` | **46 checks, 0 failures** (floor 24) | **PASS** |
| `lodl_water.sh` | no count line; **33 `ok` lines, 0 `FAIL`**, control PASS | **PASS** |
| `lodl_open.sh` | **23 checks, 0 failures** | **PASS** |
| `ui_align.sh` (neighbour) | **11 checks, 0 failures** | **PASS**, same as on the old exe |
| `top_bar.sh` (neighbour) | **43 / 5** | = BUILD9's recorded baseline, all five pre-existing |
| `loaded_nifs.sh` (neighbour) | **166 / 2** | baseline was 166 / **3** -- one fewer |
| `files_tab.sh` (neighbour) | **28 / 2** | = BUILD11's baseline, same two by name |
| `hkxanim_ui.sh` (neighbour) | **48 / 1** | = BUILD9/BUILD11's baseline (the wheel floor that cannot fire unfocused) |
| `animws.sh` (neighbour) | **57 checks, 0 failures, 1 skip** | **PASS** -- the required 57 / 0 held |

Not run, and why: everything the change does not reach -- the lodgen /
terrain / impostor / gltf / hkx-file suites, the collision and block suites,
`skeleton_overlay.sh` (SKELFIX's, flaky by BUILD11's own measurement and
untouched here). `animws.sh`'s 1 skip is BUILD11's gate (i), still undischarged.

**`water_flow.sh` and `water_mark.sh` cannot print PASS as they are written.**
Their three and two failures are, by name:

* `water_flow.sh` -- "the selftest exited 1", "F2 island bank is red",
  "F5 the 99th is red". The last two are WATER4's PRE-REGISTERED reds, and the
  first is the exit code those two produce. The spell subtracts them in its
  FLOOR arithmetic (17 = 19 - 2) but still counts them in its verdict loop, so
  the floor passes and the spell reports FAIL. Nothing regressed.
* `water_mark.sh` -- "the model self-test exited 1" and "the model self-test
  did not pass", the same two reds plus X2b below. Its own new work is green:
  the dock half is 20 / 0 against a floor of 16 and the log's first line reads
  `WW_WATER_MARK_BODY=3`, which is BUILD10's red 4 discharged.

## 8. BUILD10's five reds, measured

1. **X2b, the new net-flux ring: HALF green.** On body 2 (`water_weights.sh`)
   the pin adds **0.595** of net outward flux per texel through a ring at two
   pin widths over 1052 texels, against the floor of **> 0.5** registered
   before the code -- green -- and the no-pin control on the same ring reads
   **0.173**, under its **< 0.2** floor. On body 3 (`water_flow.sh`,
   `water_mark.sh`) the same instrument reads **0.407** against the same
   > 0.5 floor -- **RED** -- with its control at **0.155**, also under 0.2.
   So the replacement instrument still disagrees between the two bodies
   (0.595 / 0.407, a ratio of 1.46) though by much less than the retired disc
   metric it replaced, which is printed beside it and reads 0.742 on body 2
   against 0.371 on body 3 (a ratio of 2.00). **The floors both hold, so the
   ring is measuring the pin and not the channel; what is unsettled is whether
   0.5 is the right floor or whether body 3's pin really is a weaker source.**
   Candidates, not a cause: (a) the floor was registered off body 2's number
   and body 3 is legitimately weaker (25,114 wet texels against 29,312, and
   647 ring texels against 1052); (b) the ring at two pin widths clips body 3's
   narrower channel. The discriminator is the same ring at one and at four pin
   widths on both bodies -- one run, no code change. Not landed here
   (`nifskope-ww-resume-pending` s6).
2. **`water_flow.sh` reads verdict lines only: green.** No gate was called red
   by an informational line in this run; "F8 the solve" is `ok`.
3. **The floor is 17 and the arithmetic is printed:** `flow gates green: 17
   (floor 17 = 19 registered F-gates - 2 pre-registered red)`. Green.
4. **`water_mark.sh` pins body 3: green**, printed in the log's first line.
   **Note for the director:** `water_weights.sh` does NOT pin it and still runs
   on body 2, the marsh -- which is why its selftest shows 8 failures against
   body 3's 3. Four of the extra five are the dye gates
   ("the river's dye reaches the body it drains into", 0 texels on 0 receiving
   fields; the mouth weight; the 1/8 falloff; "every dyed texel names the
   river"), the exact family BUILD10's red 4 says has no mouth on body 2, and
   the fifth is F5's mean direction. Its own X-gates are green on body 2, so
   this is not a regression -- but the body pin was applied to two of the three
   spells, not three.
5. **What the tool saves: green, and gated by name in both halves.**
   (a) `ok F7 a dye pin's weight one half-distance downstream is 1/2 (0.5000)`
   in both `water_flow.log` and `water_mark.log` -- the weight is written AND
   read. (b) `ok the override came back out of the table (name 'harness river')`
   in `water_window.log` -- the first named body reads its name back. And the
   property the byte-0 reservation had to preserve is itself gated:
   `ok P8 round trip: save, reopen, save is byte-identical (39,235,147 vs
   39,235,147 bytes)`. `water_window.sh` 46 / 0, `lodl_water.sh` 33 ok / 0 FAIL
   and `lodl_open.sh` 23 / 0 (on the user's own installed `Commonwealth.lodl`)
   all pass on the 17:45:29 exe.

## 9. Neighbours against their recorded baselines

| harness | BUILD9/BUILD11 baseline | now | reading |
|---|---|---|---|
| `ui_align.sh` | 11 / 0 | 11 / 0 | unchanged; it also took the BEFORE picture |
| `top_bar.sh` | 43 / 5 | 43 / 5 | the same five ("Panels lists the Block List / Block Details / Header / NIF Browser dock" and "the panel toggles it absorbed") |
| `files_tab.sh` | 28 / 2 | 28 / 2 | the same two by name |
| `hkxanim_ui.sh` | 48 / 1 | 48 / 1 | the wheel floor that cannot fire unfocused |
| `animws.sh` | 57 / 0 | 57 / 0 (1 skip) | held; the skip is BUILD11's undischarged gate (i) |
| `loaded_nifs.sh` | 166 / **3** | 166 / **2** | one of the three is now green |

The two that remain in `loaded_nifs.sh` are "the top selector orders Header,
Blocks and NIFs without remapping modes" and "NIF Browser is above Loaded NIFs
in its own mode". **Which of the three turned green cannot be said**, because
BUILD9 kept no `loaded_nifs` log -- only the count. A candidate, named as a
candidate: "each reordered selector button opens its named stable mode" is `ok`
now and is the check nearest to the code WATER7 changed (`setLeftColumnMode`'s
clamp and the mode numbering that the fourth tab required). The discriminator
is one run of `loaded_nifs.sh` against `release/NifSkope.before12.exe` -- the
copy of the 17:08:39 exe this lane took before applying the hook-up. **It is
KEPT, not deleted**, precisely so that run is still possible; it is a plain
copy beside the live exe, not a rung of the rollback ladder
(`release/NifSkope_inuse_20560.exe` is that), and it can be deleted the moment
the director does not want the comparison.

## 10. Documents

| document | what changed | assertion |
|---|---|---|
| `WW_CHANGES.md` | WATER7's entry rewritten IN PLACE by `scratchpad/build12_20260910/splice_wwchanges.py` -- "NOT BUILT" becomes the built block, the row-height PREDICTION becomes the measurement, the OWED pictures become the delivered four, plus the gate table and the five reds | 1,574,208 -> 1,578,992 bytes, **CR 19,020 unmoved** (asserted before and after), three anchors each matching exactly once |
| `MISTAKES.md` | two BUILD12 entries appended | 212,575 -> 216,188 bytes, CR 0 -> 0, append-only (the original bytes are a prefix, asserted) |
| `scratchpad/lane_water7_report.md` | `## Build (BUILD12)` appended; nothing of the lane's own text touched | 26,447 -> 30,210 bytes, CR 0 -> 0, append-only |
| `.claude/skills/nifskope-ww-resume-pending/SKILL.md` | new section 11 | 10,999 -> 12,810 bytes, CR 0, append-only |
| `scratchpad/build12_20260910/HANDOFF_BLOCK.md` | written | -- |

Nothing was committed (CONSTITUTION 8; the tree still carries the ~225
uncommitted paths).

## 11. Mistakes

Both are in `MISTAKES.md` in full.

1. **A resume's "corrected" marker count was corrected in the wrong direction.**
   WATER7 moved the marker expectations into a derivation -- right -- and then
   wrote a second TYPED number (2) into `PENDING.md`'s table and into its own
   MISTAKES entry, where the derived and actual value is 3. The rule: when a
   number moves into a derivation, delete the typed copy rather than correcting
   it, or show the command that prints it.
2. **A gate whose comment promises 1 px and whose assertion allows 8.**
   `water_ui.sh` R3, the "and the buttons" half of bungo's ruling, passes with
   the buttons 4 px taller than their row. The rule: a gate's header comment and
   its assertion are read against each other in the same sitting, and a check
   that quotes a tolerance prints the measured value beside it.

Nothing else went wrong. One near-miss, caught before it ran and written into
the skill rather than here: the chain helper in `nifskope-ww-resume-pending`
section 5 is a shell FUNCTION, and `SHOT=... run water_ui ...` would have left
`SHOT` set for `ui_align.sh` further down the chain and overwritten the picture
that was the point of the run.

## 12. Finished-work skill review (CONSTITUTION 1a)

**Loaded and used:** `nifskope-ww-resume-pending` (the read order, qmake before
make, the dependency and object read-backs, the exe-newer sweep over the whole
working set rather than one file, the sequential chain with its `run` helper,
"a PENDING's numbers are PREDICTIONS" -- which is what caught the two wrong
marker rows -- section 6's "measure the cause and STOP", section 8's rule that
the before-picture copy of the exe is kept until the replacing gate passes, and
section 10's neighbour baselines); `nifskope-ww-build-verify` (the gated chain
through `tools/ww_build.sh` with the process guard immediately before the link,
make's own exit code as the gate, the style.qss copy check, the changed-FLAG
staleness trap -- checked and found not to apply, and the changed-HEADER one,
which did); `ww-anchored-hookup` (why `hookup.py --check` prints counts and
markers instead of "ok", and why the applied state is read from the markers).

**Consulted, then not used:** `nifskope-ww-render-shot` -- the deliverable was
in-app dock and strip grabs through the harnesses' own `SHOT=` hook, not a
render of geometry, and that skill's first section says so. `ww-test-harness-add`
-- read for section 9 (a first-ever gate run finds defects in the GATE), which
is exactly what R3 turned out to be; `nifskope-ww-panel-style` -- not loaded,
because no panel code was written by this lane, only built.

**Written:** section 11 of `nifskope-ww-resume-pending` in the REPO tree
(`E:\Projects\NifskopeWildWastelandEdition\.claude\skills`), covering the two
procedures this lane had to work out for itself: taking the BEFORE picture with
a sibling spell because the pending lane's own harness does not exist in the
old exe, and the `VAR=x function` leak in this skill's own chain helper. **The
director mirrors it to `E:\Projects\Claude\.claude\skills`** (CONSTITUTION 1a,
the two trees drift).

**Declined:** a skill for "rewrite a WW_CHANGES entry from NOT BUILT to built".
The procedure is `nifskope-ww-resume-pending` section 7 plus the CR-count rule
in CONSTITUTION 8, both already written, and this lane added nothing to either.
