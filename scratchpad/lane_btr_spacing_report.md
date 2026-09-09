# Lane BTRSPACING — terrain LOD geometry resolution, measured

Question (bungo): *"what is the resolution of each LOD chunk, versus the actual
geometry up close in cells"*. The texture half was already known (a 512 sheet per
chunk, so 32/64/128/256 units per texel at levels 4/8/16/32). This lane measures
the GEOMETRY half from Bethesda's own shipped `.btr` meshes.

## 1. Method and files

**Corpus (never a mod folder):**
`E:\Tools\Fallout 4\DataUnpacked\Data\Meshes\Terrain\Commonwealth\Commonwealth.<level>.<x>.<y>.BTR`
— 2304 files at level 4, 576 at 8, 144 at 16, 36 at 32 (3060 total, **all parsed,
none skipped**).

**Ours (the only regenerated `.btr` on disk), both matched to the vanilla tile:**

| ours | vanilla control | control check |
|---|---|---|
| `scratchpad/mountains_20260907/images/ours4_fix/Commonwealth.4.-60.36.BTR` (13:42) | `.../images/van4/Commonwealth.4.-60.36.BTR` (13:02) | `cmp` vs the shipped file: **byte-identical** |
| `scratchpad/mountains_20260907/images/ours16/Commonwealth.16.-64.32.BTR` (13:11) | `.../images/van16/Commonwealth.16.-64.32.BTR` (13:02) | `cmp` vs the shipped file: **byte-identical** |

No regenerated set exists at levels 8 or 32, so the "ours" column below is blank
there. (`scratchpad/lodt_20260909/` holds `.lodt` planes, not meshes;
`scratch_water/` holds water-lane experiment chunks, not a level ladder.)

**Instruments**, all in `scratchpad/btr_spacing_20260909/`:

* `btrparse.py` — exact BSTriShape reader. Finds each geometry header by the
  PHANTOM4 signature (`<IHI>` = numTriangles u32 / numVertices u16 / dataSize u32,
  with `dataSize == numVertices*vertexSize + numTriangles*6`) and reads the
  `BSVertexDesc` in the 8 bytes before it. **Validated against
  `nifskope-cli dump -b 1` on Commonwealth.4.0.0.BTR: 4215 triangles, 11713
  vertices, dataSize 165846, desc 52776558133763 — all four match**, and the first
  four positions match `nifskope-cli verts` line for line. It is used instead of the
  CLI text dump because half-float positions must not go through a 6-significant-digit
  print before their gaps are differenced.
* `spacing.py` — per-chunk measurement (columns, extent, gaps, grid membership,
  triangle edges, 4×4 tile breakdown).
* `survey.py` — the same over the whole worldspace, per level.
* `analyse.py` — the first, superseded pass over `nifskope-cli verts` output (kept
  because Mistake 1 below is in it).

**The two conventions the numbers depend on, both read from the files, not recalled:**

1. A `.btr` land shape stores positions in *miniature space*; the `BSTriShape`'s
   world **scale equals the level** (`nifskope-cli world` prints S=4/8/16/32 for the
   four (0,0) tiles, and S=4/S=16 for both ours). World position = local × level.
   Chunk extents therefore come out at exactly 16384 / 32768 / 65536 / 131072 units
   = 4 / 8 / 16 / 32 cells of 4096.
2. Vertex format is `desc 0x300000000203` on every chunk measured: 12 bytes,
   `VF_VERTEX | VF_UV`, **full precision OFF** — positions are half floats. Every
   multiple of `128/level` in local space is exactly representable at these
   magnitudes, so grid membership is tested at zero tolerance and is honest.

**The measurement of "spacing" that is used.** A vanilla chunk is *not* a regular
grid, so the gap between adjacent sorted unique x values is not the vertex spacing —
it is dominated by whichever pair of rows happens to be closest anywhere in the
chunk (see Mistake 1). The per-axis spacing quoted is derived from the area a chunk
covers and the number of distinct (x,y) columns in it:
`spacing = 4096 / sqrt(columns per cell)`. The sorted-unique-gap distribution is
reported separately, as the shape of the mesh rather than its resolution.

Populated chunks only (>100 columns): 894 of 2304 level-4 chunks, and 217 of 576 at
level 8, are open water or dead flat and collapse to **9 columns / 24 triangles** —
including them would report the ocean, not the terrain.

## 2. The table

Vanilla, median over every populated chunk in the worldspace (1230 / 317 / 82 / 31
chunks at the four levels):

| level | cells per chunk | chunk extent (units) | stored verts | distinct xy columns | triangles | spacing (units) | samples per cell per axis | vs LAND 128 |
|---|---|---|---|---|---|---|---|---|
| 4  | 4×4 = 16      | 16 384  | 1060 | **1000** | 2058 | **518**  | 7.91 | 4.05× coarser |
| 8  | 8×8 = 64      | 32 768  | 1066 | **1000** | 2063 | **1036** | 3.95 | 8.10× coarser |
| 16 | 16×16 = 256   | 65 536  | 1097 | **1000** | 2092 | **2072** | 1.98 | 16.19× coarser |
| 32 | 32×32 = 1024  | 131 072 | 1233 | **1000** | 2196 | **4145** | 0.99 | 32.38× coarser |
| — near geometry (LAND) | 1 | 4 096 | — | 33×33 = 1089 | 2048 | **128** | 32 | 1× |

Ours, on the one tile per level where a matched regenerated file exists:

| level | tile | | distinct columns | triangles | spacing | samples/cell/axis | vs LAND |
|---|---|---|---|---|---|---|---|
| 4  | (−60,36) | vanilla | 1000 | 2084 | 535  | 7.66 | 4.18× |
| 4  | (−60,36) | **ours**| 1106 | 2341 | 508  | 8.06 | 3.97× |
| 16 | (−64,32) | vanilla | 1000 | 2091 | 2140 | 1.91 | 16.72× |
| 16 | (−64,32) | **ours**| 1099 | 2313 | 2038 | 2.01 | 15.92× |

Ours is ~10% denser than vanilla on both tiles, on the same 128-unit grid (100% of
our columns are on it), in the same 12-byte half-float vertex format, at the same
scale convention. Ours spreads its vertices more evenly: quarter-tile column counts
vary 1.7× / 1.8× across our chunks against 4.0× / 3.1× across vanilla's.

### What the table says in one line

**Vanilla gives every terrain LOD chunk the same budget at every level — 1000
distinct columns and ~2100 triangles — regardless of how much ground the chunk
covers.** 89% / 95% / 94% / 84% of populated chunks at the four levels sit inside
950–1050 columns. Because the chunk's extent quadruples with each level while the
budget does not, the spacing follows exactly:

> **vertex spacing = 128 × level units**  (512 / 1024 / 2048 / 4096, measured 518 /
> 1036 / 2072 / 4145) — i.e. the LOD keeps **one in every `level` LAND samples per
> axis**, and level 32 is exactly **one vertex per cell corner**.

Against the texture sheet, which is 512×512 per chunk at every level: the geometry
budget is ~32×32 per chunk at every level, so there are **16 texels per vertex edge,
256 texels per vertex quad, at every LOD level** — the ratio never changes, only the
world size of both.

## 3. Anything non-uniform

**Yes, in four separate ways. None of these is a regular grid.**

1. **The mesh is adaptive, not a grid.** The budget is fixed, so detail is spent
   where the ground is rough. Sorted-unique gaps at level 32 run from 8 to 1664
   units with a mode of 128; triangle edge lengths on the (0,0) chunks run
   2 → 4138 units at level 4 and 128 → 39 550 at level 32 (medians 277 and 3442).
   Quarter-tile column counts within a single chunk vary by 2.5×–16×.

2. **Grid alignment is near-total but not total.** Columns sit on the 128-unit LAND
   grid in 2292 of 2304 level-4 chunks, 558 of 576 at level 8, 131 of 144 at 16 and
   32 of 36 at 32 — 100% exactly. The rest carry a **cell-seam band**: a *pair* of
   vertex rows straddling every interior 4096-unit cell boundary at ±2 units in
   miniature space (±8 world at level 4, ±16 at level 8), whose along-seam positions
   are arbitrary (23.625, 25.14, 53.06 …). Those chunks also carry roughly double
   the budget. This is a minority feature: 12 chunks at level 4, 18 at level 8, and
   essentially none at 16/32 (worst case 87.9% and 94.6% aligned).

3. **The tile the brief named is the single worst outlier in the worldspace.**
   `Commonwealth.4.0.0.BTR` is the *least* grid-aligned level-4 chunk of all 2304
   (50.5%) and the densest (1982 columns / 4215 triangles — twice the budget), and
   it is stored as a near-unindexed triangle soup (11713 stored vertices for 1982
   columns; the median chunk stores 1.06 vertices per column). Every headline number
   above is therefore the corpus median, not that tile.

4. **Water is a separate shape on a separate grid.** Each `.btr` carries a second
   `BSMultiBoundNode 'WATER'` with its own shape, on a **4096-unit grid — one vertex
   per cell corner at every level** (5×5 columns at level 4, 33×33 at level 32),
   mostly flat at z = 450. It is not part of the land resolution and is excluded
   from the table.

5. **Half the worldspace is empty at the near levels.** 894 of 2304 level-4 chunks
   and 217 of 576 level-8 chunks are 9 columns / 24 triangles.

Raw outputs: `vanilla_spacing.txt` (the four (0,0) tiles in full),
`ours_spacing.txt` (the matched pairs), `corpus_survey.txt` (all 3060 chunks),
`vanilla_raw.txt` (the superseded first pass).

## 4. Mistakes

Both are also appended at the top of `MISTAKES.md` at the repo root.

1. **Took the minimum gap between sorted unique coordinates as the grid step.**
   `analyse.py` reported the level-4 chunk as a "13129 × 13129 grid at step 1.248
   units, 0.0% occupied". On an adaptive triangulation the smallest gap anywhere in
   the chunk is not a step, and the number is meaningless — 172 million grid points
   for a 4215-triangle mesh. Found by disbelieving the absurd occupancy figure and
   re-deriving spacing from area ÷ column count instead. Rule: **on an irregular
   mesh, spacing is columns over area; a gap histogram describes the mesh's shape,
   never its resolution** — and a derived figure that implies an impossible
   magnitude is a broken instrument, not a finding.

2. **Generalised from the tile at the origin.** The first three measurements were
   all taken on the `(0,0)` tiles the brief named, and level 4 there is 50.5%
   grid-aligned with double the vertex budget and a triangle-soup layout — which
   led to a written-down belief that vanilla terrain LOD carries sub-LAND-grid
   detail. It does not: `Commonwealth.4.0.0.BTR` is the least grid-aligned and
   densest of all 2304 level-4 chunks. Found only by surveying the whole worldspace
   (`survey.py`), which was not in the plan until the level-4 result disagreed with
   levels 8/16/32. Rule (CONSTITUTION 4, "the whole corpus, not a sample"): **a
   per-tile measurement is not a per-worldspace fact until the corpus is swept, and
   the cheapest sweep goes first, not last.** Sweeping 3060 files cost 90 seconds;
   believing one tile cost two wrong conclusions.

## 5. Finished-work skill review

**Loaded:** `nifskope-ww-lodgen` (the CLI surface, the one-instance rule, the
"measured facts" block that supplied FO4 LAND = 33×33 VHGT at 128 units so it did
not have to be re-derived), `nif` (which is where `nifskope-cli verts` / `world` /
`dump` came from, and where the PHANTOM4 `<IHI>` signature that `btrparse.py` is
built on is written down — that note alone saved the lane a half-float/uint16
header hunt).

**Written this session:** none yet — see below; the decision is bungo's or the
director's, because the procedure spans two skills that already exist.

**The missing skill, named.** Everything in this lane that was worked out from first
principles is one procedure: *measure a terrain or object LOD chunk's geometry
against vanilla's*. Its parts, all re-derived here:

* the miniature-space convention (world = local × the shape's scale, which equals
  the level) — nothing in either skill says this, and getting it wrong scales every
  extent and every spacing by 4× to 32×;
* `desc & 0xF` × 4 = vertex size, `(desc >> 44) & 0x400` = full precision, and
  therefore whether positions are half floats;
* distinct **columns**, not stored vertices — a vanilla chunk can store 11713
  vertices for 1982 positions, so any per-chunk vertex figure taken from the header
  is wrong by up to 6× without warning;
* spacing = `4096 / sqrt(columns per cell)`, and the gap-histogram trap that is
  Mistake 1;
* excluding the 9-column empty chunks and the separate WATER shape before quoting a
  median;
* the byte-identity control (`cmp` the "vanilla" copy in a comparison folder against
  the shipped file) before comparing anything to it.

Recommendation: fold this into `nifskope-ww-lodgen` as a **"Measuring a chunk"**
section rather than a new skill, since it shares that skill's corpus paths, CLI and
one-instance rule, and carry `btrparse.py` in `tools/` as the reader it points at.
It will recur the moment ours-vs-vanilla geometry is compared again, which the
hybrid LOD ladder guarantees. Not written here because the lane's rules forbid
writes outside the report and `scratchpad/btr_spacing_20260909/`.
