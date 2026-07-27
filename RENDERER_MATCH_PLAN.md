# Renderer match — FO4 spec/gloss + PBR from `.pbrm`

Goal (user, 2026-07-26): make NifSkope's renderer match the PBR Material
Editor's — correct **FO4 spec/gloss** shading, and **PBR rendering from `.pbrm`
materials** — **without breaking the CPU particle simulation or the screen-space
refraction preview** (`WW_CHANGES 2026-07-06`).

This is the renderer half of [the merge](WW_CHANGES.md) and is deliberately
phased: the safety net comes first, the self-contained shading fix second, and
the new material format last.

---

## 0. Regression guard — BUILT, one case not yet real

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

### OPEN — the refraction case does not engage

`tests/render/refraction_fixture.nif` (donor.nif with `SLSF1_Refraction` and
Refraction Strength 0.8 set via the CLI) renders **byte-identical to the
unmodified head** — same MD5. So `doRefraction` is false and that baseline is
currently guarding nothing. Ruled out so far:

- the flag *is* in the file — CLI `get` returns `2151712259`, and `& 32768` is set;
- `flags1` comes straight from the NIF for `bsVersion < 151`
  (`BSShaderLightingProperty::setFlags1`) — no BGSM override;
- `scene->showRefraction` is forced true by the harness;
- the branch (`renderer.cpp` ~810) is **not** version-gated;
- `Scene::grabRefractionSource()` only fails on a null renderer or an empty viewport.

Leading hypotheses, in order:
1. **FO4 bit semantics.** NifSkope's `ShaderFlags::SLSF1_Refraction = 1 << 15`
   is Skyrim's layout (note the `// 15!` comment in glproperty.h) applied to
   FO4's `Fallout4ShaderPropertyFlags1`. Confirm bit 15 is what FO4 actually
   uses, and confirm against a **vanilla** asset that already has it rather than
   a synthesized one.
2. **Pass ordering.** The comment says the shape "draws in the second pass, so
   the framebuffer already holds the scene behind it" — an opaque shape may not
   reach that setup. If so the fixture needs alpha blending enabled too.

**Do not start §1 or §2 until this is resolved** — it is the exact thing the
change is supposed to not break. A vanilla FO4 asset with the flag set is the
better fixture; find one by scanning the corpus for the bit rather than
authoring it.

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
