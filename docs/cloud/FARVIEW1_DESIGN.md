# FARVIEW1 -- distant light from the surfels (a light LOD)

Lane FARVIEW1 (prep, cloud), 2026-10-03. Design plus a Python twin; no C++ yet, no game data used.
Twin: `tests/spells/farview1_check.py` (python3 + numpy, about 45 s, peak memory about 120 MB).

## 1. What this is, in one paragraph

Today a town's placed lights only light what is near the camera: the game (and the cell view) evaluate a light
only while its cell is loaded, so past the loaded grid a lit street goes dark and the skyline loses its night
glow. FARVIEW1 lights distant surfaces from light that was already baked: the probe bake's surfels
(docs/PRTP_PLAN.md 2g) are relit by every placed light, direct plus bounce (2i, 2aj). We keep that result per
surfel and let a far surface read it. Near the camera the real lights stay; in a band the two are cross-faded;
past the band only the baked surfel light is used. Diffuse only. Lights that change (flicker, switched on and off)
get relit later by a GPU relight lane; lights that move are never baked and stay real.

## 2. Prior art (public sources), and what we take from each

Fetching was limited: dev.epicgames.com, adriancourreges.com, documentation.enlighten.siliconstudio.co.jp and
advances.realtimerendering.com were all blocked by this session's network proxy. What follows comes from search
result summaries of those pages, not from reading them in full. Anything more specific is left out rather than
guessed.

| Source | What it does (as far as the summaries say) | What we take |
|---|---|---|
| GTA V, Adrian Courreges' "GTA V Graphics Study" (2015): https://www.adriancourreges.com/blog/2015/11/02/gta-v-graphics-study/ and part 2: https://adriancourreges.com/blog/2015/11/02/gta-v-graphics-study-part-2 | Distant lights are drawn as small textured quads (a 32 x 32 sprite), instanced in large batches; each one stands for a real light you can drive to, and the full model takes over as you get closer. | That is a light LOD for the **bulb** (what you see when you look at the lamp), not for the light on walls and streets. It goes well with ours but is a separate feature (section 9, open question 1). Their hand-off is by distance, like ours. |
| Red Dead Redemption 2, F. Bauer, "Creating the atmospheric world of Red Dead Redemption 2", SIGGRAPH 2019 Advances course | Only the title and course were found; the slides could not be fetched. | Nothing taken. Listed so the local lane can read it. |
| Enlighten (Geomerics / Silicon Studio): "How Enlighten works" and "Lightmap LOD", https://documentation.enlighten.siliconstudio.co.jp/wiki/spaces/SDK310/pages/904999286 and .../904999384; M. Einarsson, "A Real-Time Radiosity Architecture", SIGGRAPH 2010 Advances, https://www.advances.realtimerendering.com/s2010/ | The precompute splits surfaces into clusters and stores form factors between them; the runtime solves radiosity one bounce per iteration and outputs **diffuse indirect only** (lightmaps, probes, cubemaps). Lightmap LOD solves coarser lightmaps for distant systems (their hut: 4264 -> 2640 -> 1400 -> 880 pixels). | (a) Distant surfaces may use a coarser light representation than near ones. (b) Cost depends on the size of the light representation, not on the light count. Our far term is the same idea, but it holds direct light as well as bounce, because past the band no light is evaluated at all. |
| Lumen (Unreal Engine 5): "Lumen technical details", https://dev.epicgames.com/documentation/en-us/unreal-engine/lumen-technical-details-in-unreal-engine and the tech blog https://www.unrealengine.com/en-US/tech-blog/unreal-engine-5-goes-all-in-on-dynamic-global-illumination-with-lumen | A near surface cache and detail traces up to about 200 m; "far field" traces against coarser (HLOD) geometry carry GI out to about 1 km; a sparser world-space radiance cache serves the distance. | The near/far split by camera distance, with a coarser stand-in further out. Lumen's far field still lights in real time; ours is baked because our far lights do not change (moving and changing lights are excluded, section 7). |
| Lightcuts, B. Walter et al., SIGGRAPH 2005, https://www.graphics.cornell.edu/~bjw/lightcuts.pdf | Many point lights grouped in a tree; cost grows much more slowly than the light count. | Considered and rejected for the far field: its cost still grows with the light count (sublinearly), and our lights are static, so baking them gives a cost that does not grow at all. It may suit the near band with many lights (open question 5). |
| The Division, N. Stefanov, "Global Illumination in Tom Clancy's The Division", GDC 2016, https://gdcvault.com/play/1023273/Global-Illumination-in-Tom-Clancy | Radiance-transfer probes with relit surfels; the deck the PRTP plan follows (docs/PRTP_PLAN.md). | The surfels and their per-sector storage we read from. |

**The blend rule we picked (section 4):** a smoothstep cross-fade on camera distance between "real lights + near
GI" and "baked surfel light". This is the near/far hand-off Lumen and LOD cross-fades use, applied to light.
The twin measures it against a hard switch (red 2).

## 3. How a far surface finds its surfel light -- measured, not argued

Three options were named in the brief. The twin builds all of them on one synthetic street and measures each one
against a brute-force reference (direct light from every light plus one bounce, at 1600 random surface points).
Relative error on lit points (median / p90 / p99) and the ratio of total light:

| Far stand-in | 50 lights | 500 lights |
|---|---|---|
| **nearest surfels**: 70 u cells, same side, 27-cell neighborhood, R 105 (**chosen**) | 0.026 / 0.140 / 0.652; 0.994 | 0.022 / 0.131 / 0.839; 0.990 |
| same, point moved +-48 u along its normal (LOD mesh error) | 0.025 / 0.137 / 0.658; 0.994 | 0.021 / 0.127 / 0.833; 0.991 |
| nearest surfels, R 70 | 0.034 / 0.150 / 0.607; 0.998 | 0.026 / 0.104 / 0.709; 0.995 |
| six-axis voxel grid 64 u (splatted from the surfels) | 0.053 / 0.155 / 0.758; 1.025 | 0.043 / 0.138 / 1.059; 1.022 |
| six-axis voxel grid 128 u | 0.065 / 0.291 / 1.192; 0.992 | 0.057 / 0.303 / 0.983; 0.989 |
| six-axis voxel grid 256 u | 0.212 / 0.831 / 2.459; 0.972 | 0.169 / 0.671 / 2.118; 0.984 |
| probe grid 256 u (irradiance volume: each probe takes every light at its own point + the surfels' bounce) | 0.494 / 1.000 / 6.597; 0.853 | 0.374 / 1.000 / 4.046; 0.854 |
| (the reference against itself, a second bounce seed) | 0.004 / 0.023 / 0.103 | -- |

What this says:
- **The probe grid is wrong for this job.** A probe holds the light arriving at the probe, not at the wall. A wall
  lamp 40 units off a facade makes a bright pool on the wall that no probe 128+ units away can see. Probes
  are right for bounce (that is how 2i uses them) and wrong for direct light. Rejected.
- **A voxel grid works if it is fine**, but at 64 u it is no better than the surfels, while storing far more
  (a 128 u grid already needs 297 bricks of 4 x 4 x 4 = 559 KB for this street, against 161-178 KB for the
  lit surfels at 16 B; a 64 u grid needs about 8 times more voxels).
- **The surfels win on accuracy and on size.** They are also already there, keyed by (cell, side) the way the
  `.tbk` keys them, so the far file can be built straight from the relight without new geometry.
- Moving the far point up to 48 units along its normal (a LOD mesh that does not sit exactly on the surface)
  changes nothing measurable, because the 27-cell lookup covers +-1 cell.

**The lookup, exactly** (twin: `surfel_knn`, `SurfelLight`):

    side   = the axis the normal faces most (+X -X +Y -Y +Z -Z)
    k0     = floor((p - 0.5 * axis(side)) / 70)           (the .tbk surfel key)
    for each of the 27 cells k0 + {-1,0,1}^3 that holds a lit surfel s on `side`:
        w_s = (1 - |p - x_s|^2 / R^2)^2, clamped at 0,    R = 105 (1.5 cells)
    F(p, n) = sum w_s E_s / sum w_s                       (0 when no lit surfel is in reach)
    far diffuse = albedo x F(p, n)                        (same units as the light sum in PRTP2 section 7)

E_s is the surfel's **total irradiance from placed lights**: direct (PRTP2 curve, cone, N.L, shadow ray) plus
the bounce the relight settles on (2aj: E_direct + E_feed / pi). It is irradiance, not B = albedo x E, so a
far LOD texture's own albedo multiplies it. Each point reads 27 table entries, whatever the light count.

## 4. The blend rule and the band

    d      = distance from the camera to the shaded point
    w      = smoothstep(D0, D1, d) = t^2 (3 - 2t), t = saturate((d - D0) / (D1 - D0))
    placed = (1 - w) x [ sum over real lights + near GI's placed-light part ] + w x albedo x F(p, n)
    lit    = placed + (sun, sky and their bounce: unchanged, from the near grid / far map as today)

- **D1 = how far real lights reach, minus a margin.** With a 5 x 5 loaded grid the camera is at least 2 cells
  (8192 u) from the grid's edge. A surface just inside that edge can be lit by a light just outside it, which is
  not loaded, so D1 = 8192 - r_cap. With r_cap = 1024 (the twin's largest radius is 1000): **D1 = 7168**.
  Past D1 every pixel uses the baked light only, so a missing real light cannot be seen.
- **D0 = D1 - 4096 = 3072** (one cell wide). This was measured: the largest change one pixel sees per 32-unit step
  of the camera is 0.8-1.0% (p99) with this band. The change per step is (F - real) x the slope of w, so it scales
  with 1 / band width: a 2048-wide band would roughly double it to about 1.5-2%, which is at the bar. The
  blend has zero slope at both ends (smoothstep), so there is no Mach-band crease at D0 or D1.
- **No double counting.** w and 1 - w add to one for placed lights. Two things must be kept apart in the
  renderer: (a) the near GI grid holds placed-light bounce and sky/sun bounce together; only the placed-light
  part may be faded by (1 - w). The relight already keeps the sky part apart per probe (`probeSkyE`), so the
  near grid needs one more pair of slabs, or the sky part kept as its own grid. (b) The far term F must not contain the
  sun's bounce (`surfelSun`): the sun stays with the far map (2h).
- **Moving lights** (on actors, projectiles) are never in F. They keep their own real evaluation and simply stop
  where the game culls them.
- The band is set in camera distance because that is what decides whether a light is loaded. The `.lodi` ring-3
  cross-fade is set as a projected size (LODGEN_NATIVE_LODO_LODI 4.6.5), but that is about geometric detail; here the
  question is whether the light exists.

## 5. Storage per sector, and where it lives

Ruled 2026-09-30 (PRTP_PLAN line 15): no surfels or probes inside the LOD lanes. So the far light does **not** go
into `.lodo` / `.lodi` vertex streams. It goes beside the probe bake, one small file per 4096-unit sector:

    <bake folder>/farlight_%+05d_%+05d.fvl     (same sector naming as sector_*.tbk)

    header 64 B: 'FVL1', version 1, sectorX, sectorY, float surfel cell (70), u32 count,
                 u64 light-set hash (the placed lights baked in, for the relight lane), u32 flags,
                 u32 changing-light count + offset (ids of lights in this sector that flicker or switch)
    records, sorted by key, 16 B each:
        u16 cx, cy, cz   cell index from the sector's min corner (cz from the sector's lowest surfel)
        u8  side         0..5 (+X -X +Y -Y +Z -Z); a .tbk v4 back surfel is just its own side
        u8  pos[3]       the surfel's mean point inside its cell, 70 / 256 = 0.27 u steps
        u32 E            RGB9E5 (shared-exponent HDR, 4 bytes)
        u8  pad, flags   (flags: 1 = lit only by changing lights)

Only **lit** surfels are written (total E above 1/1000 of the sector's 99th percentile, the twin's threshold), so
an unlit stretch of wasteland costs a header and nothing else. Measured on the synthetic street (8192 x 3200 u =
1.56 sectors, 17,994 surfels): 10,304 lit surfels with 50 lights, 11,387 with 500. That is about 6.6-7.3k
lit surfels per built-up sector, **about 103-114 KB per sector** at 16 B (161 / 178 KB for the whole street). At runtime, with an open-addressed hash on
(cell, side) at load factor 0.5 (two 4-byte slots per surfel), about 24 B per lit surfel, so about 155-170 KB per
built-up sector in GPU memory. The near bake bounds the worst case: a sector cannot hold more lit surfels than its
`.tbk` has surfels (Concord's bake: 26,865, so at most about 430 KB).

## 6. The twin, and what each check proves (actual output below)

`tests/spells/farview1_check.py` builds everything in code: a ground plane and 22 box buildings in two rows with
alleys, 50 or 500 point lights (half street lamps 350 u up, half wall lights 40 u off a street facade), radius
300-1000, record colors raised to the power 2.2, the PRTP2 section 1 curve with (bias, scale, exponent) =
(0, 1, 2) for 80% of the lights and (0, 1, 1) or (0, 1, 4) for the rest, shadow segments from P + 2N to 24 u
short of the light. Two light evaluators are written separately (one by points x lights, one by a loop over
lights with a Liang-Barsky box test) and must agree (C0).

- Reference: at 1600 random surface points, direct from every light + one bounce from 256 cosine rays a point,
  each hit relit by every light. Its own noise (a second seed): median 0.004, p90 0.023.
- Bake: 70 u surfels keyed by (cell, side), lit by the second evaluator, one bounce from 64 rays a surfel read
  from the surfel table (as the relight reads its own surfels). 10 of 1.15 million bounce rays land where no
  surfel is keyed (the edges of cut ground patches) and count as dark. Both sides use one bounce, so the comparison
  measures the stand-in and the lookup, not the bounce count.

| Check | Proves | Measured (50 / 500 lights) | Bar | Red that must fail it |
|---|---|---|---|---|
| C0 curve | the doc's curve (1, 0.531049, 0, 0.867634 at x = 0, .5, 1, .25); the two evaluators agree | max dev 1.9e-16 | 1e-9 | -- |
| A band agreement | the far term matches the reference where the two are blended | median 0.026 / 0.022, p90 0.140 / 0.131, energy 0.994 / 0.990 | median 0.08, p90 0.25, energy 0.90-1.10 | `nosurfel`: median 1.000, energy 0.000 |
| B far glow | past D1 (no real lights) lit points keep their light | sum 0.994 / 0.990; points keeping half: 99.4% / 99.9% | sum 0.85, kept 90% | `nosurfel`: 0.000 / 0.000 |
| C no step | sweeping the camera 2816-7424 u by 32 u, no pixel jumps | p99 per step 0.0076 / 0.0098 (max 0.024 / 0.025) | 0.02 (a 2% Weber rule of thumb) | `noblend` (hard switch at the band middle): p99 0.652 / 0.839 |
| D flat cost | the far lookup's work and time do not grow with the light count | 27 -> 27 lookups a point; 3.00 -> 2.97 us a point (ratio 0.99; timing varies run to run, about 0.9-1.05); the near path does 50 -> 500 light tests a point | work ratio 1.05, time ratio 1.5 | `perlight` (far term evaluates the lights): work 77 -> 527 (6.84x), time 6.99x |
| E bounce | points lit mostly by bounce (direct shadowed) agree too: the far term carries the bounce | 174 / 109 points; median 0.066 / 0.046, p90 0.196 / 0.153 | median 0.15, p90 0.35, at least 30 points | `nobounce` (direct only): median 0.963 / 0.658 |

How the bars were set: once, from the measured runs (a 500-point smoke run, then the 1600-point run), at about 1.8
times what the chosen far term measured, and above the reference's own noise. They were not tuned per red. Every
red fails its bar by a wide margin (C: 33-80 times over).

Run it: `python3 tests/spells/farview1_check.py` (green plus every red; exit 0 only if green passes and every red
fails what it targets). One red alone: `FARVIEW_RED=nosurfel|noblend|perlight|nobounce` (exit 1 when it fails,
as it must). `FARVIEW_QUICK=1` runs a 500-point smoke run that is not the gate.

Actual output of the full run (2026-10-03):

```
scene  50 lights: 22 boxes, 17994 surfels (10304 lit), 1600 reference points; bake 4.5 s (bounce rays off the table: 10), reference 3.1 s; 128-u voxel grid would be 297 bricks of 4^3 = 559 KB; far-light file (16 B a lit surfel, docs section 5) = 161 KB
scene 500 lights: 22 boxes, 17994 surfels (11387 lit), 1600 reference points; bake 5.4 s (bounce rays off the table: 10), reference 16.3 s; 128-u voxel grid would be 297 bricks of 4^3 = 559 KB; far-light file (16 B a lit surfel, docs section 5) = 178 KB

candidates: far term vs reference on lit points (rel err median / p90 / p99; energy)
   50 lights  nearest surfels, 70 u cells, R 105 (chosen)                    0.026 / 0.140 / 0.652; 0.994
   50 lights  nearest surfels, point moved +-48 u along N (LOD mesh error)   0.025 / 0.137 / 0.658; 0.994
   50 lights  nearest surfels, R 70                                          0.034 / 0.150 / 0.607; 0.998
   50 lights  voxel grid 64 u                                                0.053 / 0.155 / 0.758; 1.025
   50 lights  voxel grid 128 u                                               0.065 / 0.291 / 1.192; 0.992
   50 lights  voxel grid 128 u, point moved +-48 u along N                   0.066 / 0.290 / 1.192; 0.992
   50 lights  voxel grid 256 u                                               0.212 / 0.831 / 2.459; 0.972
   50 lights  probe grid 256 u (irradiance volume)                           0.494 / 1.000 / 6.597; 0.853
  500 lights  nearest surfels, 70 u cells, R 105 (chosen)                    0.022 / 0.131 / 0.839; 0.990
  500 lights  nearest surfels, point moved +-48 u along N (LOD mesh error)   0.021 / 0.127 / 0.833; 0.991
  500 lights  nearest surfels, R 70                                          0.026 / 0.104 / 0.709; 0.995
  500 lights  voxel grid 64 u                                                0.043 / 0.138 / 1.059; 1.022
  500 lights  voxel grid 128 u                                               0.057 / 0.303 / 0.983; 0.989
  500 lights  voxel grid 128 u, point moved +-48 u along N                   0.057 / 0.303 / 0.985; 0.989
  500 lights  voxel grid 256 u                                               0.169 / 0.671 / 2.118; 0.984
  500 lights  probe grid 256 u (irradiance volume)                           0.374 / 1.000 / 4.046; 0.854
  reference against itself (another bounce seed, 50 lights): 154 lit 0.004 / 0.023 / 0.103; 23 bounce-dominated 0.023 / 0.090 -- the floor under any bar

green:
  C0  PASS curve at x=0,.5,1,.25 (bias 0 scale 1 exp 2) = [1.0, 0.531049, 0.0, 0.867634] (doc [1.0, 0.531049, 0.0, 0.867634]); evaluators max dev 1.9e-16 of max on 400 surfels (39% lit)
  A50 PASS 50 lights, 667 lit points: far vs reference rel err median 0.026 (bar 0.08) p90 0.140 (bar 0.25), energy 0.994 (bar 0.90..1.10)
  B50 PASS past the band (7680 u): sum ratio 0.994 (bar 0.85), lit points keeping >= 50%: 0.994 (bar 0.90)
  C50 PASS camera sweep 2816..7424 u by 32 u: largest per-step change p99 0.0076 (bar 0.020), max 0.0242; at D0 the picture is the near one (dev 0.0e+00)
  E50 PASS 174 bounce-dominated lit points (floor 30): rel err median 0.066 (bar 0.15) p90 0.196 (bar 0.35)
  A500 PASS 500 lights, 766 lit points: far vs reference rel err median 0.022 (bar 0.08) p90 0.131 (bar 0.25), energy 0.990 (bar 0.90..1.10)
  B500 PASS past the band (7680 u): sum ratio 0.990 (bar 0.85), lit points keeping >= 50%: 0.999 (bar 0.90)
  C500 PASS camera sweep 2816..7424 u by 32 u: largest per-step change p99 0.0098 (bar 0.020), max 0.0248; at D0 the picture is the near one (dev 0.0e+00)
  E500 PASS 109 bounce-dominated lit points (floor 30): rel err median 0.046 (bar 0.15) p90 0.153 (bar 0.35)
  D   PASS far lookup work a point 27.0 -> 27.0 (ratio 1.00, bar 1.05); time 3.00 -> 2.97 us a point (ratio 0.99, bar 1.5); near path light tests a point 50 -> 500 for contrast

red nosurfel (must FAIL A50, A500, B50, B500):
  A50 FAIL 50 lights, 667 lit points: far vs reference rel err median 1.000 (bar 0.08) p90 1.000 (bar 0.25), energy 0.000 (bar 0.90..1.10)
  B50 FAIL past the band (7680 u): sum ratio 0.000 (bar 0.85), lit points keeping >= 50%: 0.000 (bar 0.90)
  A500 FAIL 500 lights, 766 lit points: far vs reference rel err median 1.000 (bar 0.08) p90 1.000 (bar 0.25), energy 0.000 (bar 0.90..1.10)
  B500 FAIL past the band (7680 u): sum ratio 0.000 (bar 0.85), lit points keeping >= 50%: 0.000 (bar 0.90)
  -> FAILS as it must (A50, A500, B50, B500)

red noblend (must FAIL C50, C500):
  C50 FAIL camera sweep 2816..7424 u by 32 u: largest per-step change p99 0.6524 (bar 0.020), max 2.0618; at D0 the picture is the near one (dev 0.0e+00)
  C500 FAIL camera sweep 2816..7424 u by 32 u: largest per-step change p99 0.8390 (bar 0.020), max 2.1142; at D0 the picture is the near one (dev 0.0e+00)
  -> FAILS as it must (C50, C500)

red perlight (must FAIL D):
  D   FAIL far lookup work a point 77.0 -> 527.0 (ratio 6.84, bar 1.05); time 9.82 -> 68.66 us a point (ratio 6.99, bar 1.5); near path light tests a point 50 -> 500 for contrast
  -> FAILS as it must (D)

red nobounce (must FAIL E50, E500):
  E50 FAIL 174 bounce-dominated lit points (floor 30): rel err median 0.963 (bar 0.15) p90 1.000 (bar 0.35)
  E500 FAIL 109 bounce-dominated lit points (floor 30): rel err median 0.658 (bar 0.15) p90 0.993 (bar 0.35)
  -> FAILS as it must (E50, E500)

FARVIEW1 PASS: green PASS, reds all fail; 40 s
```

What the twin does **not** prove: the near side of the band is taken as the reference itself, but in the real
renderer it is real lights plus the near probe grid, which has its own error. Also not covered: spot
cones, light shapes (HEMI1), specular, sky, sun, more than one bounce, and real LOD meshes (a +-48 u offset is
the stand-in for them).

## 7. Lights: which ones are baked

| Light | Far term | Why |
|---|---|---|
| Static placed light (LIGH ref, not moving) | baked into E_s | the whole point |
| Changing (flicker, pulse, switched by script or by power) | baked in its "on" state at its mean strength; its id is listed in the sector header; the surfels it alone lights carry flag 1 | the later GPU relight lane rewrites E_s for the listed sectors when the state changes |
| Moving (on an actor, a projectile, a carried item) | never | a baked position would be wrong; it stays real and is culled with its cell |

## 8. What the local lane must still do with real cells and worldspaces

1. **Write E per surfel from the relight.** `probegi` keeps B per surfel (`surfelBounce`), not E. Add a dump of
   E_total = E_direct + E_feed / pi (placed lights only: take the sun part, `surfelSun`, back out) and write
   `farlight_*.fvl` per sector from it. Exteriors only; interiors have no far view.
2. **Set r_cap from the data.** Measure the radius distribution of exterior lights (base + XRDS delta, PRTP2 section 9) on
   the PRTP1 light walk, and choose r_cap (and so D1) to cover, say, 99.9% of them. Confirm the game's real
   light culling distance in a capture (PRTP4), because "a light is evaluated while its cell is loaded" is
   assumed here, not measured.
3. **Measure the real lookup error from LOD to surfel.** The twin assumed far LOD surfaces sit within 48 u of the
   baked surfaces. Measure the actual offsets of `.lodo` level-0 / level-1 meshes from the near surfels in a
   town (Concord, Diamond City's walls) before trusting the 27-cell reach.
4. **A pixel gate in the cell view.** Render a lit town at night twice from the same camera: once with every light
   real (the cell view can load lights well past the game's grid), once with the far term past D0. Compare
   pixels in the band and past it. Reds: `nosurfel` (far goes dark) and `noblend` (a line at the band).
5. **Storage census.** From the PRTP5 census rows (lights per cell), count the exterior sectors with lights and
   the lit surfels per sector across Fallout4.esm. That gives the real total size; the twin's
   103-114 KB per built-up sector is a synthetic estimate.
6. **Split the near GI grid** into placed-light and sky/sun parts (section 4), so only the placed part fades.
7. **The FO4CS reader** comes last, by standing order: the `.fvl` reader, the 27-cell hash lookup in the far
   shader, the blend.

## 9. Open questions

1. **Bulbs on the skyline.** The glow you see from far away is partly the light sources themselves (GTA V draws
   them as sprites). This lane lights surfaces only. Are the emissive bulb meshes in the LOD set, or does
   that need its own light-sprite LOD?
2. **Band width.** 4096 u keeps the camera-sweep change under 1% a step in the twin; is 2% the right bar for a
   night scene (eyes are less sensitive at low light, so it is probably safe, but this is not measured)?
   Should the band shrink to save near-light work, or move with the game's grid setting?
3. **Resolution far out.** At 30,000+ units a 70 u surfel is under one pixel; a coarser level (for example 280 u,
   made by merging 4 x 4 x 4 surfels the way Enlighten's lightmap LOD does) would cut memory for distant
   sectors. Not measured here.
4. **Changing lights.** Is "on, at mean strength" the right baked state for lights that are switched by power
   or by quests?
5. **Many lights in the band.** Inside the band both terms run. If a town has hundreds of lights in the
   band, a Lightcuts-style clustering of the near term may be needed; the twin's near path did 500 light tests a point.
6. **The bounce count.** The twin compares one bounce on both sides; the real relight settles 4-8 passes
   (2aj). Error should not grow with more passes, since the surfels carry them, but that is not shown here.

## 10. Files

- `docs/cloud/FARVIEW1_DESIGN.md` -- this page.
- `tests/spells/farview1_check.py` -- the twin (no game data, python3 + numpy).
- No C++ in this lane: the far lookup only makes sense inside the renderer and the relight, which need real
  cells to test; section 8 lists it.
