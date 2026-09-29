## LOD map fixes merged: water, ground contact, tidy sheets, building groups, ground normals and sky, one-value tiles (lanes WATER1, GROUND1, TIDY1, IDENT1, TERR1, TILING5, FLAT2, GPU1; merged by MERGE1, 2026-09-29)

bungo reviewed the 79 LOD map pictures on 09-26 and called most of them broken; lane AUDIT1 listed the faults, and
these lanes fixed them. None of this has been flown in the game yet.

### Water in the landscape file (WATER1)
- LOD landscape file: water bodies are on by default (version 3). `--no-water-bodies` writes the old version 2.
  A default run falls back to version 2 by itself when the body step refuses a worldspace, and says so.
- In a version-3 file the cell "has water" flag now means water above the ground (it was set on every cell).
- Placed water that runs down a slope (rivers, falls) is baked with its real surface, not flattened to one height;
  the far-LOD viewer draws it sloped.
- Far-LOD viewer: water is drawn as flat water at its body's height, over the terrain, with bridges and dry ground
  left out; the water views (height, type, body, flow, shore, cell flags) colour the water surface and leave the
  ground as it is. Flow is a colour wheel. Each view prints its legend. The depth view shows body height minus ground.
- Far-LOD viewer: `WW_LODL_HEIGHT_RANGE=lo,hi` pins the water-height view's colour scale (for before/after
  pictures); unset = the old per-region stretch.
- Water LOD shapes carry no vertex colour any more (the vertex-colour depth bake was dropped at bungo's word).

### Ground contact per vertex (GROUND1)
- Distant objects now carry ground contact per vertex instead of one value per object, so a building fades from
  "in the ground" at its foot to "in the air" at its top instead of drawing as one flat grey.
- The `.lodi` moves to version 12 when it carries the new stream (Boston: +1.3 MB, about +25%).
- The viewer's `ground` channel shows the per-vertex values and says on its note line which source it drew.

### No black glow sheets, one layer per identical texture, honest labels (TIDY1)
- A texture or card array whose glow (emissive) is black on every layer now writes no `_g` file, and its `.lodm`
  names no emissive. Boston: 21 files, 50 MB saved; the installed 09-25 whole-map bake carried 90 MB of them.
  The aspen tree cards really glow a little and keep theirs.
- Layers whose texels are identical merge into one (Boston 114 -> 106 layers; whole map 399 -> 371).
- Viewer labels: the colour plane is "Terrain vertex tint"; cell range says it is a culling table, one value per
  4096-unit cell; ground cover says it is Fallout 76 only.
- Gate-only ways back: `WW_LODGEN_KEEP_BLACK_EMISSIVE=1`, `WW_LODGEN_NO_LAYER_DEDUPE=1`.

### Object groups and occluder boxes per building (IDENT1)
- New default grouping of distant objects: pieces whose placed triangles come within 32 units join one group,
  and no group may grow wider than 4,096 units (`--identity-join contact`). `--identity-join proximity` gives
  back the old files byte for byte.
- One occluder box per building, fitted to the building and checked against its triangles
  (`--occluder-fit building`, default); `--occluder-fit piece` is the way back.
- Whole Commonwealth: 21,140 groups, 511 boxes; street coverage 0.029 -> 0.560.
- Viewer: the `identityraw` channel is renamed `placement-lowbyte`; the captions say what each channel draws.
- Known open: Diamond City splits into 15 groups, the terraced row houses weld into 1, and 5 of 511 boxes poke
  more than 1% out of their building. These need bungo's ruling (a landmark join rule), not a code fix.

### Ground normals carry roads and rails; ground sky sees buildings (TERR1)
- The far terrain's normal sheet now carries the normals of flat objects painted into the ground colour (roads,
  rails, slabs, kerbs, decals), so rails and kerbs catch the light.
- The ground's sky value now sees buildings, walls and bridges: street canyons and ground under bridges go darker,
  by a cosine law checked against a ray cast (canyon mean 87.4 against physical 98.8).
- Ways back (command line): `--no-stamp-normals`, `--no-sky-objects`.

### Land blend by height and large-scale colour variation, both OFF (TILING5)
- `--land-height-blend on` and `--land-macro on` (or `--land-sample relief` for both) are new and OFF by default.
  They did not reach their own targets; kept for later work. Off = the old bytes.

### One-value terrain tiles stored small (FLAT2)
- Terrain texture files (.lodt): a tile's sheet that is a single value over the whole tile is stored as one
  16-byte value instead of the full sheet (open sea and the empty border: about 30% of the whole-map bake).
  Nothing the viewer draws changes. `--no-collapse-uniform` writes every sheet in full, as before.
- **The installed bake is made WITH `--no-collapse-uniform`** until FO4CS can read the small form.

### Faster bake (GPU1)
- LOD bake: much faster identity join (Boston box 109 s -> under 1 s) and card dilate; same output.
- LOD bake: card normal sheets are BC7-encoded on the GPU when one is available (OpenGL 4.3), about 2.3x faster
  than all CPU cores on the encoder alone; equal or better quality on the encoder's own error measure. On by
  default; Settings > NIF > LOD bake > Use GPU turns it off; `lodgen --no-gpu` for one run.

### FO4CS readers owed (FO4CS is built last, by standing order)
The installed bake carries `.lodl` version 3 (water bodies, 0x100 header) and `.lodi` version 12 (ground contact
per vertex), and texture sets with no emissive named. FO4CS's current readers refuse `.lodl` v3 and `.lodi` v12 until
their reader changes land (lists in HANDOFF, lanes WATER1 / GROUND1 / TIDY1 / FLAT2).
