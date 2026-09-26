# Lane FLAT1 (2026-09-26) -- flat ground objects painted into the LOD terrain colour

Worktree E:\Projects\NifskopeWWE-flat1, branch flat1-20260926, from d4c64ef7 (ROADS1 head). Nothing installed,
nothing merged, nothing pushed. Started 16:02 (`date`).

## 1. Skills loaded
- nifskope-ww-worktree-build (the first build of this worktree: rung = release/NifSkope.before_flat1.exe, sha1 05c3510b27b7...)
- nifskope-ww-lodgen (CLI, the two colour writers, editing traps)
- ww-road-stamp-faithfulness (the independent raster, the faithfulness gate, confinement with the 4x4 allowance)
- Read first: CONSTITUTION.md, HANDOFF.md top block, ROADS1 DONE.md and its work scripts (reused from
  scratchpad/roads1_20260926/work in this worktree: roadgeo.py, nlc.py, pave_faith.py, faith_cmp.py, confine.py).


## 2. Census (read-only, Boston box -8 -12 3 -1, before any code)
Scripts in work/: flatgeo.py (mesh + placement + LAND reader), roadz.py (the ROADS1 stamped road surface per
16-unit texel, 5,412 road placements, 2,078,696 texels), flat_census.py (every STAT placement, SCOL parts
expanded), flat_rule.py (the rule and every number below; full print in work/out_rule.log, not tracked).

Measured per placement, from the full mesh with the REFR scale and rotation applied, binned into the 16-unit
squares it covers ("ground" = LAND, raised to the stamped road surface where a road lies above LAND --
4,108 of the painted placements touch a road that is above LAND, so LAND alone calls a curb-height slab on a
road "floating"):
- top = 90th percentile over its squares of (highest point in the square - ground)
- underside = median over its squares of (lowest point in the square - ground)
- side/top = area of steep faces (|nz| < 0.7) that rise more than 8 units above the ground / area of up faces
  not buried more than 16. A thing that stands up has side; a flat thing, however big, has almost none.

**Why these thresholds (histograms):**
- top, among underside <= 16 (placements, of which pass the side test): <0 1030 (992), 0-8 3078 (3009),
  8-16 2625 (2077), 16-24 1933 (1002), 24-32 1895 (587), 32-40 1542 (336), 40-48 1216 (232), 48-56 991 (109),
  56-64 910 (72), 64-72 764 (31), 72-80 623 (26), 80-96 851 (27), 96-128 1191 (34), 128-256 2728 (30),
  >256 1013 (32). The brief's 70 lands in the tail; railway track pieces reach a top of 57 (rail heads on a
  gravel bed), so the cut is **64**. Above it the pass-the-side-test counts are flat noise (26-34 a bin).
- side/top (underside <= 16, top <= 64): 0-0.05 5623, .05-.1 744, .1-.2 1000, .2-.3 674, .3-.35 375,
  .35-.4 257, .4-.5 588, .5-.75 945, .75-1 840, 1-1.5 933, 1.5-2 468, 2-4 835, >4 1939. The valley is at
  0.35-0.4. By group (10th/50th/90th percentile): rails .12/.19/.26, leaf piles 0/.02/.05, trash edges ~0,
  decals ~0, trash clumps .02/.07/.21, trash pile walls .05/.10/.26, foot paths <= .11; standing things start
  higher: cars .41 (10th) / .82 (median), car frames .84/1.48, jersey barriers .94/2.39, guard rails
  .44/2.99, barriers 2.0/2.62, fences >= 3, benches .82, barrels 1.16. The cut is **0.35**.
- underside (median): <-64 3741, -64..-16 4763, -16..-4 3527, -4..0 2638, 0-4 3796, 4-8 1608, 8-16 2312,
  16-32 2987, 32-64 4465, 64-128 5034, >128 44967. On the ground = within **16** above (a slab resting on
  rubble); any depth below is allowed (a sunk slab is still on the ground), but a top below **-8** is buried
  and the terrain hides it (552 placements, mostly building floors under the land).

**Decisions (85,254 STAT placements examined, 79,838 measured):** painted 6,510; not on the ground 56,499;
too tall 6,691; stands up 5,287; under water 4,291; road (the road stamp owns it) 3,465; model has no shapes
1,755; under the ground 552; no model 141; initially disabled 55; no top surface 8.

**Top painted per kind** (kind = a label from the path, report only; squares = 16-unit texels):
- pad: 38 models, 150 placements, 6,762 squares (building shell floors, lobby floors, a pool floor, shack floors)
- rail: 13 models, 26 placements, 35,082 squares (RRTrackRamp01 5,819; RRTrackStr01 5,510; splits, curves)
- path: 14 models, 34 placements, 2,105 squares (FootPath_Short01/Long01, FreedomTrail pieces)
- decal: 14 models, 338 placements, 8,130 squares (TrashDecal01-03, DecalDebris01-06)
- debris: 413 models, 5,962 placements, 789,454 squares (TrashPileWall01 78,525; LeafPile01 63,168;
  TrashClump02; RockPileL01; TrashEdge01-03)

**Top refused:** trees and hedge rows (not on the ground: their trunks stand on the land but the median
square is canopy), RockCliff05 / HedgeRow03 / Bramble04 (too tall), TrashPileWall01 on 148 placements (not
on the ground, median underside 56: piled on other things), RockL01 (under water).

**Standing things** (placements; refused reasons; painted):
- guard rails 43: stands up 9, not on the ground 28, too tall 3, under water 2; 1 painted (a broken piece lying flat, side/top 0.17)
- chain-link fences 416: too tall 159, not on the ground 256, stands up 1; 0 painted
- picket fences 72: 70 refused; 2 painted (fallen pieces, top 7-14)
- wrought iron 202: 200 refused; 2 painted (a flat barricade piece, top 8-20)
- jersey barriers 65: stands up 45, too tall 5, not on the ground 12, under water 3; 0 painted
- police/road barriers 51: stands up 15, too tall 19, not on the ground 17; 0 painted
- cars (vehicles/automotive) 388: 380 refused; 8 painted -- cars sunk into the land (top 26-54,
  side/top 0.17-0.34): measured, they are low mounds. Kept honest here; the override demonstration below
  uses one of them for its `nobake` line.
- car frames 25: all refused (stands up 15, ...); retaining-wall pieces 1,011: 1,001 refused, 10 fully sunk
  pieces painted (top <= 8); planters 37, benches 28, mailboxes 4: all refused. Curbs and hydrants: none in the box.

**Mods:** 617 placements from non-Bethesda plugins in the box (575 measured): BNS Trees.esp 557,
UltraExteriorLighting.esp 59, ccBGSFO4115-X02.esl 1. None qualifies (trees, lights, one standing prop).

**Has-LOD among painted: 59** (ShackBridgeFree01 22, WrhsFloor01 6, ...). Painted, as ROADS1 paints a
has-LOD road that lies on the ground: the distant model sits at the same height over the paint, so nothing
is drawn twice in the air.

**Under water: 4,291** (boulders and coast rocks; water 450 default, also 850 and 578).

## 3. The rule and the override file (in src/lodgen.cpp, beside the road stamp)
Code: `LodgenRoadSet` (gather -> addPlacement -> evaluateFlat -> finishFlat; rasteriseFlat after the road pass).
* Candidates: every STAT that is not a road model (SCOL parts expanded), in the gather rectangle plus 2 cells,
  not disabled. No path and no kind is read by the decision (the kind column in the report is a label only).
  Has-LOD pieces are painted like any other, as ROADS1 does for the ground road pieces.
* Measured per placement, mesh x scale x rotation, over 16-unit squares, against LAND raised to the highest
  stamped (non-raised) road triangle there. Refused, in this order: would not load; no land under it; under
  water (origin below the cell's water); underside median > 16 above the ground (not on the ground: a bridge,
  a raised piece, a roof); top p90 < -8 (under the ground); top p90 > 64 (too tall); steep side rising above 8
  / visible top > 0.35 (stands up); no top surface. The census histograms (section 2) chose 64 / 0.35 / 16 / -8;
  the brief's "start near 70" became 64: railway track reaches 57, and above 64 the counts that pass the side
  test are flat noise (26-34 a bin).
* Override file: plain text, one line each `bake <path>` or `nobake <path>`, `#` comments. A path ending in
  .nif is one model, otherwise a folder prefix; lower case, either slash, a leading data/ and meshes/ ignored.
  Its lines win over the rule (the longest match wins, a later line wins a tie).
  **Location and name: `lodgen_flat_objects.txt` beside NifSkope.exe** (the folder the exe runs from). Made
  with only its header comment when missing, never written again; read on every bake;
  `--flat-objects-file <path>` names another file (a named file that is missing is a stated problem, not made).
  The chunk-pass digest carries `flatObjects` and `flatObjectsRules` (count + sha1 of the rule lines), so a rule
  edit rebakes and a comment edit does not.
* Switches: default ON with the roads. `--no-flat-objects`, panel row *Paint flat ground objects* (Roads
  section, ticked). `--roads-legacy` leaves it off unless `--flat-objects` is named too.
* Per-bake report: `<mod>/FO4CSLOD/<ws>/<ws>.flat_objects_report.txt`, written by the terrain VT bake beside
  its sheets, never inside a .lodt. One tab-separated row per base model: model, plugin(s), placements, median
  top, median underside, side/top, decision breakdown (painted xN; refused, <reason> xN; overridden bake|nobake
  (line N; the rule: ...) xN), kind label, has LOD, squares, texels written. The bake log carries the counts
  (flatExamined, flatPainted, flatOverridden, flatHasLod, flatShapeTiles, flatTexels, flatDecalTexels,
  flatRefusedNoTexture, flatReport <path>).
* Out of scope, stated: TXST projected decals (1,982 in the box) have no mesh and are not painted; the legacy
  per-chunk .BTR path paints the flat objects too but writes no report file; the flat pass runs only when the
  roads are on.
