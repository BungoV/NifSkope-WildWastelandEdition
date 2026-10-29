# Lane GLAREBLOW1 (2026-10-29, cloud): the blow-out near the sun at Concord D7

The problem as given (WEATHERBLEND5's measurement): in D7, the Concord hardware store with glare on, 12.8% of the frame is
blown (37.1% of the left third). The air pass's sky term near the sun goes over 1.0. Multiple scattering is about 40% of
it, and bloom spreads it. With the sky off it is 2.1% blown; with bloom off, 0.9%. Glare, exposure and flare play no part.

## What "the air's sky term" is

It is the cell air (VOLAIR1), not the physical sky (ATMO1). `cell_volsky.frag` lays `wwVolAirSky` (lookdev_fog.glsl) over the
sky: the froxels, then a closed-form tail to where the ray leaves the air. A ray at or below the horizon never leaves it
(T = 0), so there the light is J itself. J is `airSun` in cell_volair.frag:

    J = D x [ vis Ts pi p(c)  +  sum_{i=1,2} a^i vis_i Ts^(b^i) pi p_{g c^i}(c) ]

- D is the cell's directional colour.
- Ts is the air's transmittance up to its top.
- p is the god-ray record's phase: Rayleigh plus forward and back Henyey-Greenstein lobes, weighted by luminance so that
  they sum to 1.
- The sum is GAPLOOK1's octaves (Wrenninge et al. 2013, "Oz"), with a = b = c = 1/2. The medium is the fog records' fit
  (sigma0, zb, H, top).

## Step 1: the measurement (offline, no exe)

Model: `model/glareblow_model.py` evaluates the shader's law by exact quadrature along the ray. Its references:

- **MC**: a backward Monte Carlo of the same medium and phase, all orders, with a ground of albedo 0.2. Its first order
  matches our single scattering within 1.7% on all 225 fixture rows (test (e)), so the port and the MC agree.
- **PRE**: Preetham 1999, with T = 3, an absolute zenith luminance and Kasten's direct normal illuminance.
- **CIE**: CIE type 12, clear sky. Its circumsolar peak is about 3x Preetham's; it is listed in table B only.
- **HIL**: the repo's own Hillaire 2020 model (tests/spells/atmo_model.py, which the repo validated against a Monte Carlo):
  clear air with no haze.

Medium: the D7 values are not reachable from here (no game data). The "Concord-like" stand-in is sigma0 0.48/km, H 0.3 km,
top 2 km, a vertical optical depth of 0.14 (the hazy FO4 street fog). Thin (0.05) and thick (0.45) media bracket it. The
phase is the code's fallback record (forward g 0.75), with forward g also set to 0.5 and 0.3.

**The brief's "MS about 40% of it near the sun" is the forward g ~0.3 case.** With g 0.75 our octaves are 12-16% of the
near-sun sky; with g 0.3 they are 38-48% (table C in `step1_tables.txt`).

### The table (Concord-like medium, sun 15 deg, view at the sun's elevation; full set in `table_final.txt`)

Sky over sunlit ground means the sky's radiance divided by that of a ground of albedo 0.2 lit by the sun and the sky. For
ours, the ambient is taken at Preetham's diffuse share.

Forward g 0.30 (the measured D7 case):

| angle from sun | ours SS (per D) | MS share old / new / MC | old / MC | new / MC | PRE | HIL | ours old | ours new | new / PRE | new / HIL |
|---|---|---|---|---|---|---|---|---|---|---|
| 2  | 0.151 | 39.0 / 24.1 / 18.2 % | 1.34 | 1.08 | 15.0 | 5.25 | 3.58 | 2.88 | 0.19 | 0.55 |
| 5  | 0.151 | 39.1 / 24.2 / 18.5 % | 1.33 | 1.07 | 13.7 | 4.81 | 3.57 | 2.86 | 0.21 | 0.60 |
| 10 | 0.148 | 39.3 / 24.3 / 18.6 % | 1.33 | 1.07 | 11.8 | 3.92 | 3.52 | 2.83 | 0.24 | 0.72 |
| 20 | 0.140 | 40.1 / 24.9 / 19.6 % | 1.35 | 1.08 | 8.89 | 2.92 | 3.36 | 2.68 | 0.30 | 0.92 |
| 35 | 0.120 | 42.0 / 26.4 / 21.9 % | 1.35 | 1.06 | 6.14 | 2.38 | 2.99 | 2.36 | 0.38 | 0.99 |
| 40 | 0.114 | 42.7 / 26.9 / 22.9 % | 1.34 | 1.05 | 5.52 | 2.25 | 2.86 | 2.24 | 0.41 | 0.99 |
| 45 | 0.107 | 43.4 / 27.5 / 23.6 % | 1.35 | 1.05 | 4.99 | 2.14 | 2.72 | 2.13 | 0.43 | 0.99 |
| 90 | 0.066 | 49.0 / 32.0 / 32.2 % | 1.33 | 1.00 | 2.89 | 1.55 | 1.86 | 1.40 | 0.48 | 0.90 |

Forward g 0.75 (the fallback record):

| angle from sun | ours SS (per D) | MS share old / new / MC | old / MC | new / MC | PRE | HIL | ours old | ours new | new / PRE | new / HIL |
|---|---|---|---|---|---|---|---|---|---|---|
| 2  | 1.034 | 12.1 / 6.6 / 7.3 %   | 1.06 | 1.00 | 15.0 | 5.25 | 16.97 | 15.97 | 1.06 | 3.04 |
| 5  | 0.933 | 13.2 / 7.2 / 7.8 %   | 1.06 | 0.99 | 13.7 | 4.81 | 15.50 | 14.50 | 1.06 | 3.01 |
| 10 | 0.683 | 16.9 / 9.4 / 10.0 %  | 1.09 | 1.00 | 11.8 | 3.92 | 11.85 | 10.87 | 0.92 | 2.77 |
| 20 | 0.315 | 28.9 / 17.1 / 17.0 % | 1.17 | 1.00 | 8.89 | 2.92 | 6.40 | 5.49 | 0.62 | 1.88 |
| 35 | 0.131 | 45.1 / 29.2 / 27.0 % | 1.32 | 1.02 | 6.14 | 2.38 | 3.45 | 2.68 | 0.44 | 1.13 |
| 40 | 0.107 | 48.5 / 32.0 / 29.6 % | 1.36 | 1.03 | 5.52 | 2.25 | 3.01 | 2.28 | 0.41 | 1.01 |
| 45 | 0.091 | 50.9 / 34.1 / 32.2 % | 1.38 | 1.03 | 4.99 | 2.14 | 2.67 | 1.99 | 0.40 | 0.93 |
| 90 | 0.045 | 56.0 / 38.1 / 41.9 % | 1.32 | 0.94 | 2.89 | 1.55 | 1.48 | 1.06 | 0.37 | 0.68 |

### Verdict

1. **The single scattering is right.** It equals the MC's first order of the same medium (within 1.7%).
2. **The multiple scattering was too bright.** Oz's a = 1/2 is a thick cloud's value: half of the light scattered once meets
   the medium again. In street fog with a vertical optical depth of 0.05-0.45, most of it leaves (re-scatter chance
   0.18-0.43 at the eye). Against the MC of the same medium, the octaves gave:
   - near the sun: 1.05-1.07x the true total with g 0.75, and **1.31-1.40x with g 0.3** (D7's 40% case);
   - at 35-90 deg: 1.3-1.6x.

   This is the cause in the air code, so Step 2 applies.
3. **Against the absolute skies** the near-sun sky is not off the scale. With g 0.75, ours is 1.06x Preetham near the sun
   (and 3x Hillaire's haze-free air). After the fix, at 35-45 deg ours is 0.4x Preetham and 0.93-1.13x Hillaire. Hillaire
   puts that band at 2.1-2.4x the ground at sun 15 and 1.4-1.5x at sun 20, which is the brief's "roughly 1.6x". The
   references themselves disagree near the sun by 3x (Preetham, Hillaire) to 9x (CIE). The aureole depends on the
   turbidity, which the fog records do not give.

## Step 2: the fix

`cell_volair.frag` `airMsA(z)`: the octaves' a = min(1/2, f). f is the chance that light scattered isotropically at z meets
the air again:

    f = 1/2 (1 - E2(tau_up)) + 1/2 (1 - E2(tau_down)) + 1/2 E2(tau_down) x 0.2

- tau_up is the air's column to its top; tau_down is the column to the local ground (airGz).
- E2 is the exponential integral, the mean of exp(-tau/mu) over the hemisphere, taken by a 4-point Gauss-Legendre rule.
- 0.2 is the ground's albedo for the light that escapes downward.

In the limits, f is 0.1 with no air and 1/2 in deep fog (as before); the cap means no row is brighter than before.

- Prototypes (`model/proto_fix.py`, `proto_fix.txt`):
  - 'f' (no ground term) is 20% low at 90 deg.
  - 'fg' (ground term, uncapped) is within ~8% in the thin and Concord media but brightens dense fog.
  - 'min' (fg capped at 1/2) was chosen: it never brightens.
- **Red** `WW_CELL_GLAREBLOW_RED=off`: a = volMs.x = 1/2, the same float operations as before, so the old picture bit for
  bit (owed live: a pixel diff).
- **CPU twin**: `src/gl/airmsweight.h`.
- **Test**: `tests/glareblow1_test.cpp`, with the fixture `tests/fixtures/glareblow1_mc.tsv` (225 rows, 100k paths each,
  from `model/make_fixture.py`). The green run passes 7 of 7. The red build (`-DGLAREBLOW_RED`) fails (c) (104 of 120
  rows outside 15%, worst 1.60) and (d) (no darkening). Green (c): worst 0.107, against 0.601 for the old law. Green (d):
  near-sun Concord g <= 0.5 is 0.757-0.862 of the old value, and no row is brighter.
- **Bars**: the bars of (a) and (d) were first set at 0.006 and 0.85. They were loosened to 0.015 and 0.90 after the first
  run, which measured 0.0109 and 0.757-0.862. The other bars are as first written.
- **Census**: the `cell air:` line gains `octaves=medium|half(red)|off`.
- **Cost (estimate)**: 8 exp and 2 column evaluations per sun sample (4 per froxel slice), against the cascade-shadow fetch
  already there. The live GPU ms is owed.
- **Out of scope, found in passing**: in dense fog (vertical optical depth 1.8) ours is 0.03-0.9x the MC. The sun's light
  diffused through thick fog is missing from the sun term; the pass's GI and ambient terms are meant to carry it. The
  fix leaves that regime as it was (the cap).

## What the fix does to D7 (estimate; the live rows below decide)

D7's air near the sun is the g ~0.3 case (MS 40%). There the near-sun air drops to 0.76-0.81 of its old value. A pixel blown
before (value v > 1) is unblown after only if v < 1 / 0.79, about 1.27. The sky-off run left 2.1% blown, so at most
12.8 - 2.1 = 10.7 points of the frame depend on the air's sky term at all. How much of that 10.7 sits between 1.0 and 1.27
needs the frame's histogram (owed).

### If the blow-out is the scene's real range

It is partly real. Even after the fix, every reference puts the sky within 5 deg of the sun at sun 15 at 4.8-5.3x
(Hillaire), 13.7-15.0x (Preetham) and 37-42x (CIE) the sunlit albedo-0.2 ground. If exposure puts that ground at display
0.18, the near-sun sky is about 0.18 x 5 = 0.9 (Hillaire) up to 2.7 (Preetham) and 7 (CIE). That is blown in two of the
three, and bloom then spreads it.

- **Exposure**: to bring the near-sun sky under 1.0 at Preetham's ratio, the exposure must drop by log2(2.7) = 1.4 EV, which
  puts the ground at 0.067.
- **Bloom**: with bloom off it is 0.9% blown against 12.8%, so bloom triples the blown area. FO4CS bloom's soft-knee
  threshold at 1.0 would spread only what is above 1.0. Raising the threshold to the near-sun sky's peak (about 2.7 at
  Preetham's ratio) would keep the aureole from feeding it.

## Step 3: owed live (main session; Windows, the game data, the exe built from this branch)

Build: qmake, then make (nifskope-ww-build-verify). Then:

1. **Compile.** `cell_volair.frag` compiled clean here under glslang 15.1 (old and new, with the includes inlined and the
   name `cast` renamed because glslang reserves it). `cellvolair.cpp` was not compiled (no Qt here). Row: the exe links,
   and the cell air log line shows `octaves=medium`.
2. **Read D7's air.** From the D7 run's `cell air:` line, take sigma0, zb, H, top, phase=<record> and the record's forward g,
   plus the sun's elevation. Re-run the model with them:

       python3 -I notes_cloud/GLAREBLOW1/model/glareblow_model.py --sun <elev>

   with that medium in `media['concord']`.
3. **Re-shoot D7, before and after.** Use the D7 shot exactly as WEATHERBLEND5 took it (Concord hardware store, glare on),
   in two arms: `WW_CELL_GLAREBLOW_RED=off` (before) and unset (after). Rows:
   - `D7 blown share whole frame: before=__% after=__%` (WEATHERBLEND5 measured 12.8%)
   - `D7 blown share left third: before=__% after=__%` (37.1%)
   - `D7 blown, sky off: before=__% after=__%` (2.1%; must not change: the fix touches only the air's sun term)
   - `D7 blown, bloom off: before=__% after=__%` (0.9%)
   - `D7 air sky term near the sun (the WEATHERBLEND5 probe, WW_CELL_AIR_PROBE=1): max before=__ after=__ ratio=__`
     (expect 0.76-0.81 at g 0.3, 0.94 at g 0.75)
4. **Red is identical.** Diff the red against the rung exe (the pre-lane build) on D7: 0 pixels.
5. **Cost.** Air pass GPU ms, before and after, on D7 (expect < +0.05 ms at 1080p).
