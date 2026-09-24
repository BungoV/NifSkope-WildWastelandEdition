# PBRRENDER0 research: particles, effect shaders, and the old-vs-new shot harness

Read-only research, 2026-09-23 22:28 (clock read). Line numbers are for the tree as read at that time;
each has its anchor text beside it.

## 1. Particles and effect shaders today

### Headline

* **Right now nothing can reach the PBRM program at all.** `src/gl/glproperty.cpp:1005`
  `static constexpr bool pbrmFeatureEnabled = false;` and `pbrmMode()` (`:1026-1030`) returns
  `PbrmModeLegacy` whatever QSettings holds. So `wantPbrm` in `Renderer::setupProgram` is false for
  every shape on the current tree. Any stage that turns PBR on has to flip that constant (or add a
  new override), and that is the moment the gates below start to matter.
* **Particles never go through `Renderer::setupProgram`.** They are a different class (`Particles`,
  not a `Shape`), pick `particles.prog` by name, and have no `bslsp`. The PBRM route cannot select
  them, not even by mistake.
* **Effect-shader meshes cannot be routed to PBRM either**: the route needs `mesh->bslsp`, and a
  `BSEffectShaderProperty` shape gets `bsesp` set and `bslsp` left null.
* What CAN still move a particle pixel: the shared global uniform block, shared GL state left behind
  by the shape drawn just before, and the colour already in the framebuffer that the (additive or
  alpha) particle is blended over. Details below.

### Where each kind is drawn

| what | class / file:line | program | how chosen |
|---|---|---|---|
| NiParticleSystem, BSStripParticleSystem, every `NiParticles` descendant | `src/gl/glscene.cpp:352-354` `blockInherits( iNode, "NiParticles" )` -> `new Particles` | `particles.prog` (`particles.vert/.geom/.frag`, point sprites expanded in the geometry shader) | by NAME, `src/gl/glparticles.cpp:286` `useProgram( !scene->selecting ? "particles.prog" : "selection.prog" )` |
| BSStripParticleSystem specifically | same class as above; no strip-specific draw exists in `src/gl` (grep of `BSStripParticleSystem` in src/gl: no hits) | `particles.prog` (drawn as sprites, not as a strip) | same |
| Procedural lightning (ProcLightningController) | `src/gl/controllers.cpp:2697` `ProcLightningController::drawPreview()`, queued from `src/gl/glnode.cpp:1968` `scene->pendingBolts.append( procLightning )` | `boltstrip.prog` | by NAME, `controllers.cpp:2722` |
| BSEffectShaderProperty on BSTriShape (FO4, bsver 130-139) | `src/gl/bsshape.cpp:300` `shader = context->setupProgram( this, shader )` | `fo4_effectshader.prog` = **`fo4_default.vert` + `fo4_effectshader.frag`** | condition scan, `res/shaders/fo4_effectshader.prog` (`check BSEffectShaderProperty`) ; setup in `setupProgramCE1`, effect branch `src/gl/renderer.cpp:929` `else if ( esp )`, uniforms ~`:1160-1230` |
| BSEffectShaderProperty, Skyrim/SSE (83-100) | `src/gl/glmesh.cpp:790` / `bsshape.cpp:300` | `sk_effectshader.prog` (own vert + frag) | condition scan |
| BSEffectShaderProperty, FO76 (151-169) | same | `f76_effectshader.prog` = `f76_default.vert` + `f76_effectshader.frag` | condition scan |

### Blending, sorting, depth, soft fade, additive

* **Particle blend**: `src/gl/glparticles.cpp:320-335`. Honours a linked NiAlphaProperty via
  `AlphaProperty::glProperty`; if it has no blend bit, uses the alpha-property src/dst fields
  (`:331-332`) or falls back to additive `glBlendFunc( GL_SRC_ALPHA, GL_ONE )` (`:334`).
* **Particle depth**: `glDepthMask( GL_FALSE )` around the draw (`:513-517`); z test from
  `ZBufferProperty::glProperty` (`:467`). Drawn as `GL_POINTS` (`:515`).
* **Particles go to the second pass** when they have an alpha or shader property
  (`:274-277`) or an alpha-blend property (`:281-284`).
* **Draw order**: `Scene::drawShapes` `src/gl/glscene.cpp:537-548` = collect (opaque shapes draw
  immediately, translucent ones queued) -> grid -> selection -> `drawDeferredShapes` -> bolts.
  `drawDeferredShapes` (`:565-580`) calls `secondPass.alphaSort()` (`src/gl/glnode.cpp:170-173`,
  comparator `compareNodesAlpha` ~`:140-161`: shapes with an AlphaProperty first, then by
  `viewDepth()`), draws every non-particle, then **every particle system last** (`:578-579`),
  then lightning bolts last of all (`drawShapeEffects`, `:582-588`).
* **Which shapes go to the second pass**: `Shape::updateShader` `src/gl/glshape.cpp:605-633`
  (`translucent` from `bslsp->alpha < 1 || hasRefraction` or `bsesp->getAlpha() < 1 && !alphaProperty`;
  otherwise alpha-blend property, BGSM/BGEM `hasAlphaBlend()/hasDecal()`, or decal flags).
* **Effect-shader blend**: `setupProgramCE1` `src/gl/renderer.cpp:1303-1360` (BGEM `mat` blend
  map at `:1311-1318`; no material -> `translucent` alpha blend at `:1346-1351` or
  `AlphaProperty::glProperty`). Depth `:1362-1368`.
* **Soft / depth fade: not implemented anywhere.** `falloffDepth` is uploaded
  (`renderer.cpp:1178` `prog->uni1f( "falloffDepth", esp->falloff.softDepth )`) but only declared
  in `fo4_effectshader.frag:33`/`f76_effectshader.frag`, and `sk_effectshader.frag:60` says
  "Unused right now". No depth texture is sampled by any particle/effect shader. (The CE2
  `softEffect` uniforms at `renderer.cpp:488-489` are Starfield only.)
* **Refraction pass**: only lighting-shader shapes with `SLSF1_Refraction`, inside
  `setupProgramCE1` `renderer.cpp:1121-1131` -> `Scene::grabRefractionSource()`
  (`glscene.cpp:488-535`, blits the framebuffer so far into a texture). Particles are drawn after
  all other translucent shapes, so they are never inside a refraction source and never refract.
  NOTE for the PBR design: `setupProgramPBRM` (`renderer.cpp:764-917`) has **no refraction branch**,
  so a refractive lighting shape routed to PBRM would lose its refraction (not a particle issue, a
  legacy-look issue).

### Shared pieces: can a PBR change reach particles?

| shared piece | reaches particles (particles.prog / boltstrip.prog)? | reaches effect-shader meshes? | why, file:line |
|---|---|---|---|
| `Renderer::setupProgram` PBRM routing | **NO** | **NO** | particles never call it (`glparticles.cpp:286` by name). Effect meshes: `wantPbrm` needs `mesh->bslsp` (`renderer.cpp:191`), and `glshape.cpp:522-525` sets `bsesp`, never `bslsp`, for a BSEffectShaderProperty. The condition scan skips `pbrm_default.prog` because it has no conditions (`renderer.cpp:268` `!program->conditions.isEmpty()`). |
| program hint cache / `pbrmProgramSeen` | **NO** | **NO** | the hint is `Shape::shader` (`bsshape.cpp:300`, `glmesh.cpp:790`); Particles is not a Shape and has none. An effect shape's hint can never be the PBRM program because it is never selected for it; `stalePbrmHint` (`renderer.cpp:237`) only fires when the hint IS that program. `Shape::updateImpl` resets `shader = nullptr` if the property block changes (`glshape.cpp:515`). |
| `res/shaders/fo4_default.vert` | **NO** | **YES** | `fo4_effectshader.prog` is `fo4_default.vert fo4_effectshader.frag`, same vert as `pbrm_default.prog` and `fo4_default.prog`. Any edit to that vertex shader (outputs, the A/C/D varyings at `fo4_default.vert:61-63`, skinning, ViewDir) changes every FO4 effect-shader mesh. The PBR stage must add a NEW vertex shader (e.g. `pbrm_default.vert`) or leave this file byte-identical. |
| `res/shaders/uniforms.glsl` `globalUniforms` (std140) | **YES (layout)** | **YES** | included by `particles.vert:11`, `particles.geom:6`, `particles.frag:30`, `boltstrip.vert:8`, every effect shader. Particles read `projectionMatrix`, `lightSourcePosition` (vert), `toneMapScale`, `brightnessScale` (geom), `envMapRotation` (frag). The block must match the C++ struct in `src/gl/glcontext.hpp` ~`:280-300`; appending fields at the end is safe only if both sides move together; reordering or retyping moves every particle read. VALUES are written in `src/glview.cpp:3752-3804` (`toneMapScale`, `brightnessScale`, `glowScale`, light) for the whole frame: changing a value there for PBR moves particles and effects too. |
| tone map functions | **NO** | **NO** | `tonemap()` is a per-file copy: `fo4_default.frag:238`, `pbrm_default.frag:103`. `particles.frag` has no tone map (`fragColor = color` at `:138`; the A/D it gets from the geom shader are unused). `fo4_effectshader.frag` only does `color.rgb * sqrt(D.a)` (`:145`). Editing `pbrm_default.frag`'s copy touches nothing else. |
| `Scene` draw order (`glscene.cpp:537-580`) | **YES if edited** | **YES if edited** | particles last among translucents is enforced here. A PBR stage has no reason to touch it; if it does, particles move. |
| refraction pass | **NO** | **NO** | only `setupProgramCE1` for `lsp->hasRefraction` (`renderer.cpp:1121`). |
| GL state left behind by the previous draw | **YES (indirect)** | partly | `Particles::drawShapes` sets blend, depth mask, z/stencil props, texture units 0-3; it does NOT set `GL_FRAMEBUFFER_SRGB`, cull face or polygon mode. `BSShape::drawShapes` turns sRGB off for bsver<151 (`bsshape.cpp:296-299`) before `setupProgram`, and `setupProgramPBRM` sets polygon mode/depth (`renderer.cpp:890-897`). If a PBR stage ever enables `GL_FRAMEBUFFER_SRGB` or a linear-light framebuffer for FO4 shapes, the particles drawn right after inherit it. Rule: PBR path restores every GL state it changes. |
| framebuffer content under the sprite | **YES (by design)** | **YES** | particles blend (usually additively) over whatever was drawn. Any change to an opaque mesh behind a sprite changes the composited particle pixel even though the particle shading is identical. The particle gate must therefore be shot on a particle-only scene, or with the background masked. |

Verdict: the only live channels from a PBR change to particle pixels are (a) editing
`uniforms.glsl`/the `GlobalUniforms` struct or its per-frame values in `glview.cpp`, (b) leaking GL
state (sRGB above all), and (c) the background under the sprite. Effect-shader meshes add (d) any
edit to `fo4_default.vert`.

## 2. The existing still-render hooks and image-diff tooling

### Environment switches usable for a still (file:line where read)

| switch | read at | what it pins |
|---|---|---|
| `WW_RENDER_SHOT=<abs png>` | `src/nifskope_ui.cpp:22097` (arm; needs a file on argv) and `:22100` (path); grab + save `:22389-22394` | the grab (`grabFramebuffer`, 2.5 s after `completeLoading`, `:22099`) then quit |
| `WW_RENDER_SIZE=WxH` | `src/nifskope_ui.cpp:22110`; also `src/harnesswindow.cpp:348` (fallback for `WW_WINDOW_SIZE`, `:346`) | window size; docks and viewport header hidden first (`:22148-22174`). Read the PNG size back: width has a floor, height = asked - chrome |
| `WW_RENDER_VIEW=n` | `src/nifskope_ui.cpp:22200`; `src/glview.cpp:6444-6445` (inside the pin) | `GLView::ViewState`; unset = Front; negative = startup camera |
| `WW_RENDER_CENTER=x,y,z` | `src/glview.cpp:6369` | look-at, PIN (re-asserted every paint) |
| `WW_RENDER_DIST=d` | `src/glview.cpp:6391` | eye distance |
| `WW_RENDER_FOV=deg` | `src/glview.cpp:6404` | vertical FOV, perspective |
| `WW_RENDER_ORTHO=w` | `src/glview.cpp:6421` | orthographic half-width (exact units per pixel) |
| `WW_CAMERA_CENSUS=<file>` | `src/glview.cpp:6574` | where the `grab` camera line goes (default `release/ww_camera_pin.log`) |
| `WW_CAMERA_LOG=<file>` | `src/glview.cpp:179` | every reorientation and who asked |
| `WW_RENDER_TIME=s` | `src/nifskope_ui.cpp:22355` | scene time (default 1.0); the particle sim is time-driven |
| `WW_RENDER_SEQ=<name>` | `src/nifskope_ui.cpp:22350` | animation sequence (FO4 VFX default to `autoPlay`) |
| `WW_RENDER_CLEAN=1` | `src/nifskope_ui.cpp:22248` | no grid, axes, nodes, 3D cursor |
| `WW_RENDER_FLAT=1` | `src/nifskope_ui.cpp:22271` | vertex colours only |
| `WW_RENDER_REFRACTION=0` | `src/nifskope_ui.cpp:22228-22229` (also `:23756`) | refraction off; otherwise FORCED on. `showParticles` is always forced on (`:22230`) |
| `WW_RENDER_SS=0..3` | `src/nifskope_ui.cpp:22390` | supersampled grab through an offscreen FBO |
| `WW_LOD_CHANNEL=n` | `src/nifskope_ui.cpp:22269-22270` | channel preview (also forces FO4 shapes to `fo4_default.prog`, `renderer.cpp:217`) |
| `WW_PROGRAM_CENSUS=<abs file>` | `src/gl/renderer.cpp:124` | one row per (shape, program) actually used this run, plus the view-space light |
| `WW_WINDOW_AT=x,y` | `src/nifskope_ui.cpp:1421,1428,1655` | second-monitor placement (`_harness.sh` exports 1960,40) |
| `WW_WINDOW_VISIBLE=1` | `src/nifskope_ui.cpp:1543,1624` | opaque window (watching only; not for gates) |
| `WW_SETTINGS_SCOPE=<name>` | `src/harnesswindow.cpp:274` | moves the WHOLE QSettings tree to `NifSkope 2.0 <name>`; bungo's key unreachable |
| `WW_LODGEN_RESOURCES=<root>` | `src/main.cpp:198` | extra resource roots ahead of the user's, session only |
| `WW_UI_SHOT`, `WW_UI_SHOT_DOCK` | `src/nifskope_ui.cpp:20563,20567` | chrome grab; viewport comes out black, not usable here |

**What is NOT pinned by any switch (all from QSettings):** the light rig and tone map.
`src/ui/widgets/lightingwidget.cpp:61-68` reads `Settings/Render/Lighting/Directional Level`,
`Light Color`, `Ambient Level`, `Cube Map Rotation` (and the rest of that block) and drives
`GLView::setBrightness/setToneMapping/setAmbient/setFrontalLight` (`lightingwidget.cpp:51-56`,
`src/glview.cpp:5752-5804`). The Scene render options (DoLighting, DoSpecular, DoGlow,
DoCubeMapping ...) also come from the persisted menu state. There is no `WW_LIGHT*`, `WW_TONE*`,
`WW_SUN` or `WW_PBRM*` switch in `src` (search for those names: no hits). Today the only way to pin
them is `WW_SETTINGS_SCOPE=<fresh name>`, which yields the code defaults on both arms.

### Existing diff tooling

* `tools/render_regression/capture.ps1` (146 lines): the original "render regression baseline".
  Seven-file corpus (`:48-56`: `Effects\AttachFXMist01.nif` particle sim + flipbook,
  `Effects\BlackGlowFill01.nif` effect-shader particles, `CA-PowerArmorVisorGlass01.nif`,
  `GlassShader01.nif`, `tests\render\refraction_fixture.nif`, `SetDressing\ACDucts\ACDuctConnector01.nif`
  spec/gloss, `tests\rigging\fixtures\donor.nif` skinned). `-Mode baseline|compare`,
  `-Tolerance` = allowed differing pixels per image, **default 0** (`:17`). Verdict rows:
  `OK <name> identical` / `DIFF <name> N px differ, max channel delta M` / `FAIL ... size mismatch`
  / `SKIP missing asset`, then `mode=.. ok=.. diff=.. failed=.. missing=..` (`:142-146`).
  Stale as a gate: no `--port`, no `WW_RENDER_SIZE`, no `WW_RENDER_CLEAN`, no `WW_SETTINGS_SCOPE`
  (so it reads bungo's lighting settings), and its baselines are from July/August 2026 (baseline
  folder dated Jul 27) -- older window sizes, so every row would read size mismatch now.
* `tools/render_regression/imgdiff.ps1` (59 lines): shared pixel comparer; prints ONE line
  `<differingPx> <maxChannelDelta> <mean|d|> <fraction> <x0> <y0> <x1> <y1> <leftPx> <rightPx> <WxH> <cx> <cy>`
  or `ERR size-mismatch`. RGB only (alpha ignored), per-pixel sum of |dR|+|dG|+|dB|. Writes no diff image.
* `tests/spells/refraction.sh` uses `imgdiff.ps1` (`:47`, extractors `:153-157`); its bars are
  counts in the check text, e.g. `>= 2000 px` (`:210`) and a repaint-noise bar
  `<= 200 px and <= 4` max delta (`:368`, measured 6-8 px, max 1 between repaints).
* `tests/spells/native_lighting.sh` + `native_lighting_check.py`: the precedent for a
  must-not-change gate -- `gate (a)` asserts four legacy frames are **byte-identical** to
  `tests/baselines/native_lighting/*.png` (`native_lighting_check.py:235-246`), verdict
  `N checks, M failures` then `PASS`/`FAIL`.
* `tests/spells/render_shot.sh` (919 lines): gates the hook itself (exit without the save dialog,
  opacity 0, off primary, camera pin); ends `render_shot.sh: N checks, M failures` + `PASS`/`FAIL`
  (`:917-918`). Not an image-diff tool.
* Known noise: `grabFramebuffer()` is not always bit-stable between repaints (skeleton overlay
  measured 0..37 px of ~1.2 M; refraction 6-8 px, max delta 1). So "exact zero" has to be proved
  against a rung-vs-rung repeat first, per scene.

## 3. One old-vs-new shading harness (design, not built)

**Files.** `tests/spells/pbr_shade_ab.sh` (driver), `tests/spells/pbr_shade_ab.py` (diff + verdict,
numpy/PIL), `tests/spells/pbr_shade_ab_cases.txt` (one row per case: name, nif, gate, bar, view pins,
pbrm mode). Every stage adds rows; nobody writes a second harness.

**Two arms.** OLD = a frozen rung FOLDER (exe + `shaders/` + Qt DLLs), because programs load from
`applicationDirPath()/shaders` at run time (`src/gl/glcontext.cpp:933-963`) -- an exe alone would
render with NEW shaders. Snapshot once per stage into `scratchpad/<lane>/rung/`, record its sha8.
NEW = `release/`.

**Pinned per shot, identical on both arms.**
* `WW_SETTINGS_SCOPE=pbr_ab_<case>` (fresh scope = code defaults for lights, tone map, scene
  options; this is the ONLY way lighting is pinned today).
* `WW_RENDER_SHOT` (absolute, `winpath`), `WW_RENDER_SIZE` = W above the width floor x (H+59), size
  read back with PIL and refused on mismatch between arms.
* `WW_RENDER_CLEAN=1`, `WW_RENDER_VIEW`, `WW_RENDER_CENTER` + `WW_RENDER_ORTHO` (or FOV+DIST),
  `WW_RENDER_TIME` (particles: fixed, e.g. 1.0), `WW_RENDER_SEQ` where a sequence matters.
* `WW_PROGRAM_CENSUS=1` and `WW_CAMERA_CENSUS=1` per shot; the census lines are compared between
  arms so a changed camera or program is named, not inferred from pixels.
* unique `--port` per run, `timeout 120`, `_harness.sh` (second monitor, opacity 0).

**Noise floor first.** Per case, OLD vs OLD twice. Must-not-change gates need 0 px there; if the
repeat is not 0, the case's bar = a fixed count above the worst of 5 measured repeats, written into
the case row (render-shot skill 5c), never a percentage.

**Gates.**
* `zero` -- particles, effect meshes (BGEM + embedded), legacy meshes in Legacy mode, and legacy
  meshes in LegacyAndPBR mode when no .pbrm exists: px == 0 (or == the measured noise bar).
* `change` -- a deliberate PBR upgrade: px <= stated bar AND a MUST-DIFFER floor (px >= N, so a
  route that never fired cannot pass) AND the NEW census names `pbrm_default.prog`.
* Each run also re-scores the same bar against a known real difference (a `WW_RENDER_FLAT=1` shot)
  so a bar that swallows everything is caught in the same run.

**Output.** `<case>_diff.png` = OLD | NEW | |d| amplified x16 with changed pixels marked red on a
darkened OLD. One verdict line per case:
`pbr_shade_ab <case> gate=zero|change old=<sha8> new=<sha8> size=WxH px=<n> frac=<f> max=<m>
mean=<u> bar=<b> noise=<n0> progs=<census> -> PASS|FAIL`, and a last line `N cases, M failures`.

**Builds on.** `WW_RENDER_SHOT` hook (`src/nifskope_ui.cpp:22072-22407`), camera pins
(`src/glview.cpp:6369-6445`), `WW_SETTINGS_SCOPE` (`src/harnesswindow.cpp:274`), `WW_PROGRAM_CENSUS`
(`src/gl/renderer.cpp:124`), `_harness.sh`, the numbers format of `tools/render_regression/imgdiff.ps1`,
the byte-identical-baseline idea of `tests/spells/native_lighting_check.py:235-246`.

**Missing (each a small code change for the build lane).**
1. **`WW_PBRM_MODE` does NOT exist.** Mode today comes from QSettings `Settings/Render/PBRM Mode`
   (`src/nifskope_ui.cpp:27385`), the UI forces Legacy at `:27367-27368`, and `pbrmMode()` is hard
   gated by `constexpr pbrmFeatureEnabled = false` (`src/gl/glproperty.cpp:1005, 1019-1030`).
   Proposal: `pbrmMode()` reads `WW_PBRM_MODE=legacy|pbr|both` first and, only when it is set,
   bypasses the constexpr gate; the UI init must not overwrite it. Same for `WW_PBRM_AUTOREPLACE=0|1`
   (today QSettings, `src/glview.cpp:3290`). Log the SERVED mode once (`pbrm mode served=..`).
2. No `WW_LIGHT_*` / `WW_TONEMAP` pins -- the fresh scope's defaults stand in; fine for A/B since
   both arms read the same defaults, but a stage that changes a default must say so.
3. No `WW_RENDER_PARTICLES=0` (the hook forces `showParticles` true at `nifskope_ui.cpp:22230`),
   so a mesh+particle scene cannot be split into a particle mask.
4. `imgdiff.ps1` writes no diff PNG; the new .py replaces it for this harness.
5. `WW_PROGRAM_CENSUS` never logs particles (they bypass `setupProgram`); add a `particles.prog`
   / `boltstrip.prog` count so a particle case's census is not empty.
6. `tools/render_regression/capture.ps1` is stale (no --port/size/clean/scope, Jul 27 baselines);
   retire it or leave it, but do not extend it.

## 4. Vanilla fixtures, confirmed by `nifstrings.py` (Data = E:\Tools\Fallout 4\DataUnpacked\Data)

One verdict line each, as printed by the script:

* BGSM (lit, material file): `meshes\SetDressing\ACDucts\ACDuctConnector01.nif :: BGSM Materials\SetDressing\AcDuctsRusted.BGSM exists=1 | bsver=130 shapes=1`
* BGEM (effect, material file): `meshes\Effects\GlowFlatPlaneOneSided01.nif :: BGEM materials\Effects\GlowPlane01.BGEM exists=1 | bsver=130 shapes=1`
  (alternative `meshes\Effects\BlackGlowFill01.nif` = BGEM `BlackGlowFill01.BGEM` exists=1 plus a
  second embedded effect shape; note capture.ps1 labels it "particles_glow" but it holds NO particle
  system -- it is an effect trishape mesh)
* Embedded lighting: `meshes\Landscape\GuardRails\GRailCurveR01.nif :: EMB-LIT name='' texsets_dds=2 first=textures\landscape\roads\RoadGuardRail01_d.dds | bsver=130 shapes=1`
  (texture present loose under Data\textures\landscape\roads)
* Embedded effect: `meshes\Effects\GlassShader01.nif :: EMB-EFF name='' dds=5 first=textures\shared\GlassTile01_d.dds | bsver=130 shapes=1`
* Particles, fire: `meshes\Effects\MPSFireSmall01.nif :: EMB-EFF name='' dds=2 first=textures\effects\FireTorchFlameAnim.dds | PSYS BSMasterParticleSystem,NiParticleSystem | bsver=130 shapes=0`
* Particles, smoke: `meshes\Effects\MPSSmokeFireMed01.nif :: EMB-EFF name='' dds=2 first=textures\effects\SmokeBurstPuffAnim.dds | PSYS BSMasterParticleSystem,NiParticleSystem | bsver=130 shapes=0`
* (already in capture.ps1) `meshes\Effects\AttachFXMist01.nif :: EMB-EFF ... BitsBokehBitsAtlas.dds | PSYS NiParticleSystem`

**BSStripParticleSystem: none found.** All 692 NIFs under `meshes\Effects` scanned (`--only STRIP`),
0 hits. Not searched outside Effects. It renders through the same `Particles` class
(`src/gl/glscene.cpp:352-354`), so the NiParticleSystem cases cover its draw path.
Caution: `FXFireMed01.nif` / `FXFireSmall01.nif` read EMB-EFF with 0 .dds -- avoid them.
