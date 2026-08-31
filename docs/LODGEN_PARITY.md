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

Magenta shapes, the REAL story (corrected after checking archive
membership file-by-file with the BA2 name-hash tool): the source LOD
textures our BTOs reference (`textures\LOD\...`) are NOT in the shipped
game at all — absent from every base-game and DLC BA2 tested, and the
game folder holds no loose textures. They are CK LOD-GENERATION
resources; they exist in E:\Tools\Fallout 4\DataUnpacked (which carries
them) but a stock install cannot resolve them, in the viewer or
in-game. Vanilla never hits this because its pipeline consumes those
textures at generation time and ships only the baked atlas
(`Commonwealth.Objects.DDS`), which its BTOs reference.

CONSEQUENCE: for release-quality output the `--atlas` pass is
REQUIRED, not an optimization — or the referenced source textures must
ship loose with the output. Our --atlas writes vanilla's exact naming,
so atlased runs are fully resolvable on a stock install.
