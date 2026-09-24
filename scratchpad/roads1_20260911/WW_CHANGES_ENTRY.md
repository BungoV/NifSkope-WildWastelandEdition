### Roads and decals in the far terrain, the way vanilla does it (2026-09-11, lane ROADS1)

bungo, gap review 2026-09-11 10:0x, verbatim: *"We do the same with roads and
decals as vanilla."* What vanilla does was measured before any of this was
written, on Bethesda's own shipped
`Textures\Terrain\Commonwealth\Commonwealth.4.-20.20.DDS` — the Sanctuary
loop-road chunk — against an independent top-down projection of the placed road
meshes.

**What vanilla was measured to do.**

* The road is **not in the LAND paint**: all sixteen cells of that chunk have a
  LAND record and not one of the sixteen landscape textures painted across them
  is a road, asphalt, concrete or pavement texture.
* It **is at the road meshes' own footprint, and no other family's**. Scoring
  vanilla's sheet with each family's projected footprint, each against its own
  floor (the same mask displaced five ways, area and spectrum preserved):
  road **0.716** bright / **0.678** grey against floors that never pass 0.601 /
  0.513; trees 0.529, rocks 0.448, architecture 0.621, set dressing 0.560 and
  every generic `bDecal` shape in the region 0.564 — all inside their own floors.
  Ceiling 1.000.
* The colour is the road **material's own diffuse under the sheet's own
  grading**: `SancRoad01_d` averages luminance 112.8 and vanilla's sheet reads
  92.6 inside the footprint (×0.82); `DriedGrass01_D` averages 100.2 and the
  sheet reads 82.9 on the plain background (×0.83).
* The **`_msn` normal sheet does not carry it**. Vanilla's `_msn` against a
  normal computed from the LAND heightmap alone disagrees by 13.58° on the road
  footprint and 14.14° on the background (displaced control 14.45°) — the road
  agrees with the bare heightmap slightly BETTER than its surroundings.

**What shipped.** Road meshes are scan-converted top-down into the far-terrain
**colour sheet only**, in both bake paths, between the VCLR multiply and the
grass tint — after VCLR because the road lies on ground the artist already
shaded, before the tint because a road suppresses the ground cover under it
(`cover *= 1 − coverage × --road-cover-suppress`, default 1: grass grows beside a
road, not through it). The topmost triangle covering a texel centre wins; colour
comes from the shape's diffuse at the interpolated UV times the vertex colour,
at a mip taken from the triangle's own texture-area-to-footprint ratio; an
opaque shape covers fully and only a shape whose material or `NiAlphaProperty`
says so honours the texture's alpha, which is what cuts out an alpha-tested road
decal. The normal, height, roughness, metallic and emissive sheets are untouched.

**The mesh test, never a substring** (`MISTAKES.md`'s "sTREEt" lesson has a live
counter-example here: twelve `SetDressing\RailRoad\WaxCandle02Off.nif` are placed
in Sanctuary): base signature `STAT`, and the model path — separators
normalised, lower-cased, a leading `meshes` dropped — has `landscape` first and
`roads` or `sidewalks` second, with a component after them.

**The switch.** `--roads` / `--no-roads`, **on by default under both targets
because vanilla does it**, plus `--road-cover-suppress F`. `--no-roads` never
allocates the plane, never reads a `REFR` and never opens a road model:
byte-identical to the bake before this existed, **9 of 9 files** over the
Sanctuary region.

**The gate.** Road-presence on the loop-road chunk, with the mask taken from
**vanilla's own sheet by its colour** (chroma ≤ 17.5 and luminance ≥ 87.7, the
midpoints of the two measured populations, eroded 3×3), and the extractor itself
controlled first: 51.1 % of the mask falls inside the independent geometric
projection against 3.5–16.3 % for the same projection displaced five ways.
Fraction of the mask within 16 of 255 of vanilla — **ceiling 1.0000, floor
(`--no-roads`) 0.1274, after (`--roads`) 0.3065**, the surrounding ground 0.3442;
pre-registered bars ≥ 2× floor and ≥ 0.8× the ground, both met. Whole-tile mean
colour error 24.39 → **22.81**; on the road centreline **38.85 → 24.36**.

**The census**, written whether or not it found anything: `roads`,
`roadPlacements`, `roadMeshes`, `roadShapeTiles`, `roadDecalShapeTiles`,
`roadTriangles`, `roadTexels`, `roadDecalTexels`, `roadAlphaRejected`,
`roadRefusedNoLoad`, `roadRefusedNoTexture`, `roadRefusals` on the `--vt` report
line, and one `roads …` line per chunk on the stock path. Measured: the loop-road
region 253 placements / 73 meshes / 88,513 triangles / **27,695 texels** / 915
decal texels / 577 alpha-rejected; cells −20..−17 × 24..27, which have no roads,
**0 texels** with the placements and meshes still counted from the two-cell
gather margin.

**A defect found by the census and fixed for the road pass only.** Every
`Landscape\Roads\Country\*` and `\Alley\*` piece names its material as an
absolute Bethesda build path (`C:\Projects\Fallout4\Build\PC\Data\materials\…`)
and carries an EMPTY texture set; `lodgenLoadModel`'s fix-up prepends
`materials/` and resolves nothing, so 65 of 270 road shapes had no diffuse and
every decal among them was invisible. `lodgenRoadMaterialPath` keys on the last
`materials/`. **The shared loader is unchanged** — widening it would move output
the object byte-identity gates pin — so the object bakes still drop those
textures. bungo's call.

**New:** `tests/spells/lodgen_roads.sh` (11 checks, 0 failures) and
`tests/spells/lodgen_roads_metric.py`. Its floors fire in the same run: the same
comparison says "differ" on the road-bearing region and "byte-identical" on the
road-free one, and `roadTexels` reads 27,695 against 0. It exists because
**none** of the terrain spells touch roads — `lodgen_terrain_vt.sh`'s fixture
reports `roadTexels 0`.

**Contract:** `docs/LODGEN_TERRAIN_VT.md` gains section 1a (the measurement, the
rule, the mesh test, the order in the composite, the channels, the census, the
way back) with its own provenance block; `docs/LODGEN_PARITY.md`'s road gap line
is closed with the measured after-state and the correction that the tile the old
note pointed at has no roads in it.

**Gates at their baselines:** `lodgen_terrain.sh` 26/0, `lodgen_terrain_vt.sh`
41/1, `lodgen_ground_cover.sh` 29/5, `lodgen_terrain_pbrm.sh` 14/0,
`lodgen_texture_arrays.sh` PASS, `lodgen_card_arrays.sh` PASS,
`lodgen_native.sh` 18/0, `lodl_open.sh` 23/0, `ui_align.sh` 11/0,
`water_ui.sh` 82/0.

**Pictures:** `scratchpad/roads1_20260911/images/cmp_sanctuary_road.png`
(vanilla | ours without | ours with, same 512 texels, plus 4× zooms of the
cul-de-sac), `road_mask_and_metric.png` (the mask over all three with the gate's
numbers), `top_region.png` (the same ground from above through the render hook,
each side served its own sheets).
