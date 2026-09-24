# PBRRENDER0 research: NifSkope scene lighting as a PBR material sees it

Agent 6, 2026-09-23. Read-only audit. Paths relative to E:\Projects\NifskopeWildWastelandEdition.
Scope: FO4 (BSVersion 130) unless said otherwise; FO76/Starfield (>=151) noted where they differ.

## 1. Lights: count, type, direction, colour, intensity, A/D varyings

- ONE directional light. The uniform block has room for 3 (`res/shaders/uniforms.glsl:6-7`
  `lightSourcePosition[3]`, `lightSourceDiffuse[3]`) but only slot 0 is ever written
  (`src/glview.cpp:3774-3780`, `:3804`) and every shader reads only `[0]`
  (`res/shaders/fo4_default.vert:81`). No point lights, no spot, no NiLight from the NIF.
- Direction is a unit vector in VIEW space (w=0), so it is a sun, not a point light.
  - "Frontal" (default ON, `src/glview.h:440`; persisted `src/ui/widgets/lightingwidget.cpp:76`):
    `lightSourcePosition[0] = (0,0,1)` in view space = a head light that follows the camera
    (`src/glview.cpp:3779-3780`).
  - Frontal OFF: world-fixed light from Euler(declination, 0, planarAngle)
    (`src/glview.cpp:3766-3776`), third matrix row, up-axis shuffled, then rotated into view
    space by `viewMatrix`. So it stays put in the world while the camera orbits.
  - declination/planarAngle are NOT sliders: set by Shift+left-drag (`src/glview.cpp:23339`,
    `rotateLight` at `:6076-6083`), rotate keys (`:21894-21897`), Shift+T (0,0) / Shift+L (90,90)
    / Shift+F (frontal) (`:23055-23074`). Light gizmo shown briefly (VisLightPos, `:6082`).
  - Bug seen in passing: saved as `Settings/Render/Lighting/Declination` / `Planar Angle`
    (`lightingwidget.cpp:125-126`) but read back from `Lighting/Declination` / `Lighting/Planar Angle`
    (`:78-81`), so the angles never survive a restart.
- Colour: one "Light Color" slider = colour temperature (comment: 6548 K * exp(lc*2.04),
  `src/glview.cpp:5788-5794`), mapped through a 6th-order polynomial to RGB, clamped 0..1
  (`src/glview.cpp:3785-3796`). Default slider 720 -> lightColor 0 -> RGB (1,1,1).
- Intensity: "Directional" slider -> `brightnessL` via `convertBrightnessValue`
  (`src/glview.cpp:5758-5774`: lower half = sRGB curve 0..1, upper half = 2^x 1..16);
  default 720 -> 1.0. `lightSourceDiffuse[0] = rgb(temp) * brightnessL` (`:3796`, `:3804`).
  Units: none, a scale on an sRGB-ish 0..1 colour; no lux/nits.
- Lighting OFF (`Scene::DoLighting` clear): ambient = 7.0, diffuse = 0 (`:3798-3801`).
  Silhouette: brightnessScale = 0 (`:3760-3761`).
- Varyings (`res/shaders/fo4_default.vert:83-85`, flat):
  - `A = vec4( sqrt(lightSourceAmbient.rgb) * 0.375, toneMapScale )`
  - `D = vec4( sqrt(lightSourceDiffuse[0].rgb), brightnessScale )`
  The sqrt puts both lights into "square-root space" (gamma 2.0 approximation, see item 4/5).
  `A.a` carries the tonemap strength, `D.a` the overall Brightness slider (exposure).
  pbrm_default.prog reuses this vertex shader unchanged (`res/shaders/pbrm_default.prog:13`).

## 2. Ambient

- A single constant colour, not per-scene, no hemisphere, no SH, no irradiance for FO4.
  Source: "Ambient" slider -> `ambient` (`src/glview.cpp:5802-5806`, same convert curve),
  `lightSourceAmbient = FloatVector4(ambient)` (grey, `:3783`, `:3803`). Default 1.0.
- In the shader it becomes `A.rgb = sqrt(1.0) * 0.375 = 0.375` in sqrt space
  (~0.14 linear) and is added flat: `color += A.rgb * albedo` (`res/shaders/fo4_default.frag:531`),
  `color += A.rgb * base * (1-metal) * ao` (`res/shaders/pbrm_default.frag:206`).
  Direction-independent: a normal map is invisible in shadowed areas.
- The same `ambient` value also scales the cube reflection indirectly: FO4 legacy
  `spec += cube.rgb * diffuse` where `diffuse = A.rgb + D.rgb*NdotL` (`fo4_default.frag:431`, `:489`).
  PBRM does NOT scale its cube by A (`pbrm_default.frag:213`), so the Ambient slider moves
  PBR diffuse fill but not PBR reflections.
- Only FO76/Starfield get an irradiance cube (see item 3).

## 3. Environment / cubemaps

- Candidates (`src/gl/renderer.cpp:68-69`): `cube_sk = textures/cubemaps/bleakfallscube_e.dds`
  (Skyrim, absent from FO4 archives), `cube_fo4 = textures/shared/cubemaps/mipblur_defaultoutside1.dds`.
  FO76/Starfield use settings paths `Cube Map Path FO 76` / `Cube Map Path STF` (`renderer.cpp:99-100`).
- The property's own slot 4 (`fileName(4)`) is used first.
  - Legacy FO4 path (`renderer.cpp:1085-1114`): env mapping on + slot empty/unloadable -> falls
    back to cube_fo4 and still reflects.
  - PBRM path (`renderer.cpp:846-868`): cube only if `lsp->hasEnvironmentMap` AND slot 4 loads.
    Otherwise cube_fo4 is bound only to keep the sampler complete and `hasCubeMap = false`, so a
    PBR material without the legacy env-map flag gets NO environment reflection at all.
- cube_fo4 as shipped (header read of the unpacked corpus copy): 128x128 per face, 8 mips,
  uncompressed 32-bit BGRA UNORM (not sRGB, not HDR). "mipblur" = Bethesda's own blurred mip
  chain, not a GGX convolution; LDR, so the sun and sky saturate at 1.0. Caveat: the unpacked copy
  has caps2 = 0 (no cube flag); NifSkope reads from the BA2, whose loader writes the header, not
  checked here.
- Mip selection: legacy `textureLod(CubeMap, R, 8 - smoothness*8)` (`fo4_default.frag:479`);
  PBRM `textureLod(CubeMap, R, rough*8)` (`pbrm_default.frag:209`). Both are linear in
  roughness/gloss, both ask for lod 8 on a 0..7 chain (clamps), neither matches a GGX lobe width.
- Prefiltered IBL EXISTS but only for BSVersion >= 151: `TexCache::texLoadPBRCubeMap`
  (`src/gl/gltexloaders.cpp:913-985`) runs `SFCubeMapCache` GGX importance sampling into 7
  roughness mips (width = `Ibl Cube Map Resolution`, default 512; samples default 256;
  `src/gl/gltex.cpp:579-595`) AND builds a 32px roughness-1 diffuse cube (irradiance
  stand-in) at texture id+1 (`gltexloaders.cpp:966-982`), bound as `CubeMap2` for FO76
  (`renderer.cpp:1104-1111`). Gated by `getBSVersion() >= 151` at `gltexloaders.cpp:1017` and
  `:1263`. Already-prefiltered R9G9B9E5 DDS with mips skip the filter (`:938-946`).
- HDRI: Radiance `.hdr` accepted only for >= 151 (`gltexloaders.cpp:1248-1250`, parsed
  `:923-936`, exposure via `Hdr Tone Map` level, `gltex.cpp:591-594`).
- Skybox: `Renderer::drawSkyBox` (`renderer.cpp:1760+`) returns early for < 151
  (`:1772`); background mip = `Cube Map Bgnd` setting (default 1, `renderer.cpp:93-94`).
  `skybox.frag` adds the sun disc as a GGX lobe and tonemaps with the same curve
  (`res/shaders/skybox.frag:15-25` GGX, `:27-39` tonemap, `:41-66` main). FO4 gets a flat clear colour (`glview.cpp:5741`).
- User choice: "Choose PBR Environment Cubemap..." button (`lightingwidget.cpp:35`, `:58`) ->
  `selectPBRCubeMapForGame` returns false for < 151 (`glview.cpp:3383-3386`): a no-op on FO4.
  The only FO4 environment control is the Cube Map Rotation slider (`glview.cpp:5808-5811`,
  applied in `glcontext.cpp:1009-1036` as `envMapRotation`, used by `reflMatrix`,
  `fo4_default.vert:72-75`).
- No irradiance/SH for FO4. No reflection-probe from the scene.

## 4. Exposure and tonemapping

- Curve: Hable / Uncharted-2 filmic constants (A..F = .15 .50 .10 .20 .02 .30), identical in
  `fo4_default.frag:238-250`, `pbrm_default.frag:103-114` and `skybox.frag:27-39`:
  `z = x*x * D.a * (A.a * 4.22978723); z = hable(z); out = sqrt(z / (A.a * 0.93333333))`.
  The `x*x` converts the sqrt-space colour to linear, the final `sqrt` re-encodes it.
- Inputs:
  - `D.a = brightnessScale` = "Brightness" slider (`lightingwidget.cpp:54`, `glview.cpp:5776-5780`,
    `:3753`) = the exposure multiplier, applied to linear colour. Range 0..16, default 1.
  - `A.a = toneMapScale` = "Tone Mapping" slider, `toneMapping = 4.2298^((v-1440)/720)`
    (`glview.cpp:5796-5800`), default slider 720 -> 0.2364 (`glview.h:431`).
- Behaviour (evaluated from the formula, output in sqrt space, x = sqrt-space input):
  - default 0.2364: x 0.1 -> 0.112, 0.5 -> 0.549, 1.0 -> 1.000, 2.0 -> 1.53, 4.0 -> 1.88.
    It is almost identity up to 1.0 and then CLIPS in the 8-bit framebuffer; there is no
    shoulder that rolls highlights to white at default.
  - slider max (1.0): 1.0 -> 0.754, 2.0 -> 0.917, 4.0 -> 0.977, a real filmic shoulder.
  - slider min (~0): pure gain of ~1.12 in sqrt space (Hable toe slope 0.278 * 4.23 / 0.933 = 1.26
    linear). Even "tonemap off" is not identity.
- No auto-exposure, no EV, no view transform (Filmic/AgX), no "look", no dithering.
- Output of the FO4 and PBRM shaders is sqrt-encoded (gamma 2.0), not linear, not true sRGB.

## 5. Gamma / sRGB end to end (FO4 BSVersion 130)

- Texture upload: DDS goes through GLI (`gltexloaders.cpp:874-890`, `:609-645`); the GL
  internal format is GLI's translation of the file's DXGI format. So a `*_UNORM` DDS stays raw
  (sampled values are still sRGB-encoded) and a `*_UNORM_SRGB` DDS is decoded to linear by the
  sampler. Vanilla FO4 diffuse textures are UNORM, so legacy shading sees sRGB-encoded albedo.
  No per-slot override: a `.pbrm` BaseMap/EmissiveMap commented "sRGB" (`pbrm_default.frag:19`,
  `:22`) is decoded or not purely by what DXGI format the author saved.
- Shader math space: "sqrt space". Lights enter as `sqrt(light)` (`fo4_default.vert:83-85`),
  albedo raw sRGB-ish, products formed there, then `x*x` in tonemap (item 4). This is gamma-2
  shading: e.g. `albedo * D * NdotL` squared gives an NdotL^2 falloff, and additions (spec +
  diffuse + ambient) are summed in the wrong space. Emissive uses `glowScaleSRGB = sqrt(glowScale)`
  (`glview.cpp:3805`) to match.
- Framebuffer: `GL_FRAMEBUFFER_SRGB` is disabled for BSVersion < 151 before the shape draw
  (`src/gl/bsshape.cpp:295-299`) and at resize (`glview.cpp:5740`); enabled only for >= 151
  (`bsshape.cpp:296-297`, `BSMesh.cpp:72-73`, skybox `renderer.cpp:1784`). So FO4 and FO4-PBRM
  write the sqrt-encoded value straight to an 8-bit UNORM buffer: display gamma is ~2.0 not 2.2.
- Consequence for PBRM on FO4: the BRDF (Fresnel, GGX, Lambert/PI, metal tint `f0 = base`) runs
  in sqrt space on either sRGB-encoded (UNORM file) or linear-then-squared (SRGB file) base
  colour. With an SRGB-format base map, albedo is effectively linearised twice (too dark and too
  saturated); with UNORM, only the lighting sums are in the wrong space.

## 6. What a PBR material cannot show under this lighting (FO4, PBRM path)

1. Metals with no environment. `hasCubeMap` for PBRM needs the legacy env-map flag AND a
   loadable slot-4 file (`renderer.cpp:846-853`). Without it: metal diffuse = 0
   (`pbrm_default.frag:198`), ambient term `* (1-metal)` = 0 (`:206`), no cube (`:208`). A
   full metal shows black except the one GGX highlight. Base-colour tint of a metal is visible
   only inside that highlight.
2. Metals with the environment: LDR 128px blurred FO4 cube, `cube * envReflection * f0 * ao`
   (`:213`) with no BRDF LUT; nothing brighter than 1.0 exists to reflect, so chrome and gold
   read as dull grey/brown, and there is no horizon/sky contrast to judge polish by.
3. IOR / specular weight (dielectric F0). The default light is the head light: L = (0,0,1) in
   view space (`glview.cpp:3780`) and V is ~(0,0,1), so H ~ V and `VdotH ~ 1`: Fresnel is
   pinned at F0 over the whole surface (`pbrm_default.frag:190`). The grazing Fresnel rise
   that IOR controls is never shown by direct light, and with no environment (item 1) not by
   indirect light either. Changing F0 0.04 -> 0.08 moves only the small central highlight.
4. Roughness. Direct: only the size of one highlight. Indirect: `lod = rough*8`
   (`:209`) into a box-blurred 8-mip chain; not a GGX lobe, no tail, lod 8 clamps to the 1x1
   mip, and rough 0.5 already samples an 8px face. FO76's own mapping for its GGX chain is
   `m = r*(10-4r)` (`f76_default.frag:188`) - PBRM does not use the prefiltered chain at all.
5. Energy / brightness vs legacy. The math runs in sqrt space (item 5), so PBRM's Lambert `/PI`
   (`pbrm_default.frag:198`) becomes `/PI^2` (~0.10) in linear after the tonemap squares it,
   while legacy Oren-Nayar returns ~NdotL with no `/PI` (`fo4_default.frag:169-175`). Same
   texture, same light: the PBR diffuse is about 3x darker in display value than legacy. Any
   side-by-side "PBR vs legacy" judgement is biased before materials differ.
6. Shape reading in shadow. Ambient is one flat grey (item 2): normal maps, AO and curvature
   vanish on the unlit side; a metal's unlit side is black.
7. Emission in physical units (editor: nits = intensity x100). No exposure in EV and no
   absolute unit anywhere (`brightnessScale` is a plain multiplier), so emissive vs lit
   balance cannot be judged, and the default tonemap clips above 1.0 (item 4).
8. Sheen/fuzz, retroreflection, coat and anisotropy (the editor's OpenPBR lobes) have no
   shader terms here; under a head light retroreflection would also be indistinguishable from
   plain specular.
9. Colour accuracy. Gamma 2.0 output and possible double linearisation of SRGB-format base
   maps (item 5): saturated or dark albedo reads wrong before lighting is even involved.

## 6a. Hazard found for any new mode (particles gate)

- `GL_FRAMEBUFFER_SRGB` is per-draw state set by bsshape/BSMesh (`bsshape.cpp:295-299`,
  `BSMesh.cpp:72-73`) but `glparticles.cpp` never sets it (no hit). Particles inherit whatever
  the last shape left. On FO4 every shape disables it, so particles are stable today; a stage
  that enables the sRGB framebuffer for PBR shapes would silently change particle pixels.
  Encode sRGB in the shader instead, or make particles/legacy draws disable it explicitly.
- Render-shot and gate harnesses inherit every lighting slider from QSettings
  (`lightingwidget.cpp:61-81`); no WW_RENDER_* switch pins them (no hit in `glview.cpp`). A
  pixel gate must force the lighting state it measures.

## 7. Blender reference and a proposed NifSkope "lighting stage"

### Blender Material Preview (lookdev), as the reference
(RNA facts below read from the live Blender API lookup; behaviour notes from Blender 4.x.)
- Lighting = one HDRI "studio light" world (bundled .exr set: forest, city, courtyard,
  interior, night, studio, sunrise, sunset); scene lights and scene world OFF by default
  (toggles "Scene Lights" / "Scene World").
- Controls (View3DShading): HDRI picker; Rotation (Z, -180..180); "World Space Lighting"
  `use_studiolight_view_rotation` ("HDR rotation fixed, not following the camera", RNA default
  True); Strength `studiolight_intensity` (default 1.0); World Opacity
  `studiolight_background_alpha` (default 0.0 = background hidden, theme colour shown);
  Blur `studiolight_background_blur` (default 0.5 = background drawn from a blurred mip).
- Renderer: EEVEE. The world is prefiltered into a reflection probe (GGX mip chain for
  specular) plus low-order spherical harmonics for diffuse; split-sum with a BRDF LUT. EEVEE
  Next (4.2+) can also extract the brightest part of the world into a sun for shadows.
- All shading in scene-linear. Textures carry a colour space (sRGB for colour, Non-Color for
  data). Output goes through Colour Management: View Transform (Standard / Filmic <=3.6 /
  AgX default since 4.0 / Khronos PBR Neutral 4.2+), Look, Exposure (stops), Gamma, display sRGB.
  The viewport uses the same view transform as the final render.

### Proposed stage for NifSkope (mirror Blender, reuse what already exists)
- Scene mode row "Lighting: Legacy | Studio". Legacy = today's code, untouched. Studio =
  new behaviour for the PBRM program only (legacy FO4 program can opt in later).
- Environment: reuse `SFCubeMapCache` (`gltexloaders.cpp:913-985`) for FO4 by lifting the
  `>= 151` gate for Studio only. It already gives a GGX-prefiltered 7-mip specular cube and a
  32px diffuse cube (irradiance stand-in; SH9 projection optional). Sample with FO76's roughness
  -> lod law `m = r*(10-4r)` (`f76_default.frag:188`) and an analytic split-sum BRDF term
  (Karis fit) so no LUT texture is needed. Sources: a bundled set of small .hdr HDRIs
  (Radiance loader exists, `gltexloaders.cpp:923-936`) plus "use the material's own cube".
  Controls: HDRI picker (fix the dead "Choose PBR Environment Cubemap" button for FO4,
  `glview.cpp:3385`), Rotation = existing `envMapRotation` slider, World Space Lighting =
  inverse of today's Frontal, Strength, World Opacity + Blur = `drawSkyBox` with
  `cubeBgndMipLevel` (skybox already exists, gate at `renderer.cpp:1772`).
- Sun: keep the single directional light (`lightSourcePosition[0]`) in LINEAR units, colour
  temperature kept, intensity in the same scale as the HDRI; optional "sun from HDRI".
- Space: linear throughout. Decode base/emissive as sRGB regardless of the DDS's DXGI tag
  (GL_EXT_texture_sRGB_decode or shader-side decode keyed by slot), lights not sqrt'd, and a
  shader-side sRGB encode at the end (NOT GL_FRAMEBUFFER_SRGB, see 6a, particles).
- Exposure + tonemap: Exposure in stops (`2^EV` into the existing `brightnessScale`), view
  transform choice Standard (clip) / AgX-style / Khronos PBR Neutral (cheap, hue-preserving,
  good for material judgement); keep Hable as the Legacy curve only.
- Uniform plumbing without moving the std140 layout: `unusedUniform1/2`
  (`uniforms.glsl:20-21`) can carry mode + exposure, or a Studio-only uniform on the PBRM
  program; legacy/particle programs never read them.
- Gate plan: Legacy mode must be byte-identical (the mode bit only selects the PBRM program's
  Studio branch). Harnesses must force the mode and every lighting slider explicitly (6a),
  and the particle gate must run once with a Studio PBR shape drawn before particles to prove
  no state leaks.

### Divergences from Blender (deliberate)
- Keeps a sun on by default (Blender Material Preview is HDRI-only): FO4 content is authored
  for a key light and the editor preview uses one.
- Default background = existing clear colour (Blender: World Opacity 0) - same result.
- No shadows, no screen-space reflections/AO, no probes from the scene: one world, one sun.
- Irradiance from a 32px convolved cube rather than SH (equivalent at this frequency; SH is a
  later optimisation).
- Legacy mode stays the default until bungo rules; Blender has no legacy lighting to preserve.
