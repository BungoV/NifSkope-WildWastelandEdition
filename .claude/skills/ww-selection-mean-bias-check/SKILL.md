---
name: ww-selection-mean-bias-check
description: Before reading any spectral / band-share gate on a LOD land arm that picks or weights samples per texel (height blend, relief-weighted hex joins, max-blend, any "winner takes the texel"), measure the arm's mean-luminance shift and its large-scale correlation against the floor bake -- and if it is a selection bias, correct it by regression (blend the relief-predicted colour with the painted weights, select only the residual). A selection on a variable that correlates with brightness moves every texture's average, and a band-SHARE gate then goes red for a reason that is not grain. Use when a band-shape gate (TILING4 G2-band or similar) is red on an arm whose grain gates are green.
---

# Selection moves the mean: check it before the band gate

Written by lane TILING5 (2026-09-27/28). Its height blend picked texels by relief; the grain gates were green,
TILING4's band-shape gate G2-band was red on 5 of 7 sheets, and the reason was a brightness shift of +2.2 of 255
on every sheet, not anything in the grain.

## 1. Why it happens
A per-texel weight `w(h)` on a variable `h` that correlates with the colour `s` changes the expectation:
`E[s w(h)] / E[w(h)] != E[s]`. FO4 land relief integrated from the normal map correlates +0.2 with diffuse
luminance (lit tops), so "the raised texel wins" also means "the brighter texel wins". Every texture shifts by
its own amount, so the large-scale pattern of textures gains contrast -> power in the coarsest band rises ->
band SHARES move, and a zero-tolerance share gate (G2-band: `<= rung + 1e-12`) goes red.
Splitting mean from detail (blend repeat averages linearly, only details by relief) removes the first-order form
(two materials' mean difference dithered per texel). It does NOT remove this one: the detail selection and a
relief-weighted hex join over three taps of the SAME texture still bias the mean.

## 2. The check (no bake, existing sheets only)
Scripts in the NifSkope WW lane folder `scratchpad/tiling5_20260927/` (branch tiling5-20260927; reuse by copying):
* `t5_meanbias.py ARM [FLOOR]` -- per sheet: mean luminance of arm and floor, the difference image low-passed by a
  box r=32 (29 m on a 512-texel 4-cell sheet), its SD, and its correlation with the low-passed floor.
  A selection bias reads: dMean of one sign on (nearly) every sheet, r > 0 on most (it amplifies existing
  contrast), and dMean growing with the sharpness constant (TILING5: beta 1 -> +1.39, beta 2 -> +2.23).
* `t5_g2bd.py ARM [FLOOR]` -- per sheet: the six band shares (vanilla / floor / arm), the per-band move, and the
  ABSOLUTE band power arm/floor. Shares alone cannot tell "coarse band gained power" from "middle bands lost
  it"; the absolute ratio can. TILING5: coarse x1.14 median -> real large-scale gain.
Both import `t5_gates.py` for sheet paths and the frozen chunk sets, and TILING4/TILING3 modules from the main
tree's scratchpad (read-only).

## 3. The fix that was built (TILING5 commit 020ae381, `src/lodgen.cpp`)
Regression, not a bias table: per texture and per relief level measure `G = cov(colour, h)` (h at unit SD) once,
against the diffuse mip whose texel matches the level (`lodgenLandSlopeFor`). Model colour = mean + G h + e.
Every choice then blends the predicted part `G h` with the PAINTED weights and applies the relief weights only to
the residual `e` (whose expectation does not depend on the choice):
* weighted taps: `acc -= G * (sum w'_k h_k / |w'| - sum w_k h_k / |w|)` (w' = relief weights, w = painted);
* opacity blend: `c' = c + (lc - c) ah + ((ml - m) + (pl - p)) (a - ah)`, p = the composite's predicted detail,
  tracked like its mean (`p' = p + (pl - p) a`).
For a Gaussian relief this is the same as subtracting `E[s e^{bh}]/E[e^{bh}] - E[s] = b cov(s,h)`, but it works
per texel and at any weight. Off path: every new pointer null -> the rung's bytes.
Re-measure dMean first (must drop toward 0 on every sheet), then the band gate. See the lane's DONE.md D4 for
the numbers this fix actually reached.

## 4. Reading
* dMean ~ 0 and coarse power ~ 1.0: the band gate's red is about grain/shape -- look at the fine bands.
* dMean one-signed on all sheets, growing with the constant: selection bias. Fix it in the sampler (section 3),
  never by loosening the gate.
* Label it right in the report: the shift is MEASURED; the relief-brightness mechanism is REASONED unless a
  per-texture test isolates it (or the regression fix removes the shift -- that is the test).
