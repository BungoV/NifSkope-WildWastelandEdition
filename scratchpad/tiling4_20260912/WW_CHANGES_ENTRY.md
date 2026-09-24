# WW_CHANGES entry -- lane TILING4, 2026-09-12

Splice under the LOD generation section. Text only; the overseer owns the file.

---

## 2026-09-12 — `--land-sample stochastic` is now a hex tiling, not a warp: the repeat breaks without the swirls

**The experimental repeat fix no longer strains the texture.** TILING3's
`--land-sample stochastic` removed the land textures' tiling repeat by warping
world position smoothly before the lookup, and that warp is visible as swirls --
it has to strain the ground by about 0.72 of a texel per texel to break the
phase, and the strain IS the swirl. This lane replaced the geometry behind the
same switch word with one that breaks the phase and strains nothing.

* **`--land-sample stochastic` now means histogram-preserving hex tiling**
  (Heitz & Neyret 2018): the land texture is sampled at three per-hex-cell
  random offsets on a triangle lattice of **256 world units** and blended with
  variance-preserving weights, plus a mip bias of **-0.22**. No strain
  anywhere. Like the warp it is a pure function of world position, so it is
  seamless across chunk and cell boundaries and byte-identical at 1 vs 16 chunk
  threads (measured: 97 files, 0 differing, on a 16-chunk block).
* **`--land-sample warp` is TILING3's warp, kept reachable** so its
  measurements can be repeated. It reproduces TILING3's bake **byte for byte**
  on both test tiles.
* **`--land-hex UNITS`** sets the hex cell size on its own; `0` is off and is
  the default. `--land-hex 0 --land-mip-bias 0` is the exact way back from
  `--land-sample stochastic`, and it was measured, not asserted: 9 of 9 files
  identical to the previous build's bake on both test tiles.
* **Alpha is never blended.** The three taps are blended for colour only; the
  alpha comes from the largest-weight tap, because a variance-preserving blend
  of a constant 1.0 alpha would have pushed it to about 1.07.
* **The default did not change, and the number that decides that is the
  repeat.** Fourteen shipped sheets were baked by the new exe and by the
  previous build and scored by one piece of code: the hex tiling passes the
  repeat law on **9 of 14** sheets, the warp on **11 of 14**, and the shipped
  default on **0 of 14**. Four sheets miss on the amplitude -- (-20,20) 0.623,
  (-4,-20) 0.338, (4,-24) 0.324, (-12,-20) 0.301 against a 0.264 ceiling (0.366
  on (-20,20), which has its own higher control) -- and (-36,-20) misses on the
  ratio law even though its amplitude falls twentyfold there, 1.501 to 0.073,
  because that law divides by the sheet's own no-repeat floor and the floor
  falls with it; the warp fails that sheet the same way. So the switch stays
  off by default, exactly as TILING3's warp did.
* **What it buys, on the same fourteen real bakes:** the swirl reading
  (orientation coherence of the 1-5 texel grain over each sheet's own
  phase-twin floor, the repeat notched out) passes on **13 of 14** sheets
  against the warp's **7 of 14**; the warp reads over the ceiling on eleven
  sheets, the hex tiling on one -- (-20,20), where the previous build is
  already over it and the hex tiling reads below the previous build. And
  per-sheet grain stays within 20 % of the previous build's on **14 of 14**
  sheets against the warp's **1 of 14**.
* **What it substitutes for the swirls** is a soft blotchiness at its own
  256-unit cell scale, which no instrument in this lane measures. It is
  visible at 1:1 in `scratchpad/tiling4_20260912/images/sheet_tiling4.png` and
  it is a judgement call, not a number.

Nothing else moved. With the switch off the bake is byte-identical to the
previous build's; with it on, only the chunk colour sheet and the two virtual-
texture pyramid files change -- `_data`, the `_msn` (still vanilla's own file
byte for byte), the BTR, the BTO, the BTO manifest and `Commonwealth.VT.lodm`
are unchanged. The landscape tiling constant, the road pass, TILING3's
vanilla-reuse path and the crevice term were not touched.

**Your open NifSkope window needs a restart to pick this up.**
