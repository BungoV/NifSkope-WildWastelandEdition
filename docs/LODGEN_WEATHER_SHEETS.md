# Lodgen bake data for weather (parked, 2026-09-16 12:01)

Status: PARKED ledger, not queued. bungo 2026-09-16: "biome dependant weather
sounds good", then "sounds good for all" (the five sheets + the cost table)
and basin depth below (water distance withdrawn: shipped already). Nothing here is a lane until he names one. FO4CS readers come
last by standing order; every sheet below is optional and the reader ignores an
absent one.

Why lodgen: the bake already holds the height field, the terrain normals, the
land-texture paint, the ground-cover density and every placed object. Each item
below is a rasterise pass over data in hand, written as one more optional sheet
beside the `.lodt` tiles (the `--vt-height` R16 sheet is the pattern). No
`.lodl`/`.lodo`/`.lodi` format change.

## The sheets

| sheet | per texel | from | what FO4CS does with it |
|---|---|---|---|
| **biome** (first) | u8 class | dominant LTEX, ground-cover density (mask A), water proximity (v3 water table), building density (`.lodi` cell counts), elevation | fog thickens over marsh/water, dust over open dry ground, shimmer over the Glowing Sea, less rain reach + drip under the urban canopy; weather stays global, the medium reads the class |
| **puddle / drainage** | u8 | local minima, slope, flow accumulation over the heightmap | rain wetness pools in ruts and hollows, dries from slopes first; Physical Weathers drives wetness and has nowhere to put this today |
| **terrain sky visibility / horizon** | u8 (instances already carry a sky byte; terrain does not) | height field + object footprints | snow and wetness stop under overpasses and against walls; far sky-light occlusion |
| **rain occlusion height** | R16 | top-down max of terrain + object heights | far precipitation and wetness stop under bridges and roofs without a runtime top-down render for the far ring |
| **wind exposure** | u8 | ridges vs sheltered valleys from the height field | sway amplitude and precipitation drift scale by it |

Already shipped that weather can use: v3 `.lodl` water bodies (mist over
water), `--vt-height` (fog height falloff against real far ground), instance
`sky` + `ao` bytes, per-vertex `sway` weight + per-instance `seed`.

Later `.lodl` item that helps valley fog: per-node hmin/hmax (DS2 pass-23
evidence, `reference_ds2_lod_evidence`), a version bump.

## Cost (estimates from the measured grids, 192x192 cells; baseline `.lodl` 34 MB / 6.8 s, `.lodt` 7.4 s)

| sheet | grid | raw | on disk | bake |
|---|---|---|---|---|
| biome, wind exposure, basin depth | 8 a cell | 2.4 MB each | <1 MB each | <2 s each |
| puddle, terrain sky visibility | 32 a cell | 37.7 MB each | ~19 MB each (BC4) | 3-10 s each |
| rain occlusion height R16 | 32 a cell | 75.5 MB | 20-40 MB | 10-30 s |

Peak bake memory +~300 MB (height grid as float + two working sheets). Coarse
sheets resident whole at runtime; fine sheets tile and stream with the `.lodt`.
Puddles at 32 a cell = 256 u texel: hollows yes, ruts no (ruts = runtime near
height). A rut-scale sheet at 32 u/texel would be 600 MB raw: rejected.

## Volumetric Air hooks (bungo 2026-09-16 "sounds good")

| sheet | per texel | from | what the froxel medium does |
|---|---|---|---|
| **basin depth** (cold pool) | u8, 8 a cell | fill-sinks depth over the heightmap: height of the basin's spill point minus the texel height; same flow pass as puddles | fog pools below the spill height and clears from the rim; the later `.lodl` per-node hmin gives the same floor per quadtree node in the far ring |

Water needs NO new sheet (bungo 2026-09-16 "we already have water depth, and
water distance?"): the `.lodl` per-cell table carries the water plane per cell
(depth = sample height - plane, a runtime subtraction), and v3 `--water-bodies`
carries a per-body wet-side shore distance (u8, 32 u a step, to 8,160 u). The
one gap is the DRY-side distance (how far land is from the nearest water): the
runtime gets it at cell rate from the per-cell has-water flags; a dry-side
chamfer at 8 a cell (<1 MB) only if cell rate proves too coarse for mist.

Runtime only, nothing to bake: exponential air density with height (scale
height), aspect (south slopes burn fog off first: from the normal sheet), fog
advection by wind exposure (already a sheet), far fog needs the ground height
per ray = the shipped `--vt-height` sheet.

## Boundaries

- Weather selection by position (which WTHR plays where) is a plugin job, not
  lodgen. Physical Weathers owns the WTHR side.
- Moving fronts are a runtime world-space field FO4CS animates; nothing to bake.
- Biome classes: proposed coast, marsh, forest, urban core, open ruin, Glowing
  Sea, water. Class list is bungo's ruling before any bake.

## Related parked rows (same day)

- **Enable groups in `.lodi`**: bake walks each reference's enable-parent chain,
  stores a u16 group per instance (parallel table) + per-chunk group table (root
  form ID, initial state, opposite-of-parent flag). Runtime flips whole groups
  (Prydwen at the airport). Version bump.
- **Cell-override `.lodl`**: a second file owning a cell set at every level,
  picked by a state flag (voidout crater case). Reader rule, no writer change.
