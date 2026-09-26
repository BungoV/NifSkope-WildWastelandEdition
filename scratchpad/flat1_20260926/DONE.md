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

## 4. Faithfulness per kind, and the order things are painted in
**What a painted texel is** (src/lodgen.cpp `rasteriseFlat`, the same reader the road stamp uses):
- The material: the shape's BGSM (or BGEM: its base map, slot 0), after the material swap. Swap order as the road
  stamp: the REFR's XMSP, else the MODS of the placing base (the SCOL's for a SCOL part), else the part's own MODS.
- The diffuse, sampled with the NIF's UVs times the material's UV scale plus offset, at the mip the footprint
  asks for (texture area over footprint area, as the road pass), read as stored (the textures are UNORM, so no
  gamma step, like the road stamp).
- Times the material tint, times the vertex colour when the shape has one; a blended shape with Vertex_Alpha
  also takes the vertex alpha as coverage.
- A fragment whose height is more than 8 below the ground under it is skipped (the terrain covers it in game).

**Order:**
1. The roads first (ROADS1, unchanged).
2. Opaque flat shapes: highest wins (max z against the road z buffer); they replace what is there.
3. Decal, alpha-blend and alpha-test shapes: sorted by their mean height, lowest first, each painted OVER what
   is there (alpha-test = its cut, alpha-blend = texture alpha x material alpha), skipped where it is more than
   4 units under the top already painted.

The alpha of the colour word also clears the ground-cover byte under the paint, as the roads do.

**The gate** (work/flat_faith.py): an independent raster in Python. It reads each placement's own NIF, BGSM,
swap and DDS, with none of the C++, and is compared with the baked VT.2 colour sheet.

The sample is texels where one painted shape wins at full coverage, not under a road, not a BGEM shape (the
raster does not read BGEM), not a margin placement, and 16 texels in from the box edge. Mean |luma difference|,
in 0-255 levels. The off bake is the same exe with `--no-flat-objects`: it must fail, and it does.

Strict sample (3x3 of the same winning shape):

| kind | texels | flat1 mean / p90 | flat off mean / p90 |
|---|---|---|---|
| pad | 15,382 | 2.22 / 5.0 | 36.10 / 75.9 |
| rail | 22,270 | 2.17 / 4.5 | 9.44 / 19.9 |
| path | 346 | 2.33 / 4.9 | 11.22 / 24.9 |
| decal | 41 | 3.43 / 6.5 | 18.20 / 43.7 |
| debris | 141,745 | 2.88 / 6.7 | 16.34 / 33.2 |

Decals are thin and rarely hold a 3x3 of one shape, so the brief's 200 texels per kind comes from the looser
sample (the 3x3 rule dropped):

| kind | texels | flat1 mean / p90 | flat off mean / p90 |
|---|---|---|---|
| pad | 22,656 | 2.68 / 6.3 | 34.72 / 73.6 |
| rail | 63,256 | 2.63 / 5.5 | 9.14 / 19.1 |
| path | 1,112 | 2.62 / 5.5 | 12.81 / 28.1 |
| decal | 1,836 | 3.43 / 6.6 | 19.51 / 38.9 |
| debris | 313,537 | 3.29 / 7.5 | 17.00 / 35.4 |

The remaining 2-3 levels are BC1 compression of the sheet (ROADS1 measured the same floor on the road plane).
Nothing was tuned toward Bethesda's LOD colours; the target is the in-game diffuse.

These bakes used `--land-shade 0`, but this bake mode already runs with land shade 0 (the log says
`landShade 0.000`, `chunksShaded 0`), so ls0_on is byte-identical to flat_default. That also shows two bakes of
one exe are deterministic.

## 5. Commits (branch flat1-20260926, by explicit path, not pushed, not merged)
- dbda923d: the census of flat ground objects over the Boston box; rule thresholds from histograms.
- b23f5631: the implementation (src/lodgen.cpp, lodgen.h, esmdata.h/.cpp, lodgenchunkpass.cpp, nifcli.cpp,
  lodgenmanager.cpp, nifskope_ui.cpp), with its patch scripts, bake.sh and flat_faith.py.
- 31b70b79: the report names each placing plugin with its painted count; an unloadable model carries no
  override; the TAB literals are now escapes; flat_confine.py and flat_pics.py.
- cde019eb: faithfulness per kind (the loose sample for decals), the confinement can-fail run, the override pair
  gate, the oblique shot script.
- 60214db0: DONE.md sections 5-8, DELIVERABLE_TEXT.md (this line added in the commit after it).

## 6. Gates, with numbers
All bakes are the Boston box -8 -12 3 -1, run from a copy of the exe under the session scratch
(flat1/run). The final exe is release/NifSkope.exe 17:02:29, sha1 784eeb66.

- **Build**: tools/ww_build.sh RC=0 (three builds), and the exe is newer than every changed source.
- **Final bake** (flat_final, final exe): rc 0, 166 s. flatExamined 144,655, flatPainted 10,834, flatHasLod 102,
  flatTexels 730,420, flatDecalTexels 478,813, flatRefusedNoTexture 14. VT.2 and VT.4 are byte-identical to
  flat_default (build-1 exe), so patch 6 changed the report only.
- **Off = old**: `--no-flat-objects` (flat_off) is byte-identical (VT.2, VT.4) to ROADS1 new_default.
- **Confinement**, flat_off vs flat_default:
  - VT.2 colour changed 992,102 texels; outside the painted footprint strictly 176,881 (the BC1 blocks);
    **outside its 4x4 blocks 0**; changed only under a margin placement 3,440.
  - Non-colour sheets: VT.2 108 and VT.4 27 slices compared, 0 differ.
  - Mask (ground-cover byte, cleared under the paint like the roads): changed 500,804; outside the blocks **0**.
  - Can fail: the same bakes against the footprint moved 32 texels east give 610,857 colour and 302,673 mask
    texels outside the blocks.
- **Faithfulness per kind**: section 4. Every kind has at least 1,112 texels in the loose sample; flat1 is at
  2.6-3.4 mean |dluma|, while the off bake is at 9.1-34.7.
- **Standing things refused** (final report: placements; top / underside / side-top medians; reasons):
  1. FenceChainlink01: 163; 182.5 / 24.4 / 41.8; not on the ground 98, too tall 65.
  2. GRailStrShort01 (guard rail): 27; 60.0 / 24.6 / 6.28; not on the ground 19, stands up 8.
  3. JerseyBarricade01: 27; 60.1 / -2.9 / 2.94; stands up 17, too tall 6, not on the ground 3, under water 1.
  4. ECDBenchShrt01 (bench): 32; 54.3 / 19.6 / 1.06; not on the ground 18, stands up 10, under the ground 4.
  5. FenceChainlinkEndLeft02RR: 44; 146.6 / 89.3 / 2.74; not on the ground 43, stands up 1.
  6. CarFrame03: 12; 50.8 / 6.2 / 1.75; stands up 6, not on the ground 4, under the ground 1, under water 1.
  7. Coupe_Postwar_Cheap01 (car): 34 placements, 32 refused (not on the ground 18, too tall 6, under water 7,
     stands up 1). 2 are painted: cars sunk into the land, which measure as low mounds (section 2). The override
     demonstration turns them off.
  Retaining-wall and building wall pieces are refused by the thousand (for example DecoMainA1x1Wall01:
  2,725 placements, 0 painted).
- **Mods**: the final report's plugin line: BNS Trees.esp 50 examined / 0 painted;
  UltraExteriorLighting.esp 34 / 0; ccSBJFO4003-Grenade.esl 1 / 0; DLCCoast.esm 4 / 0;
  Fallout4.esm 144,566 / 10,834. Every mod placement was examined by the same rule, and none is a flat ground
  object. (The Python box census agrees: 617 non-Bethesda placements, none qualifies.)
- **Override**:
  - A file with a `nobake` line on every top folder (ovr_all): painted 0, overridden 141,397. VT.2, VT.4 and
    .lodm are byte-identical to flat_off, so nobake reverts exactly.
  - The pair (ovr_pair): `nobake Vehicles/Automotive/Coupe_Postwar_Cheap01.nif` plus
    `bake setdressing/bricksblocks/jerseybarricade04.nif` (a refused model). Painted 10,858, overridden 60 =
    the coupe's 34 placements plus the jersey's 26. Changed 957 texels (coupe 544, jersey 126); outside the two
    models strictly 287, **outside their 4x4 blocks 0**. So bake paints a refused model, and nobake removes a
    painted one, touching nothing else.
- **Determinism**: two default bakes of one exe (flat_default, ls0_on) are byte-identical.

## 7. Pictures (scratchpad/flat1_20260926/pics/, not in git)
- Top-down close-ups, one per kind, installed | ROADS1 new_default | FLAT1 (VT.2 colour, 160 texels square):
  pad_, rail_, path_, decal_, debris_installed_roads1_flat1.png. Texels of each kind in view: pad 6,650,
  rail 7,698, path 547, decal 336, debris 11,608.
- The oblique shot (a copy of the persp shot.sh, work/shot.sh; the brief's command, LV=2 SLOT=0 SDIM=2
  WW_LODL_AO=1, cells -5 -10 2 -3, view 8): oblique_roads1_new_default.png and oblique_flat1.png;
  oblique_roads1_over_flat1.png stacks them, ROADS1 on top; oblique_zoom_roads1_flat1.png is the 600-pixel window
  where they differ most (the river embankment and the streets west of it: flat debris now shows).
  - 69,113 of 2,598,400 pixels differ between the two, 40,712 by more than 8 levels.
  - The renderer is the persp script's own exe (scratchpad/incr2/ns_run1), which is exactly the brief's script.
    The FLAT1 exe rendered oblique_flat1 once, pixel-identical (0 pixels differ), but after that it exited at GUI
    start with no output and rc 0 on 23 of 24 tries. The pre-FLAT1 rung of this worktree
    (release/NifSkope.before_flat1.exe) started on the same script right before it.
  - FLAT1 changes nothing on the start path: main.cpp is untouched, and the first thing the GUI does is bind its
    UDP port and quietly exit with 0 if the bind fails. So this looks like the fresh exe being refused the port,
    like the new-exe block in nifskope-ww-worktree-build section 8. It is not proven.
  - Owed: one ordinary GUI start of the merged exe.

## 8. Skills review
- Written: `E:\Projects\Claude\.claude\skills\ww-flat-object-stamp\SKILL.md`: the measured rule and why each
  number, the painting order, the override file and report, the five gates each with its can-fail run, and the
  traps (land shade is already 0 in this bake mode; TAB literals from a patch escape; a lazy import).
- ww-road-stamp-faithfulness was right and was enough for the raster reader; no change is needed.
- nifskope-ww-worktree-build section 8 (a fresh exe is blocked from running for minutes) held: the final exe ran
  on the first try, and the retry loop is in the bake command.
- Declined: a separate skill for the oblique shot. It is the persp shot.sh with its exe and turn name changed,
  and the skill above names it.

**Out of scope, stated:**
- TXST projected decals (1,982 in the box) have no mesh.
- The legacy .BTR path paints the flat objects but writes no report.
- The pass runs only with the roads on.
- The mask sheet's ground-cover byte is cleared under the paint (as the roads do).
