# LODGEN vanilla parity audit — 2026-08-31

Chunk-for-chunk comparison of generated output against the base game's
shipped LOD files, run after the six-item completion round (SCOL, formID
remap, splat fixes, UV2, repetition breaking, atlas). Method: the new
CLI `verts` command dumps raw shape vertex positions (both sides share
the same miniature-space conventions, so raw is comparable), a grid-hash
nearest-neighbour comparator scores mutual coverage, and `info`/`get`
diff block anatomy and descriptors. Audit chunks: Commonwealth (0,0) at
dim 4/8/16/32 (harbor + downtown), (-20,24) dim 4 (Sanctuary, SCOLs +
trees + ponds), (0,-4) dim 4 (dense downtown), (-16,16) dim 16
(far-ring dropout).

## Matches — where it should

| What | Result |
|---|---|
| Land vertex desc | equals vanilla `52776558133763` at every ring (default profile) |
| Block anatomy | exact type-tally match at dim 4/8/16/32: Land BSTriShape + dim4-only segmented water BSSubIndexTriShape, far rings BSTriShape x2, one effect shader |
| Terrain surface | harness surface-height test: median |dz| 0.00 vs vanilla at shared sample points (authoritative; see divergence note on vertex sets) |
| Terrain density | per-CHUNK budget ~2100 tris on every ring, matching vanilla's measured 128/32/8/2 tris-per-cell falloff (ours 2267/2252/2280/2260 at 0,0) |
| Water rule | hasWater flag + resolved height (explicit XCLW else WRLD default) + exposure above the cell's terrain minimum. Harbor chunk 0,0: our 12 wet cells at height 450 equal vanilla's set EXACTLY (zero difference either way); Sanctuary: only the two explicit-height ponds, like vanilla |
| Object placement | after the euler-convention fix: downtown and dim16 medians 0.0u (p90 0.0–3.6u), 98–100% within 64u both directions — vertex-exact against vanilla. Sanctuary 9–10u median / 98–99% (residual = designed tree rotation+mirror). REFR world rotation = `fromEuler(-x,-y,-z)`: Bethesda's stored angles are negated relative to NifSkope's convention, proven per-object on multi-axis RockCliff refs (62% vs 14% vertex match) |
| Ring dropout | empty MNAM slot drops the object, per vanilla: dim16 (-16,16) ours 7,646v vs vanilla 7,122v (was 244,186v with slot substitution — now opt-in `slotFallback`) |
| Skirt | duplicated border ring dropped 1000 world units, harness-held invariant |
| SCOL contribution | Sanctuary tris 25,475 vs vanilla 24,482 (104%); was 41% before expansion |

## Divergences — where it should

- **CS profiles are opt-in extras**: `--terrain-identity` adds
  COLORS+UV2 (desc `686095322853893`), identity/AO/sway on objects,
  EyeData geomorph, manifests. Default output carries none of it.
- **Tree orientation**: position-stable hash rotation + U-mirror per
  tree (repetition breaking). Shows up as part of the residual NN
  distance on forest chunks; deterministic across regenerations.
- **Vertex sets differ under identical surfaces**: meshopt picks
  different triangles than Bethesda's decimator, so vertex-NN medians
  on terrain run 30–50u even where the surface test reads 0.00. The
  surface test is the correctness gate; vertex NN is only a sanity
  bound.
- **Atlas layout**: `--atlas` packs fixed 256² cells on one 4096×2048
  sheet; vanilla's sheet uses variable regions. Functionally
  equivalent, not byte-comparable.

## Known gaps — divergences that are NOT by design (open)

- Terrain texture bakes do not rasterize road meshes; vanilla's bakes
  do (the Sanctuary loop road is plainly visible in vanilla's tile and
  absent from ours). Needs top-down object rasterization.
- Splat grading: structure and orientation confirmed (identity best of
  8 transforms), luminance correlation ~0.55 downtown; vanilla's bakes
  look additionally graded/filtered.
- Water shape names/material bindings not audited beyond type and
  flags (vanilla water names are empty strings, same as ours).
- In-game load remains the only gate for engine acceptance — not
  claimable from file-level parity.

## Post-audit follow-up (same night)

bungo spotted a rotated highway in the screenshots that the aggregate NN
medians had hidden. Per-object orphan analysis (identity channel + the
extended `verts` dump) traced it to the REFR euler convention: angles
must be NEGATED into `fromEuler` (world R = Rx(-x)·Ry(-y)·Rz(-z)),
tested per-object against vanilla across four candidate conventions.
With the fix, object chunks are vertex-exact (medians 0.0u). Lesson
recorded in docs/MISTAKES.md: aggregate medians pass while individual
objects are wrong — verify per-element, and never extrapolate a
convention proven on one data source (SAM poses) to another (REFR).
Also fixed in the same pass: the tree classifier matched "sTREEt" and
spun street pieces (now TREE records / trees folder / tree-prefixed
names only), and the atlas now uses vanilla's exact naming
(`data\Textures\Terrain\<ws>\Objects\<ws>.Objects.DDS`).

Magenta shapes, FINAL correction (2026-08-31, second reversal): the
"CK-only textures" claim was a broken probe — the scratch BA2 tool's
hash lookup false-negatives, and a name-table grep finds every source
LOD texture in `Fallout4 - Textures6.ba2` and the vanilla atlas in
`Textures4.ba2`. Direct source-texture references are STOCK-LEGAL
(xLODGen ships this way); the direct-refs render is near-identical to
vanilla's. The `--atlas` pass is an opt-in draw-call optimization
(vanilla: 1 texture set per chunk vs our ~8 with direct refs), writes
its OWN name (`<ws>.LodgenObjects.DDS` — vanilla's name shadows the
archived sheet under every vanilla BTO still in play), BC3 with dilated
RGB. Full post-mortem: docs/MISTAKES.md 2026-08-31b.

## Vanilla clips object geometry below the terrain; we do not (2026-09-04)

bungo, looking at the rendered comparison: "the rocks are fuller in our lod, is
it because during LOD gen part of them under the terrain gets culled?" Yes.

Measured on Sanctuary (-20,24) dim 4, our rock vertices against vanilla's, then
each vertex against the chunk's own .btr surface:

| our rock vertices | count | below ground | median height |
|---|---|---|---|
| present in vanilla | 3696 | 46.0% | +12 |
| absent from vanilla | 796 | **97.7%** | **-424** |

So the 18% of rock geometry vanilla does not have is almost entirely buried.
Vanilla's generator drops it; ours keeps it, which is why our rocks read as
fuller and why this chunk is 104% of vanilla's triangles.

FIRST ANSWER WAS WRONG, and the reason is worth keeping: the orphan vertices
were first measured against each ROCK'S OWN Z range, where they sat at 0.18-0.62
of the height and looked like "the middle of the rock, so not a terrain cut". A
half-buried boulder puts its buried half in exactly that band. The reference has
to be the ground, not the object.

Per material, ours against vanilla, same chunk:

| texture | verts | vertex-exact in vanilla |
|---|---|---|
| ShackLOD01 | 201 | 100% |
| CommonwealthRockSlab01LOD | 1467 | 83.2% |
| CommonwealthRockSlab02LOD | 3044 | 81.9% |
| tree trunks and branches | ~30k | 0-4% |

The trees are near zero BY DESIGN -- the deterministic repetition breaking
rotates and U-mirrors every tree, so no vertex coincides. That is what makes the
global figure (86% of vanilla's vertices unmatched) meaningless on its own.

Two things follow. Shacks at 100% and rocks at 82% vertex-exact say vanilla's
object LOD is built from the same per-object `_LOD_N.nif` meshes we read from
MNAM -- not from precombined geometry, which would not coincide vertex for
vertex with the shipped LOD meshes. And a below-terrain cull is an open item for
us: it would cut ~18% of rock geometry with no visible change.
