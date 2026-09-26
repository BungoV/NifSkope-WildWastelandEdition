# Lane ROADS1 (2026-09-26) -- pavements in the LOD terrain colour; the riverside strips

Worktree E:\Projects\NifskopeWWE-roads1, branch roads1-20260926, from 6382a09a. Nothing installed, nothing merged.

## 1. Skills loaded
- nifskope-ww-lodgen (CLI, the two colour writers, editing traps)
- nifskope-ww-worktree-build (section 5b: objects copied from the sibling NifskopeWWE-fix1, which sits at 6382a09a,
  clean, `make -n` = 0 compiles). First build 11:46, rung saved as release/NifSkope.before_roads1.exe (sha1 5feb97ce).

## 2. Faithfulness -- the stamp now wears the in-game texture, material swaps included
What was wrong: the stamp loaded every road and pavement model with its OWN material and ignored the material
swap the game applies (the REFR's swap, else its base's, else a SCOL part's own). 229 placements in the box wear
a swapped material in game; the old stamp painted them with the unswapped one. Nothing was tuned: the fix is the
game's own swap rule, restated in lodgen.cpp, plus the has-LOD ground pieces of section 4.

Gate (work/faith_cmp.py, RULE=new, both bakes with `--land-shade 0`, same switches otherwise). Sample: 594,016
pavement texels (the in-game winner is a pavement, full coverage, a 3x3 neighbourhood of the same piece); 3,169
of them wear a swapped material in game. Expected from the independent raster: in game 142.2 luma (swapped subset
97.9); the old code's rule 142.5 (swapped subset 147.3).

| bake | sheet luma | mean abs vs in game | mean abs vs old rule | swapped subset: sheet | vs in game | vs old rule |
|---|---|---|---|---|---|---|
| rung 6382a09a (`rung_sw_s0`) | 140.9 | 6.78 | 6.54 | 147.7 | **50.26** | 4.83 |
| ROADS1 (`new_s0`) | 142.2 | **5.12** | 5.38 | 97.6 | **2.25** | 50.08 |

The rung FAILS the in-game gate on the swapped texels (50 levels off) and matches the old rule instead; the new
bake matches the game (2.25) and no longer the old rule. The ~5-level floor over all texels is present in both
sides: block compression and the sheet's filtering, not colour. Per channel, new vs in game: R 5.62, G 5.15, B 5.18.
No sRGB step exists (the textures are UNORM, vertex colours white, UV transforms identity).

## 3. Tile edges -- REAL slab joints in the texture. Kept.
Measured (work/tile_edges.py on the rung bake `rung_sw_s0`, pavements on):
- The in-game diffuse `landscape/roads/sidewalkconcrete01_d.dds` (2048 x 2048) is itself four slabs: the joint
  lines at u = 0, 0.5 and v = 0, 0.5 read 88.4 and 81.0 luma against 139.5 for the slab faces.
- The sheet follows an independent re-rasterisation that samples ONLY the texture: correlation 0.946 over 889,217
  pavement texels. 97.1 % of the texels the sheet shows as dark lines are dark in the texture-only raster too.
- No step at the bake's own grid: sheet minus raster is +0.26 luma at cell edges, +0.11 at 16-texel block edges and
  +0.13 at 4-texel compression block edges, against +0.12 to +0.13 inside. A bake seam would show there.

## 4. Riverside -- yes, there is a road there in game. It was refused; now fixed.
Where: his three strips are the river bank east of Diamond City, cells about (-4..1, -4..-3), along Storrow Drive
(pics/river_decisions_-6_-6_1_-2.png, magenta = refused; pics/_river_decisions_rot180_half.png is the same map turned
to his camera's direction, where the three magenta stretches line up with his three circles).

What lies there (work/river_list.py, full table in work/out/river_strips.tsv, not committed):
- 11 road placements refused. Each is `Landscape\Roads\River\RRoadCurveCustom01..11.nif`, Fallout4.esm, with the
  reason "raised-haslod". Together they cover about 130,000 texels at 16 units each:

  | ref | model | base |
  |---|---|---|
  | 0006485A | RRoadCurveCustom01 | 0006485B |
  | 0021A582 | 02 | 00064877 |
  | 0006D67B | 03 | 000648CE |
  | 0021A581 | 04 | 0006487D |
  | 0021A584 | 05 | 0021A583 |
  | 0021A585 | 06 | 000648C2 |
  | 0006487B | 07 | 001E32A5 |
  | 0021A58A | 08 | 0021A586 |
  | 0021A58B | 09 | 0021A587 |
  | 0021A58D | 10 | 0021A588 |
  | 0021A58E | 11 | 0021A589 |

  They lie on the ground: the model spans z -238..+327 with brick walk, bridge brick and asphalt materials. They
  were refused only because each base carries its own distant-LOD mesh.
- 46 road and pavement placements whose footprints touch the strips' edges were already stamped:
  - 7 roads (crosswalks, RoadStr01/02, RRoad2SCustomBridgeEndL, intersections)
  - 39 pavements, which were stamped only with `--road-sidewalks`
- 1,835 other placements sit on the strips (1,713 STAT; the rest TXST, CONT, MISC, ACTI and so on). They are not
  road models and have no footprint in the stamp: trees, rubble, benches, lights. All are Fallout4.esm, except 10
  from UltraExteriorLighting.esp.

Why they were refused: the stamp treated "the base has its own distant LOD" as "raised". Across the whole load order
(work/haslod_census.py), the has-LOD road bases outside the HighwayOverpass and Bridge folders are exactly:
- the 11 river pieces
- 16 park pavement bases (Sidewalks\Park, 45 placements)
- PlazaSwanPond01

All of them are laid on the ground.

The fix: the raised rule is now the folder rule only (HighwayOverpass and Bridge), so the river road and the park
pavements are painted with their in-game texture, on by default.

## 5. Commits (branch roads1-20260926, by explicit path, not pushed, not merged)
- aa8f096b: the independent road/pavement raster, the riverside census, the tile-edge measurement; sections 1, 3, 4
- 273b704d: lodgen roads -- pavements on by default, material swaps applied, has-LOD ground pieces painted
  (src/lodgen.cpp, src/lodgen.h, src/lodgenmanager.cpp, src/nifcli.cpp, src/nifskope_ui.cpp, src/lodgenchunkpass.h)
- the report commit that carries this file, DELIVERABLE_TEXT.md and the last scripts (confine.py, pics.py)

Code in one breath: `lodgenRoadEffectiveSwap` (the game's swap order), a per-swap cache in `LodgenRoadSet`, the swap
passed to `lodgenLoadModel`; "raised" = the HighwayOverpass / Bridge folders only; `roadSidewalks = true`, panel
row default ticked, self-test default "1"; census `roadSwappedPlacements`; `kLodgenGeneratorRevision` 1 -> 2.
`--road-sidewalks` STAYS as a no-op (old command lines and bake records keep parsing); `--no-road-sidewalks` is
the way to leave pavements out. No fix-only toggle: the swap and the has-LOD rule have no switch.

## 6. Gates
- **Build:** the worktree build (skill section 5b objects), make rc 0, exe 12:22:42 newer than every changed
  source; run from the copied folder `scratchpad/roads1b/ns_run1` (sha1 31e53b1e). Rung = 6382a09a's build
  (`release/NifSkope.before_roads1.exe`, sha1 5feb97ce).
- **Game check:** Fallout4.exe not running before each bake (bake.sh refuses otherwise).
- **Faithfulness:** PASS -- section 2: 594,016 pavement texels, 3,169 swapped; new 2.25 vs in game on the swapped
  texels, the rung 50.26 (FAILS, as it must).
- **Census:** new_s0 `roadPlacements 5408` (rung 5351; the independent reader: 5,412 vs 5,355, the same +57),
  `roadRefusedRaised 255` (rung 312: the 57 has-LOD ground pieces now painted), `roadSwappedPlacements 229`,
  `roadSidewalksIncluded 1` with no switch spelled.
- **Confinement, rung vs new (same switches):** of 186,035 changed colour texels, 0 lie outside the road/pavement
  footprints' 4x4 blocks (7,006 outside strictly: the compression blocks), against the union of the in-game and
  old-rule footprints. With the in-game footprint alone, 11,293 fall outside: a swapped material and its original
  cover different fragments (alpha test, or a texture missing from the archives such as quarrymarblefloor01), so
  the old paint that was removed lies under the OLD footprint; the union settles it. VT.2 and VT.4 (the next rung
  of the same colour ladder) differ; VT.lodm is byte-identical.
- **Confinement, pavements on vs off (same code) -- NOT RUN.** The `--no-road-sidewalks` bake and the default
  bake are queued behind the machine's NifSkope turn lock, which I took by mistake (see the end of this file).
- **Harness:** tests/spells/lodgen_roads.sh reaches this change (default pavements ON can move R5, the
  road-presence metric against vanilla) -- NOT RUN for the same reason.

## 7. Pictures (scratchpad/roads1_20260926/pics/, not committed)
- river_decisions_-6_-6_1_-2.png, _river_decisions_rot180_half.png: the riverside decision map (section 4).
- installed | new pictures (downtown, riverside, riverside turned to his camera): NOT MADE. work/pics.py is ready
  (`python pics.py new_default`); running it was refused by the permission check, and the default bake it wants
  is queued behind the turn lock.

## 8. Skills review
- New skill `ww-road-stamp-faithfulness` (E:\Projects\Claude\.claude\skills\, copied to E:\Tools\AISkills\):
  the independent raster and its four reader traps (DX10 texture BA2s, Pillow's legacy FourCC, the vertex-desc
  nibbles 8/24, the swap order), the sample set, the gate that must fail on the old exe, confinement with the
  block allowance and the footprint union, the tile-edge test, and has-LOD is not raised.
- nifskope-ww-lodgen and nifskope-ww-worktree-build were right as written.

## Blocker at hand-back (13:00)
`bash turn.sh status` is not a command of FIX1's turn.sh: any word but `release` ACQUIRES the turn, so my
status check took the lock as "anon" at 13:00:08 (E:\Projects\NifskopeWWE-fix1\scratchpad\fix1_20260926\.ns_turn).
Releasing it (`bash turn.sh release anon`) was refused by the permission check, so it is still held and blocks
every lane that uses turn.sh, mine included. My chain (new_nosw_s0, then new_default) waits behind it and behind
AO2's headless NifSkope (scratchpad\ao2\ns_before, which turn.sh does not see: it only looks for NifskopeWWE-*
paths). Owed after release: `python confine.py new_s0 new_nosw_s0`, `python pics.py new_default`, lodgen_roads.sh.
