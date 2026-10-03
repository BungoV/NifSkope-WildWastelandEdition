# GPURELIGHT1 -- relight the baked surfels and probes on the GPU, without rebaking

Lane GPURELIGHT1 (cloud prep), 2026-10-03. Research and a Python twin. No C++ in this lane.
Twin: `tests/spells/gpurelight1_check.py` (python3 + numpy, synthetic scene, about 46 s, peak 174 MB).
Builds on docs/PRTP_PLAN.md 2g (the bake), 2i (PRTPGI relight), 2aj (BOUNCE2 passes), docs/PRTP2_LIGHT_MODEL.md
(the curves), and what `src/probegi.cpp` does today.

## 1. What we want

Today the cell view relights the bake on the CPU (`probeGiRelight`, 0.1-0.5 s per relight, plan 2aj). That is fine
for a still picture. We want the same answer every frame while lights change: a lamp switched off, a color
changed, a flickering fire, the sun and sky moving with the time of day. And we want it without rebaking: the
bake (probe rays, surfels, links) takes seconds to minutes per cell.

The claim this lane proves on a synthetic scene: **a relight that keeps the bake and a small, light-independent
precompute gives the same numbers as a full rebake of the same scene under the new lights**, to float32 precision,
as long as it runs the bounce passes. Skip the passes and it is off by 22-40% of the brightest surfel.

## 2. Sources

What I could read, and what I could not. This session's network blocked every talk host I tried (gdcvault.com,
advances.realtimerendering.com, slideshare.net, gamedev.net, gamedeveloper.com, cedil.cesa.or.jp,
history.siggraph.org). GitHub was reachable. So the only source read in full is CasualPRT. For the talks I have only
search-result summaries (quoted below) and what this repo's plan already took from the Division deck. Where I add
something from memory, it is marked **(recalled, not re-read)**. Check it against the talk before relying on it.

| Source | URL | Read? | What it gives this lane |
|---|---|---|---|
| CasualPRT (MIT, AKGWSB, commit ac5e321, 2023-05-21) | https://github.com/AKGWSB/CasualPRT (write-up: https://zhuanlan.zhihu.com/p/571673961) | yes, all shaders + scripts | a complete GPU relight loop (section 2.1) |
| Stefanov, "Global Illumination in Tom Clancy's The Division", GDC 2016 | https://gdcvault.com/play/1023273/Global-Illumination-in-Tom-Clancy | no (blocked) | the plan's surfel/probe/brick model (PRTP_PLAN section 1, 2aj, slides 24-32, 46-49) |
| Gilabert & Stefanov, "Deferred Radiance Transfer Volumes: Global Illumination in Far Cry 3", GDC 2012 | https://gdcvault.com/play/1015326/Deferred-Radiance-Transfer-Volumes-Global | no (blocked); search summary only | probes relit on the fly for time of day and local lights; hybrid CPU/GPU |
| Martin & Einarsson, "A Real-Time Radiosity Architecture for Video Games" (Enlighten), SIGGRAPH 2010 Advances course | https://advances.realtimerendering.com/s2010/Martin-Einarsson-RadiosityArchitecture(SIGGRAPH%202010%20Advanced%20RealTime%20Rendering%20Course).pdf | no (blocked); search summary only | "single bounce with feedback"; a lighting pipeline separate from the frame |

### 2.1 CasualPRT (read)

Files: `Assets/Shaders/SurfelSampleCS.compute`, `SurfelReLightCS.compute`, `SH.hlsl`, `Assets/Scripts/Probe.cs`,
`ProbeVolume.cs`, `PRTRelight.cs`. What it does every frame, for **every** probe (no amortization):

1. Bake (once): each probe renders a G-buffer cube (world position, normal, albedo) and samples 32 x 16 = 512 random
   directions. Each sample is a surfel `{position, normal, albedo, skyMask}`. Surfels are per probe and not shared
   between probes.
2. Relight (one compute dispatch per probe, a thread per surfel, `SurfelReLightCS`):
   - direct = albedo x mainLight.color x saturate(N.L) x shadow, where the shadow is a lookup in the engine's main-light
     **shadow map** (cascades), times (1 - skyMask);
   - sky = the environment cube sampled along the probe-to-surfel direction, x skyMask x an intensity;
   - **history** = the previous frame's probe volume sampled at the surfel (8 nearest probes, trilinear, weighted by
     saturate(dot(dir to probe, normal))), as irradiance x albedo / pi, times `_GIIntensity`;
   - radiance = direct + sky + history, projected onto SH9 with weight 4 pi / 512, and summed into the probe's
     coefficients with `InterlockedAdd` on ints (floats scaled by 100000, `FIXED_SCALE`).
3. The volume buffer is double-buffered: `SwapLastFrameCoefficientVoxel` then `ClearCoefficientVoxel` each frame.

So CasualPRT gets multiple bounces as **one bounce per frame with feedback**: the light converges over frames, like
Enlighten's "single bounce with feedback". Where we differ: surfels are shared and stored once (the `.tbk`), links
carry the weights, and the history is read through our own feed lists (BOUNCE2), not a trilinear grid. We sum with a
thread per row, so there are no atomics and the result is deterministic. Only one directional light is supported.
Nothing was copied; the loop shape is what we take.

### 2.2 Far Cry 3, Deferred Radiance Transfer Volumes (search summary only)

From the session summary: "a sparse volume of radiance transfer probes, with each probe storing spherical harmonics
coefficient matrices", "relit on the fly to support illumination under time of day changes as well as local lighting
such as muzzle flashes and explosions", shading "on the GPU in screen space", "a hybrid CPU/GPU implementation"
for the consoles of that time. The idea that matters for us: the probe stores a **transfer**, not light, so it is
relit when the lights change. **(Recalled, not re-read:)** the matrices map a low-order lighting input (sky + sun) to
the probe's SH output, and the relight ran on the CPU at a reduced rate. Verify both before citing.

### 2.3 Enlighten (search summary only)

The four points of the 2010 architecture talk, from the summary: "a separate lighting pipeline, single bounce with
feedback, lightmap output, and relighting from target geometry". For us:
- **Separate pipeline**: the relight is its own stage with its own rate, not part of drawing the frame (section 5).
- **Single bounce with feedback**: one bounce per update, reading the previous update's output. That is our amortized
  mode (check G). **(Recalled, not re-read:)** the precompute is form factors between surface samples, and the
  runtime is a sparse matrix-vector product on the CPU at a few ms per update.

### 2.4 The Division (taken from this repo's plan, not re-read)

PRTP_PLAN section 1 and 2aj list the deck's model: surfels store position, normal and albedo only ("lighting-free,
relit live", s26). Probes gather from surfels, or from 4 m bricks (s25). Each surfel adds its probe's light from the
previous frame (C11, slides 46-49). **(Recalled, not re-read:)** the relight runs on the GPU in compute, with the
surfels' direct light taken from the sun's shadow cascades and local lights, and the work spread across frames. The
deck's own timings are not in this repo. Fetch them before quoting a number for The Division.

## 3. Our data, and the loop mapped onto it

What the bake gives (`.tbk` v4, plan 2g/2y; `src/probegi.cpp` reads it):
- **Surfels** `s`: position p_s, normal n_s, albedo a_s (linear), with a back side for thin walls.
- **Probes** `j`: position, links {surfel, weight w, octahedral direction d, door id, glass tint t}, linkWeightScale,
  unlinked weight u_j, sky share per octant skyVis_j[8], room ids.
- **Built by the relight from geometry** (`probegi.cpp`, step 3 and BOUNCE2): each surfel's feed list {probe j,
  weight f_sj} (visible probes within r, weight (1 - d^2/r^2)^2, else the closest visible within 2r, room-filtered),
  and each grid voxel's blend list {probe j, weight}.

**Key fact: after the shadow tests, every step is linear in the lights, and every operator depends only on
geometry.** So we split the work into a one-time precompute and a per-frame part:

| Once per bake (or cell load) | Per frame (or when lights change) |
|---|---|
| visible surfel-light pairs within each placed light's baked radius (shadow segment, 24 u fixture clear) | light color x dimmer x on/off, radius <= baked, spot cone, falloff curve |
| link coefficients c_ja = w x scale x 4 pi x max(axis_a . d, 0) x k_j, with k_j = (linked + unlinked) / linked | sun direction and color; the sun's visibility (section 4) |
| feed lists f_sj (normalized), the surfel's facing axes | the six sky (DALC) colors |
| voxel blend lists | door states (a mask on links carrying that door's id; see open questions) |
| sky share per probe per octant | |

### 3.1 The per-frame kernels (GPU compute, one thread per row, float32)

With L = lights and their PRTP2 terms (radial `(1 - sat(scale x^exp + bias))^2.2`, cone, N.L):

1. **Direct** (a thread per surfel, over its visible pairs):
   `E_s = sum_{l in pairs(s)} color_l x atten_l(|x_l - p_s|) x cone_l x max(n_s . L, 0)  +  sun_rgb x max(n_s . sun, 0) x V_sun(s)`
   `B1_s = a_s x E_s`.
2. **Gather** (a thread per probe, over its links; `probegi.cpp` step 2):
   `E_ja = sum_links c_ja x t_link x B_s  +  Sky_ja`, where
   `Sky_ja = pi x amb_a x 0.25 x sum_{octants o on side a} skyVis_j[o]` (`probeSkyCube`).
3. **Feed** (a thread per surfel, over its feed list; BOUNCE2):
   `F_s = sum_j f_sj x sum_{axis i} n_si^2 x E_j,face_i(s)`,  then `B_s <- B1_s + a_s x F_s / pi`.
4. Repeat 2-3 until `max |dB| <= 1e-3 x max B` (BOUNCE2's bar), at most 64 times, or once per frame with history
   (section 5).
5. **Grid** (a thread per voxel, over its blend list): the six slabs the renderer samples (unit 13), from the last E.

Steps 2-5 have fixed operators: the per-frame cost does not depend on how many lights changed.

### 3.2 Two alternatives considered

- **Per-light basis** (precomputed transfer per light): the settled B is linear in each light's color, so one could
  store, per light, its settled response (N x 3 floats a light) and sum. Check L proves the linearity. Cost: memory
  N x K x 12 bytes (300k surfels x 256 lights = 920 MB). Too big for exteriors, and the sun and sky need their own
  bases (the sun's shadow changes with direction). Keep it as an option for interiors with few lights; not the plan.
- **Dense form factors (Enlighten-style surfel-to-surfel)**: collapsing gather + feed into one surfel-to-surfel matrix
  (N x N sparse) gives more entries than links + feed (each surfel sees about 256-1024 others), and it loses the
  probes as an output we need anyway. Not the plan.

## 4. Time of day: the sun's shadow

Placed lights do not move, so their visibility is precomputed (the pairs). The sun moves. Two ways:
- **Rays (exact)**: a compute shader traverses the soup's BVH (`src/probebvh.h` already builds one) for each
  sun-facing surfel, only when the sun has moved more than a threshold. It is amortized over frames: 1/16 of the
  surfels a frame. In the twin, check E: identical to the rebake (float32 noise, 1.5e-7 to 2.6e-7).
- **Shadow map**: an orthographic depth map of the soup along the sun, looked up per surfel with a bias (CasualPRT and,
  as recalled, the Division use the engine's cascades). In the twin, check S at 768^2 with a bias of 3 u + one texel:
  0 / 0 / 1 of about 1100-1200 sun-facing surfels change state, total B within 0.14%.
  The red (`noshadow`: map ignored) changes 77% and adds 115% to the total B, so it FAILS.

The sky colors (DALC, six per hour) only enter Sky_ja: free per frame.

## 5. Amortization plan

Measured in the twin (light 0 switched off, warm start from the old settled answer):
- **Settled each frame**: 8-9 passes. Fine for an interior; see the budget (section 6).
- **One pass a frame with history** (Enlighten / CasualPRT): the change falls under 1e-3 of the brightest after
  **9 frames**, and under 1e-5 after 14. The result is the rebake's fixed point within 6.4e-6 (check G). At 60 fps,
  a toggled lamp's bounce settles visibly in 0.15 s.
- **Round robin** (a quarter of the probes regathered a frame, every surfel every frame): under 1e-3 after **20
  frames**, under 1e-5 after 36, the same fixed point within 1.3e-5 (check R).

The plan:
1. Direct light (kernel 1) every frame for every surfel: it is cheap, and flicker must not lag.
2. Bounce: one pass a frame with history (steady state). When a light is toggled or its color changes by more than
   a threshold, run 2-4 extra passes in that frame (a burst) so the first frames are already close.
3. Big exteriors (5x5 block): round robin by sector (cells nearest the camera every frame, the rest 1/4 or 1/8),
   using the probes' sector order from the `.tbk` files.
4. The sun's visibility: re-traced (or the map re-rendered) only when the sun moves more than about 0.25 degree,
   spread over 16 frames.
5. The grid (kernel 5) follows the probes it blends: only voxels whose probes changed.

## 6. Budget

A mid GPU is taken as 224 GB/s DRAM (an RX 6600 / GTX 1660 Super class card), at 35% efficiency for the scattered
reads (78 GB/s effective), plus 10 us per dispatch. All kernels are bandwidth-bound: a few FMAs per byte. Bytes per
element: surfel 20 B in + 8 B out (half4 B); pair 4 B; link 16 B (surfel index + packed coefficients) + an 8 B read
of B; feed entry 30 B (probe index, weight, the read of the probe's facing axes); probe write 48 B.

| Scene (N surfels / M probes / K lights, links a probe) | direct | one bounce pass | settled (8 passes) | amortized (direct + 1 pass) | resident |
|---|---|---|---|---|---|
| interior, 30k / 1000 / 64, 512 | 0.03 ms | 0.22 ms | 1.8 ms | 0.25 ms | 18 MB |
| big interior or 3x3 block, 100k / 2500 / 128, 512 | 0.09 ms | 0.60 ms | 4.9 ms | 0.69 ms | 53 MB |
| 5x5 exterior block, 300k / 5000 / 256, 768 | 0.27 ms | 2.0 ms | 16 ms | 2.3 ms (0.8 ms with round robin by 4) | 178 MB |

Add the grid: about 400k voxels x 8-27 probes x 6 B each, 20-65 MB read, 0.3-0.8 ms. Updating only the voxels
whose probes changed (a quarter, with round robin) cuts it to 0.1-0.2 ms. The sun's shadow map (a 2048^2 depth of the
soup) costs about 1 ms, but only when the sun has moved.
The per-element counts in the twin are: links 256 a probe (the cap is reached everywhere), feed 6.4 per surfel,
visible pairs 1.6 per surfel (only 4 lights). Real cells must be counted (section 8). Vault111Cryo has 26,978
surfels (plan 2aj). Its probe count is not printed in a census this lane can read; the Museum has 975-1017.
**These numbers are a model, not a measurement.** The local lane measures them on bungo's GPU.

The CPU twin's own float32 relight (numpy, 2068 surfels, 67 probes, 8 passes) takes 21-23 ms. That is no
guide to the GPU.

## 7. The twin and what each check proves

Scene (built in code): a building 700 x 490 x 280 u with two rooms, a doorway in a green partition, a window in the
south wall, a red pillar, a table, a ceiling, and dirt ground outside. There are three placed point lights and one
spot (PRTP2 curve, bias 0 / scale 1 / exponent 2, spot cone with falloff exponent 2) and, for the time of day, a sun
and six sky colors.
- 67 probes, 1024 Fibonacci rays each. Surfels sit in 35 u cells, keyed by position and face axis (the Division's
  key, s24): 2068 of them.
- Links are capped at 256 a probe and the rest is unlinked weight (mean 0.203), so the relight's renormalization
  is exercised.
- Feed lists use radius r = 2 x the median probe spacing (190 u), with visibility rays.

Two pipelines:
- **REBAKE**, the reference, in float64 with dense matrices: for each light state it re-traces the probe rays,
  re-forms surfels, links and feed lists, lights each surfel with fresh shadow segments and a fresh sun ray, then
  runs the passes until BOUNCE2's bar.
- **RELIGHT**, the GPU plan, in float32: the original bake plus the precompute. Each kernel is a segmented sum
  (a thread per row).

Shared by both: the box tracer, the bake procedure, and the curve functions. Check 0 proves the rebake meets the same
geometry. The C++ versions of the curves and the gather are already gated by cell_gi_check stages A, B and F.

Bars: |dB| <= 1e-4 of the brightest B, |dE| <= 1e-4 of the brightest probe axis, and the same pass count, for A-E. The
float32 noise measured is 0.8e-7 to 3.2e-7, so the bar has about 300x margin, and the red misses it by 2,000-4,000x.

Output of `python3 tests/spells/gpurelight1_check.py` (exit 0):

```
0 bake PASS 2068 surfels, 67 probes, 17152 links (256 a probe, unlinked mean 0.203), 13324 feed entries (6.4 a surfel), 3240 visible surfel-light pairs (1.6 a surfel), blend radius 190 u
A toggle (light 0 off) PASS max |dB| 3.22e-07, max |dE| 1.30e-07 of the brightest (bar 1e-04); passes relight 8 rebake 8; bounce share of the rebake 39.3%
B colour (light 1 -> red) PASS max |dB| 2.50e-07, max |dE| 8.47e-08 of the brightest (bar 1e-04); passes relight 9 rebake 9; bounce share of the rebake 43.0%
C flicker (light 2 dimmer x 0.35, 1.25, 0.8, 0.05) PASS: max |dB| 2.80e-07, max |dE| 1.32e-07 ... (all four frames under 2.8e-07)
D radius (spot light 3 radius x 0.7) PASS max |dB| 2.84e-07, max |dE| 1.46e-07 of the brightest (bar 1e-04); passes relight 9 rebake 9; bounce share of the rebake 41.6%
E sun-ray (3 hours) PASS: 08:00 max |dB| 2.55e-07 ... 9/9 passes | 12:00 1.51e-07 ... 8/8 | 17:30 2.34e-07 ... 8/8
S sun-map (768^2 map, bias 3 u + a texel) PASS (bar: <= 2% flips, |total B| <= 1%): 08:00 0 of 1188 flips, total B -0.000% | 12:00 0 of 1188, -0.000% | 17:30 1 of 1091 (0.09%), +0.136%
L linear PASS relight(even lights) + relight(odd lights) vs relight(all), 16 passes: 9.09e-08 of the brightest (bar 1e-05)
G amortize (one pass a frame, warm start, light 0 off) PASS: |dB| 6.35e-06 of the brightest vs the rebake settled to 1e-7 (bar 2e-4); change under 1e-3 after 9 frames, under 1e-5 after 14 (tight rebake 18 passes)
R robin (a quarter of the probes regathered a frame) PASS: |dB| 1.33e-05 of the brightest (bar 2e-4); change under 1e-3 after 20 frames, under 1e-5 after 36
--- red nobounce (must FAIL A B C D E G)
    A toggle ... FAIL max |dB| 2.75e-01, max |dE| 3.37e-01 ...; passes relight 1 rebake 8
    B colour ... FAIL max |dB| 3.77e-01 ...   C ... FAIL 3.90e-01 ...   D ... FAIL 3.90e-01 ...   E ... FAIL 3.97e-01 / 2.23e-01 ...
    G amortize ... FAIL: |dB| 2.76e-01 ...
red nobounce: FAILS as required (A B C D E G R S)
--- red noshadow (must FAIL S)
    S sun-map ... FAIL: 08:00 912 of 1188 sun-facing surfels change lit state (76.77%), total B +115.384% ...
red noshadow: FAILS as required (E S)
gpurelight1 PASS
```
(Abridged where marked `...`; the full lines are printed by the script.)

What each proves:
- **A-D**: switching a light off, changing its color, flickering it and shrinking its radius need no rebake. The
  precomputed pairs plus the live curve, then the bounce passes, give the rebake's answer, including 39-43% of the
  light that is bounce.
- **E**: the time of day (sun direction and color plus the six sky colors) needs no rebake either, when the sun's
  visibility is re-traced.
- **S**: a shadow map is good enough for the sun's visibility at this resolution and bias.
- **L**: the relight is linear in the lights. This is why warm starts and per-light bases are valid.
- **G, R**: one pass a frame with history, alone or with round robin, reaches the same fixed point; the frame counts
  are the latency.
- **Reds**: `GPURELIGHT1_RED=nobounce` (pass 1 only) misses by 22-40% of the brightest; `=noshadow` (the shadow map
  ignored) puts sunlight into 77% of the shaded sun-facing surfels. Both turn their checks FAIL. The plain run
  self-tests both and exits nonzero if either red passes.

## 8. What the local lane must still do with real cells

1. **Count real cells.** Dump from Vault111Cryo, ConcordMuseum01 and Concord -15,17: links a probe (the cap is 1024),
   feed entries a surfel, voxel blend entries, and visible surfel-light pairs (with radius x 1.25 margin). Put them
   in section 6's model. This needs a census line from `probeGiRelight`; no game data comes into the repo.
2. **A CPU "relight from precompute" path in `src/probegi.cpp` first**: build the pairs, coefficients, feed and blend
   lists once, then relight from them. Gate: with the same light state it must reproduce today's dumps
   (`gi_bounce.bin`, `gi_probes.bin`, grid) to 1e-5 relative (double vs double: near-byte). Then toggle a light in
   the cell (a `WW_CELL_GI_LIGHTS` mask) and compare with a full `probeGiRelight` under the same mask. That
   comparison is this twin's check A on real data. Red: one pass.
3. **The GL 4.3 compute version** (skill ww-gl-compute-stage): SSBOs for pairs, links, feed and blend lists, and a
   thread per row with no atomics (unlike CasualPRT), so it is deterministic. Gate it on step 2's numbers (float32
   bar 1e-4 of the brightest). Measure the ms of each kernel on bungo's GPU and print them in the census.
4. **The sun**: the BVH ray kernel, amortized, or a shadow map; check S on a real exterior (Concord, Lookdev
   CommonwealthClear at 08:00 / 12:00 / 17:30).
5. **Flicker sources**: which LIGH flags and fields drive flicker or pulse in FO4 (not in PRTP2 yet). The preview
   needs the curve over time; the relight only needs the dimmer each frame.
6. **UI**: a light list in the PRTP band with on/off and a dimmer, and the time-of-day slider of Lookdev driving the
   relight live (it re-runs the whole CPU relight today, `relightKept` in `src/probesky.cpp`).

## 9. Open questions

- **Doors.** Links carry door ids (BAKE4). Zeroing a closed door's links is the FO4CS rule, but it is not equal to a
  rebake: in a rebake the closed door is a new surface that reflects light, and the rays it stops find other surfels
  behind it, not "unlinked". How big is the difference on a real doorway, and should a closed door's share go to
  unlinked (renormalized) or to darkness? The twin has no doors yet.
- **Lights that move** (carried torches, a swinging lamp): the pairs assume fixed lights. They would need a cube
  shadow map per moving light. Out of scope here.
- **A radius larger than baked**: pairs are stored only within the baked radius. The proposal is a margin (x 1.25)
  at precompute time and a refusal line beyond it.
- **Settle bar on the GPU**: deciding "settled" needs a max reduction and a readback, or a fixed pass count. The twin
  uses the same rule both sides; the GPU plan uses amortization instead, so the bar only matters for gates.
- **The talks' own numbers** (the Division's relight ms, Far Cry 3's update rate, Enlighten's precompute model)
  were not read this session (network blocked). Fetch them before quoting them anywhere.
- **Half precision**: storing B and E as half floats saves half the bandwidth. The twin is float32 only; the bar for
  half is untested.
