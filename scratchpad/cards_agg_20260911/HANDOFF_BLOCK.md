- CARDS-AGG block, written 16:42 (lane text verbatim):

**CARDS-AGG LANDED AND IS GATED, EXE FREE.** `release/NifSkope.exe`
**2026-09-11 16:20:02**, **21,419,520 bytes**, md5 `3ebf175826feee6c6545cf0873635acc`
(BAKEPERF1's was 14:48:52, 21,261,312). **ONE build (15:49:39) plus ONE counted
relink (16:20:02, for a defect the picture gate's own ceiling found).** `qmake`
was re-run: two NEW sources and a new header that three existing files include.
Markers: `scratchpad/cards_agg_20260911/DONE` in, `BUILDING` gone. Rung
`release/NifSkope.before_cards_agg.exe` = the launch bytes exactly, md5
`b9b8f55472514b1e1c2bb7a5de780737`, written once. Report
`scratchpad/lane_cards_agg_report.md`; entry text
`scratchpad/cards_agg_20260911/WW_CHANGES_ENTRY.md`; **five MISTAKES entries
NOT appended by the lane** -- `.../MISTAKES_ENTRIES.md`. **Nothing committed.**

## What bungo gets

**His "1 sounds good" is in the bytes.** Every FORESTED cell now gets one
impostor card set, composited from that cell's own trees' cards at the rotation
and mirror the repetition breaker gives them, from **8 horizon azimuths**. The
`.lodi` is **version 4** and carries one aggregate row a cell plus the list of
instances each one stands for.

**THE CENSUS HE ASKED FOR, printed before any code** (report section 1, from
`Fallout4.esm` alone): the Commonwealth holds **64,662 tree placements with a
LOD base over 3,685 cells**; **2,631 cells are forested at 8 trees or more and
hold 60,605 of them (93.7 percent)**; the typical forested cell is a dozen to
three dozen trees (p50 14, **max 92 -- no cell reaches 128**). A further
**99,274** tree placements have no LOD model at all and cannot be aggregated.
9-chunk Sanctuary: 97 forested cells, 3,423 trees.

Pictures, `scratchpad/cards_agg_20260911/images/`:
**`agg_cell_sheet.png`** (the whole 512x48 sheet of one 65-tree cell, all eight
azimuths, coverage and height, texel level),
**`cmp_individual_vs_aggregate.png`** (the calibrated pair: the same 65 trees
composited finely, the ceiling, the shipped card, a WRONG cell, and the
difference), **`agg_region_map.png`** (the region from above, read back from the
`.lodi`, every cell's own coveredCount).

## Gates (all on the 16:20:02 exe, sequential, one instance)

| gate | numbers | baseline |
|---|---|---|
| count identity, three instruments a cell | **10 / 0**; 97 rows, 3,414 == 3,414 == 3,414; FLOOR 135 cells under the threshold, none aggregated | newly registered |
| the calibrated picture gate | **8 / 0**; mass error subject **0.0525** (worst 0.1160), ceiling 0.0138, floor 0.3142, over 128 cell-views | newly registered |
| the height channel | span 124..156 of 255, sd **8.89**, correlation with cluster size **0.773**, flat-plane floor 0 | newly registered |
| `--aggregate` off vs the rung | **27 files, 0 differ**; comparator RED first on a flipped byte and a missing file | newly registered |
| standalone v4 layout | **18 / 0** + the fixture's **21 / 0**; 13 named refusals | newly registered |
| `lodgen_native.sh` | **18 / 0 PASS** | 18 / 0 |
| `lod_generation.sh` | **116 / 0 PASS** | 116 / 0 |
| `lodgen_panel_run.sh` | **125 / 0 PASS** | 125 / 0 |
| `lodgen_terrain.sh` / `lodgen_stage_times.sh` / `lodgen_roads.sh` | 26 / 0, 16 / 0, 11 / 0 | same |
| `ui_align.sh` / `water_ui.sh` | 11 / 0, 82 / 0 | same |
| `lodgen_terrain_vt.sh` | **41 / 1** (V9b) | 41 / 1, the carried red |
| `lodgen_octahedral.sh` | **FAIL, 1 check (F1, worst 1.88)** | **pre-existing: the same check, the same 1.88, on the RUNG exe.** Control log kept |

**Not one count moved.** Consistency: no changed file under `src res tools
tests` or `NifSkope.pro` is newer than the exe; `res/style.qss` and
`release/style.qss` byte-identical.

## The defect the gate found

The composite normalised each tree's layer by the NUMBER of samples that landed
in a target texel instead of by the share of the texel's AREA, so a tree
covering a tenth of a coarse texel composited as if it covered all of it: the
aggregate carried **94 percent more coverage mass** than the same cluster
composited finely, against a ceiling of 1.4 percent. Only the CEILING arm could
have found it -- the subject looked plausible and the floor was comfortably
worse. Fixed, one relink, now 5.25 percent against that same ceiling.

## FOR BUNGO -- one call, and two deviations

1. **THE CALL: what the aggregate may cost.** A per-tree card is paid once per
   TREE TYPE (all 36 Commonwealth types = about **28 MB**, measured on disk). An
   aggregate is paid once per CELL: **135.6 MB** for the worldspace at the
   shipped tile 64 / threshold 8, removing **57,974 quads** from the far band;
   **571.9 MB** at tile 128; **29.5 MB** at tile 64 / threshold 32 for 23,962
   quads. Both are switches (`--aggregate-tile`, `--aggregate-min`) and neither
   needs a rebuild.
2. **DEVIATION: the photograph is an orthographic COMPOSITE, not a render.** The
   render route is 21,048 photographs and over seven hours of sleep for the
   Commonwealth; the sheets have been orthographic and metric since 2026-09-10,
   so compositing them is an exact resample. Contract `LODGEN_CARD_SHEETS` 10.4.
3. **DEVIATION: the `.lodi` version word is CONDITIONAL** -- 3 without
   aggregates, 4 with -- because that is what makes the module's off value
   byte-identical (Deviation 12). Every earlier bump was unconditional. **One
   line to make it always move, and every v3 baseline is re-pinned.**

## Owed / red

* `lodgen_octahedral.sh` F1 is red on the rung too -- a lane of its own.
* A **relative `--impostors` path silently finds no card set** (the CLI does not
  resolve it; the refusal at least names the directory). One line, not taken.
* **No panel row**: `--aggregate` is CLI-only.
* The card library on disk holds **20 of the worldspace's 36 tree types**; a
  full Commonwealth bake wants a full card library first.
* **Nothing in FO4CS reads any of it** -- the rows, the covered blob, the
  threshold and the sheets are all owed to the runtime, and so is the dithered
  cross-fade.
* The **view-basis handedness is argued, not proved**: the one-tree control (a
  single-tree cell's aggregate must reproduce that tree's own card frame) was
  not run.

## Restart

**YES** -- any window bungo has open predates **16:20:02**. No window held the
exe at either link, so nothing of his was renamed aside and nothing killed. Game
down and zero NifSkope processes at every check and at the end.

## State

Nothing committed (CONSTITUTION 8). Changed: `src/lodifile.{h,cpp}`,
`src/nativeemit.{h,cpp}`, `src/lodgen.{h,cpp}`, `src/nifcli.cpp`,
`NifSkope.pro` (CR 0 throughout), the NEW `src/lodgenaggregate.{h,cpp}`, and the
three contract pages `docs/LODGEN_LODM_FORMAT.md` (3a),
`docs/LODGEN_CARD_SHEETS.md` (10), `docs/LODGEN_NATIVE_LODO_LODI.md` (4.6 +
Deviations 12, 13). **Two NEW skills in the REPO tree for the director to mirror:**
`ww-downsample-gate`, `ww-module-off-is-identical`.
