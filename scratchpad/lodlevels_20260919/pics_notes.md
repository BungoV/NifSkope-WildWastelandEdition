# LODLEVELS -- pictures lane (pics_*)

Deliverable: 16 frames + 4 contact sheets in
`E:/Projects/NifskopeWildWastelandEdition/scratchpad/lodlevels_20260919/images/`.
Everything below is reconstructed from `esm.pkl` + the STAT MNAM slot meshes on
disk. No `.lodo`/`.lodi`/`.lodt` bake is read anywhere in this lane.

## Files

Contact sheets (2x2, levels 4 / 8 / 16 / 32, camera fixed across the four):

* `images/sheet_grey_east.png`   637,063 B
* `images/sheet_ident_east.png`  727,067 B
* `images/sheet_grey_full.png` 1,137,359 B
* `images/sheet_ident_full.png` 1,276,502 B

Individual frames, 1600x900 + caption band:
`images/{grey,ident}_lod{04,08,16,32}_{east,full}.png` -- 16 files, 258,935 B to
941,716 B, every one verified with `os.path.getsize`.

## Per-level table -- 'east' camera

| level | MNAM slot | placements drawn | triangles drawn | kit pieces drawn | placements covering >=1 pixel | objects' share of frame |
|---|---|---|---|---|---|---|
| 4  | 0 | 46,138 | 333,681 | 43,604 (95%) | 3,238 (2,970 kit) | 53.1% |
| 8  | 1 | 45,242 | 290,614 | 43,075 (95%) | 2,770 (2,498 kit) | 52.7% |
| 16 | 2 |    526 |  24,362 |    351 (67%) |   122 (55 kit)    | 13.8% |
| 32 | 3 |     17 |   4,954 |      0 (0%)  |    10 (0 kit)     |  6.1% |

'full' camera, same four levels: 35,069 / 33,382 / 409 / 21 placements,
341,737 / 263,965 / 27,957 / 9,061 triangles, 31,865 / 31,093 / 153 / 0 kit.

Whole-window context (all 280,438 Commonwealth refs in the pickle, no camera):
59,364 refs have a level-4 mesh, 55,710 a level-8, **931** a level-16 and **26**
a level-32. Only 3,081 / 2,940 / 456 / 51 of the 28,932 base records carry a
mesh in slots 0/1/2/3 at all.

**That is the finding the pictures make visible.** LOD4 and LOD8 are both "the
whole city, piece by piece" -- essentially the same placement set, LOD8 simply
15% cheaper in triangles. Between LOD8 and LOD16 the data falls off a cliff:
98.8% of the placements vanish, and what survives is not a decimated city, it is
a *different, hand-authored set* -- whole-tower landmark meshes and the elevated
highway, not wall/roof/window kit pieces. Kit share collapses 95% -> 67% -> 0%
across 4/8 -> 16 -> 32. At LOD32 there is no kit geometry at all in either view.

## Kit-piece rule (printed in every caption)

A base counts as a kit piece when the first path component of its near MODL
path is `Architecture`, **or** any path component of the MODL path or of that
level's LOD path contains `kit` case-insensitively (`MetalKit`, `DecoKit`,
`BldgKit`, `WoodKit`, ...).

## Cull box

Per camera, per level: frustum cull on each placement's OBND bounding sphere
(radius = scale * (half the OBND diagonal + |OBND centre| + 64), floored at 64),
with the frustum half-angles widened x1.6 and a 256-unit slack, out to 140,000
world units along the camera forward axis. No world box beyond that -- distant
placements are kept and simply recede into the haze, so the coarse levels are
not made to look emptier than they are by a tight box.

## Control: Bethesda's own bake (`pics_control.py`, `pics_control.json`)

`Meshes/Terrain/Commonwealth/Objects/Commonwealth.4.4.-12.BTO`, read with the
same NIF reader, shapes carried up their own node chain, vs my level-4
reconstruction restricted to the same cells (x 4..7, y -12..-9):

|  | triangles | vertices | placements | bbox x | bbox y | bbox z |
|---|---|---|---|---|---|---|
| Bethesda's BTO | 28,016 | 49,657 | -- | 16,218 .. 32,976 | -49,562 .. -31,744 | -266 .. 2,948 |
| my level 4     | 24,774 | 45,343 | 2,183 | 16,380 .. 32,980 | -49,562 .. -31,737 | -1,424 .. 2,949 |

**Ratio mine / BTO = 0.884.** The X/Y/Z-max bounds agree to within ~160 units
(4% of one cell) and the Y-min agrees exactly, so the placement convention
(`M = fromEuler(-rx,-ry,-rz)`, `world = pos + scale * (M @ local)`) is right.
The 12% triangle shortfall and the low Z-min are honest gaps, not hidden:

* the bake merges in refs whose *origin* sits outside the chunk but whose mesh
  overhangs into it; my restriction is by ref origin only;
* SCOL (static collection) bases, 2,617 of them in the pickle, place one REFR
  but bake as many parts;
* the bake adds its own stitching/terrain-hugging geometry per shape.

My Z-min of -1,424 vs the bake's -266 is one placement sunk deep under the
terrain that Bethesda's bake dropped and I keep -- it is invisible in every
frame. I did not chase it.

Two other BTO files were opened as a scale reference (there is no level-8 BTO
for this chunk, the level-8 grid is coarser): `Commonwealth.16.0.-16.BTO`
26,619 tris and `Commonwealth.32.0.-32.BTO` 9,596 tris -- both cover far more
ground than one level-4 chunk, which is the same "few big things over a wide
area" shape the level-16/32 pictures show.

## What I simplified

* **No cast shadows.** sunsim1's sheared-sun suffix-maximum machinery is not
  built here; every surface is handed `lit = 1.0` and only N.L + ambient models
  it. Sun az 120, el 45. This lane is about what geometry exists, not light.
* **No textures, no vertex colour, no alpha** -- one grey object albedo and one
  warmer terrain albedo, straight out of sunsim1's `shade.py`.
* **The terrain ray march runs once per camera** and is reused by all eight of
  that camera's frames (cached in `pics_ter_east.npz` / `pics_ter_full.npz`);
  the terrain is identical at every object LOD level.
* **Set B haze** is a mild fade toward dark grey at 170,000 units rather than
  sunsim1's 95,000-unit blue haze, so the identity colours survive to the
  skyline. Set B's sky is a flat dark neutral, not the sunsim1 gradient.
* **Backface handling**: LOD shells are not consistently wound, so interpolated
  normals are flipped to face the camera (sunsim1 does the same).
* Near-plane clipping is sunsim1's: whole triangles with any vertex nearer than
  16 units are dropped (0 to 601 triangles a frame, printed by the run).

## Nothing was missing

0 slot meshes absent from disk and 0 refused by the NIF reader, across every
path referenced by any of the four slots in the culled sets.

## Scripts (all under this folder)

`pics_build.py` (slot-mesh loader, node-chain composition, placement transform,
kit rule, frustum cull), `pics_scene.py` (Terrain only, lifted from sunsim1),
`pics_render.py` / `pics_shade.py` / `pics_nifread.py` (copies), `pics_cams.py`
(east + full, eye/target/fov verbatim from sunsim1's `cams.py`), `pics_run.py`
(the 16 frames + 4 sheets), `pics_control.py` (the BTO control),
`pics_probe1.py` / `pics_probe2.py` (pickle and budget probes).
Outputs: `pics_control.json`, `pics_table.json`.
