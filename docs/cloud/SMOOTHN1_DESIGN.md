# SMOOTHN1 -- extra rays where a surfel is noisy, and careful sharing between neighbors

Lane SMOOTHN1 (prep, cloud), 2026-10-03. Design plus a Python twin: `tests/spells/smoothn1_check.py`.
No C++ in this lane (see "C++" at the end). No game data was used or added; the scene is built in code.

The owner's rule, which everything below obeys: **quality is never traded for bake speed.** Ray counts only go up
from today's count. A noisy surfel gets more rays until its noise is under a bar (or it reaches a hard cap). No
surfel ever gets fewer rays than today.

## 1. Today's fixed count (the floor)

The only fixed bake ray count in the tree is the probe bake's:

- `src/probebake.h:39` -- `int rays = 2048;            //!< per probe, Fibonacci sphere`
- `src/probebake.cpp:295` -- `const int N = qBound( 64, spec.rays, 1 << 16 );` (the count the bake uses)
- `src/cellview.cpp:2944-2946` -- `WW_CELL_PROBE_BAKE_RAYS` can override it; nothing sets it by default
- `docs/PRTP_PLAN.md:203` -- "Rays: a Fibonacci sphere of N (default 2048), each worth 4pi/N, from each probe"

Today a surfel does not trace rays of its own. It gets its light from the probes it can see (`src/probegi.cpp:615`,
lane BOUNCE2: "What each surfel reads: the probes within the radius it can see ..."). So a per-surfel gather is
new work, and "today's count" for it is taken as **2048 rays per surfel**: the same number a probe traces today.
A surfel gathers over a hemisphere, not a sphere, so 2048 over a hemisphere is twice a probe's ray density per
direction. That is a floor no lower than today's in any reading.

**Mapping in the twin:** twin floor F = 256 = 2048 / 8. Every twin ray count times 8 is the production count:
cap 4096 in the twin = **32768** in production. The scene is tuned so that the hard case is hard at the twin's
floor (a surfel far from the window expects fewer than one window hit in 256 rays). That is the situation the
real floor faces in a dark room lit by a small opening. The logic does not depend on the scale.

## 2. The estimator (per surfel, offline)

A surfel's value is its mean incoming radiance, E / pi. Rays are cosine-weighted over its hemisphere. Ray k
returns x_k, the radiance of what it hits. After N rays:

- mean  m = (1/N) sum x_k
- sample variance  s^2 = (1/(N-1)) sum (x_k - m)^2
- standard error  SE = sqrt(s^2 / N)

In C++, keep the sums in double precision with Welford's update (count, mean, M2), so a very bright window does not
cancel away the variance. Offline, no short-term or long-term moving averages are needed: the bake keeps every ray
of a surfel, so the plain sample statistics are exact. GIBS needed moving averages because it runs over frames;
see section 8 for what was and was not read of it.

**Directions must be random for SE to mean anything.** Today's probe rays are one fixed Fibonacci sphere. That is
low-discrepancy but deterministic, so its spread is not a noise estimate. A per-surfel bake should trace batches.
Each batch is a Fibonacci (or other stratified) hemisphere set turned by its own random rotation. The SE is then
taken from the spread of the batch means. The twin uses plain independent random rays. That is the conservative
case: stratified batches only make each batch less noisy.

### The neighbor noise floor (needed in the hard case)

The hard case fails a naive stop rule. A surfel whose 2048 rays all happen to miss a small bright window sees only
the dim walls. Its s^2 is then about 0, so it looks perfectly settled at the floor, and its value is badly wrong.
The guard is to borrow the noise level of its neighbors:

- cv2_j = s_j^2 / m_j^2 after the floor pass (relative variance), for each neighbor j in the same grid cell that faces
  the same way (n_i . n_j > 0.9) and that the depth test says it can see (V_ij > 0.5)
- prior_i = the MEAN of those cv2_j (not the median: when fewer than half of them hit the window, the median is 0)
- sigma_i^2 = max(s_i^2, prior_i x m_i^2)

The value is relative. A bright neighbor's large absolute variance would otherwise keep a dimmer, settled surfel
tracing to the cap (that happened in development; see section 7). The prior can only add rays. It never removes any.
Measured in the twin, removing it costs room A 13.2% vs 8.1% of the fixed bake's squared error, and 12% fewer rays.

## 3. The stop rule and the cap

- Every surfel traces the floor: F rays (production 2048).
- The bar: SE_i = sqrt(sigma_i^2 / N_i) <= max(EPS_REL x m_i, TAU_ABS), with EPS_REL = 0.05 (5% standard error)
  and TAU_ABS = 0.01 x the median surfel mean of the cell after the floor pass. TAU_ABS keeps near-black surfels
  from chasing noise nobody can see. It is per cell, so it follows the cell's own light level.
- A surfel over the bar traces another batch of F rays. Repeat until it is under the bar or reaches the cap.
- Hard cap: 16 F (production 32768 rays per surfel).
- Never fewer: a surfel's ray count is F + k F, k >= 0. Nothing in the rule can lower it. G1 checks this.

## 4. The sharing rule (only for surfels still noisy at the cap)

Sharing is decided per receiving surfel i. It happens only if i is still over the bar after the cap. The donors j
are the surfels **in the same sharing grid cell** (1 m in the twin) within R = surfel diameter (0.5 m in the twin).

Pair weight:

    w_ij = max(0, n_i . n_j)^8  x  (1 - (d_ij / R)^2)^2  x  V'_ij
    V_ij  = vis_i(p_j + b n_j) x vis_j(p_i + b n_i)            (GICAL1's depth test, both ways, multiplied;
                                                                b = 1 game unit, GICAL1 3.6)
    V'_ij = V_ij if V_ij >= 0.05, else 0                        (blocked means zero, not "a little")

- The normal term is exactly 0 at 90 degrees, so a floor never borrows from a wall at its foot.
- The distance term fades to 0 at R.
- The depth term blocks a neighbor behind a wall, even when the two share a grid cell and face the same way: floors
  on both sides of a thin wall. The twin's grid is placed on purpose so one column straddles the dividing wall.

Pooled neighbor estimate (it borrows the neighbors' rays, weighted):

    W_i    = sum_j w_ij N_j
    m_nb   = sum_j w_ij S_j / W_i                       (S_j = sum of j's ray values)
    b_i    = sum_j w_ij^2 N_j sigma_j^2 / W_i^2         (the pooled estimate's variance)
    a_i    = sigma_i^2 / N_i                            (own estimate's variance)

Blend (the minimum-error mix of an unbiased estimate and a possibly biased one):

    lambda_i = a_i / max( (m_nb - m_i)^2 , a_i + b_i )
    shared_i = m_i + lambda_i (m_nb - m_i)

The expected (m_nb - m_i)^2 is a + b + bias^2. So when the neighbors really differ, for example at the edge of a
sun pool, lambda shrinks and the surfel keeps its own value. A plain ray-pooled mean (lambda = W/(N+W)) failed the
"sharing never hurts" check on 12-13 surfels along the sun pool's edge, so it was replaced (section 7).

Sharing never removes a surfel's own rays and never changes its ray count.

## 5. The GICAL1 interface (GICAL1's own functions, a stand-in as fallback)

GICAL1 finished while this lane ran: origin/cloud-GICAL1, commit 30201d7 (docs/cloud/GICAL1_DESIGN.md,
tests/spells/gical1_check.py). The twin now uses **GICAL1's own functions** by default. Its file is copied into this
branch byte for byte (`tests/spells/gical1_check.py`, git blob 1f4f088, the same as on cloud-GICAL1, so the two
branches merge without conflict) and imported. The functions used are:
- `depth_ray_dirs`
- `depth_map_from_hits`
- `pack_depth_record` / `unpack_depth_record`
- `depth_visibility`

`SMOOTHN1_DEPTH=standin` runs this lane's original stand-in instead (`gical1_visibility`, still marked "stand-in for
GICAL1's test"). `SMOOTHN1_DEPTH=gical1` refuses to run without GICAL1's file. The first line of the output says
which test ran.

**The interface as used (GICAL1 3.1-3.6):**
- **Map.** 8 x 8 texels, hemi-octahedral in the surfel's frame (Duff et al. basis), horizon warp k = 4.
- **Depth rays.** GICAL1's own stratified rays (8 per texel, 512 per surfel). Each starts 1 unit (LIFT) along the
  normal, and its distance is clamped to D. In the twin, D is the surfel diameter, 0.5 m. The depth rays are traced
  by this twin's tracer. They are separate from the gather rays, and the map is built once (geometry only).
- **Storage.** Through the 16-bit record, as a reader gets it back.
- **Query.** `depth_visibility(p, n, map, D, q, lift)`. o = p + n LIFT, t = abs(q - o), and the moments are a bilinear
  read in the direction o -> q. The result is 1 if t <= mu, else (var / (var + (t - mu)^2))^3, with
  var = max(m2 - mu^2, (0.01 D)^2).
- **Units.** GICAL1 is in game units. The twin is in meters, with 1 unit = 0.0142875 m for LIFT and the receiver bias.

**SMOOTHN1's own additions:**
- Both ways, multiplied (GICAL1 3.6 asks for this). The stand-in used min before GICAL1 landed and now uses the
  product too: rays changed by 16 out of 5.19M, no verdict changed.
- The hard cut V < 0.05 -> 0.

**Measured with both tests.** Same seeds, every check, 16 repeats.

| | GICAL1's test | stand-in (4 x 4 from the bake rays) |
|---|---|---|
| green | PASS all; G4 cross-wall weight 0; G6/G7 change 0 | PASS all; the same |
| green rays per bake | 5,188,528 (x5.87) | 5,189,872 (x5.87) |
| red `wall` (depth off) | FAIL G6 (0.295), G4 | FAIL G6 (0.295), G4 |
| red `corner` (normal off) | FAIL G7 (0.333), **and G6 (0.278)**, G4 (cross-wall weight 498.7) | FAIL G7 (0.328), G4; G6 PASS |
| red `speed` | FAIL G1, G2 | FAIL G1, G2 |

**A finding for GICAL1.** Its test has no "behind the surfel" rule. `hemioct_encode` clamps z to >= 0, so a point
behind the surfel's plane is read from a horizon texel. Along a wall, the rays in that texel run far, so mu is
about D and the point reads as visible.

The red `corner` shows it. The two faces of the dividing wall (normals -x and +x, 0.1 m apart, the same grid cell)
pass GICAL1's test both ways once SMOOTHN1's normal weight is off: room B's light reaches room A's wall (G6, 0.278).
The stand-in returns 0 behind the plane, so it does not leak there.

In SMOOTHN1's sharing, the normal weight already removes these pairs (green is clean with both tests). But any other
caller of GICAL1's test has no normal weight, for example the pixel apply (GICAL1 4.2). Suggested fix, for GICAL1
to decide: return 0 when n . (q - o) < 0.

**Still to reconcile:**
1. The behind-plane rule above.
2. GICAL1's clamp D must be >= the sharing radius R. Here R = D, so a neighbor at the edge of the radius sits at the
   clamp.
3. In production, whether the depth rays are GICAL1's separate 512, or the gather rays binned into the map. The
   stand-in did the latter, and both pass here.

The twin's G6/G7 isolation checks are the acceptance test for any further change to the depth test.

## 6. The twin and its checks

Scene: two closed box rooms (6 x 4 x 3 m), split by a 0.1 m wall. Surfels sit on a 0.25 m lattice on every face:
3452 in all.

- Room A, the hard case: dim faces (radiance 0.2). One small window (0.4 x 0.4 m, radiance 600) high on the north
  wall near the dividing wall. A sun pool (1 x 1 m, radiance 40) on the floor at the foot of the dividing wall.
  Most rays from most surfels miss the window.
- Room B, the easy case: an open, lit room. Ceiling 60, walls 10, floor 5.

A ray returns the radiance of what it hits: a one-bounce gather of a known radiance field. In production the field
is the relight's surfel radiance B.

**Reference.** Exact, with no noise. Each room is convex with nothing inside, so every face is fully seen. Lambert's
polygon formula then gives every surfel's E / pi in closed form (faces plus window and pool patches). A
very-high-count Monte Carlo cross-checks the analytic code against the tracer: 256 surfels x 65536 rays, with its
own SE reported.

**Statistics.** 16 independent repeats (seeds [20261003, repeat, round]). Adaptive and fixed share their first F
rays, so per-surfel comparisons are paired. "At least as low everywhere" is tested per surfel. Over the repeats,
d = err_adaptive^2 - err_fixed^2, and a surfel FAILS when mean(d) / SE(d) > 4.5. That is one-sided. The SE comes from 16 repeats, so the right tail is Student's t with 15 degrees of
freedom: P(t > 4.5) = 2.1e-4 per surfel. Over 3452 surfels that is up to about 0.7 expected false alarms per run if
the surfels were independent and normal. They are neither (neighbors share light, and d is skewed), so treat 0.7 as
an order of magnitude. Z_GATE was set as if the tail were normal (Bonferroni, about 0.01 false alarms); that was
an error in the reasoning, found after the bar was set, and the bar was not moved.
- The seeds are fixed, so the verdict is reproducible.
- The green run's largest z is 2.72, far under 4.5.
- With fewer than 16 repeats the script refuses to give a verdict. At 3 repeats, green "failed" on 6 surfels from
  noise alone.

This is a test of "significantly worse". It cannot prove "never worse by any amount": the worst measured RMSE ratio
is printed beside it so the size is visible.

| check | what it proves | bar |
|---|---|---|
| R reference | exact reference = the tracer's own high-count answer | worst abs(z) <= 4.5 |
| G1 never-fewer | every surfel, every repeat: F <= rays <= cap | exact |
| G2 no surfel worse than fixed | per-surfel paired test vs the fixed bake | z <= 4.5 on every surfel |
| G3 hard case lower | room A total squared error, adaptive / fixed | <= 0.5 |
| G4 sharing stays on its face | sharing weight actually used across the wall / between 90-degree faces | <= 1e-3 each |
| G5 sharing never hurts | per-surfel paired test, shared vs the surfel's own rays | z <= 4.5 on every surfel |
| G6 no light through the wall | the same rays with room B's lights off: no room A estimate moves | rel. change <= 1e-9 |
| G7 no light round the corner | the same rays with the floor's sun pool off: no room A floor estimate moves (a floor never sees its own floor) | rel. change <= 1e-9 |

Reds (each runs inside the plain run, and alone with `SMOOTHN1_RED=<name>`):
- `corner`: the normal weight off. Must FAIL G7.
- `wall`: the depth test off. Must FAIL G6.
- `speed`: start at F/4 and let surfels already under the bar stop there. Must FAIL G1.

The script exits 0 only when green passes every check and every red fails its check.

### Output (the run committed with this doc, `python3 tests/spells/smoothn1_check.py`, GICAL1's test, exit 0)

The stand-in run (`SMOOTHN1_DEPTH=standin`) differs only as the table in section 5 shows. It also exits 0, in 75 s.

```
depth test: GICAL1 (8x8 texels, warp 4, D = 0.5 m, lift and bias 1 unit = 0.0142875 m), maps built in 5.6 s
scene: 3452 surfels (room A hard 1724, room B easy 1728), 30652 same-cell neighbor pairs within 0.5 m; floor F=256 (production 2048), cap 4096 (production 32768), bar SE <= max(0.05 x mean, 0.01 x median mean), 16 repeats, seed 20261003
reference (exact) mean radiance: room A median 0.837 [0.203, 16.826], room B median 20.543
R reference: PASS  MC 256 surfels x 65536 rays vs exact: worst |z| 2.94 (bar 4.5); MC relative SE median 0.0469 max 0.1227; exact reference noise 0
--- green
G1 never-fewer: PASS  fewest rays on any surfel in any repeat 256 (floor 256); surfel-bakes under the floor 0; most 4096 (cap 4096)
G2 no surfel worse than fixed: PASS  0 of 3452 surfels significantly worse (paired z > 4.5 over 16 repeats); largest z 2.72 at room A face 0 p=(0.000,2.875,2.625), RMSE adaptive/fixed there 1.098; worst RMSE ratio anywhere 1.297
G5 sharing never hurts: PASS  0 of 3452 surfels significantly worse (paired z > 4.5 over 16 repeats); largest z 2.62 at room A face 5 p=(3.125,3.875,3.000), RMSE shared/own rays there 1.336; worst RMSE ratio anywhere 1.341
G3 hard case lower: PASS  room A total squared error adaptive/fixed 0.081 (bar 0.5); relative RMSE room A median 0.851 -> 0.306, p95 1.915 -> 0.626; room B ratio 0.487, relative RMSE median 0.0551 -> 0.0379
G4 sharing stays on its face: PASS  summed sharing weight across the dividing wall 0, between faces at 90 degrees 0 (bar 1e-3 each, worst repeat)
G6 no light through the wall: PASS  room B switched off: largest relative change of a room A surfel 0 at nowhere (bar 1e-09, 2 repeats, same rays)
G7 no light round the corner: PASS  floor sun pool switched off: largest relative change of a room A floor surfel 0 at nowhere (bar 1e-09, 2 repeats, same rays)
rays per repeat: fixed 883712, adaptive 5188528 (x5.87; room A 4325856, room B 862672); surfels at the cap 899, surfels sharing 893, rounds <= 16; bake seconds per repeat fixed 0.70 adaptive 2.90; variant 18 s
--- red corner
G1 never-fewer: PASS  fewest rays on any surfel in any repeat 256 (floor 256); surfel-bakes under the floor 0; most 4096 (cap 4096)
G2 no surfel worse than fixed: PASS  0 of 3452 surfels significantly worse (paired z > 4.5 over 16 repeats); largest z 2.71 at room A face 0 p=(0.000,2.875,2.625), RMSE adaptive/fixed there 1.097; worst RMSE ratio anywhere 1.297
G5 sharing never hurts: PASS  0 of 3452 surfels significantly worse (paired z > 4.5 over 16 repeats); largest z 3.33 at room A face 1 p=(6.000,3.125,1.875), RMSE shared/own rays there 1.479; worst RMSE ratio anywhere 1.535
G3 hard case lower: PASS  room A total squared error adaptive/fixed 0.091 (bar 0.5); relative RMSE room A median 0.851 -> 0.313, p95 1.915 -> 0.628; room B ratio 0.487, relative RMSE median 0.0551 -> 0.0379
G4 sharing stays on its face: FAIL  summed sharing weight across the dividing wall 498.7, between faces at 90 degrees 291.4 (bar 1e-3 each, worst repeat)
G6 no light through the wall: FAIL  room B switched off: largest relative change of a room A surfel 0.278 at p=(6.000,2.875,0.125) face 1 (bar 1e-09, 2 repeats, same rays)
G7 no light round the corner: FAIL  floor sun pool switched off: largest relative change of a room A floor surfel 0.333 at p=(5.875,2.375,0.000) face 4 (bar 1e-09, 2 repeats, same rays)
rays per repeat: fixed 883712, adaptive 5188528 (x5.87; room A 4325856, room B 862672); surfels at the cap 899, surfels sharing 893, rounds <= 16; bake seconds per repeat fixed 0.70 adaptive 2.90; variant 15 s
red corner: FAILS as it must (G7 no light round the corner); failing checks: G4 sharing stays on its face, G6 no light through the wall, G7 no light round the corner
--- red wall
G1 never-fewer: PASS  fewest rays on any surfel in any repeat 256 (floor 256); surfel-bakes under the floor 0; most 4096 (cap 4096)
G2 no surfel worse than fixed: PASS  0 of 3452 surfels significantly worse (paired z > 4.5 over 16 repeats); largest z 2.72 at room A face 0 p=(0.000,2.875,2.625), RMSE adaptive/fixed there 1.098; worst RMSE ratio anywhere 1.297
G5 sharing never hurts: PASS  0 of 3452 surfels significantly worse (paired z > 4.5 over 16 repeats); largest z 2.67 at room A face 3 p=(5.875,4.000,1.375), RMSE shared/own rays there 1.141; worst RMSE ratio anywhere 1.341
G3 hard case lower: PASS  room A total squared error adaptive/fixed 0.081 (bar 0.5); relative RMSE room A median 0.851 -> 0.306, p95 1.915 -> 0.628; room B ratio 0.487, relative RMSE median 0.0551 -> 0.0379
G4 sharing stays on its face: FAIL  summed sharing weight across the dividing wall 15.64, between faces at 90 degrees 0 (bar 1e-3 each, worst repeat)
G6 no light through the wall: FAIL  room B switched off: largest relative change of a room A surfel 0.295 at p=(5.875,1.625,0.000) face 4 (bar 1e-09, 2 repeats, same rays)
G7 no light round the corner: PASS  floor sun pool switched off: largest relative change of a room A floor surfel 0 at nowhere (bar 1e-09, 2 repeats, same rays)
rays per repeat: fixed 883712, adaptive 5188528 (x5.87; room A 4325856, room B 862672); surfels at the cap 899, surfels sharing 893, rounds <= 16; bake seconds per repeat fixed 0.70 adaptive 2.86; variant 15 s
red wall: FAILS as it must (G6 no light through the wall); failing checks: G4 sharing stays on its face, G6 no light through the wall
--- red speed
G1 never-fewer: FAIL  fewest rays on any surfel in any repeat 64 (floor 256); surfel-bakes under the floor 24582; most 4096 (cap 4096)
G2 no surfel worse than fixed: FAIL  6 of 3452 surfels significantly worse (paired z > 4.5 over 16 repeats); largest z 4.97 at room B face 5 p=(9.475,1.625,3.000), RMSE adaptive/fixed there 3.302; worst RMSE ratio anywhere 3.951
G5 sharing never hurts: PASS  0 of 3452 surfels significantly worse (paired z > 4.5 over 16 repeats); largest z 2.02 at room A face 5 p=(4.375,3.375,3.000), RMSE shared/own rays there 1.023; worst RMSE ratio anywhere 2.503
G3 hard case lower: PASS  room A total squared error adaptive/fixed 0.472 (bar 0.5); relative RMSE room A median 0.851 -> 0.563, p95 1.915 -> 0.801; room B ratio 0.498, relative RMSE median 0.0551 -> 0.0395
G4 sharing stays on its face: PASS  summed sharing weight across the dividing wall 0, between faces at 90 degrees 0 (bar 1e-3 each, worst repeat)
G6 no light through the wall: PASS  room B switched off: largest relative change of a room A surfel 0 at nowhere (bar 1e-09, 2 repeats, same rays)
G7 no light round the corner: PASS  floor sun pool switched off: largest relative change of a room A floor surfel 0 at nowhere (bar 1e-09, 2 repeats, same rays)
rays per repeat: fixed 883712, adaptive 2954424 (x3.34; room A 2165524, room B 788900); surfels at the cap 459, surfels sharing 1012, rounds <= 16; bake seconds per repeat fixed 0.70 adaptive 1.76; variant 9 s
red speed: FAILS as it must (G1 never-fewer); failing checks: G1 never-fewer, G2 no surfel worse than fixed
peak memory: main 487 MB, largest worker 316 MB, 4 workers: bound 1750 MB
VERDICT:PASS (green PASS; reds corner fail, wall fail, speed fail) in 70 s
```

Ablation (scratch run with the stand-in test, same seeds, not part of the gate). Room A squared error as a fraction of the fixed bake's:
- shared estimate 0.081, own rays only 0.101. Sharing takes off another fifth.
- without the neighbor noise floor: shared 0.132, own rays 0.149, with 4.56M rays per repeat instead of 5.19M.

### What the numbers say

- **Total rays.** Fixed: 883,712 per bake. Adaptive: 5,188,528 (x5.87; the stand-in 5,189,872).
  - The hard room takes almost all of it: x9.8 (4.33M against 441k).
  - The easy room takes x1.95 (863k against 442k). The fixed bake's median relative RMSE there (5.5%) sits just over the
    5% bar, so about half its surfels take one or two more batches.
  - 899 surfels reach the cap (a single-repeat count: 876 in room A, 0 in room B). 893 of them share.
- **Quality.**
  - Room A's squared error falls to 8.1% of the fixed bake's. Median relative RMSE goes 0.85 -> 0.31, p95 1.92 -> 0.63.
  - Room B's falls to 48.7%.
  - No surfel is significantly worse anywhere: largest z 2.72 against a bar of 4.5. The worst RMSE ratio (1.30) is
    within what 16 repeats can resolve.
- **Bake-time cost.**
  - Rays: x5.87.
  - Wall clock in the twin:
    - x3.5 measured serially with the stand-in (`SMOOTHN1_WORKERS=1`: fixed 0.84 s, adaptive 2.93 s per bake).
    - x4.1 with 4 workers and GICAL1's test (0.70 vs 2.90 s).
    - GICAL1's depth maps cost 5.6 s once for 3452 surfels, the Python loop over its per-surfel ray generator.
  - The bookkeeping costs little next to the rays: per-round SE, two visibility passes over 30k pairs, and one sharing pass.
  - In production, the ray multiplier depends on how much of a cell is lit only through small openings. It must be
    measured on real cells (section 9).
- **The hard case still has a hard floor.** A surfel whose whole cell neighborhood missed the window in the floor pass
  cannot be flagged by any noise statistic. Only more floor rays find it. That is one more reason the floor never goes
  down. The neighbor floor narrows the gap; it does not close it.

## 7. History: what changed after the first run (stated plainly)

These bars were set before the first run and **never moved**: F = 256, cap 16 F, EPS_REL 0.05, TAU_FRAC 0.01,
Z_GATE 4.5, HARD_RATIO 0.5, ISO_TOL 1e-9 (set with G6/G7), and the G4 bar of 1e-3.

What did change, in order:
1. **First scene:** window on the far wall, no sun pool, a dimmer room B. Neighbor floor = median; sharing = a plain
   ray-pooled mean.
   - Green passed G1-G4.
   - But the `corner` red failed only the structural G4. Its smear was lost in the fixed bake's noise, so G2 did
     not see it.
   - The `wall` red changed nothing: no noisy surfel sat by the wall.
   - A diagnostic showed the median floor was 0 in most cells, so many window-missing surfels stopped at the floor.
2. **Scene changed to put contrast where the reds act.** The window moved near the dividing wall, the floor sun pool
   was added and room B made brighter. G5 ("sharing never hurts") was added.
3. **Green then FAILED G5:** 12-13 wall surfels along the sun pool's edge, where the plain pooled mean added
   gradient bias.
   - Fix 1: the neighbor floor became relative (cv2) and a mean. Still failed G5.
   - Fix 2: the blend became the minimum-error lambda of section 4, and the depth weight got a hard cut (VIS_CUT
     0.05, a new constant). Green passed.
4. **The lambda blend shrinks a smear's effect on error until G2/G5 cannot see it.** The corner red still smears
   33% onto one floor surfel, but 16 repeats do not resolve that statistically. So the isolation checks G6/G7 were
   added. Same rays, a light switched off where the surfel cannot see it, an exact answer. These are the checks the
   reds now fail. G4 (structural) fails for both too.
5. GICAL1 landed (30201d7) and its own functions replaced the stand-in as the default (section 5). The stand-in's
   both-way combination changed from min to product to match GICAL1 3.6. No bar moved, and every verdict is the same
   with both tests. The red `corner` now also fails G6 under GICAL1's test, which is how its missing behind-plane
   rule was found.
6. A floating-point artifact (z = inf on a surfel whose error is identical in every repeat) was fixed in G2/G5.

## 8. Sources: what was and was not read

**Not read.** The proxy blocked every copy of the slides that was tried:
- `media.contentapi.ea.com` (the SEED PDF)
- `www.ea.com/seed/news/siggraph21-global-illumination-surfels`
- `www.advances.realtimerendering.com/s2021/...Surfel GI.pdf`
- the Chalmers thesis "Diffuse Global Illumination using Surfels" (`odr.chalmers.se`)
- the summary pages at `evermotion.org` and `gxvtronics.altervista.org`

No public text describing the slides' short/long-term mean and variance estimator, their variance-driven ray
counts, or their irradiance sharing was found. **So nothing in this design is taken from those slides, and no
slide content is quoted or paraphrased.** The talk and its PICA PICA (2018) origin are known only from search-result
snippets: GIBS by Henrik Halen and Andreas Brinck, SIGGRAPH 2021 Advances, first built for PICA PICA mainly by
Tomasz Stachowiak.
- https://media.contentapi.ea.com/content/dam/ea/seed/presentations/seed-siggraph21-surfel-gi.pdf (blocked)
- https://www.ea.com/seed/news/siggraph21-global-illumination-surfels (blocked)

**Used, from general knowledge of public methods (not re-read in this lane):**
- Sample mean, variance and standard error; Welford's online variance.
- Chebyshev's inequality as a visibility test: Donnelly and Lauritzen, "Variance Shadow Maps", I3D 2006. The
  p^3 sharpening follows DDGI: Majercik et al., "Dynamic Diffuse Global Illumination with Ray-Traced Irradiance
  Fields", JCGT 8(2), 2019, https://jcgt.org/published/0008/02/01/
- The minimum-error blend of an unbiased and a biased estimator (standard shrinkage).
- Lambert's polygon form factor for the exact reference.

The rules here are this lane's own design, checked by the twin. If the GIBS slides become readable, compare
section 2's estimator and section 4's weights against them. Change the design only through the twin's gate.

## 9. Open questions, and what the local lane must still do with real cells

Open:
- **Where the gather's radiance comes from.** In production a surfel ray hits a surfel cell and reads its B from the
  relight (`src/probegi.cpp`), or sky through glass. The radiance field changes with lights and weather, so the
  noise does too. The bake is light-free by rule (PRTP_PLAN 2g: "the bake never stores light"). Options:
  - drive the ray counts from a reference lighting (the cell's own lights plus a neutral sky)
  - drive them from a light-independent proxy, such as the variance of hit albedo x facing, or the spread of hit
    distance
  - re-decide the counts per relight

  This must be decided before any C++.
- Whether the per-surfel gather replaces or feeds the probes' gather (BOUNCE2's surfel <- probe feed).
- EPS_REL 0.05 and TAU_FRAC 0.01 are twin choices. The visible bar depends on exposure (the imagespace, IMGS1).
- The grid cell (1 m) and the sharing radius (= surfel diameter): the real surfel cell is 70 units. Scale both, and
  check that GICAL1's clamp distance is >= the sharing radius.
- Does a value-range weight belong in the design too? It would hide the reds, which is why it is not in this design.
- Rotated-Fibonacci batches (section 2) instead of random rays: the twin did not test them.

The local lane (with real cells) must:
1. Implement the per-surfel gather (or decide it is the probe's gather) at a floor of 2048. Prove floor-only output
   identical to a fixed-2048 run (byte for byte, same rotations).
2. Count, per cell (Vault 111 cryo, Concord, the Museum interior):
   - surfels
   - rays fixed vs adaptive, and the multiplier
   - surfels at the cap
   - surfels sharing
   - bake wall clock before and after
3. Use GICAL1's test (already the twin's default), with whatever behind-plane rule GICAL1 settles on, and re-run G6/G7's idea on a real cell: switch a room's lights off, require no
   estimate in the next room to move. Use BAKE4's room ids to pick the rooms.
4. Build a per-surfel reference at 16x the cap on a sample of surfels (or the existing brute-force tracer,
   tests/prtp_reference.cpp, extended to surfels). Run G2's paired test on real surfels with a measured reference
   noise.
5. Keep the three reds (normal off, depth off, fewer rays) as switches of the real bake, the way `ProbeBakeSpec::red`
   does today.

## C++

None in this lane. The pieces that would be isolated and unit-testable are a Welford accumulator, the stop rule,
and the lambda blend. They are small enough that writing them before the open questions in section 9 are settled
would only fix choices too early. The interfaces above (sections 2-5) are what the C++ should implement.
