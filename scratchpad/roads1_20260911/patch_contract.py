"""ROADS1's contract edits (`ww-contract-provenance`).

  docs/LODGEN_TERRAIN_VT.md   gains section 1a, "Roads and decals", and three
                              rows in the CLI table (section 5), and a ROADS1
                              block under Provenance.
  docs/LODGEN_PARITY.md       its "known gaps" road line becomes the measured
                              after-state.

Every anchor is asserted to occur exactly once, and the file's own line ending
is carried (both files are LF-only, measured).
"""

import hashlib
import io
import os

VT = 'docs/LODGEN_TERRAIN_VT.md'
PAR = 'docs/LODGEN_PARITY.md'

SECTION = """## 1a. Roads and decals in the far colour

bungo, 2026-09-11 10:0x, verbatim: *"We do the same with roads and decals as
vanilla"*. What vanilla does was MEASURED first, on Bethesda's own shipped
`Textures\\Terrain\\Commonwealth\\Commonwealth.4.-20.20.DDS` -- the Sanctuary
loop-road chunk, cells -20..-17 x 20..23, 512 texels at 32 world units each --
against an independent top-down projection of the placed road meshes
(lane ROADS1, `scratchpad/lane_roads1_report.md` section 1).

### 1a.1 What vanilla was measured to do

1. **The road is not in the LAND paint.** All sixteen cells of that chunk have a
   LAND record, and the sixteen landscape textures painted across them are
   `LDriedGrass01`, `LDriedGrass01NoGrass`, `LRubbleRock01`, `LDirtGravel01`,
   `LRiverbedSilt01`, `LDriedGrass02Weeds`, `LForestFloor01`, `LRootsEroded01`,
   `LRootsEroded01Grass01`, `LDirtGravel01Grass`, `LRiverbedSilt01Wet`,
   `LRubbleRock01Grass`, `LRiverbedRocks01Grass`, `LRiverbedRocks02Wet`,
   `LRiverbedRocks02WetGrass`, `LDebrisGround`. Not one road, asphalt, concrete
   or pavement texture among them.
2. **It is at the road MESHES' footprint, and at no other family's.** Each
   family's projected footprint was scored against vanilla's sheet, each against
   ITS OWN floor -- the same mask displaced five ways, area, shape and spatial
   spectrum preserved, registration destroyed. AUC of the sheet's brightness as
   a detector of the mask:

   | family | texels | AUC(bright) | its displaced floor | AUC(grey) | floor |
   |---|---|---|---|---|---|
   | **road** | 23,321 | **0.716** | 0.470 .. 0.601 | **0.678** | 0.448 .. 0.513 |
   | generic `bDecal` shapes | 37,993 | 0.564 | 0.494 .. 0.603 | 0.518 | 0.477 .. 0.509 |
   | trees | 51,462 | 0.529 | 0.454 .. 0.533 | 0.482 | 0.480 .. 0.528 |
   | rocks / cliffs | 42,599 | 0.448 | 0.388 .. 0.580 | 0.527 | 0.457 .. 0.578 |
   | architecture | 18,313 | 0.621 | 0.513 .. 0.614 | 0.536 | 0.479 .. 0.509 |
   | set dressing | 6,491 | 0.560 | 0.459 .. 0.597 | 0.559 | 0.464 .. 0.498 |

   Ceiling (a mask scored by itself) 1.000. Only `road` clears its own floor on
   both scores.
3. **The colour is the road material's own diffuse under the sheet's own
   grading.** `SancRoad01_d.dds` averages luminance 112.8 and vanilla's sheet
   reads 92.6 inside the footprint -- a factor 0.82. `DriedGrass01_D.dds`
   averages 100.2 and the sheet reads 82.9 on the plain background -- 0.83. The
   same grading on both, to within a percent.
4. **The `_msn` normal sheet does NOT carry it.** Vanilla's `_msn` against a
   normal computed from the LAND heightmap alone disagrees by 13.58 deg on the
   road footprint and 14.14 deg on the background (displaced control 14.45) --
   the road agrees with the bare heightmap slightly BETTER than its
   surroundings, where a baked road mesh would have to disagree. The statistic
   has range: against straight up instead of the heightmap normal the same
   comparison reads 14.39 deg on the road and 18.49 deg on the background.

### 1a.2 The rule

> Road meshes are scan-converted top-down into the **colour sheet only**, at the
> mesh's own footprint, with the mesh's own material diffuse. The normal sheet
> stays the heightmap's. No other object family is baked.

### 1a.3 The mesh test

`lodgenIsRoadModel()` plus the caller's `STAT` requirement. **Component
equality, never a substring** -- `MISTAKES.md`'s "sTREEt" lesson has a live
counter-example in Sanctuary, where twelve `SetDressing\\RailRoad\\
WaxCandle02Off.nif` are placed and the letters `road` are in every one of their
paths:

    the model path, separators normalised, lower-cased, a leading `meshes`
    component dropped, has `landscape` as its FIRST component and `roads` or
    `sidewalks` as its SECOND, with at least one component after them.

`Landscape\\Sidewalks\\*` is carried by the rule and is NOT evidenced by the
measurement tile: it contributes 187 of that chunk's texels against
`Landscape\\Roads`' 23,170. It is in because it is the same road surface under
the same folder root, and that inclusion is untested.

### 1a.4 Where it sits in the per-texel composite

Between the VCLR multiply (1.5) and the grass tint (1.5), in both the chunk path
and the tile path:

```
splat composite  ->  cover byte  ->  VCLR multiply  ->  ROAD  ->  grass tint
```

* **After VCLR**, because the road lies ON the ground the artist shaded and must
  not be shaded a second time.
* **Before the tint**, because the tint's weight is the cover byte and a road
  suppresses the cover under it: `cover *= 1 - coverage x roadCoverSuppress`
  (default 1). Grass grows beside a road, not through it. On the chunk path the
  cover PLANE is rewritten with the same byte, so what a consumer reads out of
  the sheet's alpha is what the tint used.

A texel in a cell with no LAND record gets no road: the whole composite is
skipped there, and such a texel is not terrain.

### 1a.5 What one texel gets

The topmost road triangle covering the texel CENTRE wins -- a maximum-z buffer
in world Z, so a driveway laid over a road wins and a road under a bridge deck
does not. From that triangle:

* **colour** = the shape's diffuse sampled at the interpolated UV, multiplied by
  the interpolated vertex colour. The mip comes from the triangle's own texture-
  area-to-footprint ratio, `0.5 x log2(uvArea / pxArea)`, not from the landscape
  path's world tiling: a road mesh does not tile with the world;
* **coverage** = 1 for an opaque shape -- an opaque road's diffuse alpha is not a
  silhouette and reading it as one would punch the road full of holes. Only a
  shape whose MATERIAL (`bAlphaTest` / `bAlphaBlend`) or whose `NiAlphaProperty`
  says so honours the texture's alpha. That is the clause that cuts out an
  alpha-tested road decal, and the refused texels are counted.

**The material is resolved by the last `materials/` in its path.** Every
`Landscape\\Roads\\Country\\*` and `\\Alley\\*` piece names its material as an
absolute Bethesda build path (`C:\\Projects\\Fallout4\\Build\\PC\\Data\\materials\\
Landscape\\Roads\\AsphaltAndSWEdgeDecals01.BGSM`) and carries an EMPTY texture
set. The road pass keys on the last `materials/` and resolves them; the shared
`lodgenLoadModel` still does not, which is a live defect for the OBJECT path and
is recorded as such.

### 1a.6 The channels touched

| sheet | touched | why |
|---|---|---|
| colour (role 1) | yes | measured: vanilla carries the road there |
| `_msn` normal (role 2) | no | measured: vanilla's `_msn` is the heightmap's on the road too |
| mask (role 5, RMAOS) | alpha only | the ground-cover byte is suppressed under the road; R/G/B untouched |
| emissive (role 6) | no | a road emits nothing |
| height (role 4) | no | the road is not the ground's height |
| retired `data` plane and the `.btr` `_data.DDS` | alpha only | the same cover byte, the same reason |

### 1a.7 The census

Chunk path: one `roads ...` line per chunk on stderr while the pass is on.
Pyramid path: the same fields in the `--vt` report line -- `roads`,
`roadPlacements`, `roadMeshes`, `roadShapeTiles`, `roadDecalShapeTiles`,
`roadTriangles`, `roadTexels`, `roadDecalTexels`, `roadAlphaRejected`,
`roadRefusedNoLoad`, `roadRefusedNoTexture`, `roadRefusals`. `roads 0` means the
switch was off; `roads 1 roadTexels 0` means it was on and this ground carries
none. `roadPlacements` and `roadMeshes` count the GATHER, which runs over the
region grown by two cells, so a region whose roads all sit in that margin
reports placements without texels -- measured on cells -20..-17 x 24..27:
`roadPlacements 32 roadMeshes 13 roadShapeTiles 0 roadTriangles 0 roadTexels 0`.

### 1a.8 The way back

`--no-roads`. The plane is never allocated, no `REFR` is read and no road model
is opened, so the bake is byte-identical to the bake before roads existed by
CONSTRUCTION -- measured: 9 of 9 files identical over the Sanctuary region.

"""

CLI_ANCHOR = ('| `--dump-cover FILE` | — | also write the raw 512\u00b2 u8 plane, '
              'north-up, headerless |\n')
CLI_ADD = (
    '| `--roads` / `--no-roads` | **on** | rasterise placed road meshes into the '
    'colour sheet (\u00a71a). On by default under both targets because vanilla does '
    'it; `--no-roads` is byte-identical to the bake before roads existed |\n'
    '| `--road-cover-suppress F` | 1.0 | how much of the ground cover a road '
    'removes under itself, 1 = all of it, 0 = leave the cover plane alone |\n')

PROV = """
### ROADS1, 2026-09-11

Section 1a is new. Its measurements come from Bethesda's shipped
`Commonwealth.4.-20.20.DDS` and `_msn`, from `Fallout4.esm`, and from the
scripts under `scratchpad/roads1_20260911/` (`esm_refs.py`, `rasterlib.py`,
`placements.py`, `matinfo.py`, `measure_vanilla.py`, `cmp_vanilla2.py`,
`cmp_vanilla3.py`, `family_auc.py`, `road_metric.py`); the after-state numbers
are re-derived from the artefacts, not copied forward.

| file | sha256 (16) | bytes | lines |
|---|---|---|---|
%s

| claim | line | anchor |
|---|---|---|
| \u00a71a.3 the road test | `lodgen.h:%d`, `lodgen.cpp:%d` | `bool lodgenIsRoadModel( const QString & modelPath );` / `bool lodgenIsRoadModel( const QString & modelPath )` |
| \u00a71a.4 the switch and the cover suppression | `lodgen.h:%d, %d` | `bool roads = true;` / `float roadCoverSuppress = 1.0f;` |
| \u00a71a.4 the road applied between VCLR and the tint, chunk path | `lodgen.cpp:%d` | `const quint32 rp = roadPlane[size_t( py ) * RES + px];` |
| \u00a71a.4 the same on the tile path | `lodgen.cpp:%d` | `const quint32 rp = roadPlane[size_t( j ) * S + i];` |
| \u00a71a.5 the topmost triangle wins, and what a texel gets | `lodgen.cpp:%d` | `void rasterise( float wx0, float wyTop, float upt, int S,` |
| \u00a71a.5 the material path rule | `lodgen.cpp:%d` | `QString lodgenRoadMaterialPath( const QString & matName )` |
| \u00a71a.7 the census fields | `lodgen.h:%d`, `lodgen.cpp:%d` | `struct LodgenRoadCensus` / `QString LodgenRoadCensus::line() const` |
"""


def lineof(path, anchor):
    lines = io.open(path, encoding='utf-8', newline='').read().split('\n')
    hits = [i + 1 for i, l in enumerate(lines) if anchor in l]
    assert len(hits) == 1, (path, anchor, hits)
    return hits[0]


def stamp(path):
    b = open(path, 'rb').read()
    return '| `%s` | `%s` | %s | %d |' % (
        path, hashlib.sha256(b).hexdigest()[:16], '{:,}'.format(len(b)),
        b.count(b'\n'))


def main():
    s = io.open(VT, encoding='utf-8', newline='').read()
    a = '## 2. The pyramid\n'
    assert s.count(a) == 1
    s = s.replace(a, SECTION + a)
    assert s.count(CLI_ANCHOR) == 1
    s = s.replace(CLI_ANCHOR, CLI_ANCHOR + CLI_ADD)

    rows = '\n'.join(stamp(p) for p in ('src/lodgen.cpp', 'src/lodgen.h'))
    prov = PROV % (
        rows,
        lineof('src/lodgen.h', 'bool lodgenIsRoadModel( const QString & modelPath );'),
        lineof('src/lodgen.cpp', 'bool lodgenIsRoadModel( const QString & modelPath )'),
        lineof('src/lodgen.h', 'bool roads = true;'),
        lineof('src/lodgen.h', 'float roadCoverSuppress = 1.0f;'),
        lineof('src/lodgen.cpp', 'const quint32 rp = roadPlane[size_t( py ) * RES + px];'),
        lineof('src/lodgen.cpp', 'const quint32 rp = roadPlane[size_t( j ) * S + i];'),
        lineof('src/lodgen.cpp', 'void rasterise( float wx0, float wyTop, float upt, int S,'),
        lineof('src/lodgen.cpp', 'QString lodgenRoadMaterialPath( const QString & matName )'),
        lineof('src/lodgen.h', 'struct LodgenRoadCensus'),
        lineof('src/lodgen.cpp', 'QString LodgenRoadCensus::line() const'),
    )
    s = s.rstrip('\n') + '\n' + prov
    io.open(VT, 'w', encoding='utf-8', newline='').write(s)

    p = io.open(PAR, encoding='utf-8', newline='').read()
    old = ("- Terrain texture bakes do not rasterize road meshes; vanilla's bakes\n"
           "  do (the Sanctuary loop road is plainly visible in vanilla's tile and\n"
           "  absent from ours). Needs top-down object rasterization.\n")
    assert p.count(old) == 1
    new = ("- CLOSED 2026-09-11 by lane ROADS1. Terrain texture bakes now\n"
           "  rasterize road meshes top-down into the colour sheet, as vanilla's\n"
           "  do, on by default under both targets (`--no-roads` is byte-identical\n"
           "  to the bake before it, 9 of 9 files). MEASURED on the Sanctuary\n"
           "  loop-road chunk (-20,20), which is the chunk that HAS the loop road --\n"
           "  chunk (-20,24), the tile the earlier note and lane TERRAIN-R's picture\n"
           "  used, contains zero road triangles: on the road centreline extracted\n"
           "  from vanilla's own sheet by colour, the fraction of texels within 16 of\n"
           "  255 of vanilla goes from 0.127 to 0.307 against a ceiling of 1.000 and\n"
           "  the surrounding ground's own 0.345, and the mean colour error on those\n"
           "  texels falls from 38.85 to 24.36 of 255. Whole-tile mean error 24.39 ->\n"
           "  22.81. Vanilla's `_msn` was measured NOT to carry the road, and ours\n"
           "  does not either. Still open: the road our bake paints is lighter and\n"
           "  less blue than vanilla's, which is the same splat-grading gap as the\n"
           "  next line.\n")
    p = p.replace(old, new)
    io.open(PAR, 'w', encoding='utf-8', newline='').write(p)
    for f in (VT, PAR):
        b = open(f, 'rb').read()
        print('%s  %d bytes  CR %d  LF %d' % (f, len(b), b.count(b'\r'), b.count(b'\n')))


if __name__ == '__main__':
    main()
