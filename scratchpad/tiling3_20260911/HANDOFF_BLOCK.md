# HANDOFF block -- lane TILING3, 2026-09-11 23:5x

## TILING3 -- vanilla's fine detail: where it comes from, and what we now copy

**Exe** `release/NifSkope.exe` 2026-09-11 23:26:29, 21,484,032 B. One build, no
relinks after it. Rung `release/NifSkope.before_tiling3.exe` 21,466,624 B.
Nothing committed. **bungo's open NifSkope window needs a restart.**

### bungo's rulings, verbatim, in the order he gave them

> "For now, I think we can use vanilla normal map for those tiles, use those
> details for the diffuse."

> "so now we do not use our own normal map if that is toggled, but reuse these
> ones for terrain chunks."

> "out of bounds terrain blends are not included in the actual cells out of
> bounds, they never were, so we can't recover the color data anymore, because it
> was baked in a different tool outside of fo4."

> the G channel of vanilla's `_msn` (up, = cos of the slope angle) "is the
> slopeness for the micro details our heightmap bakes do not possess"

### What the default now does

`--land-detail-source vanilla` is **the shipped default**.

1. A chunk with a shipped vanilla `_msn` -> the output `_msn` **IS vanilla's
   file, byte for byte** (`cmp` == 0). Our normal bake is skipped for it. No
   guard, no composite, no threshold: the only question asked is whether the file
   exists.
2. A chunk with **no land paint on any cell** -> the output COLOUR **IS vanilla's
   file, byte for byte**. Classification is **per chunk, by "any cell has
   paint"**, because the shipping writer assembles a chunk sheet from four
   virtual-texture tiles and has no per-quadrant seam at that point. A partly
   painted chunk counts as painted, so the rule errs toward keeping our composite.
3. Every other chunk keeps our colour composite and takes the **crevice term**:
   `dL = kDiv * div(detail normal)`, `kDiv = --land-shade`, default **-3.242**,
   added equally to all three 8-bit channels.
4. A chunk with no vanilla sheet keeps our bake, exactly as before.
5. `--land-detail-source none` == the rung's bytes, proved file by file.

`--vanilla-lod-root` (default `E:/Tools/Fallout 4/DataUnpacked/Data`) reads
vanilla's sheets **as loose files only**, never through the resource stack. That
is load-bearing: the stack would serve our own previously installed output out of
the game's `Data`, and a bake would "reuse vanilla" by copying yesterday's copy
of itself.

### The two places the measurement contradicted the instruction, and what shipped

* **A Lambert shading of vanilla's normal detail reads zero.** R^2 0.0000 at a
  twin floor of 0.00001, with the up coefficient flipping -1.34 to +0.45 sheet to
  sheet; the fit machinery recovers an injected 0.5 as 0.5000, so the null is the
  data's. A Lambert dot cannot see a rill: the two walls cancel. What measures is
  the **divergence** of the same field -- median r -0.1046 against a twin floor of
  +0.0006, same sign 7 of 7, over the twin 7 of 7 -- so that is what ships.
* **Micro-steepness (`acos(up fine) - acos(up coarse)`) also reads zero as a
  darkening driver**: median |r| 0.0039 against the crevice term's 0.1046, sign
  agreement 4 of 7 (chance). Tested exactly as instructed, kept only if at least
  as good, and it is not: **the crevice term stays.** No rebuild was needed. The
  tint half reads zero too -- the three channels move together to within 0.0018
  and the saturation term is -0.0016 -- so the shipped term is a pure darkening,
  which is what the data supports.
* **But micro-steepness wins as an ENVELOPE:** `|dTheta|` against the colour
  residual's own envelope reads +0.1269 median, beats its twin 7 of 7 and beats
  the divergence's envelope 5 of 7. It predicts WHERE vanilla's grain lives, not
  its sign. That is the strongest open lead for the next lane -- it would mean
  modulating an amplitude rather than adding a level, a second fitted parameter
  and a second build. Not smuggled into this one.

### What is still red

* **The repeat is still there on the default.** The shipped default was never
  asked to fix it; it delivers the ruling. Real bakes: rung 1.037 / 1.261, ship
  1.042 / 1.242, against vanilla's 0.201 / 0.032 and a ceiling of 0.264.
* **The repeat fix exists and is OFF.** `--land-sample stochastic` (domain warp
  A=683, lattice 1024, 1 octave, mip bias -1.00) reads repeat 0.183 with grain at
  111 % of vanilla's on (-20,24) -- both halves of the gate -- and 0.593 with
  grain 99 % on (-20,20): repeat red. Its 7-sheet selection scored **6 of 7**; the
  red is (-36,-20) at repeat 0.095 / ratio 0.698 against a 0.448 ceiling. Its RMS
  strain 0.718 is above the prototype's own "over 0.5 is a visible wobble" note.
  **Turning it on is bungo's call and the numbers are in the report.**
* **~98 % of vanilla's fine colour is not a function of vanilla's fine normal**
  (22-column ceiling R^2 0.018 / 0.023 on a residual SD of 4.476 / 5.459). No
  per-texel law from the `_msn` can do better than the crevice term by much. The
  grain's real source is the land textures sampled about four mips finer than the
  footprint, with the repeat broken -- which is what the stochastic proposal is.

### Gates, with counts

* off == rung: **9 files each, both tiles, 0 differing**.
* 1 vs 16 chunk threads, 16-chunk block: **97 files, 0 differing**.
* `_msn` cmp == vanilla: **18 of 18**; no-vanilla control (empty root): **0
  differing vs rung on 9 files**.
* Chunk classes -- t2024 1/1/0/0, t2020 1/1/0/0, edgeN 16 chunks 7 layered / 9
  layerless / 0 layerless-without-vanilla, census region 180 chunk sheets (dim 4
  and dim 8) 121 layered / 59 layerless / 0 without. **Nothing anywhere lacked a
  vanilla sheet**: Bethesda ships a complete 48x48 dim-4 grid over cells -96..95.
* Untouched by the default: `_data`, BTR, BTO, manifest, `Commonwealth.VT.2.lodt`,
  `Commonwealth.VT.4.lodt`, `Commonwealth.VT.lodm` -- byte-identical.
* Chain at TILING2's baselines, all eleven harnesses. `lodgen_ground_cover.sh`
  first read 29/6: its check C1 asserts all three sheets are 174,888 bytes (DXT1),
  and vanilla's `_msn` is BC5 at 349,680. The harness now pins
  `--land-detail-source none` on its own bakes -- a harness forces the state it
  measures -- and reads **29/5, line for line identical to TILING2's**. Shell
  only, no rebuild.

### A pre-registered gate clause that the ruling superseded

F2 as registered said "`.lodl` and `_msn` byte-identical at EVERY setting". The
`.lodl` half holds. The `_msn` half **cannot and must not** -- bungo's correction
made the `_msn` the deliverable. It was replaced by the gate he dictated, which is
stricter: every chunk with a vanilla sheet -> cmp == vanilla; every chunk without
-> cmp == rung; state the count each side. Both counts are above.

### Files

Report `scratchpad/lane_tiling3_report.md`. Instruments and logs under
`scratchpad/tiling3_20260911/` (`d1_geology.py`, `d2_deep.py`, `d3_shade.py`,
`d4_steep.py`, `a3_abc.py`, `a4_warp.py`, `a5_tune.py`, `a6_pick.py`,
`f3_gate.py`, `t3_bake.sh`, `make_pics3.py`; `logs/`, `out/`). Picture
`images/cmp_tiling3.png`. Code in `src/lodgen.h`, `src/lodgen.cpp`,
`src/nifcli.cpp`; harness `tests/spells/lodgen_ground_cover.sh`.
