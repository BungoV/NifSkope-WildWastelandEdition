<!-- Lane WATER8 + WATER8-GATE, 2026-09-10/11. TEXT ONLY, for the director to
     splice at the TOP of WW_CHANGES.md (CONSTITUTION 8). The file is MIXED;
     the 2026-09 entries at the top are LF-only and this text is LF-only.
     Splice by Python and assert the CR count does not move. -->

### The Water tool moves into the LOD Generation panel, on the right (lanes WATER8, WATER8-GATE)

bungo, on seeing lane WATER7's Water tab appear in the LEFT strip: **"What? I
wanted it in that right panel though"** -- and, earlier, *"They should be in the
LOD gen workspace"*, then over a screenshot of `Header | Blocks | Files`,
*"You'd access them like this"*. Read together: **the segmented strip was the
STYLE and the LOD Generation panel is the PLACE.** WATER7 read it as the place;
this undoes that.

The LOD Generation dock (`LodGenerationDock`, right dock area,
`src/lodgenmanager.cpp:2381`) now opens with a two-segment strip under its
title: **LOD** (the generator exactly as it was, `LodgenPanel`) and **Water**
(the whole marking tool, `WaterMarkPanel`), over a `LodPanelStack` of two
pages. The LEFT strip is back to three tabs and three modes, the standalone
Water Marking dock is gone, and so are both Workspaces-menu entries ("Water
Marking", "Water window"). The two strips carry the **same stylesheet byte for
byte** and sit in the same shared 35 px bar row, so one sheet change moves both.

**Built:** `release/NifSkope.exe` **2026-09-10 21:02:12, 20,830,208 bytes**
(`QMAKE-RC=0 BUILD-RC=0 CHAIN-RC=0`), `release/style.qss` 21:02:12 and
byte-identical to `res/style.qss`. The build is consistent as well as
successful: `src/nifskope.h` changed 20:59:18 and **35 of 35** objects that
include it are newer (20:59:57 .. 21:02:08); **116 of 116** changed files under
`src res tools tests` are older than the exe. New translation unit
`src/wateruitest_lod.cpp` is in `NifSkope.pro:357` and appears 7 times in the
regenerated `Makefile.Release`.

**Gates**, all on the 21:02:12 exe, one sequential chain, one instance at a
time (`scratchpad/water8_20260910/gates.sh`, logs under `logs/`):

| gate | this run | baseline (UI4/BUILD12, 2026-09-10) |
|---|---|---|
| `water_ui.sh` | **59 checks, 0 failures, 0 skips, PASS** (floor 48) | 48 / 0, floor 41 |
| `ui_align.sh` | 11 / 0, PASS | 11 / 0 |
| `top_bar.sh` | 43 / **5** | 43 / 5 -- the same five |
| `files_tab.sh` | 28 / **2** | 28 / 2 -- the same two |
| `animws.sh` | 57 / 0, 1 skip, PASS | 57 / 0, 1 skip |
| `water_mark.sh` | dock 20 / 0 PASS; model self-test FAIL (X2b) | dock 20 / 0, body 3, same red |
| `water_window.sh` | 46 / 0, PASS | 46 / 0 |
| `lodl_water.sh` | 33 ok, 0 FAIL, RESULT PASS | 33 / 0 |
| `loaded_nifs.sh` | **166 / 0, PASS** | 166 / **2** |

The `water_ui.sh` count moved because group T (15 checks, the Water tab in the
left strip) is RETIRED and group L (29 checks, the LOD panel's strip) replaces
it: `48 - 15 + 29 = 62`, less the three S-group picture checks this chain did
not ask for, = **59**, counted out of the log (R 19 + S 10 + L 29 + 1).

**Group L, each with a floor asked in the same run.** L1 two tabs, "LOD" at 0
then "Water" at 1 (floor: the same search finds no "Watre"). L2 the stack is
generator-page 0 and water-page 1 and nothing else, each tab's data IS its
index (floor: the same predicate at the wrong index is false). L3 selecting
Water puts the tool box, Solve, Save and the full-screen flow window's button
on screen (floor: selecting LOD puts Generate back). L4 the strip is 35 px =
`wwBarRowHeight()` = the left strip's 35. L5 the two stylesheets are identical
byte for byte (floor: the compact default, 504 chars, differs from the row
sheet, 580). L6 the left strip is 3 tabs / 3 pages with the LOD workspace OPEN
**and** CLOSED, both in one run. L7 zero "Water Marking", zero "Water window",
no `WaterMarkDock` (floor: the same scans still find "LOD Generation" and its
dock). L8 the painted first segment is 4 px clear of the row top and bottom,
27 px tall in a 35 px row (floor: the left strip reads the same two numbers).

**`loaded_nifs.sh` went 166 / 2 -> 166 / 0, and this time the two are NAMED.**
BUILD12 recorded only a count and its handoff says nobody could say which check
turned green. Run as a control against the kept rung
`release/NifSkope.before_ui4.exe` (18:25:20, which still carried WATER7's fourth
left tab) the two reds are `the top selector orders Header, Blocks and NIFs
without remapping modes` and `NIF Browser is above Loaded NIFs in its own mode`
-- both green on 21:02:12. Removing the fourth tab cured them.

**Still red, unchanged, and none of it this work:** `water_mark.sh` X2b reads
**0.407** of net outward flux per texel on body 3 against a gate of > 0.5 --
BUILD12's number to three digits, a known instrument problem (0.595 on body 2),
with its own refuter X2a green. `top_bar.sh`'s five are a View menu that lists
docks merged into "Left Editor" long ago; `files_tab.sh`'s two are BUILD9's.

**One gate is narrower than it was registered, and it matters for the next
lane.** L8 was registered as "4 px clear of the row top and bottom **and of one
another**"; as shipped (`src/wateruitest_lod.cpp:485-517`) it measures only the
row's top and bottom. **Nothing anywhere measures the gap between the LOD
strip's two segments.** The inter-segment gap is measured only on the LEFT
strip, by UI4's `(S5) every pair of segments is 4..4 px apart`, and that 4 is
the number bungo's *"Why are they separated?"* sends to 0. Lane UI6 has to add
L8's missing half or the right-hand strip will keep whatever gap it has,
silently.

**Pictures** (in-application grabs, never a desktop capture), in
`scratchpad/water8_20260910/images/`: `lodtab_lod.png` (499x741, the panel with
LOD selected -- Source / Plugins / Resources / Worldspace / Output mod, the
192x192-cell progress square, Generate and Cancel), `lodtab_water.png` (499x741,
same crop, Water selected -- Landscape file, Marking with Tool / Speed 0.250 /
Width 4096 / Dye colour / Dye fade 8192, Selected body, Water form, a folded
Bake with 32 samples a cell, the refusal line "No landscape file is open. Choose
a version 3 .lodl to mark its water.", and Water window / Reload / Solve / Save),
and `toprow_after.png` (602x82, the left strip back to Header | Blocks | Files).

**What was NOT measured:** every suite the change does not reach (lodgen,
terrain, impostor, gltf, hkx*, collision, block, water solve / flow / weights),
`skeleton_overlay.sh` (flaky by BUILD11's own four-run measurement), and
`lodl_open.sh`, whose fixture is bungo's own installed `Commonwealth.lodl` and
which nothing here rewrites.

Nothing is committed. `res/style.qss` was not touched, no gate was edited, and
no build was run by the gating lane.
