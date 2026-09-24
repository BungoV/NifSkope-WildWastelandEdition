---
name: ww-sheet-diff
description: Measure and explain a DIFFERENCE between two generated terrain or card sheets in the NifSkope Wild Wasteland tree — how far from a chunk border a change reaches, whether it stayed inside the band its consumer can reach, and, when it did not, whether the cause is our code or Bethesda's own landscape. Covers the independent BC decoder, the per-CONSUMER band read off the code before the bake, the floor that has to live inside the diff tool, the border/histogram localisation that turns a count into a direction, and the `--dump-land` cell-seam check that settles it against the MASTER. Use whenever "the sheets moved" has to become a number, and before re-pinning any terrain-sheet baseline.
---

# WW: diff two generated sheets, and explain the difference

Repo `E:\Projects\NifskopeWildWastelandEdition`. Written 2026-09-10 by lane
BUILD4, after lane VTFIX (2026-09-09) and lane CLAMP (2026-09-10) each
re-derived most of it from scratch and CLAMP's report asked for it by name.
Working code: `scratchpad/clamp_20260910/edgeband.py`,
`scratchpad/build4_20260910/localise.py`,
`scratchpad/build4_20260910/seamcheck.py`, and VTFIX's `ddsdiff.py` / `seam.py`.

This is for a TEXEL claim about a generated sheet. A rendered picture of
geometry is `nifskope-ww-render-shot`; a picture OF the sheet is
`ww-texel-picture`.

## 1. Decode the sheet from the format, not through our writer

A check that reads our output with our own writer's code passes because the two
agree with each other. Re-type mip 0 of BC1/BC3 from the DDS layout — header at
128 bytes (148 for `DX10`), 8-byte blocks for DXT1 and 16 for DXT5 with the
alpha block FIRST, the four-colour palette when `c0 > c1` or the block is BC3.
Then check the decoder against a SECOND decoder's number on one shared input
before you believe any verdict from it (CLAMP's ceiling reproduced VTFIX's count
to the texel on `Commonwealth.4.-24.24`: 207,945).

## 2. The band is PER CONSUMER, and it is read off the code BEFORE the bake

One bound for a whole sheet set is wrong, usually by more than tenfold. Read
each consumer's own reach out of its loop and convert it with the sheet's
units-per-texel (dim 4 = 32 units a texel):

| consumer | reach in the code | band |
|---|---|---|
| the `_msn` central difference, `+-1` grid step | 128 units | 4 texels |
| the colour sheet through the cover slope gate, reading that normal | 128 units | 4 texels |
| the `_data` AO march, `for dist=128; dist<=2048; dist*=1.5` | 1,458 units (the last rung under the cap) | ~46 texels |
| a flow accumulation over the grid it is handed | **the whole grid** | **no band exists** |

**And add the sampler's own footprint.** This is the step CLAMP's
pre-registration missed and it cost two red gates. A bilinear tap spreads any
grid point it reads over one full spacing in each direction, so a change to a
grid point at index `k` is visible out to texel `4(k+1) - 1`:

* a change confined to points OUTSIDE the chunk (index -1) reaches **texel 3**;
* a change that touches the chunk's own boundary row (index 0) reaches **texel 7**.

Write the band down before the bake and do NOT move it afterwards. Section 0 of
the lane report is where it lives.

## 3. The floor belongs INSIDE the diff tool

The moment two sheets go byte-identical every band bar passes on nothing, and
only a floor notices. `edgeband.py` prints
`the <role> sheets did not move at all: the ring is inert` and returns non-zero.

Prove the tool on two known-answer inputs on the OLD exe before trusting it
(`ww-control-calibration`): a sheet set against ITSELF must read 0 everywhere
and fire the floor; `--cover` against `--no-cover` is the ceiling — the same
data with one property removed — and must blow every band (CLAMP: 428,272
texels, 416,280 beyond the 4-texel band).

**A control that is expected to fire is not a failure.** CLAMP's cover-free run
asks the colour sheets NOT to move, so the floor fires there by construction; it
is the COVER run the floor guards. Say which run each line belongs to or a
correct result reads as a red one.

## 4. When a band is missed: localise before you theorise

A count says nothing about a direction. For every beyond-band texel record which
of the four borders is nearest and the histogram of its distance:

```bash
python localise.py <beforeTexDir> <afterTexDir> <stem> <role|-> <band>
```

Read the shape:

* **one distance only, spread over all four borders** (`3:999`, W=242 E=277
  N=262 S=218) — the change is confined to points outside the chunk. Healthy.
* **one border, distances 4..7** (N=1,405, `4:388 5:394 6:347 7:276`) — the
  chunk's own boundary grid row moved. Go to §5.
* **exactly 16 texels in one 4x4 run of distances** — that is one BC block, and
  one texel inside it changed for a reason with no band at all (a global
  operator, e.g. flow accumulation). Do not hunt for a local cause.

## 5. Settle it against the MASTER, never against our own output

FO4 stores each cell its own 33x33 VHGT grid, and adjacent cells DO NOT always
agree on the vertex row they share. Where they disagree, any ring fill whose
iteration order lets the later cell win necessarily rewrites the earlier cell's
boundary row — which is our code behaving as documented on data that is not
symmetric, not a bug in the fill.

```bash
release/NifSkope.exe -no-gui lodgen "<Fallout4.esm>" --worldspace 3C \
  --terrain-region <x0> <y0> <x1> <y1> --dump-land "<ABSOLUTE>/land.bin"
python seamcheck.py land.bin
```

`--dump-land` writes `int32 minX, minY, cellsX, cellsY`, then one `uint8`
presence flag per cell, then `33*33 int16` heights in units of 8 per cell, row 0
south and column 0 west. The shared row between `(x,y)` and `(x,y+1)` is the
former's row 32 against the latter's row 0. Print the max `|difference|` per
column for every seam the fixture touches, and include seams you expect to read
0 — a table that is all non-zero has not discriminated anything.

BUILD4's reading, for calibration: over cells x=-24..-17, seams y=23|24,
y=27|28, y=32|33 and both east seams read **0 across every column**, while
**y=31|y=32 reads 2,1,4,6,9,8,7,4**. One disagreeing seam in the neighbourhood
explained two red gates exactly, and its columns matched the border the
localisation had already pointed at.

## 6. Re-pinning, and who may do it

A resuming or building lane **reports the miss as a number and stops**
(`nifskope-ww-resume-pending` §6). It does not adjust the band to fit what came
back, and it does not land a cure — a band derived after the numbers are in is
not a pre-registration.

When a baseline IS re-pinned, the hashes are written into `WW_CHANGES.md`
**beside the band table that justifies them**, never alone (CONSTITUTION 4).
Keep the `before/` bake: while any gate is red it is the only picture of the old
behaviour there will ever be.

## 7. Traps

* **Absolute `--out-dir` and `--tex-dir`, always.** A relative path resolves
  against the EXE's folder, exit code 0, full census row, and the bake appears
  to have written nothing. In `MISTAKES.md` twice.
* A spell in `tests/spells/` may not import from `scratchpad/`, so the decoder
  gets written again inside the harness. That is four copies as of 2026-09-10;
  when a shared home is made, this is the skill that should name it.
* Run the diff on a COPY of `release/` when another lane holds the link.

## 9. "Is it as SMOOTH as vanilla's" is a different question from "how far apart" (lane SPLAT1, 2026-09-11)

Everything above measures a DIFFERENCE between two sheets. bungo's complaint is
often about TEXTURE instead: ours looks speckled, Bethesda's looks graded, and
the mean difference barely moves when the speckle is fixed. Two instruments
answer that one, and they only work together:

* **3x3 local variance of luminance**, meaned over the sheet, in squared 8-bit
  units. This is the number the eye calls speckle. Our far-terrain sheets read
  76 and 75 where Bethesda's read 20 and 29.
* **The radially averaged power spectrum**, Hann-windowed and normalised so the
  bins sum to the field's variance, collapsed into scale BANDS. Local variance
  alone cannot tell structure from noise — a phase-randomised twin has the SAME
  local variance, to 0.2% — so every structural claim (a tiling period, a grid
  period, a codec at the block scale) is read off the bands or off a
  correlation, never off the variance.

The floors this needs:

* **A codec floor measured on never-coded data.** Re-encoding an already-BC
  sheet reproduces it almost exactly (rms 0.27/255) and says nothing about what
  a codec COSTS. Encode a smooth synthetic instead: BC1 adds 1.52 units of local
  variance, which is the bar every "excess" is quoted against.
* **A known-answer pair**, a smooth field and the same field plus a checker, to
  show the metric separates them (25.5x) before it is pointed at anything real.
  A checker of period p has its fundamental at RADIUS sqrt(2)/p in a radial
  spectrum, not 1/p — compute the radius, never type it.
* **A correlation CEILING before any negative.** "Vanilla's sheet does not
  contain the landscape texture" is only a finding if the same correlation,
  pointed at a sheet that does contain it, reads near 1 (+0.91 and +0.88 here)
  while the subject sits inside its phase-twin floor (<= 0.008 against 0.011).

And the honest half: a fix that removes the speckle may leave the mean
difference where it was. SPLAT1's tiling fix took local variance 76 -> 13 and
the whole-tile mean difference only 16.9 -> 15.0 of 255, because the rest is the
grading. Report both columns or the reader will assume one implies the other.

Working code: `scratchpad/splat1_20260911/splatlib.py` (vectorised BC1/BC3/BC5U,
`local_var`, `radial_power`, `band_table`, `phase_twin`, `bc1_roundtrip`) and
`s0_selftest.py`, which gates all of it 12/12 before a verdict is read.
