## 2026-09-12 — the far terrain's colour is not one shade off vanilla: it is a different colour in each cell, and `--grade` is the knob that proves it

**The record said the ground was about 16 levels too dark. That is true of one
chunk and false of the one next to it.** This lane measured the colour of our
baked far terrain against Bethesda's own sheets on 25 chunks of the
Commonwealth, and there is no single brightness correction that helps. On
chunk (-20,20) our ground is 14.7 levels too dark; one chunk north, on
(-20,24), it is 6.8 levels too BRIGHT. Across 25 chunks the best per-chunk
multiplier runs from **0.615 to 1.241** — some chunks want to be dimmed by a
third, others brightened by a quarter — and across the 96 individual land cells
inside six of them it runs 0.699 to 1.388 with an average of 1.004, which is to
say: on average we are already right, and every chunk is wrong in its own
direction.

* **`--grade K` multiplies every baked colour texel by K**, on both writers (the
  per-chunk sheets and the pyramid), after the road composite and the grass tint
  and before the crevice shading. **The default is 1.0 and at 1.0 the multiply is
  not performed at all** — the branch is skipped — so a bake with no flag is the
  previous build's bytes by construction, not by an argument about floats. That
  was measured too: 24 of 24 files byte-identical across two test chunks, with no
  flag and with `--grade 1.0`. Range clamped to 0..4. The bake census prints
  `landGrade` on both paths, so a sheet can never be read against the wrong value.
* **No value is recommended, and that refusal is arithmetic, not a tuning
  failure.** The error of a constant multiplier on a chunk is a parabola with its
  lowest point at that chunk's own best value. (-20,24) wants 0.892 and (-20,20)
  wants 1.118 — one either side of 1 — so every value below 1 makes the second
  chunk worse and every value above 1 makes the first worse. Through the compiled
  binary: the pooled best value 0.8403 takes (-20,24) from 20.13 to 18.11 and
  (-20,20) from 21.90 to **28.49**. The value that fixes everything does not
  exist.
* **What it is instead.** Five models were fitted on matched texels, roads
  excluded: a constant gain, an affine curve, a gamma, and the two ways an sRGB
  conversion could have slipped. Both sRGB slips are refuted by two orders of
  magnitude (56–79 levels of error against an identity's 20–23), and the code
  contains no colour-space conversion to slip. The affine and gamma fits "win" on
  error only by collapsing onto vanilla's average and ignoring their input
  entirely (slopes 0.019 and 0.185). After the best gain, the leftover error's
  only consistent partner is our own brightness (r = +0.84 and +0.83 against
  phase-scrambled controls of −0.06 and +0.01). Height sits exactly at its
  control floor; the per-cell multipliers are blocky and straddle 1. **That is a
  per-cell content difference — which textures we blend where — and it belongs to
  the layer blend, not to a grade.**
* **The vertex-colour layer is not the cause and can stop being suspected.** Its
  per-texel multiplier averages 254.9 out of 255 on all six chunks checked;
  dividing it back out moves the fitted multiplier by 0.0003.
* **The road is only a third of the road-vs-ground contrast problem.** On
  (-20,20) the road is 6.3 levels too bright and the ground it sits in is 14.7
  too dark; grading them together (which is what this knob does) keeps a road
  inside the tolerance of its own ground.
* **We are consistently more colourful than vanilla** (saturation 0.243 against
  0.219 on average) — but a brightness multiplier leaves saturation exactly
  unchanged, so that is a separate defect with a separate cause. A global
  desaturation was fitted anyway and moves the error by at most 0.15 of a level,
  a fifth of the texture compressor's own noise floor. Not shipped.
* **The grass tint stays at 0.35, because it cannot be fitted: the ground-cover
  channel is empty on all 25 chunks**, Sanctuary included. Every `_data` sheet
  reports zero cover and its alpha is zero on 100 % of texels, so the tint branch
  never executes. That is a defect in whatever fills the cover channel, flagged
  for a later lane.

Eight harnesses at the previous build's baselines exactly (26/0, 41/1, 11/0,
29/5, 23/0, 116/0, 18/0, 14/0); the two known failures are pre-existing and
unchanged. Pictures: `scratchpad/grade1_20260911/images/cmp_tone.png` and
`curve.png`.
