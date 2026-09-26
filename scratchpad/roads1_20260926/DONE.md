# Lane ROADS1 (2026-09-26) -- pavements in the LOD terrain colour; the riverside strips

Worktree E:\Projects\NifskopeWWE-roads1, branch roads1-20260926, from 6382a09a. Nothing installed, nothing merged.

## 1. Skills loaded
- nifskope-ww-lodgen (CLI, the two colour writers, editing traps)
- nifskope-ww-worktree-build (section 5b: objects copied from the sibling NifskopeWWE-fix1, which sits at 6382a09a,
  clean, `make -n` = 0 compiles). First build 11:46, rung saved as release/NifSkope.before_roads1.exe (sha1 5feb97ce).

## 2. Faithfulness
(in progress)

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

## 5. Commits
(none yet)

## 6. Gates
(in progress)

## 7. Pictures
(in progress)

## 8. Skills review
(at the end)
