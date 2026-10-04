# VOLFOG1 -- volumetric fog lit by the probe GI (design, prep lane)

Lane VOLFOG1 (cloud, prep), 2026-10-03. Research and a Python twin only: no C++ or GLSL changed, no game data used.
Twin: `tests/spells/volfog1_check.py` (python3 + numpy, about 55 s for the green run and all four reds, peak 276 MB).

The goal: the air in a cell scatters the light that is really there. A lamp in a red room lights the haze red,
because most of the light in that room has bounced off red walls. Outdoors and through openings, the sun (and at
night the moon) draws shafts in the haze. Today the cell view has the game's distance fog only (FOG1/FOG2: a blend
toward a fog color by distance and height). Nothing in the air takes light.

## 1. Summary

- **Grid.** A camera-space froxel grid: 160 x 90 tiles at 1080p (12 px tiles) x 64 depth slices, exponential in view
  depth from 16 to 16384 units (the first slice starts at the eye). That is 921,600 froxels.
- **What lights a froxel.**
  `S = sigma_s x [ direct + GI ]`, in the viewport's units (section 4):
  - direct: each light's PRTP2 curve x the Henyey-Greenstein (HG) phase x its shadow x the fog's transmittance to the light;
  - GI: the probe relight's six-axis cube at the froxel, read as an average (L0) plus a direction (L1):
    `GI = 1/(4 pi) x max(0, (2/3) sum_6 E_axis + 3 g (E+ - E-) . d) x exp(-sigma_t dbar)`.
- **Where GI comes from.** A new AIR grid built next to probegi's surface grid: voxels through the room air (not only
  next to surfaces), each blending the probes it can see and that share its room. The froxel samples it trilinearly.
- **Integration.** Front to back per tile column, per slice `(S - S exp(-sigma_t D)) / sigma_t` (the exact integral of
  a constant S over the slice). The pixel reads the integrated volume at its depth.
- **Temporal.** Each frame samples a froxel at a jittered depth inside its slice (a 16-frame low-discrepancy cycle);
  the history is reprojected and blended at 0.95.
- **Proven in the twin** (closed boxes, no game data): the fog's color follows the walls' bounce to 0.018 in
  chromaticity in a red room and 0.026 in a red/green room; the total within 2.5%; shafts within 2.2% of the
  brightest ray. With the GI term off the red haze is gone (chromaticity off by 0.29: FAIL, as it must be).
- **Cost.** About 0.5-1 ms a frame at 1080p on a GTX 1070-class GPU, estimated (section 11). The GPU code does not exist
  yet, so nothing is timed. About 22 MB of froxel volumes.

## 2. What exists (read before designing)

- **Distance fog** (FOG1 weather fog, `res/shaders/lookdev_fog.glsl`; FOG2 interior fog, PRTP plan 2l). The game's
  formula. It blends the lit color toward a fog color by distance and height. It is not a medium: it holds no
  density, no light, and no shadow. It stays as it is (section 5).
- **Probe GI** (PRTP plan 2g, 2i, 2aj, 2ah). The bake links each probe to its surfels. The relight lights the surfels
  and gathers a six-axis cube per probe (E per axis = sum B x solid angle x max(axis . dir, 0)), repeating until the
  bounce settles. It then blends a voxel grid **next to surfaces only** (48-unit voxels, the probes within
  2 x the median spacing, each probe only if the voxel can see it). The renderer reads `cellGiE(P, N)` and divides by pi.
  The air in the middle of a room has no voxels today.
- **FO4CS Volumetric Air** (PRTP plan 2f, read-only). It uses a camera froxel grid, the sun x HG x cascades, and one
  sky term (the DALC at world up) for every froxel. It reads no probes and no point lights, and it refuses interiors. 2f
  already says what PRTP gives it: a probe lookup per froxel, gated by room ids. This design is that idea, built
  in NifSkope's own cell view first.
- **Shadows.** Placed shadow lights get a 512 depth cube each (16 nearest; SHADOW1, plan 2k). The sun has the
  Lookdev cascades (`lookdev_csmdepth`, gate pbr_csm1). Both can be sampled at a froxel's world position.
- **Rooms.** `.tbk` v4 room ids per probe and room boxes (BAKE4, BOUNCE2). BOUNCE2 already uses them to stop light
  passing between rooms.

## 3. Sources

Fetched this session:
- Godot Engine (MIT), `servers/rendering/renderer_rd/shaders/environment/volumetric_fog_process.glsl`,
  https://raw.githubusercontent.com/godotengine/godot/master/servers/rendering/renderer_rd/shaders/environment/volumetric_fog_process.glsl
  What it does (summarized from the fetched source):
  - a froxel grid with depth `pow(z, detail_spread)`;
  - a per-frame Halton jitter of the sample position, used only when the reprojection is valid;
  - history `mix(current, reprojected, temporal_blend)`;
  - the HG phase `(1/4pi) (1 - g^2) / (1 + g^2 - 2 g cos)^1.5`;
  - the directional lights' shadow taken from the shadow atlas.

  GI enters as an **isotropic** term: the SDFGI probes' average (L0) light, trilinearly interpolated, times
  `gi_inject`; VoxelGI cone samples likewise. Our design keeps the L1 term as well (section 4.3), because a six-axis
  cube carries it at no extra cost.
- Godot docs source, https://raw.githubusercontent.com/godotengine/godot-docs/master/tutorials/3d/volumetric_fog.rst:
  - volume size and depth are settings;
  - detail spread puts more slices near the camera;
  - "GI Inject" scales global illumination in the fog;
  - temporal reprojection trades smoothness for ghosting.

Named in the brief, NOT fetched (the network proxy blocked bartwronski.com, advances.realtimerendering.com, ea.com,
slideshare.net, dev.epicgames.com, docs.godotengine.org, web.archive.org). Nothing below depends on a detail only
these would give. Every formula used is derived in section 4, and every number is our own choice or our own
measurement:
- B. Wronski, "Volumetric Fog: Unified Compute Shader Based Solution to Atmospheric Scattering", SIGGRAPH 2014
  Advances in Real-Time Rendering (Ubisoft, Assassin's Creed IV). Origin of the camera-space 3D-texture
  ("froxel") approach. Slides: https://bartwronski.com/publications/ (not fetched).
- S. Hillaire, "Physically Based and Unified Volumetric Rendering in Frostbite", SIGGRAPH 2015 Advances course,
  https://www.ea.com/frostbite/news/physically-based-unified-volumetric-rendering-in-frostbite (not fetched). The
  per-slice integral `(S - S exp(-sigma_t D)) / sigma_t` is commonly attributed to it. We derive it below rather
  than cite it.
- Unreal Engine, "Volumetric Fog" documentation,
  https://dev.epicgames.com/documentation/en-us/unreal-engine/volumetric-fog-in-unreal-engine (not fetched). From a
  search snippet only: the volumetric lightmap feeds static indirect light to fog voxels by interpolation, and Lumen
  feeds dynamic GI to fog at lower quality. That is the same idea as our air grid.
- The PRTP GI itself: Stefanov, "Global Illumination in Tom Clancy's The Division", GDC 2016 (PRTP_PLAN.md).

## 4. The in-scattering term

### 4.1 Units

Everything is in the viewport's units, where a white diffuse surface shows its irradiance. `B` is a surfel's
outgoing light, `B = albedo x E`, and a surfel's radiance is `B / pi`. The probe cube's `E_axis = sum B x Omega x
max(axis . dir, 0)` is pi x the true irradiance; the shader's `cellGiE / pi` is the true irradiance. Fog radiance is
shown the same way: displayed = pi x radiance. A point light's irradiance at x is `c x atten(d)` (PRTP2:
`atten = (1 - saturate(d/r)^2)^2.2` for the vanilla constants).

### 4.2 Direct light

For a light seen from the froxel center x, let `w_L` be the unit vector from x toward the light, and `d` the unit
view ray from the camera to x:

    direct_i = pi x p_HG(w_L . d) x c_i x atten_i(|L - x|) x cone_i x shadow_i(x) x exp(-sigma_t |L - x|)

- `p_HG(cos) = (1 - g^2) / (4 pi (1 + g^2 - 2 g cos)^1.5)`. A positive g scatters forward: the haze glows when you
  look toward the light.
- The shadow is the light's existing map (SHADOW1 cube, or the sun cascades), compared at x. A froxel has no normal,
  so there is no normal offset, only the depth bias.
- The sun and the moon: `pi x p_HG(w_sun . d) x c_sun x shadow x T_sun(x)`. For a shaft into a room, `T_sun` is the
  fog's transmittance from where the sun's ray enters the air to x. The twin keeps this as a second shadow-map channel
  (the entry depth). Without that channel the shaft reads about 6% too bright in the twin's room (measured while
  building K7).
- The light's own transmittance `exp(-sigma_t |L - x|)` is exact for uniform fog. With varying density it needs a
  volumetric shadow (open, section 12).

### 4.3 GI from the six-axis cube

The in-scattered radiance from all directions is `sigma_s x integral p(w . d) L(w) dw`, where w points toward where
the light comes from. Expand the phase to first order:

    p(w . d) ~= (1 + 3 g (w . d)) / (4 pi)

The integral then needs only two moments of the incoming light:

- **fluence** `Phi = integral L dw`;
- **first moment** `M = integral L w dw`.

The six-axis cube gives the first moment **exactly**. Take the pair of axes along x:
`E_+x - E_-x = integral B max(w_x, 0) dw - integral B max(-w_x, 0) dw = integral B w_x dw`. The same holds for y and z.

The fluence is not exact. The six axes measure `integral B (|w_x| + |w_y| + |w_z|) dw`, and that weight runs from
1 to sqrt 3 with a mean of 3/2. So we use `Phi ~= (2/3) sum_6 E`. This is exact for light that is the same from
every direction. The twin's furnace check K1 proves the 2/3 and the units together, to 0.3%.

    GI = sigma_s / (4 pi) x max(0, (2/3) sum_6 E + 3 g (E+ - E-) . d) x exp(-sigma_t dbar)

- `dbar` is the probes' mean link distance, blended like E. On the way from a wall to the froxel the fog dims the
  bounce light too. The bake already stores mean distances per octant, so this costs one more channel.
- Clamped at 0, because the L1 term can go negative for large g in strongly one-sided light.

**Why not the cube's n^2 blend, as surfaces use?** A froxel has no normal. Using the cube as a radiance basis
(Valve's ambient cube) would need the six radiances, not irradiances, and a linear solve per probe. The L0 + L1
read uses the stored E as they are. It is what the twin gates.

### 4.4 Phase function

HG with one g per fog. The game's fog records give no phase. Proposal: g = 0.3 indoors (the twin's value), with
the outdoor value open (FO4CS Volumetric Air uses HG for the sun, PRTP plan 2f; its g was not read for this lane).

## 5. Integration, and the game's distance fog

Per tile column, slice k from the camera outward. Each slice has a view-depth thickness `D_z`, a ray length
`D = D_z / cos(angle to the view axis)`, and `a = exp(-sigma_t D)`:

    L += T x S x (1 - a) / sigma_t        (the integral of S exp(-sigma_t t) over the slice, S constant)
    T *= a

Store `(L.rgb, T)` at the slice's far boundary. The pixel reads `(L, T)` at its own depth (linear between the slice
bounds), and its color becomes `color x T + L`. Use `-expm1(-sigma_t D)` for `1 - a`, so a thin fog keeps its
precision. The twin uses it, and K1 runs at sigma_t = 1e-9.

**With the game's distance fog.** The game's fog is the look the cell's author chose, so it stays. The volumetric
layer is a separate row, off by default, and off must be byte-identical. Order: surface, then the volumetric
`color x T + L`, then the game's distance fog, then the imagespace (as FOG2 places fog before the imagespace).
Density with no hand dial: fit sigma_t to the cell's own fog so the two agree on how much they hide. The game's fog
alpha at distance d is `f(d) = min(max, ((d - near)/(far - near))^power)`. Take sigma_t such that
`1 - exp(-sigma_t (far - near)) = max` (a rule to confirm: open question 1). Interior fog clamps (2l) already give
every interior a far distance.

## 6. The air grid (how it plugs into the probes)

probegi's grid lights surfaces and only stores voxels next to them. The fog needs light in the air, so add a
second grid over the room air:

1. **Voxels.** 64 units across the baked volume. Keep a voxel if it lies in air the bake reached (inside a room box,
   or under the far hoist outdoors). Lane BOUNCE2 found that room boxes cover surfel air poorly (46-53% of
   surfels within 35 u). For air, a voxel center counts as "in a room" when it is inside a box, else it takes the
   room of the nearest probe it can see.
2. **Probes per voxel.** Same rule as the surface grid and the BOUNCE2 feed: a probe within the radius r that the
   voxel can SEE (a soup ray) and that is in its room. A doorway probe belongs to both rooms. Weight
   `(1 - d^2/r^2)^2`, falling back to the closest visible probe within 2r.
   - **The radius is tighter than the surface grid's: r = 1.5 x the median probe spacing** (probegi uses 2). The twin
     measured why. At 2, the haze along a colored wall loses a fifth of its tint, because the blend reaches across
     the room (section 10, table A).
   - Visibility and room lists are fixed by the geometry, so they are built once per bake. Only the cube values are
     re-blended per relight (milliseconds), as BOUNCE2's feed lists are.
3. **Stored per voxel**, as four RGBA16F 3D textures (13 values):
   - `Phi.rgb = (2/3) sum E` and `dbar`;
   - `M.x.rgb`, `M.y.rgb` and `M.z.rgb`, the first moment `(E+ - E-)` per axis.

   The shader does 4 trilinear fetches per froxel and no per-froxel sum.
4. **Outdoors** (SKY1, plan 2ah).
   - The probes' cubes already hold the sky through their sky shares, so covered air gets less sky, as 2f asked.
   - Beyond the air grid, the far map's probes (2h), one per cell 512 u over the roofline, are too high to stand for
     street-level air. Fall back to the weather's directional ambient read as L0 + L1, blended out over the grid's
     last voxel (open question 4).
5. **Interiors with Show Sky** (SKYINT1, 2ak): their sky shares are already in the probes; nothing extra.

## 7. Temporal reprojection

- **Jitter.** Each frame samples a froxel at a depth inside its slice set by a 16-frame van der Corput (base 2)
  sequence. Godot jitters xyz with a Halton table; xy jitter is a possible addition.
- **Reproject.** Take the froxel's world position this frame and project it with last frame's view-projection into
  last frame's froxel volume (trilinear). A miss (outside the old volume) takes the current sample alone.
- **Blend.** `history x 0.95 + current x 0.05`.
  - The twin measured 0.9 against 0.95 on a thin sun sheet: the displayed value swings -25%..+34% at 0.9 and
    -16%..+22% at 0.95 (section 10, table C).
  - 0.95 ghosts more when lights move. Placed lights are static in the cell view; the sun moves with the hour slider,
    so reset the history on a weather or hour change.
- The light inputs (shadow maps, air grid) are static between edits, so the history only has to absorb camera
  motion.

## 8. Shafts

- **Sun and moon outdoors.** The sun term reads the Lookdev cascades at the froxel.
  - The moon: in Lookdev the moon currently gives no light (PBRWX1). The game's night directional light is driven by
    the weather's night sun colors along the sky arc. Which direction it takes at night is unread. Open question 5:
    it must not be guessed.
- **Interior shafts through openings.**
  - Show Sky interiors with Sunlight Shadows (no vanilla cell sets it, SKYINT1) would get the sun the same way.
  - Placed shadow lights already have cube maps: a lamp behind a doorway draws its own shaft through the doorway.
  - The twin's K7 is a sun through a roof hole with an 8-unit shadow map: within 2.2% of the brightest ray, and
    dark rays read 0.
- **Thin openings.** Measured in the twin, outside the gate (table B): with 8-unit texels, a 32-unit slot's shaft
  reads -7% to +14% depending on how the slot meets the texel grid. Rule: shadow texel <= opening / 8 for 5%. The
  cell's sun cascade texel near the camera must be checked against the real window sizes (local lane).

## 9. The twin: `tests/spells/volfog1_check.py`

**Scenes.** Closed boxes 512 x 512 x 320 units (7.3 x 7.3 x 4.6 m), built in code:
- **red:** four red walls (0.75, 0.10, 0.08), grey floor and ceiling (0.45);
- **grey:** every surface 0.45;
- **split:** +X wall red, -X wall green (0.10, 0.65, 0.10), the rest grey;
- **dark:** albedo 0, with a 128 x 128 skylight, or a 32 x 384 slot, and a sun tilted (0.25, 0.10, 1).

One white point light at (256, 300, 280), radius 1000, PRTP2 curve. Fog: sigma_t = 0.02 per meter (1/3500 per
unit), single-scatter albedo 0.9, g 0.3. Camera at (256, 20, 160) looking +Y, 90 degrees across, 1080p. Test rays go
through tile centers.

**Reference (brute force, shares only the scene description).**
- Exact radiosity: 1,760 patches (25.6 u), point-to-polygon form factors (analytic), and a direct solve per channel.
- For each camera ray, 192 points (2,048 in the dark scenes). Each point gathers 4,096 directions, randomly rotated
  per point, to the walls, with the exact HG phase and the fog's transmittance to the wall. The light (or the sun's
  exact hole test) adds HG and the transmittance to it.

**Estimate (what the shader would do).**
1. Surfels in ~70-unit cells and a 4 x 4 x 3 probe lattice (128 / 128 / 107 u).
2. The bake: 2,048 Fibonacci rays per probe; links carry a weight, a mean direction and a mean distance.
3. The probegi relight with the BOUNCE2 feedback until it settles (13 passes).
4. The air grid of section 6, then the froxel grid and the integration of section 5.
5. Shafts use a light-space shadow map (8 u texels, bilinear PCF, the entry-depth channel), jittered + history 0.95.

### Checks

Bars were set before the first run except where noted.

| check | what it proves | measured (green) | bar |
|---|---|---|---|
| K0 | the reference's form factors close: every patch's row sums to 1 | worst 7.3e-14 | 1e-6 |
| K1 | furnace (walls glow 1, no lamp): fog = sigma_s x length. Proves the phase normalization, the 2/3, the units and the integration | reference 0.0000, estimate 0.0030 | 0.01 |
| K2 | the estimate's probe cubes against the exact irradiance at the probe points (the GI chain the fog reads) | median 0.039, worst 0.092 | 0.05 / 0.12 |
| K3 | red room: fog radiance (R+G+B) per ray | worst 0.0245, median 0.0053 | 0.10 |
| K4 | red room: fog chromaticity r = R/(R+G+B). Floor: the reference itself is tinted (r - 1/3 >= 0.05) | worst diff 0.0181; reference r 0.488..0.625 (floor 0.154) | 0.02 |
| K5 | grey room: no tint in either path (the tint is not an artifact of the method) | 0.00000 | 0.005 |
| K6 | red/green room: per-ray (r - g) follows the nearer wall. Floor: the reference's spread >= 0.06 | worst 0.0257; spread 0.294 | 0.03 |
| K7 | skylight shaft, 64 slices, 8 u shadow map, jitter + history: error / brightest ray over the last 16 frames. Floor: a missing ray reads < 1% | worst 0.0220; dark rays 0.0000 | 0.10 |
| K8 | thin sun sheet (32 u slot), cheap 24-slice grid, exact sun test: one fixed sample per slice misses the sheet entirely; jitter + history shows it | fixed 1.000 (floor >= 0.5); jittered worst frame 0.191, 16-frame mean 0.090 | 0.25 (set after the first run, see 10) |

Per-ray numbers in the red room (from the run):

    ray 0 tile (80, 45): ref 2.2864e-01 r 0.577 | est 2.2951e-01 r 0.565 | direct-only r 0.333 | GI share of red 0.77
    ray 1 tile (10, 45): ref 1.6213e-01 r 0.624 | est 1.6264e-01 r 0.606 | direct-only r 0.333 | GI share of red 0.82
    ray 3 tile (80, 8):  ref 2.4359e-01 r 0.488 | est 2.3932e-01 r 0.494 | direct-only r 0.333 | GI share of red 0.62

**In plain words.** In this red room, 62-84% of the haze's red comes from the bounce, not from the lamp. Without
the GI term the haze would be the lamp's white (r = 0.333). The probe-fed estimate gives r 0.49-0.61 against the
reference's 0.49-0.63.

### Reds

The default run executes all four and requires each to fail the check it targets.

| red | broken on purpose | result |
|---|---|---|
| gioff | the froxels take no GI | K3 FAIL (worst 0.70), K4 FAIL (0.29), K6 FAIL (0.165) |
| flat | one GI value (the room's mean) for every froxel, like FO4CS's single sky term | K6 FAIL (0.106) |
| noshadow | the sun's shadow ignored in the fog | K7 FAIL (3.23), K8 FAIL |
| nojitter | K8's accumulated arm without jitter | K8 FAIL (1.000) |

Run it with `python3 tests/spells/volfog1_check.py`. The run prints PASS/FAIL per check and the line
`VOLFOG1 SELF-TEST PASS|FAIL`, and exits nonzero on any failure. `--red NAME` (or `VOLFOG1_RED=NAME`) runs one red
and exits 1 when it fails, as it must.

## 10. What the twin taught (measured)

**First run.** The first run used the surface grid's blend radius (2 x spacing) for the air and left out the sun's
transmittance inside the room.
- K4 failed at 0.0220 against 0.02, and K6 at 0.0334 against 0.03. In both, the estimate's tint near the walls was
  too weak.
- Both fixes are design changes, kept in this doc (6.2, 4.2). The bars of K4 and K6 did not move.

**K8 was redesigned after its first run.**
- The first form asked a 96-frame EMA at 0.9 to land within 5% on a skylight shaft. That mixed in the 6% sun-path
  bias above, and one EMA frame is not a converged mean.
- The second form put a thin sheet on a 24-slice grid. There, one fixed sample per slice misses the sheet completely
  (error 1.000), the failure jitter exists to prevent.
- Even a full 16-sample cycle leaves -7..+11% per ray: 16 depth samples of a sheet that covers a third of a slice.
  The displayed EMA swings more (table C).
- The bar 0.25 was set from those measurements. K8 therefore proves "the sheet is seen, within a quarter, on every
  frame", not convergence.

**Table A: air-grid blend radius against the near-wall tint** (split room, the four wall-hugging rays, worst |d(r-g)|):

| probe spacing (probes) | r = 2.0 x | r = 1.5 x | r = 1.2 x |
|---|---|---|---|
| 128/128/107 u (48) | 0.033 | 0.026 | 0.021 |
| 85/85/80 u (144) | 0.025 | 0.020 | 0.018 |

64- and 32-unit air voxels differ by under 0.001. A residual of about 0.018 remains even with dense probes. No probe
stands closer to a wall than half a spacing, while the haze along the wall sees more wall than any probe does.

**Table B: shadow-map texel against a thin opening** (32-unit slot, 8 u texels, 24 slices, the 16-frame mean against
the exact sun test): -7% to +14% per ray.

**Table C: history weight on the thin sheet** (24 slices, exact sun test, the displayed value over one 16-frame
cycle, per ray): 0.9 swings -25%..+34%, 0.95 swings -16%..+22%. With 64 slices the same rays read a steady -5..-6%
from one fixed sample, so a full grid does not alias this sheet.

## 11. Cost at 1080p (estimate; nothing timed)

| item | count | note |
|---|---|---|
| froxels | 160 x 90 x 64 = 921,600 | 12 px tiles; UE-like 16 px tiles would be 120 x 68 x 64 = 522,240 |
| volumes | 3 x RGBA16F x 921,600 = 22 MB | inject (L.rgb, sigma_t), history, integrated |
| air grid | e.g. 64 x 64 x 16 voxels x 4 RGBA16F = 2 MB | an interior 4096 x 4096 x 1024 u at 64 u |

Per-frame passes:
1. **Inject** (compute, per froxel):
   - 4 air-grid fetches;
   - the sun: cascade pick + a 2 x 2 compare gather;
   - the point lights in the froxel's cluster, about 30 ALU each, plus one cube compare for a shadow light;
   - HG per light, 1 history fetch, the jitter.

   At ~10 fetches and ~500 ALU a froxel: about 9 M fetches and 0.5 GFLOP a frame.
2. **Integrate** (per tile column, 64 steps): read and write 7.4 MB.
3. **Apply** (per pixel): one trilinear fetch, plus read-modify-write of the color target (2.07 M px).

Bandwidth is about 70 MB a frame: about 0.27 ms at 256 GB/s (GTX 1070) and 0.16 ms at 448 GB/s.

ALU at 6.5 TFLOPS is 0.08 ms ideal. With the usual 5-10x loss to divergence and latency in such passes, the
estimate is **0.5-1.0 ms**, dominated by inject with many lights. Half resolution in depth (32 slices), or 16 px
tiles, roughly halves inject. K8 shows what 24 slices cost on thin shafts. The local lane must time it on bungo's GPU.

CPU, once per bake: the air grid's visibility lists. At about 20 probes a voxel and 65k voxels, that is about 1.3 M
BVH rays; probegi's grid does a similar count in a fraction of a second. Per relight, re-blending is milliseconds.

## 12. Open questions

1. **Density from the cell's fog.** Is `1 - exp(-sigma_t (far - near)) = max` the right tie to the game's fog, or
   should the volumetric layer stay a separate preview with its own record-free default? It must not become a hand
   dial (SKYFULL1's note on the physical-atmosphere direction).
2. **Height fog.** FOG1/FOG2 carry height bands. A height-varying density breaks the exact `exp(-sigma_t |L - x|)`
   to lights; it needs either a per-light march or Hillaire's extinction volume (a volumetric shadow map).
3. **Phase g** indoors and outdoors. No record carries it.
4. **The air outside the baked block** (exteriors). Fall back to the weather DALC read as L0 + L1? Over what blend
   distance?
5. **The moon's light**: its direction and color at night in the game. Unread; do not guess.
6. **xy jitter and a spatial filter** to bring K8's worst frame below 10% on a cheap grid.
7. **Effects in the fog**: the haze cards (FXLIT1) are already lit fog. Drawing both doubles the haze. Leave them
   as they are, or fade them where the volumetric layer is on?

## 13. What the local lane must do with real cells

1. Build the air grid in `src/probegi.cpp` beside the surface grid, with visibility rays and room lists. Dump it
   (`gi_air.bin`) for a checker.
2. Write the inject / integrate / apply passes. The apply goes in `fo4_cell` / `pbrm_cell` / `fo4_effectcell` before
   `wwFog`. Add a row "Volumetric fog" in the PRTP band, off by default; off must be byte-identical (the
   pbr_shade_ab rule).
3. Gate with a real-cell twin in the style of `cell_gi_check.py`:
   - rebuild the air grid from the dump and the soup (own rays, own room boxes);
   - require that no voxel blends a probe from another room or through a wall;
   - read a froxel probe (a WW_CELL_* probe mode writing one slice's S) at known positions, against the twin's L0+L1
     read of the rebuilt grid;
   - reds `gioff` and `flat` as here, and a `norooms` red that must make a lit room's haze leak into a dark
     neighbor.

   Cells: Vault111Cryo (colored lamps, rooms), the Museum of Freedom (Show Sky, openings), Concord (sun shafts under
   porches).
4. Check the sun cascade texel near the camera against the cells' real windows (table B's rule).
5. Time the passes at 1080p on bungo's GPU, against section 11.
6. Picture: the same view with the row off and on, haze near a colored wall. The red haze should be visible in a
   red-lit room; Vault 111's blue-green lighting is the natural real case.

## Appendix: the green run, as printed (2026-10-03)

```
VOLFOG1 twin: box [512, 512, 320], fog sigma_t 0.000286 /u (0.02 /m), albedo 0.9, g 0.30; froxels 160x90x64; red=none
K0 PASS reference form factors: 1760 patches, worst |1 - row sum| 7.28e-14 (bar 1e-6)
K1 PASS furnace (walls glow 1, no lamp): fog / (sigma_s x length) off by 0.0000 (reference), 0.0030 (estimate); bar 0.01
K2 PASS probe cubes (red room, 48 probes, 13 relight passes): median 0.0389, worst 0.0922 (bars 0.05 / 0.12)
   ray 0 tile (80, 45): ref 2.2864e-01 r 0.577 | est 2.2951e-01 r 0.565 | direct-only r 0.333 | GI share of red 0.77
   ray 1 tile (10, 45): ref 1.6213e-01 r 0.624 | est 1.6264e-01 r 0.606 | direct-only r 0.333 | GI share of red 0.82
   ray 2 tile (150, 45): ref 1.6072e-01 r 0.625 | est 1.6117e-01 r 0.607 | direct-only r 0.333 | GI share of red 0.82
   ray 3 tile (80, 8): ref 2.4359e-01 r 0.488 | est 2.3932e-01 r 0.494 | direct-only r 0.333 | GI share of red 0.62
   ray 4 tile (80, 82): ref 1.6409e-01 r 0.551 | est 1.6323e-01 r 0.559 | direct-only r 0.333 | GI share of red 0.81
   ray 5 tile (30, 70): ref 1.9511e-01 r 0.612 | est 1.9989e-01 r 0.604 | direct-only r 0.333 | GI share of red 0.84
   ray 6 tile (130, 20): ref 2.1033e-01 r 0.604 | est 2.1259e-01 r 0.593 | direct-only r 0.333 | GI share of red 0.79
K3 PASS red room fog radiance per ray: worst rel 0.0245, median 0.0053 (bar 0.10)
K4 PASS red room tint r=R/(R+G+B): ref 0.488..0.625, worst |est - ref| 0.0181 (bar 0.02); ref tint floor 0.154 (bar >= 0.05)
K5 PASS grey room: worst |r - 1/3| 0.00000 over both paths (bar 0.005)
   split ray 0 tile (80, 45): ref r-g +0.0107 | est r-g +0.0114
   split ray 1 tile (10, 45): ref r-g -0.1295 | est r-g -0.1073
   split ray 2 tile (150, 45): ref r-g +0.1647 | est r-g +0.1390
   split ray 3 tile (80, 8): ref r-g +0.0077 | est r-g +0.0086
   split ray 4 tile (80, 82): ref r-g +0.0111 | est r-g +0.0121
   split ray 5 tile (30, 70): ref r-g -0.1019 | est r-g -0.0882
   split ray 6 tile (130, 20): ref r-g +0.1412 | est r-g +0.1186
K6 PASS red/green room: worst |d(r-g)| 0.0257 (bar 0.03); reference spread 0.2942 (bar >= 0.06)
   shaft ray 0 tile (80, 45): ref 1.7808e-02 | est 1.8120e-02 | worst err/max over 16 frames 0.0137
   shaft ray 1 tile (80, 20): ref 2.5637e-02 | est 2.5130e-02 | worst err/max over 16 frames 0.0178
   shaft ray 2 tile (100, 30): ref 0.0000e+00 | est 1.4264e-06 | worst err/max over 16 frames 0.0001
   shaft ray 3 tile (10, 45): ref 0.0000e+00 | est 0.0000e+00 | worst err/max over 16 frames 0.0000
   shaft ray 4 tile (150, 60): ref 0.0000e+00 | est 0.0000e+00 | worst err/max over 16 frames 0.0000
   shaft ray 5 tile (60, 10): ref 2.8484e-02 | est 2.7969e-02 | worst err/max over 16 frames 0.0220
   shaft ray 6 tile (120, 45): ref 0.0000e+00 | est 0.0000e+00 | worst err/max over 16 frames 0.0000
K7 PASS sun shafts (64 slices, 8 u shadow texels, jitter + history 0.95): worst |est - ref| / max 0.0220 (bar 0.10); darkest ref ray 0.0000 of max (bar < 0.01)
   slot ray 0 tile (80, 40): ref 4.6781e-03 | one fixed sample 0.0000e+00 | shown 3.9343e-03..4.7445e-03
   slot ray 1 tile (80, 36): ref 4.9221e-03 | one fixed sample 0.0000e+00 | shown 4.1410e-03..4.9939e-03
   slot ray 2 tile (80, 32): ref 5.2698e-03 | one fixed sample 0.0000e+00 | shown 5.1911e-03..6.3776e-03
   slot ray 3 tile (80, 28): ref 5.5714e-03 | one fixed sample 0.0000e+00 | shown 4.8607e-03..5.8889e-03
   slot ray 4 tile (70, 34): ref 4.9766e-03 | one fixed sample 0.0000e+00 | shown 4.9378e-03..6.0663e-03
   slot ray 5 tile (90, 30): ref 5.6056e-03 | one fixed sample 0.0000e+00 | shown 5.5227e-03..6.7849e-03
   slot ray 6 tile (60, 38): ref 4.6524e-03 | one fixed sample 0.0000e+00 | shown 3.9133e-03..4.7191e-03
   slot ray 7 tile (100, 26): ref 6.1854e-03 | one fixed sample 0.0000e+00 | shown 5.3968e-03..6.5380e-03
K8 PASS temporal (32-unit sheet, 24 slices, exact sun test): one fixed sample off by 1.0000 (floor >= 0.5: the sheet aliases); jitter + history 0.95, worst shown frame 0.1907 (bar 0.25), 16-frame mean 0.0895
VOLFOG1 PASS (9 checks, 0 failed; red=none; 26.4 s)
```

The reds that followed (summary lines):

```
VOLFOG1 FAIL (9 checks, 3 failed; red=gioff; 6.4 s)
red gioff: FAILS AS IT MUST (failed K3, K4, K6)
VOLFOG1 FAIL (9 checks, 1 failed; red=flat; 6.3 s)
red flat: FAILS AS IT MUST (failed K6)
VOLFOG1 FAIL (9 checks, 2 failed; red=noshadow; 5.9 s)
red noshadow: FAILS AS IT MUST (failed K7, K8)
VOLFOG1 FAIL (9 checks, 1 failed; red=nojitter; 5.9 s)
red nojitter: FAILS AS IT MUST (failed K8)
VOLFOG1 SELF-TEST PASS
```
