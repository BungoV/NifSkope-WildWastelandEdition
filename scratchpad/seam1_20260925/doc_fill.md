### 2.6 The vanilla-colour fill (lane SEAM1, 2026-09-25) -- `--vt-fill-vanilla`, OFF by default

**What it is for.** Most of the Commonwealth's LAND paints nothing: of its 2,304 dim-4
chunks, 2,023 carry LAND with no BTXT on any quadrant and no ATXT layer. §2.5 step 4
paints those with the engine's one default land texture (lane SEAM1's first commit).
That texture is right in kind and wrong in colour: Bethesda's own LOD sheets for the
same cells carry each region's colour (the Glowing Sea is ~49 luminance, the
north-east ~90), baked outside the Creation Kit and not recoverable from the ESM
(§2.5b). bungo, 2026-09-25: blend the terrain the in-game blended tiles do not cover
to the vanilla colour on those tiles, "in a proper way".

**The law, per finest-level texel, after §2.5's whole composite:**

    painted cell  a LAND quadrant with a BTXT or any ATXT layer (a NULL-LTEX layer counts)
    d             world distance from the texel to the nearest painted cell (0 inside one)
    w             smoothstep(0, band, d); w = 0 on a painted cell, so its texels are untouched
    colour        colour + (T(V) - colour) * w          (RGB, 0..1; alpha untouched)
    V             Bethesda's dim-4 LOD diffuse, Mitchell-Netravali bicubic, B = C = 1/3
    T             tone + saturation match fitted on the overlap (below)

* **V is read as a loose file under `--vanilla-lod-root`**, through
  `lodgenReadVanillaSheet`, never through the resource stack -- §2.5b's reason:
  the stack would serve our own installed output. `<root>/Textures/Terrain/<WS>/<WS>.4.<x>.<y>.DDS`,
  (x, y) the chunk's SW cell, 512 texels = 32 units a texel, row 0 = north. A
  replacer at 1024 or 2048 is read at its 512 mip; any other size is refused and
  counted. The file is READ, never written, copied or shipped.
* **T is fitted once a bake** on the OVERLAP: painted cells with an unpainted cell
  within 3 cells (Chebyshev). Cell means on both sides -- ours from a 16-texel
  bake of each overlap tile through `lodgenBakeVtTile` itself (so the fit sees the
  whole composite: roads, tint, grade), vanilla's from the mean of its 128 x 128
  texels a cell. Luminance (Rec. 709 weights) gets an offset and a gain CAPPED AT
  1; chroma (rgb - lum) is scaled by the RMS ratio and shifted by the mean-chroma
  difference. The cap is measured: an uncapped gain (1.34 on Sanctuary north)
  amplified vanilla's baked relief light to 8 cell steps over the bar against
  vanilla's own 2.
* **The bar** = p99 of vanilla's own adjacent cell-mean luminance steps over the
  overlap and the ring (the unpainted cells within 3 cells of a painted one).
  **The band** = ceil(p95 over the ring of |lum ours - lum T(V)| / bar) cells,
  at least 1, at most 8.
* **Where it runs.** Every finest-level tile, right after its bake, colour plane
  only. Coarser levels, their mips and the assembled `.btr` chunk sheets inherit it
  through the existing box filter (§2.3, §2.4). A tile whose cells and one-cell
  ring are all painted is skipped whole.
* **The census line** (report, only when asked): `vanillaFill overlapCells= ringCells=
  fitTiles= gain= rawGain= offset= sat= cshift= bar= p95= bandCells= tilesTouched=
  texelsFilled= texelsNoVanilla= vanillaChunksMissing= vanillaSheetsRead= root=`.
  With fewer than 2 overlap cells to fit on, the line says the fill is off and why.

**What is pinned.** Off is the bake before the fill existed, byte for byte (the
switch is branched over). On: every role but colour is byte-identical on every tile,
and colour too on every fully painted tile (gate FG2,
`scratchpad/seam1_20260925/fill_gate.py`).

**The offline proof, before the build** (`scratchpad/seam1_20260925/fill_model.py`,
the same definitions; A = the bake before SEAM1, B = §2.5 with the engine default,
F = the fill, V = vanilla; cell-mean border steps between painted and unpainted
cells, and the step AT the line against a line bar from vanilla's own at-line steps):

| region | bar | band | A max (over bar) | B max (over) | F max (over) | F at-line max / line bar |
|---|---|---|---|---|---|---|
| Sanctuary north, cells -32..-10 x 22..34 | 12.38 | 1 | 19.00 (4 of 28) | 12.53 (1) | 12.78 (1) | 9.86 / 10.15 |
| Glowing Sea edge, -44..-24 x -44..-26 | 20.56 | 2 | 76.22 (6 of 31) | 19.41 (0) | 17.82 (0) | 9.81 |
| north-east, -4..16 x 20..36 | 10.86 | 2 | 23.63 (1 of 28) | 15.78 (2) | 15.43 (1) | 9.81 |

The fill never adds a step over the bar that B does not already have, and inside
the band its steps between two unpainted cells stay within vanilla's own (Sanctuary
north p95 8.25 against vanilla's 9.88). **These are model numbers; the C++ is BUILD
PENDING** and its gate reads the baked file (`VT2=<fill bake> fill_model.py ...`,
A = the file, plus the file-vs-model agreement line).

**What it does not do.** It does not touch a painted cell, the normal, the mask,
the height or the emissive. It does not follow a plugin that reshapes terrain:
vanilla's colour belongs to vanilla's ground, so a worldspace whose heights moved
should leave it off (as `vanilla-blend` exists for the normal, §2.5d).

