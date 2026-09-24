# BLENDEDGES1 -- deliverable text (director splices; lane edited no ledger)

Lane BLENDEDGES1, 2026-09-23. CLI only, no build. Exe `ns_run/NifSkope.exe` = `release/NifSkope.exe`
2026-09-23 07:54, 23,831,552 B, sha1 8d87c155. Fallout4.exe down. Output only under this folder.
Region: Sanctuary dim-4 chunk 4.-20.24 (cells -20..-17 x 24..27), TILING2's command line (`bake.sh`).

## Pictures
- `images/sanctuary_chunk_sheets_a_b_c_vanilla.png` -- stock chunk sheets, colour + normal, (a)|(b)|(c)|(d)
- `images/sanctuary_pyramid_a_b_c_vanilla.png` -- the .lodt pyramid's finest level, colour + normal
- `images/sanctuary_worst_line_zoom2x.png` -- 2x zoom on the worst line of (a) (horizontal, 6,144 u south of the chunk's north edge)

(a) current defaults; (b) + `--blend-edges quadrant`; (c) + `--blend-edges quadrant --msn-cache <Upscaled Terrain
Normals> --vt-finest 1 --vt-content 512`; (d) vanilla's shipped Commonwealth.4.-20.24.

## Job 1 -- what the hard lines are (logs_job1_*.txt, job1_steps.py)
Every straight line is a 2,048-unit quadrant line (64 texels). On VTBAKE1's L02 crop, the 6 steepest column
boundaries and the 6 steepest row boundaries are all quadrant lines. Across the lines, the step is 1.38x (x) and
1.73x (y) the average step. Vanilla reads 1.11x and 1.13x. Of the strongest 2% of steps, 4.2% and 7.6% sit on a
quadrant line (vanilla 1.6%, chance 1.6%). The 4-texel opacity grid shows no excess over vanilla once the quadrant
lines are taken out (1.087/1.092 vs vanilla 1.082/1.082; that shared residue is the 4x4 BC block grid). There is
nothing at 8, 16 or 32 texels either. The flag targets exactly these lines. The ragged blotch edges inside
quadrants are paint content, not a grid, and no flag addresses them.

## Seam (TILING2's instrument; mean/max over 14 lines, 1.0 = no line)
| | chunk sheet | pyramid dim 2 |
|---|---|---|
| (a) current | 1.236 / 1.784 | 1.250 / 1.792 |
| (b) + blend-edges | **0.977 / 1.099** | **0.992 / 1.106** |
| (c) + blend + msn-cache + finest 1 + content 512 | 1.036 / 1.155 (1024 px) | 1.037 / 1.155 (dim 1: 1.048 / 1.111) |
| (d) vanilla | 1.105 / 1.626 | -- |
On the worst line: 1.78 -> 0.98 (vanilla 1.04). (a) reproduces TILING2's 1.236 exactly. On (b), the per-line steps
fall back inside vanilla's range (x 1.10 vs 1.11, y 1.16 vs 1.13), and the strongest steps land on lines only at
chance (1.3% / 1.8%). No lines remain inside quadrants, so there is nothing to circle.
The flag changes only the colour DDS and the two .lodt containers. `_msn`, `_data` and `.lodm` stay
byte-identical.

## The upscaled-normals question
- msn-cache: **hit 1, miss 0, renorm 0**. The region holds one dim-4 chunk. The cache folder is only read
  (`lodgenMsnFromCache` opens files; nothing is written there).
- **Only the stock chunk `_msn` consumes it. The .lodt pyramid ignores it.** Measured: (c)'s VT.1 normal (east
  channel) correlates r 0.991 with (b)'s height-derived normal and only 0.580 with his sheet. (c)'s chunk `_msn`
  vs his sheet: r 0.996.
- Where the VT normal comes from: the tile builder in `src/lodgen.cpp` ~11395-11415. It takes a central
  difference of the heights on the 128-unit grid (`const float ngx = lx / 128.0f;` ... `out.msn[size_t( j ) * S + i]
  = lodgenTerrainMsnPixel( nrm );`). The cache is read only in `lodgenWriteChunkSheets` (~7684). No fix was
  built.
- Default (a)/(b) chunk `_msn` is vanilla's own bytes (the default copies it; `msnCopied 1`). So on the stock
  target, his sheet replaces vanilla's `_msn`, not ours.
- Side effect in (c): `--vt-content 512` makes the chunk colour 1024 px, not 2048, and `chunksShaded 0`. At that
  size the vanilla crevice shading of the colour is skipped (a size mismatch against vanilla's 512 `_msn`), so
  (c)'s colour is not just (b)'s at double resolution.
- Disk: (c) chunk sheets 24,467,168 B vs (a) 874,216 B (28x; the cached `_msn` is written uncompressed BGRA8
  2048 px with mips, 22.4 MB a chunk). Pyramid 14,712,609 B vs 945,123 B (15.6x). The exe's whole-Commonwealth
  pyramid estimate is 29.71 GB for (c) vs 2.22 GB for (a).

## Not done / not measured
The default is not flipped (brief job 4). There is no in-game look and no second tile. The (c) picture mixes three
switches, so its colour change is not attributable to any one of them.

## Mistakes (for root MISTAKES.md)
- 2026-09-23 BLENDEDGES1: two progress.md lines were stamped 09:15 and 09:24 from elapsed-time feel. The clock read
  09:12 afterwards. Found by the next `date`. Both lines are corrected in place. Rule already standing: read the
  clock, never type a time.

## Skill review
Loaded: nifskope-ww-lodgen. Missing: a "quadrant-seam / step-location" measure. `job1_steps.py` + TILING2's
`seam()` were re-typed from a lane folder. Written: none (candidate `ww-grid-step-locate` if a third lane needs
it). The skill's VT section should gain a line: `--msn-cache` feeds only chunk `_msn`, never the pyramid.
