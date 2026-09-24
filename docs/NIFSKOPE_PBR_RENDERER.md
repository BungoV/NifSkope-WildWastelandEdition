# NifSkope PBR renderer: design and staged plan

Lane PBRRENDER0, 2026-09-23 (research and design only; nothing here is built).
This doc replaces the unbuilt parts of `RENDERER_MATCH_PLAN.md` §2 ("Material resolution") and reopens
`TO_BE_IMPLEMENTED.md` §0 ("PBR draws empty frames"). Research notes with every anchor are in
`scratchpad/pbrrender0_20260923/research_*.md`.

Anchors are file:line in the tree at head `720762a`. FO4CS anchors are in `E:\Projects\Fo4CommunityShaders\wt-spec1`
(head `b0d06b4`). ED means the PBR Material Editor's `PBRMaterialEditorQt\src\materialpreviewwidget.cpp`.

---

## 0. bungo's words this plan is built on (2026-09-23)

| # | Ruling | What it binds |
|---|---|---|
| G | "Nifskope would need PBR to be rendered first, when a .pbrm materials is detected or a .json file that is linked to our nif file, that points to our shader node" | This comes before IMPOSTORPBRM1. The ".json" is the `.nifx` sidecar (ruling h). |
| a | "Make sure you do not break legacy spec/gloss materials, however, you can upgrade those where needed, if the way they're lighted up does not align with how Fallout 4 works" | Legacy stays the default path. A mesh with no `.pbrm` and no `.nifx` material link renders byte-identical at every PBR stage. Legacy upgrades get their own stage (§7), and each one is gated. |
| b | "There's particles too, so make sure you don't break their display specifically" | Hard constraint on every stage (§8). |
| c | "You can run comparison tests when needed, to compare old and new shading of materials, bgsm bgem or embedded either" | One old-vs-new harness (§9). Gates cover BGSM, BGEM and embedded materials. |
| d | "scene lighting in nifskope might need an upgrade" | Lighting stage (§5), as a scene mode. |
| e | "You can load a Fallout 4's scene, with the sky, clouds, sun and a ground plane and the cubemaps (or just the cubemap if pbrm), to test the display of materials in nifskope" | Lookdev stage (§6). |
| f | "Make it so you can load any weather from Fallout 4, so from any .esm or .esp you load" | Weather picker (§6.3). |
| g | "Also, shadows, make sure they align with whats in the vanilla game and what we have, cascaded, screen space, same goes for SSAO and SSGI, make sure you can toggle these off and on" | Screen effects (§10). |
| h | The linked json is the `.nifx` sidecar (FO4CS `NifSidecar.h`). | Material section (§2.4). |
| i | "In the future, we could read .nifx and edit it alongside our nifs" | Later stage X1 (§12). |
| j | "In the future, it might make sense to make .nifx into something more of a .nif file than a json, so holding geometry" | Owed ruling X2 (§12). |

---

## 1. What renders today

**Short answer.** A FO4 mesh with a `.pbrm` renders exactly like one without it: through the legacy
spec/gloss program `fo4_default.prog`. The PBR path exists but three separate things block it.

1. **The gate constant.** `static constexpr bool pbrmFeatureEnabled = false;` at `src/gl/glproperty.cpp:1005`.
   `pbrmMode()` forces Legacy at `:1026-1030`. The two PBR menu entries are greyed out and `activeMode` is
   forced to Legacy (`src/nifskope_ui.cpp:27354-27407`, `:27367`). The TBI note still cites `:996`, which is stale.
2. **Every v6 `.pbrm` is a parse error.** `src/io/pbrmfile.cpp:149` accepts envelope versions 4 and 5 only. The
   editor has written v6 since 2026-09-16, so every current `.pbrm` fails to parse and the BGSM wins
   (`glproperty.cpp:1080`).
3. **Detection does not match the game** (§2.2).

**What the PBR path would draw if the gate were flipped:**
- Route: `src/gl/renderer.cpp:176-202`.
  `wantPbrm = mesh->bslsp && wwLodChannelView == 0 && (mode == PBR || (mode == LegacyAndPBR && pbrmValid))`.
- Program: `pbrm_default.prog` = `fo4_default.vert` + `pbrm_default.frag`. The fragment shader has 229 lines and
  is the v5 editor BRDF.
- Setup: `setupProgramPBRM` (`renderer.cpp:764-917`).
  - Binds BaseMap, NormalMap, RmaosMap and EmissiveMap, plus a cube: slot 4 or the fallback `cube_fo4`.
  - `hasCubeMap` stays false unless the legacy env flag is set.
  - With no `.pbrm`, the "PBR" mode derives rough = 1 - gloss, metal 0, f0 0.04 from the legacy slots.
- Channel views force `fo4_default.prog` (`:217-225`). The stale-hint guard is `:237`.

**The §0 "empty frames" bug has a strong candidate cause, already fixed but never tested.**
- Commit `53fe028` (2026-08-03) added `mesh->setUniforms( prog )` at `renderer.cpp:910`
  (`Shape::setUniforms`, `glshape.cpp:649-660`).
- Before that commit the PBR path never uploaded the per-program `modelViewMatrix` and `normalMatrix`. GL
  initialises them to zero, so `v = modelViewMatrix * v` collapsed every vertex to one point.
- That matches the 07-27e RenderDoc record: "draw submitted, grid continuous, fragments never land".
- It also explains why the uniform-block checks came back clean: these two uniforms are not in the block.
- The gate constant has not been flipped since that commit. Stage R1's first gate tests this (§13). Until it
  passes, this is a candidate, not a fix.

**Known gaps in the v5 shader, even once it draws:**
- A v6 file would read the specular weight as F0.
- No base-colour decode.
- Hable tonemap in sqrt space.
- Env light is a raw LDR cube at `rough*8`.
- Metals are black without the env flag.
- PBR diffuse is about 3x darker than legacy under the same light. The Lambert /π meets sqrt space; see
  `research_scene_lighting.md` §6.

**Card bake.** It never uses PBR. `WW_IMPOSTOR_BAKE` (`nifskope_ui.cpp:22420`) calls `paintGL` with
`wwLodChannelView != 0`, and the route above forces `fo4_default` for any channel view. See §11.

---

## 2. Detection: how a shape is known to be PBR

### 2.1 The game's rules (FO4CS wt-spec1, the authority)

- **Master:** `[PBRMaterials] bPBRMMaterials` (`src\Materials\TruePBRShim.h:98-102`).
- **Rule B, texture swap** (asked FIRST, in OnLoadTextureSet2, `TruePBRShimRuntime.cpp:4489-4492`).
  - Input: the engine-loaded diffuse, which must end `_d.dds`.
  - Candidates: `Materials\<same dir>\<stem>.pbrm`, then `Materials\<parent dir>\<stem>.pbrm`
    (`PBRM.cpp:1396-1448`).
  - An existing swap binding is kept by the root seat (`TruePBRShimRuntime.cpp:4270-4281`).
- **Rule A, same-name sibling.**
  - The shader property NAME (the BGSM/BGEM path) goes through `NormalizeMaterialPath` (`PBRM.cpp:1339-1384`):
    - `/` becomes `\` and doubled separators collapse;
    - `.\` is stripped;
    - `..`, a drive or a leading separator is refused;
    - the name must end `.bgsm` or `.bgem`;
    - `Materials\` is prefixed when missing;
    - case is preserved.
  - Then `.bgsm`/`.bgem` becomes `.pbrm` (`:1386-1394`).
  - This always runs, with no toggle.
- **Rule C, FO76 BGSM.**
  - Applied only after A declines (`TruePBRShimRuntime.cpp:4404-4412`, `3447-3494`).
  - Needs version word 20..22, the PBR flag and a base colour.
- **Direct `.pbrm` name in the NIF: REFUSED.**
  - `PBRM.cpp:1368-1371`, "direct .pbrm NIF references are reserved for future runtime support".
- **Failure:** any failure keeps the engine BGSM/BGEM.
  - A missing file is negative-cached.
  - A parse error is logged once.
  - Parsed but unsupported means no binding.
  - ANY authored texture that fails to load aborts the whole binding (`TruePBRShimRuntime.cpp:1847-1879`).
- **Texture paths:** `NormalizeTexturePath` (`PBRM.cpp:1291-1337`).
  - `Textures\` is prefixed and `..` is refused.
  - A path that fails normalising falls back to the slot constant. It is not fatal.
- **Colour spaces:** base colour and emissive are sRGB. Normal, RMAOS and subsurface are linear. The specular
  colour map is sRGB with linear alpha.
- **On bind, the runtime writes NIF flags** (`TruePBRShimRuntime.cpp:3807-3828`), and a viewer should copy them:
  - sets Specular, CastShadows and ZBufferTest;
  - sets ZBufferWrite unless alpha-blend;
  - takes TwoSided and AlphaTest from the `.pbrm`;
  - clears PremultAlpha and OwnEmit.
- **Envelope:** `PBRM` magic, u32 version 4/5/6, u32 size, then JSON that must consume the file exactly
  (`PBRM.cpp:1143-1206`).
  - `schemaVersion` must equal the envelope version.
  - A v5 reader must refuse v6, with no best-effort read.
  - A v6 reader reads v4/v5 with the old F0 law byte for byte.

### 2.2 What NifSkope does today (and where it diverges)

`BSShaderLightingProperty::resolvePbrm` (`src/gl/glproperty.cpp:1047-1086`):

| Case | NifSkope | Game | Fix |
|---|---|---|---|
| Direct `.pbrm` name | taken unconditionally (`:1058`) | refused | follow the game (question Q1) |
| Same-name sibling | only when "Auto-replace" is on (`:1060-1063`, read at `glview.cpp:3290`) | always | default ON (Q2) |
| Swap rule (`_d.dds`) | absent | first | add it for the NIF's own diffuse; a real OMOD/MSWP swap needs ESP data, so this is later |
| FO76 BGSM | absent | third | add it (NifSkope already parses FO76 BGSMs for FO76 NIFs; converting one to the PBR material is new) |
| v6 envelope | parse error (`pbrmfile.cpp:149`) | read | add the v6 reader |
| Texture failure | falls back per slot | whole binding aborts | follow the game |
| `.nifx` material link | absent | absent (owed) | new, §2.4 |

`lodgenResolveMaterialMask` (`src/lodgen.cpp:1822-1930`) has its own candidate order, including a diffuse-stem
fallback that `resolvePbrm` lacks.
- **Rule:** one candidate function serves both the viewport and lodgen, so the card and the viewport can never
  disagree.

### 2.3 Target rule for NifSkope (same as the game, plus `.nifx`)

For each shape with a BSLightingShaderProperty or BSEffectShaderProperty, walk this order. The first candidate
that parses AND whose textures all load wins.
1. **Swap**, when a swap is active: diffuse `_d.dds` gives the same-dir `.pbrm`, then the parent-dir `.pbrm`.
   - NifSkope has no active swap until the ESP reader supplies OMOD/MSWP data. Until then this step is skipped.
     The NIF's own diffuse is NOT treated as a swap, because in the game an un-swapped mesh never reaches Rule B
     with a different answer than A.
2. **`.nifx` material link** for this shape's node name (§2.4).
3. **Same-name sibling** of the BGSM/BGEM.
4. **FO76 BGSM** read directly.
5. **Legacy**: the BGSM/BGEM or the embedded material, rendered by today's code.

A direct `.pbrm` name in the NIF is refused, as the game does (Q1). The material diagnostic shows the reason and
names the fix ("link it in the .nifx").

Why `.nifx` sits below swap and above sibling:
- It names one node in one NIF, so it is more specific than the sibling rule.
- A material swap (an OMOD skin) is a deliberate later override of the base look. In the game it already beats
  the base material. A skin author opts in by shipping the swapped `.pbrm`, and their skin must win over the
  base NIF's link.
- Q3 asks bungo to confirm.

### 2.4 The `.nifx` material section (proposal)

FO4CS's sidecar today (`src\NifSidecar\NifSidecar.h`, `.cpp`):
- Location: `<nifstem>.nifx` beside the NIF, lower-cased (`NifSidecar.cpp:376-415`).
- Top level: a JSON object with a required `"version": 1`; anything else is refused (`:573-585`).
- Sections are top-level keys. Only `fakeVolume` is known (`:437`). Unknown sections are kept by name and ignored
  with a note (`:595-600`).
- Node keys are bare node names, compared case-insensitively (`:613-625`).
- Limits: 1 MiB, 64 sections, 4096 nodes.
- **Nothing loads it yet** (`NifSidecar.h:385-398`).

Proposed section `material`, additive at version 1:

```json
{
  "version": 1,
  "fakeVolume": { "SmokeEmitter01": { "bulgeStrength": 0.5 } },
  "material": {
    "BarrelMesh:0": { "pbrm": "Materials\\Weapons\\M2\\M2Barrel.pbrm" },
    "Scope":        { "pbrm": "Weapons\\M2\\M2Scope.pbrm" }
  }
}
```

- **Key:** the name of the geometry node (BSTriShape, BSSubIndexTriShape, and so on) that owns the shader
  property. It is matched case-insensitively, like `fakeVolume`.
  - FO4 shader properties are named by their BGSM path, so they are not usable keys.
  - One shader property per shape in FO4 makes the shape name unambiguous.
  - If two shapes share a name, both take the entry, and NifSkope's diagnostic lists the duplicate.
- **`pbrm`** (required): normalised like `NormalizeMaterialPath`, except that it must end `.pbrm`. `Materials\` is
  prefixed when missing, and `..`, drives and leading separators are refused.
- **Reserved, not in v1:** per-entry overrides (for example a tint preset). Unknown keys inside an entry are
  kept and ignored.
- **Version:** stays 1. A new section is additive, and a v1 reader that does not know `material` ignores it with
  a note (`NifSidecar.cpp:595-600`), so `fakeVolume` and today's FO4CS parser are untouched.
- **Generations:**
  - `"version"` 1 and up, in a file whose first non-space byte is `{`, is the JSON generation.
  - A future geometry-carrying generation (§12, X2) is a binary file, told apart by its first bytes: the NIF
    header string for route A, or a `NIFX` magic for route B. It carries its own version, which is 2 or more.
  - A JSON file with version 2 or more is refused, as FO4CS already does.

**One rule, three places.**
- **NifSkope:** one `.nifx` parser/writer (§12 X1 needs a writer). Unknown sections, keys and key order must
  survive a load and save byte-stable: keep the raw JSON members and re-emit untouched ones verbatim.
- **FO4CS:** contract text plus an owed item, and FO4CS readers come last, so this is not a blocker.
  - The load seam `RegisterLoadedModel` is uncalled today.
  - A `material` section parser is needed.
  - A precedence slot is needed at the LoadBinary / ReceiveValuesFromRootMaterial seat, before the sibling probe
    (`TruePBRShimRuntime.cpp:4393-4412`).
  - The refusal at `PBRM.cpp:1368-1371` stays. Its diagnostic text should name the `.nifx` route.
- **Editor:** its `.pbrmset` (`{"schema":"FO4.PBRM.MaterialSet","version":1,"assignments":{...}}`,
  PBRM-v6.md:156-161) is the same idea keyed by submesh name. Recommend (Q5) that the editor writes the `.nifx`
  `material` section (merging, never dropping other sections) and keeps reading `.pbrmset` for old files.

**Doc fixes owed** (editor repo, not this lane):
1. PBRM-v6.md:440-441 says "PBRM is standalone. A NIF directly references its `.pbrm`; no same-name BGSM or BGEM
   is consulted." That contradicts the runtime. Rewrite it as: a NIF links a `.pbrm` through its `.nifx` `material`
   section or the same-name sibling rule; a `.pbrm` name inside the NIF is refused.
2. PBRM-v6.md:22: the envelope row still says "5; 4 accepted". Correct it to 4, 5 or 6.
3. PBRM-v6.md:384-385 gives feature bits 26/27, but the runtime uses 30/31 (`PBRM.h:119-120`). Derived and never
   serialised, but the doc should match.

This supersedes the 2026-07-27 resolution note in `RENDERER_MATCH_PLAN.md` §2 ("direct links unconditional,
discovery a toggle"). That note predates the runtime, and Q1/Q2 ask bungo to confirm the reversal.

---

## 3. Shader plan: the editor is the law

### 3.1 Programs

- **`pbrm_default.prog` gets its own vertex shader.** Its shaders become `pbrm_default.vert` + `pbrm_default.frag`.
  - Today it shares `fo4_default.vert`, which FO4 effect-shader meshes also use (`renderer.cpp:929`, §8).
  - Any varying the PBR path needs (linear light colours instead of `A = sqrt(amb)*0.375` at
    `fo4_default.vert:83-85`) must not move a byte of `fo4_default.vert`.
- **One fragment source, two outputs.**
  - A `PbrSurface evalSurface()` function turns the `.pbrm` into per-pixel values: base colour (decoded, tinted),
    normal, roughness, metallic, AO, specular weight, specular colour, IOR/F0, emission and opacity.
  - Lighting consumes that struct for the viewport.
  - A `lodChannelView` branch writes one field flat for the card bake (§11). There is no second renderer.

### 3.2 Port first (in order)

The editor lines are in `research_reference.md` §1-4.
1. **v6 specular** (Pattern B, on the `primarySpecularColor` slot values):
   - `F0_ior = ((ior-1)/(ior+1))^2`;
   - `ior = overrideIor ? ior : specA * iorMax`;
   - `tint = sRGB-decoded (overrideColor ? color : map.rgb)`;
   - `F0_diel = weight * tint * F0_ior`;
   - F82 edge tint for metals (ED:1789).
   - Weight comes from RMAOS.A when `alphaCarries = "Specular Weight"` and it is not overridden; otherwise from the
     constant `specularWeight`.
   - A v4/v5 file keeps the old law (RMAOS.A = F0, clamped 0.16).
2. **Base colour decode.** The editor uses pow 2.2. Recommend decoding via the texture's sRGB format, the way the
   game does, and stating the pow-2.2 difference as a documented error bound.
3. **Normal decode** `(s*255-128)/127` with Z rebuilt, times strength and `globalNormalStrength`. Flip the
   geometric normal for back faces BEFORE the TBN is built. Roughness floor 0.035.
4. **Emission:** `pow(colour,2.2) * mask * luminance/100`. A texture REPLACES the constant unless `overrideColor`.
   The result is added before exposure, so 100 nits = linear 1.0. It is not multiplied by the legacy glow scale.
5. **Multiscatter + split-sum environment.** Use the Lazarov analytic DFG that FO4CS uses, so no LUT texture is
   needed. Direct light and env specular both take the multiscatter factor.
6. **Glossy-diffuse energy split:** indirect diffuse `*= 1 - E_spec`, with E_spec = `(F0*A + B) * ms` (FO4CS
   `truepbr_brdf.hlsli:108-119`, `ambient_ibl_pass.hlsl:1176-1201`, gate `bIndirectEnergySplit` ships ON).
   - The editor does NOT do this yet. It uses `(1 - Schlick(NoV)) * 0.45` (ED:2713-2717), and FO4CS's own
     `OpenPbrEnergy.h:30-33` says the editor owes it.
   - Recommend (Q6) the FO4CS law in NifSkope and an owed editor item.
7. **Tint masks** (special layer 0): `tint = max(0, (1 - Σm) + Σ cᵢ mᵢ)`, then `base *= tint` (ED:2310-2316).
   - The editor passes tint colours as raw sRGB, not decoded (ED:5265-5267). Copy that, and file the editor
     inconsistency (Q13).
8. **EON diffuse** when `diffuseRoughness > 0` (ED:1792, 2680-2690). This comes second: self-contained, about 10
   lines, and off at 0.
9. **Opacity composition:** opaque, test or premultiplied (ED:2336-2337). Follow the runtime flags from §2.1.

### 3.3 Deferred to the renderer merge (last)

- **Refraction** (Glass/Water modes, composition 6, screen distortion). The editor refracts into its own
  prefiltered cube, not the scene, so a port would show sky through a bottle indoors.
- **The editor's particle/effect renderer:** flipbooks, six-way lightmaps, age/seed. NifSkope draws NIF particles
  through its own path (§8). A per-mesh age would animate every particle in lockstep.
- Coat, fuzz/sheen (LTC table), retroreflection (under a head light it would glow constantly), thin film,
  anisotropy, bent/detail normal, specular AA, cavity occlusion, surface states/porosity, subsurface, hair, eye,
  parallax, layers 2/3.
- A refractive shape routed to PBR loses its refraction: `setupProgramPBRM` has no refraction branch. Until the
  merge, any shape with refraction set stays on the legacy route. This is in the route verdict.

### 3.4 Differences NifSkope's current PBR shader has against the editor (ranked)

1. F0 model: a v6 weight is read as F0, so weight 1 gives a mirror.
2. No base colour decode.
3. Hable + sqrt instead of exposure + Reinhard + 1/2.2.
4. Env specular has no prefilter and no DFG.
5. Env diffuse is flat instead of irradiance.
6. No multiscatter.
7. No F82 for metals.
8. Normal unpack 2x-1.
9. No global normal strength.
10. Back-face flip order.
11. Roughness floor 0.02.
12. Vertex colour multiplied into base (the editor ignores it in the lit path; keep the multiply only for
    `vertexColorOverride`, question Q14).
13. Emission decode/replace and /100.
14. Light units.
15. Opacity modes.
16. No EON.
17. Missing optional lobes.

---

## 4. Uniforms and texture slots

The per-program uniforms on `pbrm_default` are the only new surface. The std140 `GlobalUniforms` block
(`uniforms.glsl`, filled at `glview.cpp:3752-3804`) keeps its layout. New scene-wide values ride in
`unusedUniform1/2` (`uniforms.glsl:20-21`), which the legacy and particle programs never read.

| Unit | Sampler | Source | Colour space | Missing → |
|---|---|---|---|---|
| 0 | `BaseMap` | `primaryBaseColor` | sRGB (format-decoded) | constant `pbrBaseColor` / white |
| 1 | `NormalMap` | `primaryNormal` | linear | flat (128,128,255) |
| 2 | `RmaosMap` | `primaryRmaos` | linear | constants |
| 3 | `EmissiveMap` | `primaryEmissive` | sRGB, A = mask | black |
| 4 | `SpecColorMap` (new) | `primarySpecularColor` | sRGB rgb, linear A (IOR) | white + constant IOR |
| 5 | `TintMaskMap` (new) | special layer 0 | linear | no tint |
| 6 | `CubeMap` | lookdev/IBL cube (§5) or the material `_e` | per source | `cube_fo4` fallback, `hasCubeMap` = 1 in Studio |
| 7 | `IrradianceMap` (new, Studio) | 32 px diffuse cube from `SFCubeMapCache` | linear | — |

Constants, existing: `pbrBaseColor`, `pbrOpacity`, `pbrRoughness`, `pbrMetallic`, `pbrAo`, `pbrNormalStrength`,
`pbrEmissiveColor`, `pbrEmissiveIntensity` (now luminance/100), `pbrFeatures`.

Changed and new:
- `pbrF0` is renamed `pbrSpecWeight`, with a separate `pbrEnvelope` (5 or 6) selecting the F0 law.
- New: `pbrSpecColor` (vec3), `pbrIor`, `pbrIorMax`, `pbrOverrideIor`, `pbrOverrideSpecColor`,
  `pbrTintColors[4]`, `pbrTintMode`, `pbrDiffuseRoughness`, `pbrGlobalNormalStrength`, `pbrComposition`,
  `lodChannelView` (now also set on this program).
- `modelViewMatrix` and `normalMatrix` are uploaded via `mesh->setUniforms` (already at `renderer.cpp:910`).

Scene values in `unusedUniform1/2`: lighting mode (0 = Legacy), exposure 2^EV, view transform id, and
screen-effect bits. Every one reads 0 in Legacy, which is today's value.

**Every new GL state set for a PBR draw is restored after it.** Never enable `GL_FRAMEBUFFER_SRGB` (§8). Encode
sRGB in the shader.

---

## 5. Scene lighting

### 5.1 Today

- **One directional light.** By default it is a head light, `(0,0,1)` in view space (`glview.cpp:3779-3780`). It
  can instead be world-fixed from declination/planar angle (`:3766-3776`). Colour comes from a temperature
  polynomial, with brightness sliders.
  - Bug found: the angles are saved under one QSettings key and read under another (`lightingwidget.cpp:125-126`
    vs `:78-81`). This is out of scope and reported for a separate lane.
- **Ambient:** one flat value, `A = sqrt(amb) * 0.375` (`fo4_default.vert:83-85`).
- **Space:** the maths runs in sqrt space. Hable tonemap with D.a exposure and A.a `toneMapScale` 0.2364 (almost
  identity, then it clips). `GL_FRAMEBUFFER_SRGB` is off for bsver below 151 (`bsshape.cpp:295-299`), and
  texture sRGB follows the DXGI tag.
- **Env:** `cube_fo4` = `mipblur_DefaultOutside1.dds`, 128 px, 8 mips, LDR.
  - A GGX prefilter (`SFCubeMapCache`, `gltexloaders.cpp:913-985`) exists but is gated to bsver 151 and up
    (`:1017`, `:1263`).
  - The skybox returns early below 151 (`renderer.cpp:1772`).
  - "Choose PBR Environment Cubemap" is a no-op on FO4 (`glview.cpp:3383-3386`).
- **What PBR cannot show under this:**
  - metals are black without env and dull with the LDR cube;
  - IOR and Fresnel are invisible, because the head light pins VdotH near 1;
  - roughness shows only as highlight size;
  - PBR looks about 3x darker than legacy;
  - the shadow side is flat;
  - emission has no units.
- **What FO4 feeds its lighting** (vanilla bytecode):
  - sun direction and HDR colour;
  - a directional ambient as a 3x4 matrix, gamma-decoded ^2.2;
  - the probe cube array (ambient pass only);
  - a cascaded shadow array;
  - linear HDR out, then an image-space tonemap. The curve was not located and is unknown.

### 5.2 Stage R2, "Lighting: Legacy | Studio" scene mode (Blender Material Preview as reference)

- **The mode is a row in the render settings panel.** Legacy is today's code, untouched. The default is bungo's
  call (Q7); the recommendation is Legacy.
- **Environment:**
  - Reuse `SFCubeMapCache` for FO4 by lifting its bsver ≥ 151 gate for Studio only. It gives a GGX-prefiltered
    specular cube and a 32 px diffuse cube.
  - Sample it with FO76's roughness→lod law `m = r*(10-4r)` (`f76_default.frag:188`) and the analytic DFG.
  - Sources: the lookdev/weather cube (§6), the material's own `_e`, or a picked HDR/DDS. Fix the dead FO4 picker
    button.
  - Rotation reuses `envMapRotation`. "World-space lighting" is the inverse of today's Frontal.
  - Background opacity/blur reuse `drawSkyBox` + `cubeBgndMipLevel`.
- **Sun:** keep the one directional light, in LINEAR units at the same scale as the environment. The colour
  temperature stays.
- **Space and output:** linear throughout.
  - Base/emissive are decoded as sRGB regardless of the DXGI tag.
  - Lights are not sqrt'd.
  - Exposure is in EV.
  - View transform: Standard / AgX / Khronos PBR Neutral. Recommend PBR Neutral as the default for judging
    material colour.
  - The sRGB encode is in the shader, NOT via `GL_FRAMEBUFFER_SRGB`.
- **PBR shapes under Legacy lighting:** the PBR program computes in linear. It un-squares the legacy light
  inputs, then encodes with the legacy Hable + sqrt. PBR and legacy side by side are then not biased 3x before the
  materials even differ. Legacy shapes are untouched.
- **Legacy shapes under Studio:** they stay on today's code until L1 (§7) lands. Joining Studio afterwards is a
  later opt-in; vanilla legacy is linear, so it fits.
- **Deliberate divergences from Blender:**
  - a sun stays on (FO4 content and the editor both use a key light);
  - the irradiance cube stands in for SH;
  - no scene probes.
- **Coexistence with the zero gates:** the harness pins the mode. In Legacy mode the mode bit only selects the
  PBRM program's branch, so legacy/particle pixels are identical by construction, and the gate proves it.

---

## 6. FO4 lookdev stage and the weather picker

### 6.1 Assets (vanilla DataUnpacked, read-only; every file confirmed to exist)

- **Cubes:** `textures\shared\cubemaps\mipblur_DefaultOutside1.dds`.
  - 128 px, 8 mips, BGRA8. Its caps2 = 0 quirk must be tolerated.
  - 1,309 of 6,616 vanilla BGSMs name it.
  - The other 70 files in that folder are selectable.
- **Ground:** a generated quad.
  - Material: `materials\landscape\Ground\CommonwealthDefault01.bgsm`.
  - Textures: `textures\landscape\Ground\CommonwealthDefault01_d.DDS`, `_n.DDS`, `_s.DDS`.
  - Drawn through the existing `fo4_default` path.
- **Sky:** `meshes\sky\AtmosphereDome002.nif`, `Atmosphere.nif`, `Clouds.nif`, `Stars.nif`.
  - All use BSSkyShaderProperty, which is **not rendered anywhere in `src\gl`**.
  - In the game the dome's colour comes from the weather, not a texture.
- **Clouds:** `textures\sky\CloudsLower01_d.DDS`, `CloudsUpper01_d.DDS`, `CloudsHorizon01_d.DDS`, plus the
  weather's own cloud list. CommonwealthClear uses the `Skyrim*` cloud files.
- **Sun:** `textures\sky\Sun.DDS`, `SunGlare.DDS`, `Sun_d.DDS`. CLMT names `Sky\Sun_d.dds` and `Sky\SunGlare.dds`.
- **Plugins:** `Fallout4.esm` (path from `cellview.h:39`), DLCs, and any `.esp`.

### 6.2 One loader, three users

One `LookdevStage` object holds the cube path, sun direction and colour, the ambient/DALC colours, ground on/off,
the weather FormID and the hour. It has one `apply()`. Each user only chooses whether to call it.
- **Harness:** `WW_LOOKDEV=1`, `WW_LOOKDEV_WEATHER=<EditorID|FormID>`, `WW_LOOKDEV_HOUR=<h>`. Default OFF, so every
  existing shot stays byte-identical.
- **Viewport:** one row, "Lookdev stage", plus the weather picker. Default is a question (Q8); the recommendation
  is OFF.
- **Card bake:** NEVER the lookdev stage. Bake under the neutral stage.
  - The card's colour sheet is unlit by contract (`LODGEN_IMPOSTOR_SPEC.md:44`), and the game lights the card
    itself, so a baked sky would light it twice.
  - Lookdev is fine for the card PREVIEW (mesh vs card side by side).
- **Old-vs-new gates** always run both arms under the SAME pinned stage.

**What NifSkope cannot draw yet:**
- the sky dome shader;
- cloud layers;
- the sun disc/glare. `skybox.frag:60-63` draws a GGX highlight, which is acceptable for now;
- shadows (§10);
- fog.

Stage 1 is cubemap-only: `drawSkyBox` background, sun and ambient from the weather, and the ground quad.

### 6.3 Weather picker (WTHR from any loaded plugin)

- **Reader:** the only plugin reader is libfo76utils `ESMFile` (`lib/libfo76utils/src/esmfile.cpp`), wrapped by
  `EsmWorld` (`src/esmdata.cpp`). Build an `EsmWeather` layer on it; there is no second parser.
  - It handles zlib, a load order via a comma list, FormID remap and `mapFormID`. The last file wins.
  - **Must add a missing-master refusal:** masters are silently aliased today (`esmfile.cpp:216-220`).
  - **Trap:** the const `ESMField` constructor returns empty fields for compressed records.
  - Also: the ESL flag is not read, and there is a hard cap of 256 files.
- **Subrecords used:**
  - `NAM0`: 19 rows × 8 times of day × RGBA8. Rows: 3 Ambient, 4 Sunlight, 5 Sun, 0 Sky Upper, 7 Sky Lower,
    8 Horizon, 1/12/17/18 Fog, 15 Sun glare.
  - `DALC` × 8: 6 axes, specular, fresnel.
  - `IMSP` × 8 → `IMGS` (`HNAM` 9 floats, `CNAM`, `TNAM`, `TX00`).
  - `FNAM` fog; `x0TX` cloud textures, `LNAM`, `NAM1`, `PNAM`, `JNAM`, `RNAM`/`QNAM`.
  - `DATA`: glare, lightning colour.
  - Reference only: `WGDR` god rays, `GNAM` LENS, `MNAM`/`NNAM` precipitation.
  - Time-of-day order: Sunrise, Day, Sunset, Night, EarlySunrise, LateSunrise, EarlySunset, LateSunset. The row
    count comes from the form version: below 111 = 4 ToD; 111-118 = 17 rows; 119+ = 19 rows.
- **Time of day:**
  - An hour slider row. CLMT `TNAM` holds times in 10-minute units; DefaultClimate `0000015F` = (30,54,102,126).
  - Phases blend in four-quarter ramps with a 0.5 h extension.
  - The sun follows the vanilla tent arc (FO4CS `SolarPosition.h:6-24`, `docs/RE/solar-daynight.md`).
- **Cubemaps are not in WTHR.** In the game, the ambient pass's probe cube array supplies them. Stage-1 source
  order:
  1. the material's own `EnvmapTexture`;
  2. otherwise `mipblur_DefaultOutside1`;
  3. otherwise the picker.
  - Later (W2) the dome is rendered into a cube and offered as a source.
- **Mods:** load order with masters, overrides win. Real use: `FO4CSPhysicalWeathers.esp` (masters Fallout4.esm,
  DLCCoast.esm, DLCNukaWorld.esm; 69 WTHR, 68 of them overrides).
- **Stages:**
  - W1: read + sun + ambient + cube, part of R2b.
  - W2: dome + sun disc.
  - W3: clouds.
  - W4: fog.
  - W5: image space and exposure from IMGS.
  - Gates in §13.

---

## 7. Legacy spec/gloss: audit against vanilla, and upgrade stage L1

Vanilla source: `Shaders011.fxp` bytecode disassembled (blobs: prepass 81bade73, sun 7c8a4acf, spot 418df8b0,
combine 260a8c3f), cross-checked against FO4CS transcriptions. NifSkope source: `res/shaders/fo4_default.frag`.

**History that matters.** `RENDERER_MATCH_PLAN.md` §1 (2026-07-26) moved `fo4_default.frag` from normalised Phong
`exp2(10g+1)` with F0 0.2 (which was the vanilla law) to the editor's GGX. bungo's ruling (a) now says legacy must
light "how Fallout 4 works". So L1 largely restores the vanilla lobe; the old `TorranceSparrow()`, `VisibDiv` and
`OrenNayar()` are still in the file, unused (`frag:100-120`, `:203-236`).

| Term | Vanilla | NifSkope (file:line) | Verdict |
|---|---|---|---|
| `_s` channels | G = gloss × fSmoothness, R = mask × fSpecularMult, white when absent | `frag:448-461`, default `renderer.cpp:1045-1046` | MATCH, keep |
| Gloss → lobe | Blinn-Phong n = exp2(10g+1), 2..2048 | GGX rough = 1-g (`frag:467-476`) | DIFFERS, strong → L1 |
| Spec model | D = NH^n(n+2)/2π; F = min((1-f⁵)·0.2+f⁵,1); vanilla G select; min(DGF/4, 15) × mask × π × light × NL | GGX, F0 0.04, no π, no clamp (`frag:181-200`) | DIFFERS, strong → L1 |
| Spec colour | not applied in the deferred lobe (cb2[1] goes to the emissive target) | × specColor (`frag:476`) | CANDIDATE; Todd's treat proof owed before any change (Q12) |
| Diffuse | simple Oren-Nayar, no (1-F) | OrenNayarFull + (1-F) (`frag:122-176`, `:508`, `:529`) | DIFFERS, moderate → L1 |
| Env intensity | specMask × 3 × sqrt(sat(g-0.3)) × scale × light, no Fresnel | × envReflection × specStrength × light (`frag:479-488`) | gloss gate DIFFERS strong; × light and no Fresnel MATCH |
| Env mip | (1-g)·6 | 8-8g | DIFFERS, cosmetic → L1 |
| Env cube | probe array | material `_e` (`renderer.cpp:1096-1103`) | keep `_e` as the stand-in |
| Rim | always on: (1-NV)^0.01·sat(V·-L)·NL·(1-g) | computed, not applied (`frag:499-505`) | DIFFERS, only with a back light → L1 |
| Backlight | power × albedo × sat(-NL), albedo applied twice | once (`frag:491-496`) | shape MATCH; square DIFFERS, cosmetic |
| Soft | adds only the wrap excess over NL | `frag:510-517` | DIFFERS, moderate (foliage) → L1 |
| Ambient | directional 3x4 ^2.2 + ambient spec 0.25(1-NV)^(3-g)·DirAmb(R) | flat A (`fo4_default.vert:83-85`, `frag:531`, `:534`) | DIFFERS; needs R2b's DALC ambient |
| Gamma | linear | sqrt space + Hable (`frag:238-250`, `:538`) | DIFFERS; comes with Studio lighting |
| Greyscale palette | t5 palette | — | UNKNOWN |

**L1 ordering: AFTER the PBR stages R1-R4**, as its own lane. The PBR stages need a frozen legacy baseline to
prove their zero-diff gates. An L1 change moves legacy pixels on purpose, so it resets that baseline and is
sequenced when no PBR stage is mid-flight.

L1 rules:
- It is a repair, so no toggle (Q11).
- One term per commit.
- Each term has a measured gate (§13).
- Particles and BGEM effects stay zero-diff.

---

## 8. Particles and effect meshes: hard constraint on every stage

**Where they draw:**
- `Particles` (`glscene.cpp:352-354`) picks `particles.prog` by name (`glparticles.cpp:286`).
- Blend at `:320-335`, with an additive fallback.
- Depth mask off at `:513-517`.
- Drawn last among translucents (`glscene.cpp:537-580`).
- There is no soft/depth fade anywhere: `falloffDepth` is uploaded but unused.
- FO4 BSEffectShaderProperty meshes use `fo4_effectshader.prog` = **`fo4_default.vert`** + `fo4_effectshader.frag`,
  via `setupProgramCE1` (`renderer.cpp:929`, `1160-1230`, blend `1303-1360`). `bsesp` is set and `bslsp` is null,
  so they never reach the PBRM route.
- No vanilla FO4 Effects NIF (692 checked) holds a BSStripParticleSystem.

**What could reach them, and the rule for each:**

| Reach | Rule |
|---|---|
| `fo4_default.vert` edits (effect meshes share it) | PBR gets `pbrm_default.vert`; `fo4_default.vert` stays byte-identical |
| `uniforms.glsl` layout / `GlobalUniforms` values (`glview.cpp:3752-3804`) | layout frozen; new values only in `unusedUniform1/2`, which read 0 in Legacy |
| GL state leaks, especially `GL_FRAMEBUFFER_SRGB` (particles never set it) | shader-side sRGB encode only; every PBR draw restores what it set |
| The background under an additive sprite | the lookdev stage/background is pinned identically in both arms |
| Blend/sort/depth order | no edits to `glparticles.cpp`, `glscene.cpp:537-580` or `setupProgramCE1` in any R/L/W stage |
| Screen effects (§10) | AO multiplies opaque colour BEFORE the transparent pass; particles are never inputs or targets |

**Gate, every stage:**
- `MPSFireSmall01.nif`, `MPSSmokeFireMed01.nif`, `AttachFXMist01.nif`, `GlowFlatPlaneOneSided01.nif` and
  `GlassShader01.nif` render pixel-identical to the rung exe at the noise floor. Pinned: `WW_RENDER_TIME`/`SEQ`,
  camera and lighting.
- Once per stage the same particle NIF is rendered with a PBR fixture shape drawn BEFORE it in the same scene, to
  prove no state leaks.
- The census row lists the program per draw, and particles must name `particles.prog`.

---

## 9. One old-vs-new shading harness

Design only; built in R0. Files: `tests/spells/pbr_shade_ab.sh`, `pbr_shade_ab.py` and `pbr_shade_ab_cases.txt`.

- **OLD arm:** a frozen rung FOLDER holding the exe AND its `shaders/`, because shaders load from
  `applicationDirPath`. **NEW arm:** the built tree.
- **Pinned, identically in both arms:**
  - `WW_SETTINGS_SCOPE` (`harnesswindow.cpp:274`, the only way to pin the QSettings lighting rig today);
  - `WW_RENDER_SHOT` (`nifskope_ui.cpp:22097`), `_SIZE` (`:22110`), `_VIEW` (`:22200`), `_CENTER`/`_DIST`/`_FOV`/`_ORTHO`
    (`glview.cpp:6369-6421`), `_TIME` (`:22355`), `_SEQ` (`:22350`), `_CLEAN` (`:22248`), `_FLAT` (`:22271`),
    `_REFRACTION` (`:22228`), `_SS` (`:22390`);
  - `WW_WINDOW_AT` on the second monitor;
  - new switches: `WW_PBRM_MODE`, `WW_PBRM_AUTOREPLACE`, `WW_LIGHTING_MODE`, `WW_LOOKDEV*`, `WW_RENDER_PARTICLES`
    (the shot hook forces `showParticles` today), and `WW_RENDER_SHADOWS`/`_CONTACT`/`_AO`/`_SSGI`.
- **Censuses:** `WW_PROGRAM_CENSUS` (`renderer.cpp:124`) and `WW_CAMERA_CENSUS` (`glview.cpp:6574`) are compared
  between arms. A new `WW_PBRM_CENSUS` prints one line per shape: route (nifx/swap/sibling/fo76/legacy), path,
  envelope and refusal reason. It echoes the resolved state, not the intent.
- **Noise floor:** `grabFramebuffer` jitter measured 0 to 37 px, so each case first runs OLD vs OLD twice and the
  bar is set from that. The precedent for a byte-identical gate is `native_lighting_check.py:235-246`.
- **Gate kinds:**
  - `zero`: diff ≤ the noise bar.
  - `change`: above the bar, AND a must-differ floor, AND the census names `pbrm_default.prog` (or the new
    program) on the expected shapes.
  - `value`: the pixel at (x,y) is within tolerance of a number computed independently in numpy.
- **Output:** `<case>_diff.png` (OLD | NEW | |d|×16) and one verdict line:
  `pbr_shade_ab <case> gate=<zero|change|value> old=<sha8> new=<sha8> size=WxH px=<n> frac=<f> max=<m> mean=<m> bar=<b> noise=<n> progs=<list> -> PASS|FAIL`.
- **Red control, once per gate kind:** a deliberately altered shader copied into the NEW arm must make it FAIL.
  This proves the gate bites.
- **Fixtures** (Data = `E:\Tools\Fallout 4\DataUnpacked\Data`):
  - BGSM: `meshes\SetDressing\ACDucts\ACDuctConnector01.nif` (`AcDuctsRusted.BGSM`).
  - BGEM: `meshes\Effects\GlowFlatPlaneOneSided01.nif` (`GlowPlane01.BGEM`); alternative `BlackGlowFill01.nif`.
  - Embedded lit: `meshes\Landscape\GuardRails\GRailCurveR01.nif`.
  - Embedded effect: `meshes\Effects\GlassShader01.nif`.
  - Particles: `meshes\Effects\MPSFireSmall01.nif`, `MPSSmokeFireMed01.nif`, `AttachFXMist01.nif`.
  - Legacy law: `meshes\Weapons\10mmPistol\10MMPistol.nif` plus one soft-lit foliage mesh (picked by header scan
    in L1).
  - PBR: a test Data folder (loose files, never the vanilla tree) holding a copy of `ACDuctConnector01.nif` with:
    - a sibling `.pbrm` (v5 and v6 twins);
    - a `.nifx` `material` link;
    - a direct-`.pbrm` variant (must refuse);
    - a unit sphere NIF for furnace and editor-match reads.
- **Existing tools reused:** `imgdiff.ps1` (verdict line, no diff PNG). `capture.ps1` is stale and is retired in
  favour of this.
- **This lane built and launched nothing.** Implementation stages run the harness.

---

## 10. Shadows, contact shadows, AO and SSGI (toggles)

NifSkope has none of these today:
- no shadow map;
- no sampled depth texture (every depth attachment is a renderbuffer);
- no AO/GI pass.

It is a forward renderer with one pass per shape. The default framebuffer is GL 4.2 core, 24-bit depth, MSAA
(`glview.cpp:328-345`). GL 4.2 has no `glPolygonOffsetClamp`.

| Effect | Vanilla FO4 (Todd's treat 1.10.155 settings / bytecode) | FO4CS (wt-spec1) | NifSkope design |
|---|---|---|---|
| Sun shadows | 3 cascades at 800 / 3000 / fDirShadowDistance (code default 3000; his 24000); blend 100 units; D16; 16-tap Poisson ±3 texels with SampleCmp; DepthBias 12, slope 6.0; fade sphere ^8; shadow sun floored 30° | vanilla filter by default (`DirectionalShadowCascadePolicy.h:258`); slope clamp ON; u16 extent repair; `bCascadeSunRedirect` = 1 | ONE fitted ortho map (object + ground patch), texel size matched to vanilla cascade 0; D16 + `glPolygonOffset(6,12)` (same formula); vanilla 16-tap Poisson; multiplies sun diffuse AND specular only |
| Contact shadows | none | Bend SSS (`ScreenSpaceShadowsBend.h`, `bend_sss_gpu.hlsli`): 384 px reach, 4 hard + 16 fade samples, thickness 0.005, contrast 4; sun DIFFUSE only (`bsdf_light_deferred.hlsl:909-915`); master off | Bend law over a single-sample depth pre-pass texture; sun diffuse only |
| AO | SAO: radius 108.2, intensity 7.1, bias 0.6; one multiply on the WHOLE lit colour in the ambient composite | GTAO (shipped INI; struct default says XeGTAO): radius 1.2×32 = 38.4, 4×4, power 1.5, falloff 0.615, half res; same application point (`ambient_ibl_pass.hlsl:5170-5200`) | GTAO with FO4CS settings; normals from depth; post-multiply on opaque colour before the transparent pass |
| SSGI | none | Skyrim-CS SSGI port: 4 slices × 8 steps, radius 256, back-depth 32, source brightness 2.0, saturation 0.8, one bounce, diffuse only, `colour*AO + GI*albedo` | same law; spatial only (no temporal history, so gates stay deterministic; stated divergence); needs the PBR shader to emit direct-only radiance + albedo (MRT) |

**Rows:** four independent rows in the render settings, "Sun shadows", "Contact shadows", "Ambient occlusion" and
"Screen-space GI". Each has a `WW_RENDER_*` twin. There are no INI-only switches.

**Defaults:** a question (Q10); the recommendation is all OFF, per the masters-off rule.

**OFF means today's code path:** no FBO allocated, no program change, no extra output. That is what makes the
identity gate provable.

**Order:** after R2 (lighting) and R3 (BRDF), because every effect modulates the lit colour they define.
- S1 shadows, S2 contact shadows, S3 AO, S4 SSGI.
- SSGI goes last, because only it changes the material shaders' outputs.

**Unknowns:**
- launcher preset values;
- whether `iDirShadowSplits` = 2 means 2 or 3 cascades (his atlas has 3);
- what `uiShadowFilter` = 3 selects;
- the SAO sample pattern;
- the slice-to-box fit of `UpdateCamerasI` (RVA 0x28CACB0), which the texel-size match approximates until it is read.

---

## 11. The card bake uses the same PBR path

**Today:**
- The bake captures MATERIAL channels, not lit colour. `fo4_default.frag:275-364`:
  - 12 = unlit base × vertex colour;
  - 8 = geometric normal;
  - 9 = depth;
  - 10 = gloss/spec or a `.lodm` raw;
  - 11 = leaf mask;
  - 13 = emissive.
- Sheets are written at `nifskope_ui.cpp:23539-23611`.
- FBO: RGBA8, no MSAA, 4× size, borrowing the live GLView (`renderOff`, `:22980-23030`).
- lodgen reads a `.pbrm` only for the terrain layer mask (`lodgenResolveMaterialMask`, `lodgen.cpp:1822-1930`,
  called at `:11299`).

**Design (no second renderer):**
1. `pbrm_default.frag` gets a `lodChannelView` branch that writes `evalSurface()` fields flat, with the same channel
   numbers as legacy. A new channel number carries specular weight × colour and IOR; its sheet home is IMPOSTORPBRM1's
   decision.
2. Route change at `renderer.cpp:185-225`: a shape whose `.pbrm` resolved goes to `pbrm_default.prog` in the BAKE
   channels (8-13 plus new). Terrain preview channels 1-6 stay on `fo4_default`. `lodChannelView` is uploaded to
   this program, and the stale-hint rule at `:237` follows the new verdict.
3. The bake's verdict does not depend on the viewport mode. It uses the §2.3 candidate function, which is also the
   one lodgen uses.
4. Card family is pbr when every textured shape resolved a `.pbrm` OR a pbr `.lodm` (today only `.lodm` counts,
   `nifskope_ui.cpp:22691`).
5. Neutral stage, never lookdev (§6.2).

**Cautions:**
- 8-bit dielectric F0 gets about 15 steps.
- Emissive above 1 clips, so the multiple rides the `.lodm` `emissiveScale`, with ×100 nits applied there.
- Using the `.pbrm` normal map in `_n` would change legacy sheets too, so gate it to the pbr family.

**What IMPOSTORPBRM1 then needs:**
- R1 (v6 reader + the shared candidate function);
- the material half of R3/R4 (the `evalSurface` struct with v6 specular, tint and emission).

It does NOT need R2's lighting. It can start once R4 lands, in parallel with S- and W-stages.

---

## 12. Later stages

**X1, the `.nifx` editor** ("read .nifx and edit it alongside our nifs"):
- Each `.nifx` entry is shown on the node it keys: the `material` link and the `fakeVolume` dials.
- Flat Name|Value rows, `skinVars[]` palette only.
- Save writes the sidecar next to the NIF.
- Built on R1's single parser/writer, which must already round-trip unknown sections and keys byte-stable (R1
  gate).

**X2, geometry-carrying `.nifx`** ("more of a .nif file than a json, so holding geometry"). This is an owed ruling.
- One node-name-keyed interface (`NifxStore`: `entries(nodeName)`, `setEntry`, `save`) whose storage backend can
  be swapped. The JSON backend comes first.
- **Route A, a real NIF 20.2.0.7:** standard blocks, with sections as extra data.
  - Pro: opens in any NifSkope, reuses the whole reader/writer, and is diffable with existing tools.
  - Con: JSON-style sections must be encoded as extra-data blocks, and the game or other tools might mistake it
    for a mesh if it is renamed.
- **Route B, a custom binary** (`NIFX` magic + version + chunks).
  - Pro: compact and exactly shaped to the data.
  - Con: a new format to specify, version, write tools for and keep in step in three places.
- **Recommendation: route A.**
- The generation is told apart by the first bytes: `{` for JSON v1, the NIF header for A, `NIFX` for B. A binary
  generation has version ≥ 2.

---

## 13. Staged plan and gates

**Every stage also runs the standing zero set:**
- particles (§8);
- the BGEM plane;
- the embedded effect;
- the BGSM duct;
- the embedded guard rail.

These run as `zero` gates against the previous rung, except where the stage's purpose is to change them (L1
changes the lit legacy fixtures). Each gate kind proves once, with a red control, that it fails on broken code.

| Stage | Scope | Gates (each fails on broken code) |
|---|---|---|
| **R0** harness | `pbr_shade_ab.*`; the new `WW_*` switches; `WW_PBRM_CENSUS`; diff PNG writer | OLD-vs-OLD noise bar per case recorded; a red-control shader edit FAILS `zero`; census identical between arms |
| **R1** detect + load + flip | v6 reader (4/5/6, the F0 law by envelope); §2.3 order with one shared candidate function (viewport + lodgen); refusal of direct `.pbrm`; `.nifx` parser/writer with the `material` section; texture-failure aborts the binding; runtime flag writes (§2.1); flip `pbrmFeatureEnabled`; debug overlay (route/path/envelope/refusal per shape, plus a "PBR route" view that tints shapes by route) | (a) **Coverage:** the PBR duct fixture in PBR mode covers ≥ 99% of the pixels it covers in Legacy (the §0 bug gives 0); (b) **route census** equals an independent Python resolver over the fixture set (swap/nifx/sibling/FO76/refuse); swapping two precedence steps FAILS; (c) **v5 vs v6 F0 read:** census F0 for v6 weight 1 ior 1.5 = 0.040 and for v5 f0 0.04 = 0.040; reading the v6 file through the v5 law gives 1.0 (red control); (d) `.nifx` round-trip with an unknown section and key order is byte-identical; (e) a direct-`.pbrm` NIF shows the refusal and renders legacy; (f) standing zero set |
| **R2a** Studio lighting | scene mode row; `SFCubeMapCache` for FO4 (Studio only); sun in linear units; EV exposure; view transforms; shader sRGB encode; the PBR program's legacy-mode output transform | Legacy mode `zero` on everything; uniform white cube L → prefiltered mip readback = L and irradiance = L (±1/255); EV +1 doubles the pre-tonemap value (tonemap None); linear 0.5 grey → 188 ±1 on screen; sRGB-tagged and UNORM-tagged copies of one base texture render identical |
| **R2b** lookdev + W1 | `LookdevStage`; ground quad; cube-only background; `EsmWeather` W1 (sun, ambient/DALC, cube source); hour row; master refusal | W G1-G5 (below) and G7; `WW_LOOKDEV` unset → `zero` |
| **R3** BRDF + v6 specular | `evalSurface`; decode fixes; v6 F0 + F82; Lazarov DFG + multiscatter; energy split (Q6); EON | **White furnace:** sphere under a uniform white env, tonemap None: metal base 1 at rough 0.1/0.5/1.0 → 1.00 ±0.02 at the centre and at 60° (fails without multiscatter at rough 1); dielectric base 1 F0 0.04 → ≤ 1.00 +0.02 (fails without the split, energy gain); **v5/v6 twins** `zero`-identical; **IOR:** normal-incidence env-spec ratio for ior 2.0 vs 1.5 = 0.111/0.040 ±3%; **editor match:** unit sphere, eye (0,0,3.2), 45° FOV, key (-0.4056,0.7071,0.5792) ×4 at (1,.929,.871), same cube, Reinhard + 1/2.2, editor bloom/SSAO/shadows OFF: metal sphere rough 0.5 at the centre pixel and at one grazing pixel (named when R0 records the fixture) within 3/255; the dielectric sphere differs from the editor by the predicted split amount (numpy) ±3/255 |
| **R4** tint + emission | tint mask law and modes; emission luminance/100, replace semantics; opacity composition | 2×2 tint-mask fixture: each quadrant pixel = base × law (numpy) ±2/255, and an overlap texel follows Normalize; emission 100 nits → linear 1.00 pre-exposure (tonemap None); textured emission ignores the constant colour unless overridden |
| **R5** card bake (= IMPOSTORPBRM1's renderer half) | channel branch; bake route; family rule | pbr fixture sheets equal numpy evaluation of the `.pbrm` texels (tinted base, R, M, AO, spec) ±1/255 at 16 sampled texels; every legacy-family card byte-identical to the rung bake |
| **L1** legacy upgrade | vanilla lobe, Fresnel, diffuse, env gate/mip, rim, soft; one term per commit | 10mm pistol, fixed ¾ front camera, one light at 45°, ambient 0: hot-spot row vs the numpy vanilla formula from the same `_s`/`_n` texels, relative error < 3% at 8 px; one matte pixel (g < 0.2) diffuse-only; env term under a uniform white cube = specMask·3·sqrt(sat(g-0.3))·scale·light; foliage terminator row at a 100° light; particles/BGEM `zero` |
| **S1** sun shadows | fitted D16 map, Poisson, bias | all four rows OFF → `zero` (0 bytes) and ON→OFF → `zero`; pole H = 100 at 45°: 50% edge at the predicted tip ±(3 shadow texels + 1 px), on the predicted side; < 0.5% dark on lit ground 200 units up-sun (acne); base gap within the bias allowance (peter-panning) |
| **S2** contact shadows | Bend march over the depth pre-pass | 4-unit cube, map OFF: the pixel by its anti-sun face darkens ≥ the named amount; beyond the reach and the sky unchanged within 1/255 |
| **S3** AO | GTAO, applied before the transparent pass | L crease: crease pixel ≥ the analytic 90° GTAO value; flat pixel changes ≤ 1/255; sky unchanged |
| **S4** SSGI | MRT direct radiance + albedo; spatial | white floor by a sun-lit red wall: R rise within ±30% of the form-factor prediction × 2.0; G,B rise < 10% of it; beyond 256 units unchanged |
| **W2-W5** weather | dome + sun disc; clouds; fog; image space | CommonwealthClear 12:00 zenith/horizon = (38,64,99)/(130,145,162) and the dome→cube +Z readback equals the zenith; cloud layer count = 11 - popcount(NAM1 disabled); fog factor 0 at DayNear, DayMax (0.85) at DayFar; IMGS Day HNAM readback = (3.0, 0.02, 0.5, 0.2, 3.25, 1.6, 4.5, 2.4, 0.18) |
| **X1 / X2** | `.nifx` editor; geometry ruling | X1: edit and save round-trip, byte-stable for untouched keys |

**Weather gates for W1** (values from the independent decoder `wthr_probe.py`, not the code under test):
- **G1:** CommonwealthClear `0002B52A` returns Sunlight Day (225,225,225), Ambient Day (93,93,93), Sunlight Night
  (53,70,87) and DALC Day Z- (101,133,169). A row swap or a wrong ToD stride fails.
- **G2:** all 71 vanilla WTHRs parse; NAM0 size histogram {608:65, 544:2, 272:4}; DALC {8:67, 4:4}.
- **G3:** blend known answers for TNAM (30,54,102,126): 12:00 Day; 02:00 Night; 6.75 h → Sunrise→LateSunrise
  t=0. A sabotaged order FAILS.
- **G4:** with Physical Weathers loaded, the winner's source file is the `.esp` and its NAM0 equals the `.esp`'s
  bytes. First confirm it differs from vanilla.
- **G5:** loading the `.esp` without DLCNukaWorld gives a refusal naming it. Red control: the check disabled
  "succeeds".
- **G7:** noon and midnight differ, and the summary line names the keys and the cube source.

**Sequence:**
1. R0 → R1 → R2a → R2b → R3 → R4.
2. Then R5 (IMPOSTORPBRM1), S1-S4 and W2-W5 can interleave, one row lane at a time.
3. L1 runs when no R stage is mid-flight.
4. X1 after R1. X2 is ruling-only.

---

## 14. Questions for bungo (recommendation, and why in one line)

| Q | Question | Recommended | Why |
|---|---|---|---|
| Q1 | A `.pbrm` name written directly in a NIF: honour it (your 07-27 note) or refuse it like the game? | Refuse, with a diagnostic naming the `.nifx` route | The viewport must agree with the game, and the runtime refuses it (`PBRM.cpp:1368-1371`). |
| Q2 | Same-name sibling auto-replace default | ON (keep the toggle for A/B viewing) | The game always consults the sibling. |
| Q3 | Precedence: swap > `.nifx` > sibling > FO76 BGSM > legacy | Yes | `.nifx` names one node, so it is more specific than the sibling, but a skin swap must still beat the base NIF's link. |
| Q4 | `.nifx` `material` key = the geometry node name, case-insensitive | Yes | It matches `fakeVolume`'s rule, and FO4 shader properties are named by BGSM path. |
| Q5 | Editor writes the `.nifx` `material` section (and still reads `.pbrmset`) | Yes | One link format that the editor, NifSkope and the game all read. |
| Q6 | Energy split: FO4CS wave 88 `(1 - E_spec)`, or the editor's `(1 - Schlick)·0.45`? | FO4CS law, plus an owed editor item | It is what the game shows, and the editor's own contract says it owes the rule. |
| Q7 | Lighting mode default | Legacy | Byte-identical by default; Studio is one click away. |
| Q8 | Lookdev stage default in the viewport | OFF; weather hour default noon; cube not tinted by weather before W2 | Nothing changes unasked; noon is the neutral read; an untinted cube shows what is authored. |
| Q9 | PBR display for shapes whose `.pbrm` resolves: default | OFF until R3's gates pass, then ON (LegacyAndPBR) | Shows what the game shows once it is proven correct. |
| Q10 | Shadows / contact / AO / SSGI defaults | All OFF | Masters ship off. |
| Q11 | Legacy upgrade L1: behind a toggle? | No; it is a repair, one gated commit per term | No fix-only toggles. |
| Q12 | Drop `cSpecularColor` on legacy spec? | Only after Todd's treat shows vanilla's cb2[1] is the emissive colour | Evidence first; it is rare in vanilla anyway. |
| Q13 | Tint colours as raw sRGB (editor) or decoded? | Copy the editor now; fix both together | The editor is the law, and a lone fix would make the two tools disagree. |
| Q14 | Vertex colour on PBR base colour | Only when the NIF's vertex-colour flag is set | The editor ignores it; FO4 meshes can carry vertex colours the game applies under that flag (to confirm in R3). |
| Q15 | Shadows: one fitted map, or reproduce the 3 cascades? | One map, texel size matched to vanilla cascade 0 | A lookdev scene sits inside one bounding sphere, and the penumbra is what the eye reads. |
| Q16 | Shadow filter / AO backend / AO boundary | Vanilla Poisson; GTAO (the shipped INI); AO on the whole lit colour | What the game and FO4CS ship. |
| Q17 | Contact shadows on sun specular too? | No, diffuse only | FO4CS law. |
| Q18 | Geometry `.nifx`: route A (real NIF) or B (custom binary)? | A | It reuses the reader/writer and opens in any NifSkope. |

---

## 15. Unknowns (named, not guessed)

- Whether 53fe028 is the whole §0 cause. R1 gate (a) decides.
- The vanilla tonemap curve (image-space pass not located).
- Vanilla handling of cb2[1] / cSpecularColor; the greyscale palette maths; whether the spec flag gates the `_s`
  map on the CPU.
- FO4CS runtime handling of tint masks (not read); whether NifSkope's reader divides luminance by 100 (`pbrmfile.h`).
- Shadow presets, cascade count, `uiShadowFilter`, SAO pattern, and the `UpdateCamerasI` fit (§10).
- `.pbrmset` file naming in the editor.

## RULINGS 2026-09-23 23:0x (bungo, asked with explanations; director splice)
- Q1 OVERRULED the recommendation: "You can have a .pbrm automatically replace a bgsm or bgem, you can directly link
  .pbrm in a shader note, and you can too do it with .nifx, but direct linking is probably the least preferable, but
  still supported". THREE supported routes: same-name sibling auto-replace, a DIRECT .pbrm name in the shader
  property, and the .nifx material entry. Direct linking is supported but least preferred (a NIF that names a .pbrm
  has no vanilla BGSM fallback without FO4CS). NifSkope honours it; FO4CS must too = OWED runtime change (PBRM.cpp
  :1368-1371 refusal lifted; built last by standing order); PBRM-v6.md:440 stays as written.
  Precedence: swap > .nifx > (direct name | sibling -- exclusive: the property name is either a .pbrm or a .bgsm/.bgem)
  > FO76 BGSM > legacy.
- Q6: the GAME's energy rule (FO4CS wave 88, 1 - E_spec); editor owes the same fix.
- Q9: PBR display OFF until R3's gates pass, then ON by default.
- Q2-Q5, Q7, Q8, Q10-Q18: ACCEPTED as recommended in the table above.
- Consequence for R1: 'refusal of direct .pbrm' and gate (e) are REPLACED -- a direct-.pbrm NIF renders PBR via the direct route; gate (e) = its census route reads `direct` and swapping direct/nifx precedence FAILS the resolver compare.
- RULED 23:2x 2026-09-23 (bungo): the .nifx lives NEXT TO its .nif -- same folder, same stem (<nifstem>.nifx); no other search path (no Materials\ mirror, no archive-root lookup).
