- NATIVE1b block, spliced 2026-09-11 (lane text verbatim):

**NATIVE1b LANDED AND IS GATED, EXE FREE.** `release/NifSkope.exe`
**2026-09-11 10:14:23**, **21,101,056 bytes**, sha256 `ab97d12c511aa598...`
(NATIVE1a's was 08:49:08, 20,989,440). ONE build (09:56:11) plus **TWO counted
relinks** (10:04:40 and 10:14:23, both for defects the gate found). Markers:
`scratchpad/native1b_20260911/DONE` in, `BUILDING` gone. Rung
`release/NifSkope.before_native1b.exe` = the 08:49:08 bytes exactly, md5
`cf7fc96177afec212bb63a90c08c88c4`, written once. Report
`scratchpad/lane_native1b_report.md`; entry text
`scratchpad/native1b_20260911/WW_CHANGES_ENTRY.md`; **four MISTAKES entries NOT
appended by the lane** -- `scratchpad/native1b_20260911/MISTAKES_ENTRIES.md`.
**Nothing committed.**

## What bungo gets

**His items 1 and 2 of 08:0x and "2 sounds good" of the second round are in the
bytes.** `.lodo`/`.lodi` are **v3** and both readers refuse v2 BY NAME.

* **The ladder**, from FULL detail down. Level 0 is byte-for-byte what v2
  emitted; above it each material's clusters are grouped four at a time,
  simplified with the group's border LOCKED so nothing can crack, and re-split
  under the same caps. A new 48-byte row per cluster carries the bounding
  sphere, the normal cone, the error against full detail, the parent link and
  the level. Nine-chunk Sanctuary: **levels 0..7, 20,678 clusters (was 10,634),
  1,900 of 2,982 meshes laddered**, 4,715 roots covering 142,138 full-detail
  triangles exactly. `.lodo` **5,692,388 -> 9,657,316 B**;
  `--native-no-ladder` writes 6,204,388 B and is the exact way back.
* **Occluders**: 280 boxes over 87 of 147 populated cells on a downtown region,
  each fitted inside a watertight mesh, shaved by a voxel and probed at 100
  interior points before it is written.
* **The selection law** stated and implemented as a reference selector: nine
  cases (0.5/1/4 px x 2,000/8,000/32,000 u), every one a PARTITION, triangles
  124,205 -> 104,090.

Pictures, `scratchpad/native1b_20260911/images/`: **`ladder.png`** (a maple at
57 -> 28 -> 10 -> 3 triangles as the error goes 0 -> 236 -> 528 -> 804 units) and
**`occluders.png`** (280 box footprints over 33,123 downtown placements).

## THE ONE THING FOR BUNGO TO DECIDE

**The ladder is correct and barely selectable.** The median level-1 cluster
deviates by **3.80 percent of its model's own diagonal**, which reaches one
screen pixel only past **52,100 units** -- so at his default 1-px tolerance the
first step is not selected anywhere in the Commonwealth, and the measured cut
saves between 0 and 16 percent of triangles on this region. **The cause is that
"full detail" in this file is already Bethesda's LOD mesh**, a mean of 47.7
triangles for a whole building. Building the library from each base's near
`MODL` instead of its `MNAM` slots would give the ladder real room and would
serve his 10:3x ruling properly; it needs **no format change at all**, but it is
a much bigger library and a bake-time cost, so it is his call and not a lane's.

## Gates (all on the 10:14:23 exe, sequential, one instance)

| gate | numbers | baseline |
|---|---|---|
| `lodgen_native.sh` (now **13 legs**) | **18 checks, 0 failures, PASS** | 13 / 0 |
| -- the decoder on the fixture | **69 / 0** | 56 / 0 |
| -- the refusal set | **44 / 0**, each refused BY NAME | 24 / 0 |
| -- the stock path with `--native` off | **25 files, 0 differ** | 25 / 0 |
| -- the decoder on the real pair | **87 / 0** | 87 / 0 |
| -- the field gate | **37 / 0, 1 named skip** | 25 / 0 |
| -- NEW, the geometry gate (spheres, cones, the cut, the boxes) | fixture **17 / 0**; real pair **15 / 0, 1 skip** | newly registered |
| -- NEW, the two ways back | one level, flag clear, 0 boxes | newly registered |
| -- NEW, the occluder region | **280 boxes, 280/280 inside, 280/280 leak when grown** | newly registered |
| `lodgen_native_baseline.sh --check` | **25 in, 25 baked, 0 differ, PASS** | 25 / 0 |
| `lodgen_terrain.sh` | **26 / 0, PASS** | 26 / 0 |
| `lodgen_identity.sh` / `lodgen_merge.sh` / `lodl_write.sh` | **PASS** | PASS |
| `lodl_open.sh` | **23 / 0** | 23 / 0 |
| `ui_align.sh` / `animws.sh` | **11 / 0** and **72 / 0, 1 skip** | same |
| `water_ui.sh` | **82 / 0** | 86 / 0 **with** its four picture arguments; 82 is the same suite without them (NATIVE1a's own note) |

Four counts moved and each is named: the suite 13 -> 18 (four new legs), the
fixture decoder 56 -> 69 (13 v3 expect keys), the mutation set 24 -> 44 (20 v3
mutations), the field gate 25 -> 37 (the ladder's 11 checks and the boxes' 4,
skipped where there is no box). Consistency: no source under `src` or
`NifSkope.pro` newer than the exe; 0 stale objects over the three changed
headers; `res/style.qss` and `release/style.qss` in step.

## Two real defects the gate found, both fixed, one relink each

1. **The sphere and the cone described the geometry that walked IN, not the
   quantised geometry written OUT** -- worst sphere overshoot **0.058 u**, worst
   cone cosine deficit **0.002033**. Both **0.000000** now; the contract states
   it as a format rule.
2. **A simplified group could open a hole.** Two meshes went from a WATERTIGHT
   level 0 to four and eight boundary edges, which is exactly what his
   far-shadow ruling forbids. A per-STEP refusal now catches it in the writer:
   **57 groups refused** on this region.

And three of my own instruments were wrong and were fixed as instruments: the
spell's `pwd -W || pwd` line (two-line paths, nine false failures), a cone floor
that could not fire on a zero-width cone, and a box floor that could not fire on
a box with room to grow.

## FOR BUNGO -- the calls, new and carried

1. **NEW: build the library from the near `MODL`?** (above). His call.
2. **NEW: the ladder simplifies on a POSITION WELD**, so levels 1 and up carry
   the first contributor's UVs where two vertices shared a quantised position --
   **117,722 welds merged differing UVs** over 263,876 source vertices. Level 0
   never uses the weld. The alternative (weld by position AND UV, far fewer
   laddered meshes) is one bake to measure.
3. **CARRIED, unchanged**: the mesh/material sort inside the CELL, and the
   placed REFR form id in the COLD record. v3 did not change either.

## Owed / red

* **The aggregate ring-3 impostors are NOT this lane** -- CARDS-AGG.
* The (-32,0) dim-32 asymmetric drop proof is **still not run**.
* The LOD panel still has **no `.lodo`/`.lodi` row** and prints no stage times --
  lane LODUI1's, and the four stage times are still not delivered.
* Nothing in FO4CS reads either file; there is still no reconstruction path, so
  the pictures are the decoder's own geometry.
* `WW_CHANGES.md`, `HANDOFF.md`, `MISTAKES.md` NOT edited by the lane.

## Restart

**YES** -- any window bungo has open predates 10:14:23. No window held the exe at
any of the three links, so nothing of his was renamed aside and nothing killed.
Game down and zero NifSkope processes at every check and at the end.

## State

Nothing committed (CONSTITUTION 8). Changed by this lane: `src/lodofile.h`,
`src/lodofile.cpp`, `src/lodifile.h`, `src/lodifile.cpp`, `src/nativeemit.h`,
`src/nativeemit.cpp`, `src/nifcli.cpp`, `NifSkope.pro` (the last two through the
refusing script `scratchpad/native1b_20260911/hookup.py`, 7 anchors each exactly
once, CR 0 -> 0), `docs/LODGEN_NATIVE_LODO_LODI.md` (rewritten to v3, 96 anchor
rows, idempotent), `tests/spells/lodgen_native_decode.py`, `_mutate.py`,
`_fields.py`, `lodgen_native.sh`, and the NEW
`tests/spells/lodgen_native_cut.py`. **Three skill files in the REPO tree were
amended and need mirroring to `E:\Projects\Claude\.claude\skills`:**
`ww-spec-gate-audit` (7,345 -> 10,041 B), `ww-control-calibration`
(4,515 -> 6,279 B), `nifskope-ww-lodgen` (27,775 -> 30,210 B), all CR 0.
