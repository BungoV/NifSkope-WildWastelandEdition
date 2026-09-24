# PBRR3 deliverable text (lane PBRR3, 2026-09-24 12:16)

## HANDOFF text

**PBRR3 (R3 BRDF + v6 specular; PBR display ON) LANDED UNCOMMITTED 2026-09-24 12:16, exe db5ccaf4** (rung release/before_pbrr3 = b6c79569).

What changed in the renderer:
- res/shaders/pbrm_default.frag now uses the FO4CS wave 89 BRDF. Every channel is resolved once into one surface (`evalSurface`) before any lighting.
  - Dielectric F0 = ((ior-1)/(ior+1))^2, multiplied by the specular colour.
  - The specular weight scales the whole dielectric Fresnel, including the grazing value (the OpenPBR rule). So weight 0 removes the lobe.
  - Metals use the editor's F82, with the specular colour as the edge tint.
  - GGX with alpha = r^2, and height-correlated Smith visibility.
  - Lazarov analytic DFG plus multiscatter.
  - Q6 energy split: the indirect diffuse is multiplied by (1 - E_spec).
  - Burley diffuse times (1 - F). EON replaces it when diffuseRoughness > 0.
  - Normal decode is (s*255-128)/127 with Z rebuilt.
  - Back faces flip the geometric normal before the tangent frame is applied.
  - Roughness floor 0.035.
- The v6 bits reach the shader:
  - bit 7 is the WEIGHT map on v6 (the F0 map on v4/v5);
  - bit 25 is the spec-colour RGB, a new SpecColorMap sampler with a white fallback;
  - bit 30 is the IOR, A x iorMax.
- src/io/pbrmfile reads `diffuseRoughness` from the base-colour values (the struct gained a field; every includer's .o was rebuilt and checked).
- Vertex colours now tint a PBR shape only when the shader has SLSF2_Vertex_Colors (Q14).
- Census meaning: `f0=` is now the untinted F0 of the IOR at weight 1. It is unchanged for v4/v5, and equals the old value for v6 at weight 1.

**Q9 FLIPPED:** the PBR display default is now **Legacy and PBR**, in the Shading menu's Material Workflow group (src/nifskope_ui.cpp ~27416, src/gl/glproperty.cpp:1091). A shape a .pbrm serves renders PBR on every launch. Every other shape stays spec/gloss. **Legacy** and **PBR** stay selectable in the same group. As in R1, a stored choice is not restored at launch. `WW_PBRM_MODE` still pins it for harnesses. Headless paths only change for shapes that have a .pbrm, so vanilla lodgen and the zero set are untouched.

New pins:
- `WW_STUDIO_SUN=<scale>` (0 = the white furnace);
- `WW_R3_TERM=diffuse|specular` (isolates one term; emission off);
- `WW_R3_RED=noms|nosplit|fo4csweight|notint`.

Gates. The driver is tests/spells/pbr_r3_gates.sh plus .py; fixtures come from pbr_r3_fixtures.py, which writes tests/fixtures/pbr_r3_data. The sphere is the vanilla preview sphere under an orthographic camera.
- furnace PASS: metal at roughness 0.1, 0.5 and 1.0, and the F0 0.04 dielectric, all read 0.9989 at the centre and at 60 degrees. Red noms BITES (metal at roughness 1 reads 0.452). Red nosplit BITES (dielectric 1.029 / 1.051).
- twins PASS: v5 f0 0.04 vs v6 weight 1 IOR 1.5, max |d| 0 over 280464 lit px. Red f0law BITES.
- s1 PASS: at weight 0 the full picture equals the diffuse-only picture (max |d| 0); at weight 1 they differ by up to 32. Red fo4csweight (the runtime's F0-only weight) BITES.
- s2 PASS: F0 read back 0.040 / 0.111. Specular-only centre ratio 2.745 (law 2.748, target 2.778 ±3%). Red f0law BITES.
- s3 PASS: specular centre 0.296 / 0.027 / 0.027 (red); diffuse centre 0.768 / 0.799 / 0.799 (not red). Red notint BITES.
- q9 PASS: with no mode pin, the census reads mode both and the sphere uses pbrm_default.prog. Red q9legacy BITES.
- zero PASS: the pbr_shade_ab set vs before_pbrr3, 10 cases, 0 failures. Three particle cases are empty in this viewer, as before.
- Neighbours PASS: R1 48/48, R2a, R2b (8 gates).

Side jobs:
- (1) The DALC direction is MEASURED RIGHT. At 1.10.155, BSShaderManager::SetDirectionalAmbientColors (RVA 0x27D64D0) builds amb = avg6 + 0.5 x [(X- - X+) n.x + (Y- - Y+) n.y + (Z- - Z+) n.z] in gamma space, then applies pow 2.2. Sky::SetDirectionalAmbientBlend (RVA 0x652F30) copies each colour to its own slot. So an up-facing normal takes Z-, and red dalcflip stays the wrong one.
- (2) docs/CLI.md has its `weather` section.
- (3) pbr_r2a_gates.sh makes `--out` absolute. Verified: a relative `--out` run wrote its 11 pictures.

OWED:
- (1) **Editor-match gate.** The Material Editor has no .pbrm shot mode with a set camera (only `--native-shot` and the graph `--shot`), so NifSkope-vs-editor pixels are unmeasured.
- (2) **FO4CS divergences for bungo to rule on.** FO4CS should follow, or NifSkope should.
  - (a) weight < 1: OpenPBR scales F90 too, FO4CS scales F0 only. They differ at grazing angles, and FO4CS fails s1.
  - (b) Metals: F82 plus the specular-colour edge tint, and env B x tint. FO4CS uses plain Schlick on albedo (f4fx_specular_tint.hlsli:32-35).
  - (c) Roughness floor: 0.035 (design doc) vs FO4CS 0.045.
  - (d) FO4CS widens the roughness for direct light by the emitter size; NifSkope does not.
  - (e) At diffuseRoughness 0 the diffuse is Burley (FO4CS). The editor uses Lambert.
- (3) The DALC weighting: the shader's n^2 pick is not the engine's linear-in-n, gamma-space blend. The direction is right.
- (4) EON is on direct light only; the ambient keeps the irradiance x albedo.
- (5) `globalNormalStrength` is not parsed.
- (6) The EON picture at EV 0 reads flat white. That is the EON bright edge under a sun near the view, not a fault (ambient alone reads 0.11), but it is a poor test picture. Show it at a lower EV.
- (7) The metal white furnace does not depend on the view angle in this DFG (A+B = 1 - 0.55 r), so only the dielectric exercises the 60-degree ring.

## WW_CHANGES text

### Materials: physically based shading for .pbrm materials (stage R3)
- Materials with a .pbrm now use the same lighting model as the game's FO4CS PBR runtime:
  - GGX highlights with multiscatter energy compensation;
  - an analytic split-sum environment term;
  - diffuse light that gives up exactly the energy the reflection takes.
  Under a uniform white sky, a white metal and a plain dielectric both read 1.00 at every angle.
- The v6 specular controls now drive the picture:
  - **Specular weight** (constant or map): 0 removes the reflection completely.
  - **Specular colour** (constant or map): tints a dielectric's reflection and a metal's edge. The diffuse keeps its own colour.
  - **Specular IOR** (constant, or the map's alpha over its maximum): IOR 1.5 gives F0 0.040, IOR 2.0 gives 0.111.
- **Diffuse roughness** (v6 base colour `diffuseRoughness`) switches the diffuse to EON (OpenPBR's rough diffuse) for direct light.
- Normal maps decode as the game does, (value x 255 - 128) / 127. Back faces light from the correct side.
- Vertex colours tint a PBR shape only when its shader has the Vertex Colors flag set.
- **The PBR display is now ON by default.** The Shading menu's Material Workflow starts in **Legacy and PBR**: any shape with a .pbrm renders PBR, and every other shape keeps the legacy look. **Legacy** is still one click away in the same menu.
- docs/CLI.md documents the `weather` command.
- Tests: tests/spells/pbr_r3_gates.sh (white furnace, v5/v6 twins, specular weight/IOR/colour, and the display default, each with a red control). pbr_r2a_gates.sh now accepts a relative output folder.

## MISTAKES text

- **2026-09-24 PBRR3: the judge's disk mask assumed the Studio probe is the final value.** WW_STUDIO_PROBE replaces the lit colour BEFORE the exposure, so 0.5 at EV log2(0.8) reads sRGB 170, not 188. Every disk gate failed with "mask ABSENT". Fixed by computing the probe value times 2^EV. Rule: a mask colour is derived from the whole output chain, never typed from the pin alone.
- **2026-09-24 PBRR3: a numpy bool is never `is False`.** The red-control summary tested `results[g] is False` and printed "absent -> BROKEN" for three reds whose gates HAD failed. Fixed with `bool()` in `verdict()`. Rule: coerce numpy results to Python bools before any identity test.
- **2026-09-24 PBRR3: `nifskope-cli set -f Name -v <string>` cannot set a string-table Name** ("cannot parse ... as string"). The fixture patches the header string table in Python instead (pbr_r3_fixtures.py `nif_set_shader_name`).
- **2026-09-24 PBRR3: a large Python patch sent through a bash heredoc died on quoting** ("unexpected EOF"). This was already a skill rule, and was broken once more. Patch scripts go through the Write tool into the scratchpad.
- **2026-09-24 PBRR3: progress.md got guessed timestamps** ("12:0x", "12:2x"), corrected after reading the clock. Rule unchanged: run `date` in the same turn.

## Skill review

- **nifskope-ww-pbr-shade-ab** (used for the zero set, the R1 neighbour run, and the house pattern for the R3 driver and judge). UPDATED in place (.claude/skills/nifskope-ww-pbr-shade-ab/SKILL.md, LF):
  - The zero-set census note now says the display default is Legacy and PBR since R3, so harnesses that need Legacy must pin WW_PBRM_MODE.
  - A new section "2b. The R3 gates" covers:
    - the commands and the six reds;
    - the orthographic 60-degree geometry;
    - the furnace recipe (white cube, sun 0, EV log2(0.8), so 1.0 lands at sRGB 231);
    - the term isolation pins;
    - the probe-before-exposure trap;
    - the view-independent metal furnace;
    - the numpy-bool trap.
  - Worked as written: an absolute `--out`, the rung FOLDER, the resource-stack rule for the loose fixture root.
- **nifskope-ww-render-shot** (used for every shot: WW_RENDER_ORTHO / VIEW / CENTER / SIZE, absolute WW_* paths, the census pins). It worked as written, with no change needed. One candidate line for the owner: WW_STUDIO_PROBE is applied before the exposure (the same trap as above), if the skill documents Studio pins.
- Not added to E:\Tools\AISkills: these are repo harness procedures, not FO4 modding knowledge.
