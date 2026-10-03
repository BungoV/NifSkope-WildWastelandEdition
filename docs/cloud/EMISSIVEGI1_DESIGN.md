# EMISSIVEGI1: glowing materials light the GI bake (design + Python twin)

Lane EMISSIVEGI1 (prep, cloud), 2026-10-03, branch `cloud-EMISSIVEGI1` from 38481ba.
Deliverables: this page and `tests/spells/emissivegi1_check.py` (numpy only, no game data). No C++ was
changed. The feature needs the cell view's soup, the bake and the relight to change together, and none of
that can be built or run here without game data (see section 8, "What the local lane must still do").

## 1. The method (owner's choice)

From GIBS (EA SEED, SIGGRAPH 2021): when a bake ray hits a surface whose material is emissive, add that
surface's own emitted radiance to the radiance of the hit, **on top of** the lit-albedo term. No new light
sources are made. In this repo's terms:

    B_surfel  = albedo x E_direct  +  Le_surfel                    (probegi.cpp step 1, pass 1)
    B_k       = B_1 + albedo x E_probes,k / pi                      (BOUNCE2 passes; Le rides in B_1, once)
    Le_surfel = mean, over the bake rays that landed in the surfel's cell, of Le(hit)
    Le(hit)   = (glowColor x glowMult x glowMap.rgb(uv_hit) x glowScaleSRGB)^2   per channel; 0 unless Own-Emit

Everything after that (probe gather, voxel grid, renderer) already reads B and needs no change.

## 2. Sources: what was read and what was not

- **Not read: the GIBS slides.** This environment's proxy blocks all of these (WebFetch: EGRESS_BLOCKED):
  `media.contentapi.ea.com` (the PDF named in the brief), `www.ea.com` and `ea.com` (the SEED article and the
  College Football 25 article), and `advances.realtimerendering.com` (the 2021 Advances copy and the 2024
  "Shipping Dynamic GI" follow-up). Nothing on this page claims to be slide content.
- Read: search-result summaries of the SEED article
  (https://www.ea.com/seed/news/siggraph21-global-illumination-surfels). They say GIBS caches irradiance on
  surfels spawned on geometry and integrates it with hardware ray tracing. They do not mention emissive surfaces.
- Read: **W298/SurfelGI** (https://github.com/W298/SurfelGI, MIT license, "based on GIBS presented at SIGGRAPH
  2021", commit 8361942f), a public third-party implementation. In
  `RenderPasses/Surfel/SurfelGI/SurfelRayTrace.rt.slang`, `handleHit()` does three things in order:
  - it adds the material's emission to the ray's radiance at the hit
    (`scatterPayload.radiance += scatterPayload.thp * mi.getProperties(sd).emission`);
  - it then adds the lit term from the analytic lights (`evalAnalyticLight`) on top;
  - it samples the material at an explicit LOD 0 (`ExplicitLodTextureSampler(0.f)`).

  The emission is added once per hit, before the throughput is multiplied by the BSDF weight. The cached
  surfel radiance that can end a path later does not carry it again. This matches the owner's method, but it
  is one person's reading of GIBS, not the slides. No code was copied from it.

## 3. How this repo decodes emissive for Fallout 4 (the exact fields)

Anchors were read at 38481ba. File hashes at reading (sha1, first 12): probegi.cpp 8fce865edcd0,
probebake.cpp 910cd8629279, fo4_default.frag 421a36f3fcde, cell_lights.glsl ae743350c46a, glproperty.cpp
c28e50de4c5e, renderer.cpp 161d71924f2a, materialfile.cpp e7b500e1a5c2.

### 3a. Reading the material file (BGSM = lighting material, BGEM = effect material)

| field | where it is read | note |
|---|---|---|
| `bEmitEnabled` (Own-Emit) | src/io/materialfile.cpp:176 (`in >> bAnisoLighting >> bEmitEnabled`) | BGSM |
| `cEmittanceColor` | materialfile.cpp:178-179 (`if ( bEmitEnabled ) in >> cEmittanceColor...`) | **only read when Own-Emit is set**; otherwise it stays (0,0,0) (src/io/material.h:131) |
| `fEmittanceMult` | materialfile.cpp:181 | always read |
| `bExternalEmittance`, `fLumEmittance` (v>=12), adaptive emissive (v>=13) | materialfile.cpp:182-186 | read but **not used by the renderer**; not fed to the bake (open question 5) |
| `bGlowmap` | materialfile.cpp:191 (`in >> bDissolveFade >> bAssumeShadowmask >> bGlowmap`) | BGSM |
| glow texture | textureList; the slot is chosen in src/gl/glproperty.cpp:1676-1681 (`case 2: // Glow`) | slot 5 (`BGSM1_GLOW`, :1638) when the list has 9 entries (v < 17, Fallout 4); slot 4 (`BGSM20_GLOW`, :1625) when it has 10. Returned only when `bGlowmap` is set and the slot is not empty |
| BGEM emittance color, `bGlowmap` (v>=16) | materialfile.cpp:266-274 | read. The effect shader's glow is its base color x scale (glproperty.cpp:2166-2167, renderer.cpp:1677-1678), not the emittance color |
| accessors | src/io/material.h:76 `emittanceColor()`, :176 `emittanceMultiple()`, :178 `emitEnabled()` | there is no accessor for `bGlowmap` (the renderer is a friend class) |

### 3b. From the material to the shader (Fallout 4 lighting shapes)

- src/gl/glproperty.cpp:1914-1915: `emissiveColor = cEmittanceColor; emissiveMult = fEmittanceMult`.
- glproperty.cpp:1932-1933: `hasGlowMap = bGlowmap; hasEmittance = bEmitEnabled`.
- With no material file (the NIF's own shader property), glproperty.cpp:1994-1998 reads:
  - "Emissive Color" and "Emissive Multiple";
  - `hasEmittance` = the Shader Flags 1 Own_Emit flag;
  - `hasGlowMap` = the glow shader type AND the Shader Flags 2 Glow_Map flag AND a non-empty texture slot 2.
- src/gl/renderer.cpp:1467: the `GlowMap` sampler is fileName(2), **falling back to black** when there is none.
- renderer.cpp:1506-1509: `glowMult` = emissiveMult when Glow and Lighting are on and the shape own-emits (the
  FO76 rule aside), else 0. renderer.cpp:1523-1525 set `hasEmit`, `hasGlowMap` and `glowColor`.
- src/glview.cpp:3845 sets `glowScale` (the viewer's glow brightness option, 0 when Glow is off). glview.cpp:3912
  sets `glowScaleSRGB = sqrt(glowScale)`. Both default to 1 (src/gl/glcontext.cpp:817-818).

### 3c. The shader

- res/shaders/fo4_default.frag:388: `vec4 glowMap = texture( GlowMap, offset )` (the same UV as the base map).
- fo4_default.frag:449-457:
  `emissive = hasEmit ? glowColor * glowMult * (hasGlowMap ? glowMap.rgb : 1) : 0`. No vertex color, no albedo.
- fo4_default.frag:565: `color.rgb += emissive * glowScaleSRGB`. Cell-lit draws hand it to
  res/shaders/cell_lights.glsl:466 `cellLit()`, which returns `sqrt( max( lin, 0 ) ) + emissive` (:498).
- The program works in **sqrt-of-linear** space, as three places in the code show:
  - the comment above cell_lights.glsl:466;
  - `alb = albedo * albedo` in `cellLit()`;
  - fo4_default.frag:580: "this colour is sqrt of linear light (tonemap squares it)".

  Fallout 4 draws without an sRGB framebuffer (src/gl/bsshape.cpp:324-327 turns it on only for BS version
  151 and up).

So the linear radiance an **unlit** emitter shows is `e^2`, where `e = glowColor x glowMult x g x
glowScaleSRGB` and g is the glow map's stored value. The bake must therefore:

1. **Square each texel, then average** (the mean of g^2, never the square of the mean of g). The twin's red
   `mipsq` (mask filtered at the ray's footprint, then squared) comes out at 0.52 of the reference: a binary
   mask with coverage c gives c^2 instead of c.
2. **Use glowScale = 1** (the viewer default), not the user's glow brightness option, so a bake does not depend
   on a view setting (open question 4).
3. **Copy the renderer's gates exactly:**
   - Own-Emit off: nothing glows, even with a color and a mask. The twin's red `noflag` breaks the byte gate.
   - `bGlowmap` off: the whole surface glows.
   - `bGlowmap` on with an empty slot: black, so nothing glows.

### 3d. What feeds the bake today, and where the new fields go

- **The soup.** The cell view builds it at src/cellview.cpp:2268 (`probeSoup.addTri( w[0], w[1], w[2], rgb )`),
  one linear albedo per triangle. Effect shapes, blended shapes, decals and refraction-only shapes never enter
  the soup (cellview.cpp:2201). So only **BGSM (lighting) shapes** can emit into the bake; BGEM glow cards stay
  out.
- **The loader.** It already carries three of the fields: src/nativeemit.h:128-130 has `ownEmit`, `emitColor[3]`
  and `emitMult`. They are filled from the BGSM at src/lodgen.cpp:2516-2518 (`emittanceColor()`,
  `emittanceMultiple()`, `emitEnabled()`), or from the NIF at lodgen.cpp:2433-2436. **Missing: the glow-map flag
  and the glow texture path.** Material has no `bGlowmap` accessor, and NativeSrcShape has no glow slot.
- **Bake pass 1.** It pools hits per cell and side: src/probebake.cpp:186-205 (`struct Bin`), the accumulation at
  :553-560, the record at :795-818 (`makeFinal`, albedo at :813). The per-hit material lookup already exists for
  the CAPTURE1 "hit" way (`matAt`, :425-490), including the UV at the hit point.
- **The relight.** src/probegi.cpp:428 `u.B[c] = u.a[c] * E[c]` is where Le gets added. The gather (:514), the
  passes (:743, `Bn = u.B + a x E / pi`) and the later gathers (:763) need no change, because they read `u.B`.

## 4. The proposed C++ (described, not written)

1. **Soup.** `ProbeSoup` gains a sparse emissive table. Each emissive triangle has an entry: its index, its three
   UVs, and an index into a list of emitters `{ color x mult (float3), glow texture or none, glow flag }`. Only
   triangles of a shape that own-emits with a non-zero color x mult go in. It lives in memory only. An empty
   table changes nothing anywhere.
2. **Loader.** NativeSrcShape gains `glowFlag` and `glowTex`, filled beside lodgen.cpp:2516-2518 with the
   glproperty.cpp:1676-1681 slot rule (this needs a `glowmapEnabled()` accessor on Material). `materialKey()` and
   the far-LOD bake must not read them. The nativeemit.h note on `effectTex0` describes the same guard.
3. **Bake pass 1.** `Bin` gains `le[3]`. For a hit on an emissive triangle:
   - take the UV at the hit (matAt's barycentrics);
   - read g from the glow map at **mip 0** (a fine load, like CAPTURE1's `loadFine`);
   - Le = (color x mult x g)^2 per channel, added to `le`.

   Hits on non-emissive triangles add nothing but still count in `n`, so the cell mean is an area mean.
   `makeFinal` stores `le / n`.
4. **File.** `.tbk` v4 gets one more tail block:
   - `reserved[3]` holds its count (as built: `reserved[2]` already holds what BAKE4 modelled);
   - each entry is 16 bytes, `{ u32 surfel index (top bit = back side), float Le[3] }`;
   - it is written **only when the count is > 0**;
   - v3 (FO4CS's reader) never carries it.

   probegi's `readTbk` must add the block to its exact-size rule. An older exe then refuses a file that has the
   block, which is the safe way to fail.
5. **Relight.** After probegi.cpp:428, add `if ( surfel emits ) u.B += Le`. Make it a guarded add, never an
   unconditional `+ 0.0`: that turns a -0.0 into +0.0 and breaks byte identity. Census line: the number of
   emissive surfels and their summed Le.
6. **Gates.**
   - A byte gate: on a cell with no emissive BGSM, the `.tbk` and the GI dump from the exe with the feature
     must be byte-identical to the ones from the exe before it (the twin's E3).
   - One red switch for each twin red.

## 5. The Python twin

How to run it:

- `python3 tests/spells/emissivegi1_check.py` is the plain run. It runs green, then every red, and checks the reds
  itself. It exits 0 only if green passes and every red fails its named checks.
- `EMISSIVEGI1_RED=<name>` runs one red as if it were green (exit 1).
- It takes about 20-30 s, with a peak of 269 MB (measured with `resource.getrusage`).

**The scene** is built in code: a street at night with no sky light.

- A wall at x = 0 facing +x (albedo 0.5).
- A facade at x = 320 (albedo 0.25), with a neon sign 6 units in front of it, facing the wall. The sign is
  256 x 96 units, albedo 0.2.
- The ground (albedo 0.05).
- One dim lamp, so the lit-albedo term is not zero.

The sign's material: Own-Emit, color (1, 0.25, 0.6), multiple 1.5, glow flag set. Its mask is 128 x 48 texels,
with "EAT" drawn from a 5 x 7 bitmap font in the left 70% (coverage 0.191).

**The bake** follows the C++ shape:

- 396 probes (6 x 11 x 6), 2048 Fibonacci rays each.
- 70-unit surfel cells, with the keep-the-dominant-side rule.
- One link per probe and cell: a u16 weight, an octahedral direction, the unlinked share renormalized.
- B = albedo x E + Le, gathered into six-axis cubes.
- The probe blend (radius 2 x the median spacing, weight (1 - d^2/r^2)^2, visible probes only), read at half a
  voxel (24 units) off the wall.
- The BOUNCE2 passes.

The emission-driven part at the wall is the bake with the sign glowing minus the same bake with it dark. In one
pass the lamp terms cancel exactly.

**The reference** treats the sign as an area light at 102 wall points (17 x 6): `sum Le dA cos_wall cos_sign /
r^2`, over 2 x 2 sub-samples a texel. It uses no rays, surfels or probes. K0 checks this integrator against
Lambert's closed form for a uniform polygon.

### What each check proves

| check | proves | green (measured) |
|---|---|---|
| K0 | the reference integrator is right | worst rel. error 2.68e-06 (bar 1e-3) |
| K1 | the probe system's own error on a uniform glowing quad (the floor) | ratio 1.1618, median 0.1549, p95 0.2133 |
| E1 | the masked emission reaches the wall at the right brightness, no worse than the floor | ratio 1.1432 = 0.984 x the floor; median 0.1400, p95 0.2120 |
| E2 | the right shape: the light comes from the letters, not the whole quad | profile L1 0.0390 (bar 0.10); centroid +24.1 vs +20.1 (20.3% of the offset, bar 25%) |
| E3 | no emissive surface: the feature changes no byte (sha256 of the bake file + relight dump, feature on vs off), and the gate is not vacuous | equal / differ |
| E4 | the emission enters once: the passes settle and the gain stays under 1/(1 - albedo) | 4 passes, gain 1.0986 (bound 2.0079) |

| red | defect | must fail | measured |
|---|---|---|---|
| noemit | emissive term off | E1 | ratio 0.0000 (the wall stays dark) |
| nomask | mask ignored, the whole quad glows | E1 and E2 | ratio 6.0157 (too bright); L1 0.1303, centroid -0.1 (100.5% off) |
| mipsq | mask filtered, then squared | E1 | ratio 0.5213 (too dark) |
| noflag | Own-Emit flag ignored | E3 | the no-emissive scene's hash differs |
| tailalways | emissive tail written with nothing emitting | E3 | the hash differs |
| perpass | passes re-add the previous pass (emission counted every pass) | E4 | not settled in 64 passes, gain 20877 |

### Bars: what was set before the first run, and what moved

**Before the first run**, every brightness check had absolute bars: total ratio 0.85..1.15, median 0.15,
p95 0.30, for K1 and E1 alike. E2's bars (L1 0.10, centroid 25%), E3 and E4 were as they are now.

**The first run: K1 failed** (ratio 1.1618, median 0.1549), and E1 passed by a hair (1.1432). The excess comes
from the probe system, not from the emissive term:

- The reference taken at the renderer's sample point (24 units off the wall) is already 1.095 x the reference
  at the wall (measured).
- The nearest probes stand 32 and 80 units out.
- A uniform glowing quad shows the same bias.

**Moved after the first run:**

- K1 became the measured floor, with broad sanity bars: ratio 0.70..1.30, median <= 0.25, p95 <= 0.40.
- E1 is now held **relative** to K1: ratio within 10% of K1's; median and p95 at most K1's + 0.05 / + 0.10;
  plus an absolute ratio band of 0.70..1.30.

E2, E3, E4 and K0 did not move. Every red failed on the first run, as it does now.

**Thin margins:**

- E2's centroid passes at 20.3% against a 25% bar.
- nomask's L1 (0.1303) is only 1.3 x its bar.

A wall 314 units away sees a smooth blur of the letters, so most of the shape signal is the centroid shift (the
letters sit left of center). The brightness checks have wide margins: nomask is 5.2 x the floor, mipsq 0.45 x.

### Actual output (plain run)

```
scene: 396 probes x 2048 rays, 628131 hits; wall points 102; mask coverage 0.191
-- green
K0 PASS integrator: worst relative error 2.68e-06 against Lambert's polygon form at 102 wall points (<= 1e-03)
K1 PASS method floor (uniform quad, no mask): total ratio 1.1618 (0.70..1.30), median rel err 0.1549 (<= 0.25), p95 0.2133 (<= 0.40)
E1 PASS brightness: total ratio 1.1432 (0.70..1.30), 0.9840 x the floor's (within 10%), median rel err 0.1400 (<= floor 0.1549 + 0.05), p95 0.2120 (<= floor 0.2133 + 0.10)
E2 PASS shape: profile L1 0.0390 (<= 0.10); centroid y 24.14 vs reference 20.06, error 20.3% of the offset (<= 25%)
E3 PASS identity: no-emissive scene feature on 01233912186be738.. / off 01233912186be738.. (equal); glowing scene on 82b03c8d2c58a088.. / off 01233912186be738.. (differ), 6 emissive surfels
E4 PASS passes: settled after 4 passes, gain 1.0986 (<= 1/(1-0.502) = 2.0079)
info: 6 emissive surfels of 704; self-feed of the sign through its own probes 1.89e-03 of its emission; renderer cross term 2 e sqrt(lin) at those surfels 4.72e-01 of e^2 (lamp lin 6.173e-03 vs e^2 1.034e-01, luminance)
-- red noemit: must FAIL E1 -> FAILS as required
   E1 FAIL brightness: total ratio 0.0000 (0.70..1.30), 0.0000 x the floor's (within 10%), median rel err 1.0000 (<= floor 0.1549 + 0.05), p95 1.0000 (<= floor 0.2133 + 0.10)
-- red nomask: must FAIL E1+E2 -> FAILS as required
   E1 FAIL brightness: total ratio 6.0157 (0.70..1.30), 5.1780 x the floor's (within 10%), median rel err 5.0797 (<= floor 0.1549 + 0.05), p95 6.6190 (<= floor 0.2133 + 0.10)
   E2 FAIL shape: profile L1 0.1303 (<= 0.10); centroid y -0.10 vs reference 20.06, error 100.5% of the offset (<= 25%)
-- red mipsq: must FAIL E1 -> FAILS as required
   E1 FAIL brightness: total ratio 0.5213 (0.70..1.30), 0.4487 x the floor's (within 10%), median rel err 0.4796 (<= floor 0.1549 + 0.05), p95 0.5496 (<= floor 0.2133 + 0.10)
-- red noflag: must FAIL E3 -> FAILS as required
   E3 FAIL identity: no-emissive scene feature on 82b03c8d2c58a088.. / off 01233912186be738.. (DIFFER); glowing scene on 82b03c8d2c58a088.. / off 01233912186be738.. (differ), 6 emissive surfels
-- red tailalways: must FAIL E3 -> FAILS as required
   E3 FAIL identity: no-emissive scene feature on 238cdd56d8bc676e.. / off 01233912186be738.. (DIFFER); glowing scene on 82b03c8d2c58a088.. / off 01233912186be738.. (differ), 6 emissive surfels
-- red perpass: must FAIL E4 -> FAILS as required
   E4 FAIL passes: NOT settled after 64 passes, gain 20876.9735 (<= 1/(1-0.502) = 2.0079)
emissivegi1 PASS (green PASS, reds all fail) in 18.8 s
```

## 6. Double counting, measured or reasoned

1. **Emission vs the lit-albedo term, at one surfel: none by construction.** Le is the emitted part and
   albedo x E the reflected part, and they come from different inputs (the glow fields vs the diffuse map). They
   would overlap only if a diffuse map had the glow painted in. Even then the painted glow is clamped to 0..1 and
   lit, so it is still not emission. This cannot be measured without real materials (local lane, item 1).
2. **Across bounce passes: none, as long as Le stays in B_1.** probegi.cpp:743 rebuilds `Bn = u.B + ...` from
   pass 1's B on every pass, so Le enters once. E4 proves it (gain 1.0986 <= 2.0079). The red perpass, which
   builds B_k from B_{k-1}, diverges (gain 20877). Rule for the C++: add Le at :428 only, never inside the pass
   loop.
3. **A flat emitter lighting itself through its probes: negligible for an axis-aligned sign.** The measured
   self-feed is 1.89e-03 (the sign's surfels with the lamp off: settled B over B_1, minus 1). The ambient cube
   keeps the sign's own light on each probe's +x side, and the sign (normal -x) reads the -x side. A sign at
   45 degrees would read some of its own light through the n^2 blend of two axes (open question 2).
4. **Placed lights that already fake the glow: the real risk.** Bethesda often puts a light next to a glowing
   fixture. With this feature the bake counts both the fixture's emission and the light. The twin cannot measure
   this without game data (local lane, item 2).
5. **The renderer's cross term: a mismatch, not double counting.** The renderer adds e after the square root, so
   a LIT emitter shows (sqrt(lin) + e)^2 = lin + 2 e sqrt(lin) + e^2. The bake sends lin + e^2
   (B = albedo x E + Le). With the twin's dim lamp, the missing 2 e sqrt(lin) term is 0.472 of e^2 at the sign's
   surfels (lin = 6.2e-3 against e^2 = 0.103, luminance). This design keeps the physical sum. Whether to copy the
   renderer's sum instead is open question 3.
6. **The unlinked renormalization.** probegi.cpp scales each probe's gather by (linked + unlinked) / linked. An
   interior probe next to an opening into unloaded space therefore spreads a nearby sign's light over the
   unlinked directions too, which makes it too bright. This already happens for lit surfaces; emitters make it
   easier to see. It is not in the twin: the twin is an exterior, where misses are sky and the night sky is 0.

## 7. Open questions

1. Are emissive surfels **rare** enough that sampling the glow map at mip 0 on every hit costs little? Measure on
   real cells: the number of emissive BGSM triangles in the soup, and the bake time in ms with and without the
   feature.
2. Oblique emitters: is the self-feed through the n^2 axis blend visible? A twin variant with a sign at
   45 degrees would measure it; that was not done here.
3. Should B use the renderer's sum (with the cross term) to match what the view shows, or the physical
   lin + e^2? This page proposes the physical sum.
4. The viewer's glow brightness option (glowScale): fix it at 1 for the bake (as proposed), or bake it in?
5. `bExternalEmittance`, `fLumEmittance` and adaptive emissive are read and then ignored by the renderer. What the
   game does with them is not known from public sources. The bake ignores them, as the renderer does.
6. BGEM glow cards never enter the soup (cellview.cpp:2201). Should an emissive effect (a soft glow card, mist)
   emit into the bake? Leaving them out avoids double counting with the placed lights that usually come with
   them.
7. Back-side surfels (v4): a two-sided emissive mesh gives Le to both sides. That is right for neon tubes. Is it
   right for one-sided signs whose back is another mesh (the back pools with the facade)?

## 8. What the local lane must still do with real cells

1. **Census.** In a few cells (an interior with neon, such as a bar; Diamond City; an exterior street at night),
   count:
   - the BGSM shapes in the soup with Own-Emit and color x mult > 0;
   - how many of them have bGlowmap set;
   - how many have it set with an empty slot (black, so no emission);
   - the glow-map slot for each material version.
2. **Double-count census.** For each emissive shape, find the placed lights within (say) 256 units, and compare
   the shape's bake emission with those lights' contribution at the same surfels. Then decide whether emissive
   shapes with a nearby light are left out, scaled or kept.
3. **Build the C++ of section 4**, with a red switch for each twin red, and prove the byte gate (E3) on a cell
   with no emissive BGSM: the `.tbk` and the GI dump byte-identical against the exe before the lane.
4. **Compare with the game.** Beside one neon sign, compare the Pass view (GI) and the final picture with the
   game: is the wall lit as much?
5. **Re-run** cell_gi.sh, probe_bake.py synth and rooms, and cell_pass.sh. Their fixtures have no emissive
   material, so no change is expected.

## 9. As built (local lane, 2026-10-04)

- The `.tbk` count sits in `reserved[3]` (section 4 is corrected): `reserved[2]` already held BAKE4's flags.
- Census (item 8.1/8.2): Goodneighbor 5,-3 at 23:00, 56 glowing shapes (7,710 triangles, 6 emitters, 3 glow
  maps), 49 of 52 placements with a lit placed light within 256 units; Vault111Cryo 251 shapes, 157 of 157
  placements; The Third Rail 20 shapes (19 of 19); The Memory Den 22 shapes (17 of 17). No glow flag without a
  map, no unread map, no shape without UVs.
- Open question 3 and item 8.2: the physical sum is baked and the placed lights stay. The glow adds a mean
  +2.8% to the probes within 400 units of the Goodneighbor neon (99th percentile +17.7%, most +45%, none
  darker); the placed lights also give the game's direct light, which the bake cannot remove.
- Real-cell gate: tests/spells/emissive_cell_check.py (MAPS, SURF, GAIN, OFF, red nomask). Its SURF twin judges
  every surfel next to a glowing triangle, weighted as the probes' rays land, with local occlusion through
  alpha-test holes: bake/twin 1.466 (bar 0.6-1.6), red 4.373.
- Byte gate (E3 on a real cell): Vault111Cryo with WW_CELL_EMISSIVE_PIN=off: soup, cube dump and all six .tbk
  identical to the exe before the lane.
- Not done: the game comparison (item 8.4) beside a neon sign.
