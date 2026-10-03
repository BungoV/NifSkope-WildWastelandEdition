# GICAL1 -- stopping GI light from leaking through walls, and filling pixels no surfel reaches

Lane GICAL1 (prep, cloud), 2026-10-03. Design + a Python twin (`tests/spells/gical1_check.py`). No product code
was changed. Everything here was measured on synthetic scenes built in code; no game data was used.

What this lane delivers:

1. a per-surfel **radial depth map** (layout, rays, storage, `.tbk` v5 bytes) and the **Chebyshev visibility test**
   that reads it, specified to the byte and to the formula, so another lane (SMOOTHN1) can implement the same thing;
2. the **gap fill**: what a pixel does when its surfel weights sum to less than 1;
3. a twin that proves both, with reds that fail;
4. a test plan for the second GICAL1 suspect: the GI term double-counting the game's own interior ambient (DALC).

Short answer for the reader in a hurry:

- The brief's default 4 x 4 map is **not enough** for walls of 2-4 units with surfels of radius 32 (measured: it
  removes only about half of the leak; section 5). The design ships **8 x 8 texels with a horizon warp** (k = 4),
  256 bytes a surfel. With it, every leak gate passes at wall thicknesses 2, 4 and 8, the bright side is unchanged,
  and the three reds fail.
- The gap fill works when it uses the **same depth test** as the main gather and a wider kernel. Without the test,
  the fill itself becomes a leak (measured: it doubled the leak excess at a 2-unit wall).
- Our renderer today does **not** gather surfels per pixel; it reads a probe-blended voxel grid. The depth map
  therefore has three concrete customers (section 4.1), and the leak in today's grid has a second cause the map
  does not touch (trilinear filtering across thin walls, section 2).

---

## 1. What was read

| Source | Read? | What it gave |
|---|---|---|
| Halen, Hayward et al., "Global Illumination based on Surfels" (GIBS), SIGGRAPH 2021 Advances, EA SEED. https://media.contentapi.ea.com/content/dam/ea/seed/presentations/seed-siggraph21-surfel-gi.pdf and the mirror https://www.advances.realtimerendering.com/s2021/SIGGRAPH%20Advances%202021%20-%20Surfel%20GI.pdf | **No.** Both blocked by the proxy (403 / egress blocked), as was the SEED news page https://www.ea.com/seed/news/siggraph21-global-illumination-surfels | Only a web search summary (surfels discretize the scene on the fly; irradiance is cached on them; ray guiding, binning, spatial filters). **Nothing in this document is taken from the slides.** The "radial depth", "moving averages of d and d^2", "clamped to the surfel diameter", "4 x 4" and "(1 - sum) x the cell's average" ideas come from the lane brief's own description; where this design goes further, it says so. |
| Majercik, Guertin, Nowrouzezahrai, McGuire, "Dynamic Diffuse Global Illumination with Ray-Traced Irradiance Fields", JCGT 8(2), 2019. https://jcgt.org/published/0008/02/01/ | **No** (jcgt.org and casual-effects.com blocked). | -- |
| NVIDIA RTXGI-DDGI SDK (the public reference code for DDGI), https://github.com/NVIDIAGameWorks/RTXGI-DDGI , commit f33e496c, files `rtxgi-sdk/shaders/ddgi/Irradiance.hlsl` and `ProbeBlendingCS.hlsl` | **Yes** (read only; its license is NVIDIA's, so no code was copied, only the math is described). | The visibility test as shipped: variance = abs(mean^2 - meanSq); if the distance to the probe exceeds the mean, weight = variance / (variance + (dist - mean)^2), then **cubed** ("increase the contrast"); a floor of 0.05; tiny weights crushed below 0.2. The distance texels are filled from ray hit distances **clamped to 1.5 x the probe spacing**, each ray weighted per texel by pow(cos, exponent), blended over time by hysteresis (a moving average). |
| Donnelly, Lauritzen, "Variance Shadow Maps", I3D 2006. https://www.punkuser.net/vsm/vsm_paper.pdf | **No** (blocked). | The one-sided Chebyshev bound P(d >= t) <= s^2 / (s^2 + (t - mu)^2) for t > mu is standard and is used here as written in the DDGI code above. |
| Duff, Burgess, Christensen, Hery, Kensler, Liani, Villemin, "Building an Orthonormal Basis, Revisited", JCGT 6(1), 2017. https://jcgt.org/published/0006/01/01/ | Not fetched (blocked); the formula is the well-known one and is spelled out in 3.2 and unit-tested (U2). | The tangent frame. |
| This repo: docs/PRTP_PLAN.md (2g, 2i, 2y, 2aj, 2ah, 2ak, 2an, 2r), docs/PRTP2_LIGHT_MODEL.md (4, 7), src/probegi.{h,cpp}, src/probebake.cpp (the `.tbk` writer), res/shaders/cell_lights.glsl, res/shaders/pbrm_default.frag, tests/spells/cell_gi_check.py, WW_CHANGES.md, docs/MISTAKES.md | Yes | Everything in section 2. |

---

## 2. Where light leaks and pixels go black today (what the code does)

The relight (src/probegi.cpp, plan 2i / 2aj) works in four steps. Which of them can leak:

| Step | What gathers from what | Visibility today | Can it leak through a thin wall? |
|---|---|---|---|
| Probe <- surfel (links) | the bake's own rays hit the surfel | exact (the ray found it); v4 keeps both sides of a thin wall apart (2y) | no |
| Surfel <- probe (bounce feed, 2aj) | probes within the blend radius | one BVH ray surfel -> probe, plus room ids | rarely (the ray is exact; rooms add a guard) |
| Voxel <- probe (the grid) | probes within the blend radius | one BVH ray from the **voxel's center** | **yes, at the voxel scale**: a voxel (48 units or more) whose center sits on the bright side of a 4-unit wall is valid and bright |
| Pixel <- voxel (shader `cellGiSample`) | 3 slabs x **trilinear** filtering, sampled half a voxel off the surface | none | **yes**: a dark-side pixel within one voxel of the wall blends in the bright-side voxel |

So the leak the user sees most likely comes from the last two rows, which are a grid-resolution problem. The surfel
depth map of this lane fixes leaks wherever something gathers **from a surfel without tracing a ray**. In our
pipeline that is:

1. a new per-pixel surfel apply pass (GIBS style, section 4.2): this is what replaces the trilinear grid read near
   surfaces and therefore removes the grid leak; the grid stays as the gap fill's source;
2. SMOOTHN1's neighbor sharing between surfels (cheap: no ray per pair);
3. FO4CS's in-game relight, which has no BVH (the reader comes last by standing order).

Black / magenta pixels. `cellGiE` returns 0 when the trilinear valid weight `s.a <= 0.01`, and the Pass view paints
those pixels magenta (2an: 40% of the magenta pixels at the Vault door have no valid voxel around them, median valid
weight 0.33-0.77 elsewhere). That is the "summed weight < 1" case of the brief; section 4.3 fills it.

---

## 3. The radial depth map (the spec another lane implements)

The importable twin of this section is in `tests/spells/gical1_check.py`: `basis`, `hemioct_encode`,
`hemioct_decode`, `depth_ray_dirs`, `depth_map_from_hits`, `pack_depth_record`, `unpack_depth_record`,
`depth_visibility`. Constants: `SIDE = 8`, `WARP = 4`, `PER_TEXEL = 8`, `LIFT = 1`, `SIGMA_MIN_FRAC = 0.01`,
`EXPONENT = 3`.

### 3.1 What it is

Each surfel (front and back sides alike) carries a small picture of its hemisphere. Each texel holds two numbers
from the short rays the bake fires from the surfel: the mean hit distance `mu` and the mean squared hit distance `m2`,
every distance clamped to `D`. `D` is the surfel's diameter: `D = 2 r`.

### 3.2 Layout

- **Origin**: `o = p + n * LIFT`, `LIFT = 1` game unit. `p`, `n` are the surfel's stored position and unit normal
  (the `.tbk` snorm16 normal, decoded and normalized: bake and reader must start from the same decoded normal).
- **Frame** (Duff et al. 2017): `s = +1 if n.z >= 0 else -1` (a negative zero counts as +1),
  `a = -1 / (s + n.z)`, `b = n.x n.y a`,
  `t = (1 + s n.x^2 a, s b, -s n.x)`, `bt = (b, s + n.y^2 a, -n.y)`.
  A world direction `w` has local coordinates `(w.t, w.bt, w.n)`.
- **Horizon warp**: before encoding, the local direction `(x, y, z)` becomes `(x, y, k max(z, 0))`, `k = 4`.
  A direction below the surfel's plane is read at the horizon. The warp gives the band near the horizon most of the
  texels (at 8 x 8 the outer ring of texels covers elevations from 0 up to 5-7 degrees with k = 4, instead of up to
18-25 degrees with k = 1). Thin-wall
  leaks are horizon queries: a receiver on the same floor or wall plane, on the other side of the wall.
- **Hemi-octahedral square**: `S = |x| + |y| + z` (after the warp), `px = x / S`, `py = y / S`,
  `u = px + py`, `v = px - py`; `(u, v)` is in `[-1, 1]^2`, the square's border is the horizon, its center the normal.
  Decode: `px = (u + v) / 2`, `py = (u - v) / 2`, `z = (1 - |px| - |py|) / k`, normalize `(px, py, z)`.
- **Texels**: `SIDE x SIDE = 8 x 8`. Texel `(i, j)` covers `u` in `[-1 + 2i/8, -1 + 2(i+1)/8]`, `v` likewise with `j`.
  Index `= j * 8 + i` (`i` along `u`).

### 3.3 Filling it (the bake)

- Rays: per texel, 8 directions uniform in the texel's `(u, v)` square, stratified as a 2 x 4 sub-grid with one jittered
  point per sub-cell, decoded to world. 512 rays a surfel, from `o`, each traced to at most `D`.
- `d = min(hit distance, D)`; a miss is `D`.
- Per texel: `mu = mean(d)`, `m2 = mean(d^2)` over its 8 rays. (GIBS and DDGI update an exponential moving
  average frame by frame; an offline bake's plain mean is what that average converges to.)
- A texel with no ray (cannot happen with this layout) stores `(D, D^2)`.
- Determinism: the bake's ray jitter must come from a seed per surfel (for example its cell key), never from thread
  order, so 1 thread and all threads stay byte-identical (the bake's standing rule, 2g).

### 3.4 Storage per surfel: 256 bytes

Per texel, in texel order: `uint16 round(65535 * mu / D)`, `uint16 round(65535 * m2 / D^2)`, both clamped to
0..65535. 64 texels x 4 bytes = **256 bytes**. (8-bit moments were rejected on paper: an 8-bit `m2` step of `D^2/255`
gives a variance error of about `(D/16)^2`, larger than the thin walls this exists for.) The twin round-trips every map
through these bytes before using it (U3 checks the round trip to half a step).

Sizes: Vault111Cryo has 26,978 unique surfels and Concord 26,865 (plan 2aj): about 6.9 MB each before the per-sector
duplicates. The rays: 512 short rays per surfel = 13.8 M rays per cell, each at most 140 units with r = 70.

### 3.5 `.tbk` v5: where the record goes

v5 = v4 unchanged + one tail block at the end. (v4 is the current version: header 64 bytes, v3 body, then back
surfels, link exts, probe exts, room boxes; reader `readTbk` in src/probegi.cpp, writer in src/probebake.cpp.)

- Header: `version = 5`. `reserved[0]` = back surfel count nb and `reserved[1]` = room box count nx, as in v4.
  `reserved[2]`: v4's bits (1 sides, 2 rooms, 4 doors, 8 glass) plus **bit 16 = depth maps**.
  `reserved[3]` = texels a side (8). `reserved[4]` = `D` as float32 bits. `reserved[5]` = warp `k` as float32 bits.
  (reserved[3..5] are zero in every v3 / v4 file written today: checked in the writer, only [0..2] are set.)
- Tail 5, after the room boxes: `(ns + nb)` depth records of `4 * side^2` bytes; the first `ns` belong to the front
  surfels in file order, the next `nb` to the back surfels in file order.
- Size = v4 size + `(ns + nb) * 4 * side^2` = `64 + 32 ns + 144 np + 12 nl + 32 nb + 8 nl + 32 np + 32 nx + 256 (ns + nb)`.
- A v4 reader that checks the exact size refuses v5 (as v3 readers refuse v4). `probebake --tbk 4` must keep
  writing v4 byte for byte; `--tbk 5` writes v5. The far map (2h) stays on v3.
- Readers dedupe surfels by position + normal (probegi.cpp's `SurfKey`). The same surfel written into two sector
  files gets the same record (same seed), so the first one read is kept.

### 3.6 The test at gather time

Inputs: the surfel `(p, n, map, D)` and a receiving point `x` with its own normal `nx`. The receiver is moved off its
surface first: `q = x + nx * 1` (the receiver's normal bias; for a probe, `q` is the probe position).

```
o    = p + n * LIFT
t    = |q - o|                         distance from the surfel's origin to the receiver
w    = (q - o) / t                     world direction; local (w.t, w.bt, w.n); warp; hemi-oct -> (u, v)
f    = clamp((u, v) + 1) * 0.5 * 8 - 0.5, [0, 7]     texel-center coordinates, clamped at the border
(mu, m2) = bilinear blend of the 4 texels around f    (blend the stored moments, not the visibilities)
var  = max(m2 - mu^2, (0.01 D)^2)
vis  = 1                                if t <= mu
       (var / (var + (t - mu)^2))^3     otherwise
```

No floor (DDGI keeps 0.05 so that a pixel never ends with no probe; here the gap fill handles that). No crush.
Beyond `D` a surfel cannot vouch for anything: `mu <= D < t`, and the test returns nearly 0. That is intended.

For SMOOTHN1: when surfel A shares with neighbor B, test **both ways** (`vis(A -> q_B)` and `vis(B -> q_A)`, with
`q = p + n * 1`) and take the product. The twin's reds show the test is direction-sensitive (reading the map the wrong
way round lets the wall through, U4r and the `flip` red).

---

## 4. Using it

### 4.1 Who calls the test

| Caller | Today | With the map |
|---|---|---|
| Pixel apply pass (new, 4.2) | does not exist; pixels read the voxel grid | the test per surfel in the pixel's cell |
| SMOOTHN1 neighbor sharing | -- | both-way test, 3.6 |
| BOUNCE2 surfel <- probe feed | BVH ray + room ids | keep the ray in NifSkope (exact and cheap here); the map is the FO4CS runtime's substitute |
| Voxel <- probe (grid) | BVH ray from the voxel center | unchanged (a probe has no surfel map); see open question 3 |

### 4.2 The pixel apply (GIBS-style; formulas as the twin runs them)

Each surfel carries its irradiance `E_i` per side (in our relight: the n^2 blend of its feeding probes' axes, the
value BOUNCE2 already computes as `E_feed`; or the six-axis grid sampled at the surfel). For a pixel `x`, normal `nx`,
over the surfels listed in the pixel's hash cell (cell size `D`; a surfel is listed in every cell its `D`-sphere
touches):

```
q    = x + nx * 1
d_i  = |q - p_i|
w_i  = (1 - (d_i / r)^2)^2 * max(0, n_i . nx) * vis_i(q)        for d_i < r, else 0
W    = sum w_i
E(x) = sum w_i E_i / W                                           if W >= 1
     = sum w_i E_i + (1 - W) * E_fill(x)                         if W < 1        (the gap fill, 4.3)
```

r = the surfel radius. The twin uses r = the lattice spacing (32); for our bake r = the surfel cell size, 70 units,
so D = 140. The thickness / radius ratios tested (2/32 to 8/32 = 0.06 to 0.25) cover 4- to 16-unit walls at r = 70.

### 4.3 The gap fill

```
f_i       = (1 - min(d_i / D, 1)^2)^2 * max(0, n_i . nx) * vis_i(q)^3     over the same cell list (d_i < D)
E_fill(x) = sum f_i E_i / sum f_i          (0 when the sum is 0: nothing in reach, the pixel stays black)
```

This is the "weighted average of its grid cell": the cell's surfels, but weighted by distance and by the **same
depth test**, with the visibility cubed once more. Both choices were measured (section 5): a flat, untested cell
average leaks from the other side of the wall into every corner pixel (corners always have W < 1). The cube on vis
cut the remaining fill leak at a 2-unit wall from 0.120 to 0.046 of the dark side's light.

For the **existing grid path** (no surfel pass yet), the same formula with the grid's valid weight standing in for
W. `s = cellGiSample(P, N)` already holds `rgb = E x valid` and `a = the trilinear valid weight`, so:

```
E = s.rgb / s.a                          if s.a >= 1      (today's result)
  = s.rgb + (1 - s.a) * E_coarse(P, N)   if s.a < 1       (today: s.rgb / s.a, and 0 below 0.01)
```

`E_coarse` = a second, coarser grid (4 x the voxel) built on the CPU, each coarse cell and slab = the mean of its
valid fine voxels. That removes the magenta/black pixels. Two things are **not tested here** (open question 2): it
changes every partly valid pixel (today those are renormalized by `s.a`), and the coarse level can itself carry a
bright voxel across a wall. The twin only proves the surfel-pass version.

---

## 5. The twin and what it measured

`python3 tests/spells/gical1_check.py` (numpy only, about 60 s, peak 485 MB). Exit 0 only if every green row passes
and every red fails. `GICAL1_RED=depthoff|gapoff|flip` runs one red alone (it prints FAIL and exits 1, as it must).
`GICAL1_SIDE` / `GICAL1_WARP` override the layout for measurement runs.

### 5.1 The scene (built from solid boxes, units = game units)

- Ground slab. Room A (x 0..256, y 0..256, walls 192 high), **open to the sun**. Room B (x 256+T..512+T) shares A's
  east wall, **thickness T = 2, 4 and 8**, roofed, lit only through a north window (x 336..496, z 32..184) where the sun
  lands on its floor by the north-east corner: dim, not black (its reference near the shared wall is about 3% of
  room A's).
- An L corridor off room A's south door (x 96..160): leg 1 runs south, turns east at the corner into leg 2. Roofed,
  walls T thick. Sunlit open ground lies on the other side of the corridor's inner-corner wall and of leg 2's north wall.
- Light: sun only, direction (-0.35, 0.25, 0.9), irradiance 3, albedo 0.6; sky radiance 0. One bounce: every surface's
  radiosity `B = albedo * sun * max(0, n.L)` behind a shadow ray.
- Surfels: a lattice of 32 units on every air-facing face (1546 surfels at T = 2), radius 32, D = 64. Each surfel's
  `E_i` from 1024 cosine rays; its depth map from 512 rays as in 3.3, round-tripped through the 256-byte record.
- Reference: brute-force irradiance, 2048 cosine rays at every sample point.
- Sample sets: **dark** = room B floor at 2..28 units from the shared wall plus its north and south wall faces at the same
  distances (105 points); **bright** = the mirror in room A; **corridor** = leg 2 just past the corner (floor + north
  wall) and leg 1 beside the inner corner wall; **gap** = 36 points of room A's floor where 4 floor surfels were removed
  (every one of them has surfel weight 0).
- **Control** (X): the same gather with the depth test replaced by the true segment test (a ray from the surfel's
  origin to `q`). It is what a perfect depth map would give at this surfel density, so it carries the method's own
  error (coarse surfels, corners) and leaves only the map's error to be gated.
- Metric: `rel err = mean |E - ref| / mean ref` over a set.

### 5.2 The checks (bars as they stand; history in 5.4)

| Row | Proves | Bar |
|---|---|---|
| U1 | the hemi-oct encode/decode (with the warp) round-trips | max err < 1e-12 |
| U2 | the frame is orthonormal for axis normals, n.z = -0 and -1, 2000 random ones | max err < 1e-12 |
| U3 | the 256-byte record round-trips | mean within half a 16-bit step |
| U4 | known answer: a floor surfel 10 units from a 4-unit wall; points behind it -> 0, points in the open -> 1 | behind < 0.01; open = 1 |
| U4r | its red: the map read the wrong way round lets the wall through | must read > 0.5 |
| L (T = 2, 4, 8) | the dark side reads what a perfect test would | rel err <= control + 0.10 |
| B | the bright side is unchanged by the test | rel err <= control + 0.02 and mean within 5% of the control |
| C | the corridor: leaks across the corner walls stopped, the light around the corner kept | rel err <= control + 0.10 and mean >= 0.9 x the control |
| G | the gap: the pixels have no surfel; filled, they read near the reference | max surfel weight < 0.05; rel err <= (same pixels with every surfel) + 0.10; mean 0.8..1.25 x the reference |

### 5.3 Results (the green run; the full output is in Appendix A)

| T | L: dark rel err (control) | dark mean: test / control / no test / ref | B: bright rel err (control) | C: corridor (control), mean ratio | G: gap rel err (all surfels), mean ratio |
|---|---|---|---|---|---|
| 2 | 0.310 (0.278) | 0.0118 / 0.0113 / 0.0394 / 0.0101 | 0.095 (0.095) | 0.571 (0.568), 1.001 | 0.080 (0.066), 1.032 |
| 4 | 0.248 (0.247) | 0.0115 / 0.0115 / 0.0285 / 0.0105 | 0.093 (0.093) | 0.638 (0.638), 1.000 | 0.101 (0.056), 1.019 |
| 8 | 0.203 (0.203) | 0.0106 / 0.0106 / 0.0148 / 0.0099 | 0.098 (0.098) | 0.611 (0.611), 1.000 | 0.069 (0.048), 1.043 |

Without the test the dark side reads 3.9x / 2.7x / 1.5x its reference at T = 2 / 4 / 8 (the bright side is 35x
brighter). With it, the excess over the perfect test is 0.046 / 0.003 / 0.000 of the dark side's own light.

The reds (every one fails, as it must):

| Red | What it breaks | First failing rows |
|---|---|---|
| `depthoff` | no depth test (every surfel visible) | L at all T (rel err 3.004 / 1.817 / 0.583), B at all T (mean 11.6% off), C at T = 2, 4 |
| `gapoff` | no gap fill: W < 1 adds nothing | G at all T (rel err 1.000: the gap reads 0), B (corner pixels lose their share), C (mean 0.83 of the control) |
| `flip` | the map read in the opposite direction | L at all T (3.077 / 2.276 / 1.817), B, C at T = 2, 4 |

`gapoff` passes L (the fill is a small leak source), which is why G exists: turning the fill off is not a way to pass.

The corridor's control error is high (0.57-0.64) because the corridor is lit by a few bright patches seen through the
door and surfels of 32 units average them out; that is the method's error, identical with the perfect test, so it
does not mask the map. The corridor row only discriminates at T = 2 and 4: at T = 8 even `depthoff` passes C
(the corner walls' far side is mostly shaded at that thickness). Said plainly: C at T = 8 proves only "not over-blocked".

### 5.4 What was set or moved after a first run (read this before trusting the margins)

The design was tuned on the same scene that gates it. In order:

1. First run, 4 x 4 unwarped, flat fill, bars = "dark rel err <= max(2 x bright rel err, 0.15)", "bright mean moves
   <= 5% from the no-test gather", corridor "mean >= 0.8 x reference", gap "rel err <= 0.20". **Green FAILED**; the
   reds failed too.
   - Room B's reference was 0.002 (0.6% of room A: an east window the sun never entered) and the gap patch read 0. A
     relative error on a near-zero reference measures noise. **Scene changed**: the window moved north and grew so the
     sun enters; the gap moved to room A's floor.
   - "Bright mean moves <= 5% from the no-test gather" failed because the test correctly removes the dark room's
     surfels from bright pixels. **Bar changed** to compare against the perfect-test control (X), and the corridor's
     ratio likewise (>= 0.9 x the control).
   - "2 x bright error" mixed two different geometries. **Bar changed** to "control + 0.10".
2. Sweep (texels a side x warp, the dark set at T = 2 and 4, 512 surfel rays, with the distance-weighted fill):

   | layout | T = 2: dark rel err (control 0.24) | T = 4 (control 0.25) | bytes/surfel |
   |---|---|---|---|
   | 4 x 4, k = 1 (the brief's default) | 1.66 | 0.68 | 64 |
   | 4 x 4, k = 4 | 0.97 | 0.52 | 64 |
   | 6 x 6, k = 1 | 0.76 | 0.33 | 144 |
   | 6 x 6, k = 4 | 0.58 | 0.30 | 144 |
   | 8 x 8, k = 1 | 0.38 | 0.26 | 256 |
   | 8 x 8, k = 4 | 0.32 | 0.26 | 256 |

   **Layout chosen from this table: 8 x 8, k = 4.** With the shipped twin, `GICAL1_SIDE=4 GICAL1_WARP=1` still FAILS L
   at T = 2 (1.666 vs bar 0.378) and T = 4 (0.679 vs 0.347) and B at T = 2 (mean 6.0% off).
   Why 4 x 4 fails: the worst pair at T = 2 is a wall surfel of room A's south wall and a receiver on room B's south
   wall, both on the same plane, the shared wall standing at 45 degrees to the line between them. The receiver sits on
   the horizon, in a texel whose rays hit that wall anywhere from about 18 to 32 units away, so the stored mean is close
   to the receiver's own distance (28) and Chebyshev lets it through: measured vis 1.00 for that pair at 4 x 4
   unwarped, 0.06 at 8 x 8 with the warp.
3. Fill: the first fill was a flat average over every surfel within D. Far surfels (40-56 units, the maps least sure)
   leaked through it into the dark corners, which always have W < 1. A distance kernel was added before the sweep;
   with it and vis^1 the dark rel err at T = 2 (8 x 8) was 0.374, while the no-fill red read 0.244. vis^3 in the fill
   brought it to 0.310 (excess over the control 0.120 -> 0.046 of the dark side). **Design changed** (4.3).
4. Gap bar: the gap's per-pixel reference is noisy even on room A's floor; the absolute 0.20 bar **was replaced** by
   "the same pixels gathered with every surfel + 0.10" and a mean ratio of 0.8..1.25.

Nothing moved after the final green run. Tightest margins: L at T = 2 (0.310 vs 0.378) and G at T = 4 (0.101 vs 0.156).

---

## 6. The second suspect: GI double-counting the interior ambient (DALC) -- a test plan for real cells

### 6.1 Why it is a suspect (from the code)

The game lights an interior pixel as `albedo x (ambient(n) x AO + direct lights)` (PRTP2 7): the cell's six DALC
colors (XCLL or the lighting template) are the level designer's stand-in for all the light that bounced. Our cell view
adds **both** that ambient and our own bounce:

- legacy path (`cellLit`, res/shaders/cell_lights.glsl): `Ed = diffOn + gi`, then `+ cellAmbient(N, P)` when
  `cellHasDalc`; the cube reflection is multiplied by `Ed`, so it doubles too;
- PBR path (pbrm_default.frag, interior branch): `outDiff = cDiff + giDiff`, then `+ cellAmbient x rho x keepInd x ao`.

Outdoors this was already settled (SKY1, plan 2ah): the grid's sky **replaces** the weather's ambient by its valid share
(`cellGiSkyK`). Indoors nothing is replaced. If the DALC is the designer's bounce, our interiors are lit about twice
in every place where bounce dominates.

### 6.2 What to compare (per cell: Vault111Cryo, DmndSolomonsHouse01, ConcordMuseum01; the gate cameras already in
cell_gi.sh / cell_lit.sh)

Existing harness probes (`WW_CELL_LIT_PROBE`): 5 = the GI irradiance / pi, 11 = the DALC affine sum x 8 (before its
2.2), 1 = the direct irradiance / 4, 2 + 3 = position. The checker turns 11 into `ambient = (v / 8)^2.2` per channel
(clamp aware: pixels at 1.0 are refused) and 5 into `gi = v x pi`.

1. **The ratio map.** Per clean pixel: `share = gi / (gi + ambient)` (both as irradiance on the pixel's normal). Report
   the median and p10/p90 per cell, separately for pixels where direct light is small (`probe 1 < 0.02`, the bounce-only
   regions) and where it is large. If the DALC is a bounce stand-in, `gi` and `ambient` are the same order in the
   bounce-only regions (share near 0.5): that is a double count by construction.
2. **The energy check, no game needed.** Render the lit picture four ways at the same camera: (a) today, (b) GI off,
   (c) DALC off (needs a pin, see 6.3), (d) neither. In bounce-only regions: `(a) - (d)` must equal `((b) - (d)) +
   ((c) - (d))` within GPU noise (linear sum: proves the two terms simply add). Then the number to report is
   `((c) - (d)) / ((b) - (d))`: how much of the designer's ambient our bounce already supplies. Near 1 = full double count.
3. **Against the game (when the PRTP4 capture flight exists).** The same cameras in game. In bounce-only regions,
   the game's linear luminance against (a), (b), (c). Expected if double-counted: the game matches (b) (DALC only) and
   (a) reads about `1 + gi/ambient` times the game.
4. **Candidate rules, measured on the same views, nothing shipped by this plan:** (i) replace like SKY1:
   `ambient x (1 - k)` with `k` = the grid's valid share; (ii) `max(ambient, gi)`; (iii) DALC off where the grid is valid.
   Report each one's mean luminance against the game capture per region.

### 6.3 What the local lane must add for it

- A pin for the interior ambient (for example `WW_CELL_GI_AMB=keep|replace|off`), off by default = today byte for byte.
- A red that proves the measure sees a double count: force `ambient := gi` (a pin) and require share = 0.5 and the
  step-2 ratio = 1; and the GI-off red (`WW_CELL_GI_PASSES=0` or the row off) must give share = 0.
- Ambient Only volumes (2r) scale the DALC per pixel: probe 11 already carries that scale, so the ratio map includes it;
  report Vault111Cryo inside and outside its three spheres separately.

---

## 7. Open questions

1. **Bake cost and determinism of 13.8 M short rays a cell**: not measured here (no product build). Seed per surfel
   from its cell key, never thread order.
2. **The grid-path gap fill** (4.3, second half) is not twin-tested: a coarse level built from valid voxels could still
   carry a bright voxel across a wall. Either gate it like G, or retire the grid read near surfaces once the surfel pass
   exists.
3. **The grid's own trilinear leak** (section 2) is untouched by the depth map. A cheap independent fix: store per
   voxel the room id (2y room boxes) and make the shader drop a trilinear neighbor whose room differs from the pixel's
   (or bias the sample point by the normal more than half a voxel). Not measured here.
4. **What `E_i` is per surfel** in our relight for the apply pass: the n^2 blend of its fed probes (BOUNCE2) or a fresh
   gather of the six-axis grid at the surfel. The twin used each surfel's own traced irradiance.
5. **Margins**: L at T = 2 passes by 0.07 on a scene the design was tuned on (5.4). The local lane should rerun the
   leak measure on a real cell with known thin walls (the Vault's partition walls) before trusting it.
6. **The brief said 4 x 4 by default.** The measurement says 8 x 8 with the warp, 4x the bytes. Is 6.9 MB a cell
   acceptable for the `.tbk` folder, or should 6 x 6 + warp (144 bytes, still failing at 2-unit walls) be the default
   with 8 x 8 only for interiors? bungo's call.
7. Whether GIBS itself uses a hemisphere or a full sphere, the same clamp, or a horizon warp: unknown (slides not read).

---

## 8. What the local lane must still do

1. `src/probebake.cpp`: per surfel (front and back), the 512 clamped rays of 3.3 through the existing BVH, seeded by the
   surfel's key; write `.tbk` v5 (3.5) behind `--tbk 5`; `--tbk 4` byte-identical to today (gate: probe_bake.py synth
   with `--tbk 4` against the exe from before the lane).
2. `tests/spells/probe_bake.py`: read v5; re-trace the depth records of 400 sampled surfels with its own tracer and the
   spec (`from gical1_check import depth_ray_dirs, depth_map_from_hits`), and compare moments (mean within 1 unit at
   D = 140 per texel, allowing for the jitter seed being the same). Red: write the map with the frame's sign rule flipped.
3. `src/probegi.cpp`: `readTbk` accepts v5 and keeps the records per unique surfel; expose them (dump `gi_depth.bin`).
4. The pixel apply pass (4.2 + 4.3) in the cell shaders as a new GI row option, with the surfel buffer and a hash grid of
   cell size D in texture buffers; the Pass view keeps magenta only where `E_fill` has nothing in reach.
5. Gate on real cells: probe 5 with the apply pass against an independent Python twin of the pass (this file's
   `depth_visibility` + `gather`), and the leak measure at the Vault's partition walls (pixels within 2 r of a wall,
   compared with the same pass using a BVH segment test as the control, as in 5.1). Reds `depthoff`, `gapoff`, `flip`.
6. Run the DALC plan (section 6) and bring the numbers to bungo before any rule changes.
7. SMOOTHN1: use 3.6 as written (both-way product); import the twin's functions for its own gate.

---

## Appendix A -- the twin's output (green run plus the three reds, as run for this document)

```
$ python3 tests/spells/gical1_check.py
PASS U1 hemi-oct round trip  max err 3.33e-16 (bar 1e-12)
PASS U2 tangent frame orthonormal  max err 4.82e-16
PASS U3 256-byte record round trip  256 bytes, mean err 4.85e-04 (bar half a step 4.88e-04)
PASS U4 known answer: wall in between  behind the wall max 0.0000 (bar < 0.01), open side min 1.000 (bar 1)
PASS U4r red flip lets the wall through  flipped vis behind the wall 1.000 (must be > 0.5)
baked 3 scenes (58.0 s)
scene: 1546 surfels (T=2), radius 32, spacing 32, D 64, 8x8 texels x 8 rays, 1024 surfel rays, 2048 ref rays
--- green
PASS L T=2 leak, dark side  rel err 0.310 vs control 0.278 (bar control + 0.10); dark ref mean 0.0101, gathered 0.0118, control 0.0113; excess over control 0.046 of the dark ref; bright ref mean 0.3525
PASS B T=2 bright side unchanged  rel err 0.095 vs control 0.095 (bar control + 0.02); mean off the control 0.0026 (bar 0.05); ref mean 0.3525
PASS C T=2 corridor corner  rel err 0.571 vs control 0.568 (bar control + 0.10); gathered/control mean 1.001 (bar >= 0.9: not over-blocked); ref mean 0.0141, gathered 0.0110
PASS G T=2 gap fill  gap pixels 36, max surfel weight 0.000 (bar < 0.05: a real gap); rel err 0.080 vs 0.066 with every surfel (bar + 0.10); filled/ref mean 1.032 (bar 0.8..1.25); ref mean 0.1414; 4 surfels removed
PASS L T=4 leak, dark side  rel err 0.248 vs control 0.247 (bar control + 0.10); dark ref mean 0.0105, gathered 0.0115, control 0.0115; excess over control 0.003 of the dark ref; bright ref mean 0.3541
PASS B T=4 bright side unchanged  rel err 0.093 vs control 0.093 (bar control + 0.02); mean off the control 0.0001 (bar 0.05); ref mean 0.3541
PASS C T=4 corridor corner  rel err 0.638 vs control 0.638 (bar control + 0.10); gathered/control mean 1.000 (bar >= 0.9: not over-blocked); ref mean 0.0144, gathered 0.0119
PASS G T=4 gap fill  gap pixels 36, max surfel weight 0.000 (bar < 0.05: a real gap); rel err 0.101 vs 0.056 with every surfel (bar + 0.10); filled/ref mean 1.019 (bar 0.8..1.25); ref mean 0.1418; 4 surfels removed
PASS L T=8 leak, dark side  rel err 0.203 vs control 0.203 (bar control + 0.10); dark ref mean 0.0099, gathered 0.0106, control 0.0106; excess over control 0.000 of the dark ref; bright ref mean 0.3550
PASS B T=8 bright side unchanged  rel err 0.098 vs control 0.098 (bar control + 0.02); mean off the control 0.0000 (bar 0.05); ref mean 0.3550
PASS C T=8 corridor corner  rel err 0.611 vs control 0.611 (bar control + 0.10); gathered/control mean 1.000 (bar >= 0.9: not over-blocked); ref mean 0.0147, gathered 0.0119
PASS G T=8 gap fill  gap pixels 36, max surfel weight 0.000 (bar < 0.05: a real gap); rel err 0.069 vs 0.048 with every surfel (bar + 0.10); filled/ref mean 1.043 (bar 0.8..1.25); ref mean 0.1445; 4 surfels removed
--- red depthoff
FAIL L T=2 leak, dark side  rel err 3.004 vs control 0.278 (bar control + 0.10); dark ref mean 0.0101, gathered 0.0394, control 0.0113; excess over control 2.781 of the dark ref; bright ref mean 0.3525
FAIL B T=2 bright side unchanged  rel err 0.138 vs control 0.095 (bar control + 0.02); mean off the control 0.1156 (bar 0.05); ref mean 0.3525
FAIL C T=2 corridor corner  rel err 0.712 vs control 0.568 (bar control + 0.10); gathered/control mean 1.076 (bar >= 0.9: not over-blocked); ref mean 0.0141, gathered 0.0118
PASS G T=2 gap fill  gap pixels 36, max surfel weight 0.000 (bar < 0.05: a real gap); rel err 0.080 vs 0.066 with every surfel (bar + 0.10); filled/ref mean 1.032 (bar 0.8..1.25); ref mean 0.1414; 4 surfels removed
FAIL L T=4 leak, dark side  rel err 1.817 vs control 0.247 (bar control + 0.10); dark ref mean 0.0105, gathered 0.0285, control 0.0115; excess over control 1.627 of the dark ref; bright ref mean 0.3541
FAIL B T=4 bright side unchanged  rel err 0.147 vs control 0.093 (bar control + 0.02); mean off the control 0.1159 (bar 0.05); ref mean 0.3541
FAIL C T=4 corridor corner  rel err 0.749 vs control 0.638 (bar control + 0.10); gathered/control mean 1.047 (bar >= 0.9: not over-blocked); ref mean 0.0144, gathered 0.0124
PASS G T=4 gap fill  gap pixels 36, max surfel weight 0.000 (bar < 0.05: a real gap); rel err 0.101 vs 0.056 with every surfel (bar + 0.10); filled/ref mean 1.019 (bar 0.8..1.25); ref mean 0.1418; 4 surfels removed
FAIL L T=8 leak, dark side  rel err 0.583 vs control 0.203 (bar control + 0.10); dark ref mean 0.0099, gathered 0.0148, control 0.0106; excess over control 0.417 of the dark ref; bright ref mean 0.3550
FAIL B T=8 bright side unchanged  rel err 0.144 vs control 0.098 (bar control + 0.02); mean off the control 0.1161 (bar 0.05); ref mean 0.3550
PASS C T=8 corridor corner  rel err 0.688 vs control 0.611 (bar control + 0.10); gathered/control mean 0.996 (bar >= 0.9: not over-blocked); ref mean 0.0147, gathered 0.0119
PASS G T=8 gap fill  gap pixels 36, max surfel weight 0.000 (bar < 0.05: a real gap); rel err 0.069 vs 0.048 with every surfel (bar + 0.10); filled/ref mean 1.043 (bar 0.8..1.25); ref mean 0.1445; 4 surfels removed
--- red gapoff
PASS L T=2 leak, dark side  rel err 0.244 vs control 0.278 (bar control + 0.10); dark ref mean 0.0101, gathered 0.0106, control 0.0113; excess over control -0.070 of the dark ref; bright ref mean 0.3525
FAIL B T=2 bright side unchanged  rel err 0.136 vs control 0.095 (bar control + 0.02); mean off the control 0.1108 (bar 0.05); ref mean 0.3525
FAIL C T=2 corridor corner  rel err 0.581 vs control 0.568 (bar control + 0.10); gathered/control mean 0.827 (bar >= 0.9: not over-blocked); ref mean 0.0141, gathered 0.0091
FAIL G T=2 gap fill  gap pixels 36, max surfel weight 0.000 (bar < 0.05: a real gap); rel err 1.000 vs 0.066 with every surfel (bar + 0.10); filled/ref mean 0.000 (bar 0.8..1.25); ref mean 0.1414; 4 surfels removed
PASS L T=4 leak, dark side  rel err 0.234 vs control 0.247 (bar control + 0.10); dark ref mean 0.0105, gathered 0.0110, control 0.0115; excess over control -0.040 of the dark ref; bright ref mean 0.3541
FAIL B T=4 bright side unchanged  rel err 0.144 vs control 0.093 (bar control + 0.02); mean off the control 0.1111 (bar 0.05); ref mean 0.3541
FAIL C T=4 corridor corner  rel err 0.643 vs control 0.638 (bar control + 0.10); gathered/control mean 0.837 (bar >= 0.9: not over-blocked); ref mean 0.0144, gathered 0.0099
FAIL G T=4 gap fill  gap pixels 36, max surfel weight 0.000 (bar < 0.05: a real gap); rel err 1.000 vs 0.056 with every surfel (bar + 0.10); filled/ref mean 0.000 (bar 0.8..1.25); ref mean 0.1418; 4 surfels removed
PASS L T=8 leak, dark side  rel err 0.205 vs control 0.203 (bar control + 0.10); dark ref mean 0.0099, gathered 0.0106, control 0.0106; excess over control -0.003 of the dark ref; bright ref mean 0.3550
FAIL B T=8 bright side unchanged  rel err 0.142 vs control 0.098 (bar control + 0.02); mean off the control 0.1109 (bar 0.05); ref mean 0.3550
FAIL C T=8 corridor corner  rel err 0.616 vs control 0.611 (bar control + 0.10); gathered/control mean 0.836 (bar >= 0.9: not over-blocked); ref mean 0.0147, gathered 0.0100
FAIL G T=8 gap fill  gap pixels 36, max surfel weight 0.000 (bar < 0.05: a real gap); rel err 1.000 vs 0.048 with every surfel (bar + 0.10); filled/ref mean 0.000 (bar 0.8..1.25); ref mean 0.1445; 4 surfels removed
--- red flip
FAIL L T=2 leak, dark side  rel err 3.077 vs control 0.278 (bar control + 0.10); dark ref mean 0.0101, gathered 0.0401, control 0.0113; excess over control 2.856 of the dark ref; bright ref mean 0.3525
FAIL B T=2 bright side unchanged  rel err 0.140 vs control 0.095 (bar control + 0.02); mean off the control 0.1160 (bar 0.05); ref mean 0.3525
FAIL C T=2 corridor corner  rel err 0.710 vs control 0.568 (bar control + 0.10); gathered/control mean 1.077 (bar >= 0.9: not over-blocked); ref mean 0.0141, gathered 0.0118
PASS G T=2 gap fill  gap pixels 36, max surfel weight 0.000 (bar < 0.05: a real gap); rel err 0.080 vs 0.066 with every surfel (bar + 0.10); filled/ref mean 1.032 (bar 0.8..1.25); ref mean 0.1414; 4 surfels removed
FAIL L T=4 leak, dark side  rel err 2.276 vs control 0.247 (bar control + 0.10); dark ref mean 0.0105, gathered 0.0338, control 0.0115; excess over control 2.129 of the dark ref; bright ref mean 0.3541
FAIL B T=4 bright side unchanged  rel err 0.149 vs control 0.093 (bar control + 0.02); mean off the control 0.1160 (bar 0.05); ref mean 0.3541
FAIL C T=4 corridor corner  rel err 0.747 vs control 0.638 (bar control + 0.10); gathered/control mean 1.049 (bar >= 0.9: not over-blocked); ref mean 0.0144, gathered 0.0125
PASS G T=4 gap fill  gap pixels 36, max surfel weight 0.000 (bar < 0.05: a real gap); rel err 0.101 vs 0.056 with every surfel (bar + 0.10); filled/ref mean 1.019 (bar 0.8..1.25); ref mean 0.1418; 4 surfels removed
FAIL L T=8 leak, dark side  rel err 1.817 vs control 0.203 (bar control + 0.10); dark ref mean 0.0099, gathered 0.0275, control 0.0106; excess over control 1.705 of the dark ref; bright ref mean 0.3550
FAIL B T=8 bright side unchanged  rel err 0.147 vs control 0.098 (bar control + 0.02); mean off the control 0.1165 (bar 0.05); ref mean 0.3550
PASS C T=8 corridor corner  rel err 0.691 vs control 0.611 (bar control + 0.10); gathered/control mean 1.004 (bar >= 0.9: not over-blocked); ref mean 0.0147, gathered 0.0120
PASS G T=8 gap fill  gap pixels 36, max surfel weight 0.000 (bar < 0.05: a real gap); rel err 0.069 vs 0.048 with every surfel (bar + 0.10); filled/ref mean 1.043 (bar 0.8..1.25); ref mean 0.1445; 4 surfels removed
--- verdict (58.9 s)
red depthoff fails, as it must
red gapoff   fails, as it must
red flip     fails, as it must
GICAL1 PASS
(exit status 0; peak resident memory 485 MB, measured with resource.getrusage)
```
