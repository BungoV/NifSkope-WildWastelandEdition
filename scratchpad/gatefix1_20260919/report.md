# GATEFIX1 -- two gate repairs, 2026-09-19

Exe used for every run: `release/NifSkope.exe` 23,505,408 B, 2026-09-19 16:48:03,
sha1 `072d78f8629aac431fcff51c7d8db349a0672a8b` -- the brief's exe, re-hashed at
17:16. Nothing was built. `tasklist` checked as its own command before every run;
no Fallout4 and no NifSkope at any check.

---

## JOB A -- lodgen_octahedral.sh row F1

Gate runs: `scratchpad/gatefix1_20260919/oct_before.txt` (17:16, PORT 42951) and
`oct_after.txt` (17:2x, PORT 42952).

### Counts

| | ok | FAIL | result |
|---|---|---|---|
| before | 112 | 1 | FAIL |
| after  | 116 | 0 | PASS |

The only red before was F1. Three rows are new (F2b, F2c, F2d); no row was
removed and no bar was raised.

### What changed, and why it is not a raised bar

The director's ruling was followed exactly: **the bar stays at 1.0 texel** and
the ROW now decodes coverage and tests at half.

`tests/spells/lodgen_octahedral.sh`

* `mask()` gains `dec=(floor, base)` and `dilate`. With `dec` the stored alpha is
  turned back into the measured coverage fraction -- `0` below `base`, else
  `floor + (a-base)*(255-floor)/(255-base)` -- before the threshold is applied.
  `measure()` passes both through.
* F1/F2 now read `measure(..., 128, (16,160))`, i.e. coverage >= 50 %.
* F1b, F3, F4, F5 are untouched: they ask about the STORED byte, which is still
  the right question for them.

### Every row of the block, before and after

| row | before | after |
|---|---|---|
| ortho span within 2 at the bake floor | ok 1.68 | ok 1.68 |
| PERSPECTIVE CONTROL exceeds 2 | ok 19.62 | ok 19.62 |
| central symmetry <= 5 % | ok 0.037 | ok 0.037 |
| PERSPECTIVE CONTROL is not symmetric | ok 0.501 | ok 0.501 |
| near vs far edge width <= 5 % | ok 0.000 | ok 0.000 |
| PERSPECTIVE CONTROL foreshortens | ok 0.850 | ok 0.850 |
| the two bakes recorded different half-extents | ok 456.40 vs 406.18 | ok, same |
| **F1** | **FAIL 1.68** (bar 1.0, reader threshold) | **ok 0.72** (bar 1.0, half coverage) |
| F2 (floor) PERSPECTIVE CONTROL | ok 19.62 | ok 17.62 |
| **F2b (floor)** one-ring dilated mask must not clear the bar | -- | **ok 2.64** |
| **F2c (known answer)** analytic cube at half coverage | -- | **ok 0.72** |
| **F2d (floor)** the same analytic cube read the OLD way | -- | **ok 1.78** |
| F1b reader threshold == bake floor | ok 1.68 vs 1.68 | ok 1.68 vs 1.68 |
| F5 contract stated / is (16,128,160) / BC3 headroom | ok, ok, ok | unchanged |
| F3 no alpha in 1..159 | ok 0 found | unchanged |
| F4 (floor) decoded fraction does carry them | ok 3824 | unchanged |
| F3 tested set == floored set | ok 117132 vs 117132 | unchanged |

### The shipped bake DOES pass at bar 1.0 with the corrected instrument

`0.72` against `1.0`. The brief asked for a finding if it did not; it did.

### The known answer, computed inside the gate

An analytically exact cube -- convex hull of the eight corners projected on each
frame's `(r, up)`, rasterised 12x12 samples per texel over the same extents --
pushed through the gate's own instrument:

    at HALF coverage                      0.72 texels   (OCTF1 computed 0.71 offline)
    read the OLD way (coverage >= 6.27 %) 1.78 texels
    the shipped bake, same two readings   0.72  and  1.68

So a defect-free bake could never have cleared 1.0 at the old reading. That is
F2d, and it is the measured demonstration that the EXPECTATION was wrong rather
than the bake. It is a numpy port of OCTF1's `f1sim.py`, verified to reproduce
its three numbers exactly (1.78 / 0.71 / 1.29) before it went in, and it costs
0.9 s.

### The red control

F2b dilates the same sheet's mask by one 8-neighbour ring -- a silhouette one
texel too wide on every side, which is the defect F1 exists to catch -- and reads
**2.64** through the same instrument. It must exceed 1.0 and does, so F1 is a row
that can still fail.
