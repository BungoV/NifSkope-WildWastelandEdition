# Lane TILING5 -- LOD terrain: height-aware blending + large-scale variation

Worktree `E:\Projects\NifskopeWWE-tiling5`, branch `tiling5-20260927` from night-trial @ 5b338d39.
Rung = this worktree's first build, `release/NifSkope.before_tiling5.exe`, 11:54:04, 26,074,624 B,
sha1 a94745fd. Objects copied from sibling NifskopeWWE-night (same commit, `make -n` 0 g++ lines).

## 1. Skills loaded

nifskope-ww-lodgen, nifskope-ww-worktree-build (section 5b path: sibling objects, 57 objects, rc 0).
More are added below as they are loaded.

## Finding before any work: the brief's "SHIPS OFF" is stale

The brief says TILING4's hex tiling ships OFF behind `--land-sample stochastic`. The tree says otherwise:
since 2026-09-12 (lane DEFAULTS1, bungo's pick) the DEFAULT land look is hex 256 + guide `flatwarp:1.0`
with warp 341 + mip bias -0.22 (`src/lodgen.cpp` g_landHexSize = 256, g_landWarpAmp = 341,
g_landGuideRule = FLATWARP; `src/nifcli.cpp` usage "DEFAULT 256 since 2026-09-12").
`--land-sample stochastic` today means hex 256 with the warp forced to 0 -- a DIFFERENT look from the
default. So "current hex" in this lane's gate table is the shipped default, and the new work is its own
switch stacked on whatever sampler is active (section 3 of the code notes says why).

## 2. Height source: height integrated from each texture's own normal map

Measured by `m1_height_source.py` (log `logs/m1_height_source.txt`, numbers `m1_height_source.json`): every land
texture the Commonwealth paints, weighted by the ground it covers, 70 of 100 textures measured = 18,607 of 20,050
coverage weight (the 30 skipped are material-backed sets the Python model does not resolve; the C++ reads them).
Reference = the relief the artist drew, integrated back out of the normal map (Frankot-Chellappa, periodic).

| candidate | coverage-weighted median corr with the relief | note |
|---|---|---|
| diffuse luminance | 0.196 raw, 0.17 low-passed | bright is NOT high; would also bias every transition toward the brighter texture |
| diffuse alpha | 0.314 (where it varies) | flat on 20.7 % of coverage: no signal there at all |
| luminance vs slope (cavity) | 0.027 | no relation |
| **normal-integrated height** | (the reference) | exists for every texture with a normal map; integrability residual median 0.54 |

Neither free candidate reaches 0.35, so the bake integrates height from the normal map itself, once per texture
(C++ FFT at <= 256 texels, per-mip unit-SD pyramid, thread-safe cache). A texture without a normal map gets h = 0,
which reduces the height blend exactly to today's linear crossfade for that pair.

## Rule slip (recorded the moment it happened)

One source patch (`#include <complex>`, a pi constant replacing M_PI) went through a Python heredoc instead of
the Edit tool, against the night rule. Checked afterwards: exactly the three intended replacements, LF-only file
unchanged in line endings. All other source edits use Edit.

Second slip (12:35): a one-word range change in my own picker script `t5_pick_rural.py` went through `sed -i`
instead of Edit. Scratch script, not source; the diff is the two `range(-40, 40 - 11, 4)` bounds and the comment.

## 3. Gates (filled in as measured)

### 3a. Off = the rung's bytes (Boston box -8,-12..3,-1, dim 4, `--vt --cover`)
Rung `run_rung` (sha1 a94745fd, this worktree's first build = night-trial @ 5b338d39) against the new exe
`run_new` (sha1 514096ef), no new switch: **terrain sheets 27 of 27 identical** (colour, `_data`, `_msn`), VT
`.lodt`/`.lodm` 3 of 3 identical, 27 `.BTR`/`.BTO`/manifests identical. The only differing bytes are the run
folder's own path in `flat_objects_report.txt` and the `.lodb` ledger's exe size / time / out path -- its switch
list is identical (the two new ledger keys are written only when ON). **PASS.**

### 3b. Macro field vs the hex patches (`t5_hexcorr.py`, log `logs/hexcorr.txt`)
The hash, hex cell, hex offsets and macro fBm re-implemented term by term in numpy. Pearson r of each macro channel
against the hex offsets: (a) at 360,000 lattice vertices, bound 4/sqrt(N) = 0.0067: largest |r| 0.0043;
(b) at 400,000 random Commonwealth points against the dominant vertex's offsets, the weighted offsets and the
max weight, bound 0.0063: largest |r| 0.0031. Control (a macro that reused the hex's key 0 on the hex lattice):
r = +1.00000, refused as it must be. Channels against each other: |r| <= 0.060. **PASS (0 of 21 fail).**

### 3c. Rural camera, picked by measurement (`t5_pick_rural.py`, log `logs/pick_rural.txt`)
Chunk-aligned 12x12-cell windows inside +-40 cells, >= 95 % dry, not overlapping Boston, ranked by the SD of cell
mid-heights: -36,-40 (3723) and -36,-36 (3709) are the far south-west (the Glowing Sea side, one blasted palette);
**-36,4..-25,15 (3549, dry 1.00)**, the west-central hills, is the rural camera.

### 3d. Layer-transition gate, today's bake FIRST (`t5_gates.py trans`, pre-registered in its docstring)
rz = SD(r=2 high-pass) in the transition zone / the same in the layer interiors; PASS per sheet rz >= 0.9 x vanilla.
**Today's default bake: 7 of 14 sheets pass -- red, as a crossfade must read** (worst -36,-20 0.569 vs vanilla 1.050,
-4,-20 0.623 vs 0.991, -20,20 0.667 vs 0.910). Vanilla's own sheets read 0.645-1.063 (median 1.0): its transitions
carry as much grain as its interiors; ours average two grains away.

### 3e. First height arm, beta 2.0 (the code's first default): overshoots
`height` = `--land-height-blend on`: transition gate 14 of 14, but rz 1.05-2.59, and TILING4's grain gate goes red
(G1 +49.5 % / G2 1 of 7 on selection). `t5_split.py` puts the whole change in the zone: median hp SD in Z 4.84 ->
9.09 (vanilla 5.39), interiors 6.58 -> 6.81 (vanilla 5.53). Beta 2 turns the transitions into a per-texel dither of
two textures. The sweep below finds the beta where the zone's grain matches the interior's.

### 3f. First macro arm (amplitudes 0.06 / 0.05 / 0.06, applied before VCLR): the hard line broke once
`relief` (= height + macro) against `height`: mean HSV saturation 13 of 14 sheets >= unmodified, **-36,-20 lower by
0.00245** (0.26069 -> 0.25824). The per-texel hold was exact where it ran, but VCLR (a tinted multiply), the road
lerp, the grass tint and the shading ran AFTER it, and none of them keeps an HSV-saturation order. Fix: the macro
is now the LAST colour step in both writers (after the grade, before quantisation), and the hold compares against
the clamped colour the texel would store. Re-measured below.

### 3g. Macro amplitude from vanilla, per band (`t5_band.py`)
The pre-registered within-sheet gate (`t5_gates.py macro`, large-scale lum SD median <= vanilla's) is ALREADY red on
today's default bake, before any macro: FROZEN14 median 7.383 vs vanilla 4.988, BOSTON9 7.802 vs 4.787. So the
licence is read per band as "what vanilla has that today does not", L = sqrt(max(0, van^2 - today^2)), on a
contiguous 3x3-sheet mosaic reduced to 15 m blocks: band A 60-234 m (box r2 - box r8), band B 234-700 m (SD of the
nine sheet means). Brightness on log luminance, hue/saturation on the opponent axes over the mosaic's mean chroma.

Boston mosaic, today (`logs/band_id_rung_boston.txt`):

| band | vanilla | today | licence |
|---|---|---|---|
| log-lum A | 0.0741 | 0.1065 | **0** |
| log-lum B | 0.0936 | 0.0978 | **0** |
| chroma A | 1.931 | 1.864 | 0.502 |
| chroma B | 1.381 | 1.357 | 0.258 |

Mean chroma: vanilla 10.6, today 19.0. The field's own band SD (64 windows): 0.057 (A), 0.136-0.155 (B).
**Vanilla licenses no brightness variation on top of today's bake** -- today already carries more large-scale
brightness variation than vanilla in both bands. Hue alone could go to 0.098 rad, saturation alone to 0.175.

Rural mosaic -36,4..-25,15, today (`logs/band_today_rural.txt`): log-lum A 0.0554 vanilla / 0.0528 today (licence
0.0167), B 0.0462 / 0.0390 (0.0249); chroma A 1.044 / 1.448 (**0**), B 0.715 / 2.359 (**0**). The two places
disagree channel by channel: Boston has no room for brightness, the rural hills none for colour.

### 3h. Beta sweep on the whole-colour height blend, and the redesign it forced
| arm | beta | transitions (of 14) | G1 sel / val | G2 sel / val | worst per-sheet grain vs today |
|---|---|---|---|---|---|
| today | -- | 7 | +11.8 % / -7.7 % | 7/7 / 7/7 | -- |
| b05 | 0.5 | 8 | +16.0 % / -2.9 % | 7/7 / 7/7 | +18.5 % (20,-24) |
| b10 | 1.0 | 13 | +23.5 % / +4.8 % | 6/7 / 5/7 | +48.2 % (20,-24) |
| height | 2.0 | 14 | +49.5 % / +17.2 % | 1/7 / 3/7 | +115.8 % (20,-24) |

No beta passes both: the grain gates allow <= 0.5, where the transitions barely move (8 of 14). The reason is in
the zone: the height opacity dithered the two materials' MEAN colours per texel, and on 20,-24 (two smooth
materials of different brightness) that mean edge is almost all of the added high-pass. Redesign
(`lodgenLandHeightLayer`): each sample splits into its texture's repeat average (1x1 mip) + detail; the averages
crossfade with the painted opacity exactly as today, only the details take the height opacity. Algebra:
c' = c + (lc - c) ah + (ml - m)(a - ah); ah = a is today's blend. Arms s20 / s10 below.
