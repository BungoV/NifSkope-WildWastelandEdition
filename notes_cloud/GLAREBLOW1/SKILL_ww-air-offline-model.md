---
name: ww-air-offline-model
description: Measure the NifSkope Wild Wasteland cell air (VOLAIR, the fog/haze froxels and the sky through them) OFFLINE, without the exe or the game data. Covers which code a "sky term" or "air" complaint is really about (the cell air, not the ATMO physical sky), the shader law written as a Python quadrature, a Monte Carlo of the SAME medium and phase as the reference (its first order must equal our single scattering), the absolute clear-sky references (Preetham, CIE, the repo's Hillaire model) and how far they disagree, compiling the GLSL with glslang in a cloud container, and the g++ test plus fixture shape that carries the verdict into the tree. Use for any "the air/fog/sky is too bright/dark/blue" lane, and before changing cell_volair.frag, cell_airmed.glsl or lookdev_fog.glsl's air.
---

# WW: the cell air, measured offline

Lane GLAREBLOW1 (2026-10-29, cloud, no GPU) spent most of its time on four things that are written down here:
finding which code "the air pass's sky term" was, writing that code's law as a quadrature, building a reference that
can tell a wrong multiple-scattering term from a wrong single-scattering one, and getting a GLSL compile without the exe.

## 1. Which code it is

There are two skies; a complaint names the wrong one about half the time.

| what | files | units |
|---|---|---|
| **the cell air** (VOLAIR1 and after): fog/haze froxels, its sky overlay | `res/shaders/cell_volair.frag` (inject: `airSun`, `airMsSum`, the GI and ambient cubes), `cell_airmed.glsl` (medium `airRho`/`airCol`/`airTup`, phase `volPhasePi`), `lookdev_fog.glsl` (`wwVolAirAt`, `wwVolAirSky`, the far tail), `cell_volsky.frag` (lays the air over the sky), `src/gl/cellvolair.cpp` (fit, phase, uniforms, census) | display = pi x radiance; game units; D = `cellDirColor` |
| **the physical sky** (ATMO1): Hillaire LUTs, sky pixels, the sun disc | `res/shaders/atmo_*.frag`, `atmo_common.glsl`, `src/gl/atmosphere.cpp`; offline twin `tests/spells/atmo_model.py` | km, sun irradiance 1 at the top |

"Sky term near the sun goes over 1.0" with "multiple scattering about 40%" is the cell air. It is `wwVolAirSky`, whose
tail runs J to where the ray leaves the air. A level or downward ray never leaves it (T = 0), so there L = J exactly.

The cell air's law, all you need for a quadrature:

- **Medium:** rho(z) = min(1, exp(-(z - zb)/H)) below the top (one band). The near band and the ground layer add
  terms in `wwAirColLaw`. sigma0 is fitted by `vaFit` to the fog records.
- **Phase:** `vaPhase` takes the god-ray record (air rgb, fwd rgb, back rgb, fwd g, back g). Each lobe is weighted by
  its luminance, so the three weights sum to 1. The fallback record is `{0.60,1.52,3.31, 2,2,2, 1,1,1, 0.75, 0, 1}`.
- **Sun:** J = D [vis Ts pi p(c) + the octaves], with Ts = exp(-sigma0 col(z, top) / max(mu, 0.02)) and no
  single-scattering albedo (it is 1).
- **Where to read the live values:** the census line `cell air: on(... sigma0= zb= H= top= phase=<record> ... octaves=)`
  prints them. Ask the main session for that line from the shot under test. Never guess it if it can be had.

## 2. The model (copy `notes_cloud/GLAREBLOW1/model/`)

- `glareblow_model.py`:
  - `ours_sky` is the shader law, integrated exactly along a ray (the froxels and the tail are its discretisation).
  - `mc_sky` is a backward Monte Carlo of the same plane-parallel medium: delta tracking with the majorant sigma0
    (rho <= 1), next-event estimation to the sun with the analytic Ts, Lambertian ground, Russian roulette past order 4.
    It returns order 1 and the higher orders separately.
  - It also holds Preetham, CIE type 12, Kasten's direct beam, and `hil_ratio` (which wraps atmo_model.py).
- **The self-check that makes the MC trustworthy:** its order-1 total must equal `ours_sky`'s single scattering (GLAREBLOW1:
  within 1.7% on 225 rows). If it does not, one of the two is wrong, and no multiple-scattering verdict can be read.
- **Vectorise over paths, not over rays.** 100k paths per ray run in about 0.4 s in numpy; 225 rays take about 90 s.

## 3. The absolute references disagree; say by how much

Sky within 5 deg of the sun, over a sunlit ground of albedo 0.2, with the sun at 15 deg:

| reference | ratio |
|---|---|
| Hillaire clear air (no haze) | ~5 |
| Preetham T = 3 | ~14 |
| CIE type 12 | ~40 |

The aureole is the aerosol's, and the fog records do not give a turbidity, so near the sun an absolute reference can
only BRACKET ours. The same-medium MC is the one that can convict a term. At 35-45 deg the references agree better
(Hillaire 1.4-2.4x, depending on the sun's elevation).

## 4. Compile the GLSL without the exe (cloud container)

`apt-get update && apt-get install -y glslang-tools`. This works through the agent proxy. Then:

1. Inline the `#include "x.glsl"` lines recursively from `res/shaders` (each file once).
2. Rename `\bcast\b` to something else: glslang reserves `cast`, and the tree uses it as a name.
3. Run `glslangValidator -S frag file.frag`.
4. Compile HEAD's version the same way first. Both must exit 0, so that a failure is yours and not the harness's.

No Qt in the container: `.cpp` changes stay "compile owed live".

## 5. Carry the verdict into the tree

- **A pure header twin** of the shader's new function (`src/gl/<name>.h`, std only).
- **A g++ test** (`tests/<lane>_test.cpp`) that reads a **fixture TSV of MC rows** (`tests/fixtures/<lane>_mc.tsv`, made
  by a script in notes_cloud) and recomputes ours itself.
- **Rows:**
  - the known answers first (the quadrature against an exact integral, the limits, and the red returning the old constant
    bit for bit);
  - the total against the MC on the regime that matters;
  - "nowhere brighter/darker than before" when the fix is meant to be one-sided;
  - single scattering against MC order 1 (the port and the fixture agree);
  - the shader string-matches the twin's constants.
- **The red build** (`-D<LANE>_RED`) must fail the rows that carry the claim.
- **Pre-register the bars,** and if a bar moves after the first run, say so in the test's header.

## 6. Line endings in the air shaders

`cell_volair.frag` is mixed, and the octave block is CRLF. Edit it with a Python bytes script: exact-once anchors that
carry `\r\n`, and new lines joined with the neighbours' ending (ww-anchored-hookup). Then check that `git diff --stat`
equals `git diff --stat --ignore-cr-at-eol`.
