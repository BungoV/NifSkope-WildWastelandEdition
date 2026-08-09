# Renderer match — FO4 spec/gloss + PBR from `.pbrm`

Goal (user, 2026-07-26): make NifSkope's renderer match the PBR Material
Editor's — correct **FO4 spec/gloss** shading, and **PBR rendering from `.pbrm`
materials** — **without breaking the CPU particle simulation or the screen-space
refraction preview** (`WW_CHANGES 2026-07-06`).

This is the renderer half of [the merge](WW_CHANGES.md) and is deliberately
phased: the safety net comes first, the self-contained shading fix second, and
the new material format last.

---

## 0. Regression guard — BUILT; real distortion case verified 2026-08-09

`WW_RENDER_SHOT=<out.png>` (nifskope_ui.cpp) renders one NIF and quits, saving
`ogl->grabFramebuffer()`. Driver: `tools/render_regression/capture.ps1`
(`-Mode baseline` / `-Mode compare`, pixel + max-channel diff via LockBits).

Three things are pinned, because a render harness that isn't reproducible is
worse than none:

| pinned | why |
|---|---|
| camera (`setOrientation( view, recenter )`) | the per-file camera is persisted, so every baseline would otherwise be framed differently |
| scene clock (`setSceneTime`, default 1.0s) | the particle sim is time-driven — a wall-clock grab can never reproduce the same pixels |
| `showRefraction` / `showParticles` forced true | both default true in `Scene` but are overwritten from the **persisted menu state** at startup; with them unchecked the harness silently guards nothing |

`ogl->grabFramebuffer()` is required, not `skope->grab()` — the viewport is a
native window, so a widget grab returns white where the 3D content belongs.
`-platform offscreen` is not an option either: it crashes on GL context
creation (exit 139).

**Baselines captured (7):** `particles_mist`, `particles_glow`, `glass_visor`,
`glass_shader`, `refraction_fixed`, `lit_setdressing`, `lit_head`.

### RESOLVED 2026-07-27 — it was a real renderer bug, not a fixture problem

**Root cause.** `BSLightingShaderProperty::updateParams` assigns `hasRefraction`
**only inside the `} else { // m == nullptr` branch** (`glproperty.cpp:1513`) —
i.e. only when the shape has no valid BGSM/BGEM. When a material *is* present,
which is nearly every FO4 mesh, `hasRefraction` kept `resetParams()`'s `false`
and the NIF's `SLSF1_Refraction` bit was never consulted at all. The
screen-space refraction preview had therefore been dead for all material-backed
FO4 content since it shipped on 07-06.

Fixed by reading it in the material branch too:

```cpp
hasRefraction = m->bRefraction || hasSF1( ShaderFlags::SLSF1_Refraction );
refractionStrength = m->bRefraction ? m->fRefractionPower
                                   : nif->get<float>( iSPData, "Refraction Strength" );
```

**Why OR and not "the material wins".** nif.xml says the FO4 flags are "mostly
overridden if Name is a path to a BGSM/BGEM file", so material-wins looks right —
and it is wrong. Measured over the corpus: of **6899** vanilla FO4 materials
under `Data\Materials`, **zero** set `bRefraction` (9 set `bRefractionFalloff`,
which is its own curiosity). Reading the material alone leaves the feature dead
for all vanilla content, so the NIF bit has to keep counting. Offsets for that
scan were validated first, by confirming every boolean field decodes as strictly
`{0,1}` while `iAlphaTestRef` shows a real range (37–200) — a misaligned read
cannot produce that.

**Hypothesis 1 was wrong.** FO4 bit 15 *is* `Refraction`: nif.xml
`Fallout4ShaderPropertyFlags1` line 7015. Skyrim's layout and FO4's agree here,
so `SLSF1_Refraction = 1 << 15` was never the problem. (Bit 2 is
`Temp_Refraction` and bit 16 `Fire_Refraction`, neither of which this path uses.)

### The thing that made this so confusing

`fo4_default.frag:383` ends the refraction branch with `color.rgb = bg; color.a = 1.0;`
— the shape is replaced wholesale by the framebuffer behind it. Over a
**featureless background a refracting shape is therefore invisible**, not
distorted. So the fix's first visible effect was the mesh *vanishing*, which
reads exactly like a catastrophic regression. It is correct behaviour with
nothing behind the shape to refract.

### Current fixture, and what it does and does not catch

`tests/render/refraction_fixture.nif` is now **`CA-PowerArmorVisorGlass01.nif`
with `SLSF1_Refraction` set and Refraction Strength 0.8 on block 5**, replacing
the old donor.nif-based one. That makes it an A/B pair with the `glass_visor`
case, which is the *same mesh with the flag off*:

| case | flag | renders |
|---|---|---|
| `glass_visor` | off | the bright white visor glass |
| `refraction_fixed` | on | the visor replaced by the background behind it |

Any regression that stops refraction engaging flips `refraction_fixed` back to
looking like `glass_visor`, and the difference is attributable to exactly one
flag on one block. That is a real guard, and strictly better than the old
fixture which was byte-identical to its own control.

The original visor fixture still only proves engagement, but the missing real
distortion case was closed with `X01_Torso_VFX.nif`. Its `autoLoop` sequence
drives Refraction Strength to 1.0 at 2.5 seconds and its own opaque fan/grid give
the normal map non-uniform pixels to bend. `WW_RENDER_REFRACTION=0/1` provides a
paired deterministic control. That pair exposed the old arbitrary `0.12`
viewport-relative offset as 100–200-pixel jumps into remote background; the
preview now caps the same authored normals at eight screen pixels regardless of
resolution.

---

## 1. FO4 spec/gloss (self-contained, no new format)

`res/shaders/fo4_default.frag` already agrees with the **measured calibration**
on channel assignment — `g = specMap.g` (gloss), `s = specMap.r` (spec), and
`smoothness = g * specGlossiness`, so a flat white `_s.G` correctly falls back
to the material scalar. **There is no mirror-finish bug here**; that was a
conversion-side finding.

What differs from the editor is the *model*:

- **`F0 = 0.2` hardcoded** in both the `TorranceSparrow` call and the ambient
  Fresnel term — roughly 5× the dielectric 0.04 the editor uses. The `_s.R`
  channel is used only as a specular *mask*, not as a specular level → F0.
- **Normalized-Phong Torrance-Sparrow** with `exp2( smoothness * 10 + 1 )`,
  versus the editor's GGX.
- **No specular at all without a spec map** (`if ( hasSpecularMap )`), so the
  scalar-gloss/scalar-spec case renders flat.
- No energy conservation between diffuse and specular.
- Correct and to keep: **no metallic term** — FO4 encodes no metallicity
  (metal is recovered via a correction mask at conversion time, not at render).

Order: map `_s.R` → F0 (dielectric range), swap in GGX + Smith visibility, add
the no-spec-map path, then energy conservation. Diff the regression set after
*each* step — `lit_setdressing` and `lit_head` are expected to change, the
particle and refraction cases must not.

## 2. PBR from `.pbrm`

Format is specified in `PBRMaterialEditorQt/docs/PBRM-v5.md`: `PBRM` magic +
`uint32` version (5, 4 accepted) + `uint32` JSON size + compact UTF-8 JSON.
Scope the first pass to the spec's own **"Minimal Standard runtime slice"** —
shader `Standard` and four Primary UV sockets:

| socket | data |
|---|---|
| `primaryBaseColor` | sRGB RGB, A opacity |
| `primaryNormal` | linear RG, optional B height, A signed curvature |
| `primaryRmaos` | R roughness, G metallic, B AO, A dielectric F0 **or** porosity |
| `primaryEmissive` | sRGB RGB, A intensity mask |

Load-bearing rules from the spec:

- A texture is sampled only when the slot is `enabled`, the path is valid, **and
  the matching `override*` flag is false** — otherwise the constant applies.
- RMAOS alpha is F0 only while `alphaCarries == "Dielectric F0"`; under
  `Porosity` F0 always uses its constant (0.04).
- `overridePorosity` is the one flag never auto-forced (absent porosity has a
  meaningful derived value).
- Path contract: `/`→`\`, strip leading `.\`, collapse separators, compare
  case-insensitively, **leading `textures\` optional**, reject drive-qualified /
  UNC / parent-traversal. Never rewrite the authored string.
- **Fail closed** on an unsupported `requirements` entry or unknown shader name —
  never silently fall back to `Standard`.
- A NIF referencing a `.pbrm` consults **no** same-name BGSM/BGEM.

**Reader: DONE** — `src/io/pbrmfile.{h,cpp}`, verified via `nifskope-cli pbrm`
(see `WW_CHANGES.md 2026-07-27b`). Nothing renders from it yet.

### Material resolution (user, 2026-07-27) — NOT BUILT

Two ways a shape gets a `.pbrm`, in priority order:

1. **Directly linked** — the shader property's material name is a `.pbrm`.
2. **Same-name discovery** — the material is `foo.bgsm`/`foo.bgem` and a `foo.pbrm`
   exists beside it, in which case the PBRM **auto-replaces** it.

**The auto-replace is a toggle** (direct links are unconditional; only discovery
is optional). Sensible home is next to the reserved PBR shading-menu entry, or
Settings ▸ Render; persist it like the other render options.

Note the tension worth keeping straight: `docs/PBRM-v5.md` says "PBRM is
standalone. A NIF directly references its `.pbrm`; no same-name BGSM or BGEM is
consulted." That is the *runtime* contract and case 1 honours it. Case 2 is a
deliberate **editor-side preview convenience** — it lets an existing FO4 asset be
previewed against a new PBRM without editing the NIF — which is exactly why it is
opt-in and why it must not be mistaken for the runtime's behaviour.

Remaining work: resolution through `nifextfiles.cpp` (both cases + the toggle),
then a `pbrm_default.{vert,frag,prog}` carrying the PBR core lifted from the
editor's shader (`materialpreviewwidget.cpp` 1559–2766 — 1200 lines including IBL,
shadows, vegetation and displacement; take the BRDF and IBL, leave the rest — and
the GGX/Smith half is already ported into `fo4_default.frag`), and connecting the
disabled **PBR: Roughness / Metallic** action (`nifskope_ui.cpp` ~5648).

## Risk register

| risk | mitigation |
|---|---|
| particle sim regressions | regression set, forced `showParticles`, pinned clock |
| refraction regressions | same — **blocked on §0's open item** |
| shader program explosion | `.pbrm` gets its own program; do not overload `fo4_default` |
| BGSM path collateral damage | `.pbrm` resolution must not change BGSM/BGEM lookup order |
| silent wrong-material rendering | fail closed per spec, and surface a material diagnostic |
