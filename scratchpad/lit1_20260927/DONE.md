# LIT1 -- the far-LOD view drawn with real lighting (2026-09-27)

Worktree `E:\Projects\NifskopeWWE-lit1`, branch `lit1-20260927`, from NifSkope main `422881d4`.
The brief names `E:\Projects\ClaudeNifskope`, which does not exist. The constitution and handoff it
means are `E:\Projects\NifskopeWildWastelandEdition\CONSTITUTION.md` and `HANDOFF.md` (top block), both read.

## 1. Skills loaded

- `search-lean` (user skill): every search is scoped to `src\`, `res\shaders\` or one named file.
- `nifskope-ww-render-shot`: the built-document recipe, the headlight fact, `WW_PROGRAM_CENSUS`, the width floor, and the rule that the file must be on the command line.
- `nifskope-ww-build-verify`: the worktree build, checking the exe on disk, and the run-folder copy.
- `nifskope-ww-lodgen`: the `.lodl`/`.lodi`/`.lodo` environment switches.
- `nifskope-ww-pbr-shade-ab`: the PBR BRDF (`pbrm_default.frag`) is the one to reuse.
- `ww-module-off-is-identical`: the OFF gate is `cmp`, not a threshold.
- `ww-toggle-lit-gate`: a lighting toggle is proved by a picture pair plus an invariant.
- `ww-channel-view-refuter`: each refuter must fail on broken code, shown red.
- `nifskope-ww-vanilla-compare`: camera pinning, reused from AO2's `pics_tower.sh`.
- `ww-texel-picture`: the whole-map picture pattern.

## 2. What existed (measured at `422881d4`)

| Input | Drawn today? | Where |
|---|---|---|
| Terrain normal sheet (`_msn`, role 2) | YES. Bound in slot 1 with SF1 bit 12 (model space). SF1 bit 0 (specular) is CLEARED and SF2 bit 1 (LOD landscape) is set | `src/btdterrain.cpp:386-406` |
| ... and how it is lit | Headlight: `lightSourcePosition[0] = (0,0,1)` in view space, so the light moves with the camera. Squared half-Lambert for msn | `res/shaders/fo4_default.frag:385-411`, `:421-429`; `src/glview.h` `frontalLight` |
| Terrain mask sheet (role 5: R rough, G metal, B sky AO, A cover) | NOT bound for shading. It is read only as a per-vertex grey for `WW_LODL_AO` / `mask-*` channel views | `src/btdterrain.cpp:1585-1832`; `src/lodtsheets.cpp:334` (`maskAo` = `sheetChannel(MASK, B)`) |
| Terrain emissive (role 6) | Not in these bakes: "the container carries no sheet with role 6" (MAPS1). Unpacked only for `WW_LODL_CHANNEL=emissive` | `src/lodtsheets.cpp:545-550` |
| Object per-vertex AO (`.lodi` v6 stream) | Only in the AO view (`WW_LODL_AO=1` / channel `ao`). The default view writes chan = 1.0 | `src/lodinative.cpp:898-921`, `:1068-1079` |
| Object sky visibility (`.lodi` v7 per-vertex, else per-placement byte) | Only in the `sky` channel view | `src/lodinative.cpp:600-601`, `:923-951`, `:1060-1067` |
| Object LOD normal maps + specular (`_n`, `_s`, or the BGSM) | YES. Slot 1 `_n` and slot 7 `_s` siblings, or the BGSM in the shader Name. Lit by `fo4_default.frag`: tangent normal, GGX with F0 0.04, `_s` R = mask and G = gloss. Headlight | `src/lodinative.cpp:341-351`; `res/shaders/fo4_default.frag:456-484` |
| Object emissive | From the BGSM, with Emissive Multiple when the `.lodo` material has the EMITS bit | `src/lodinative.cpp:352-353` |
| Card normal/depth arrays (`LodgenArrays *_n`, `.lodo` cardCount 76/79) | NOT used by the native view. `cardLayer` is never read in `lodinative.cpp`, and `nb.layer` only fills UV2.y (0 of 204 materials carry a layer). Trees are drawn as the `.lodo` tree MESHES with the vanilla tree-LOD BGSMs (37 tree materials). The only card drawer is `ImpostorChunk` (`.BTO` manifests, master OFF) | `src/lodinative.cpp:1013`, `:1048`; `src/gl/impostorchunk.h`; `glview.cpp:3959` |
| The PBR BRDF | `res/shaders/pbrm_default.frag`: `Surface`, `directLight` (GGX, height-correlated Smith, Burley/EON), `dfgLazarov`, `multiScatter`, `specAlbedo`, `surfaceF`, `srgbToLinear`, `tonemap`. It is selected only by the PBR lighting modes, by name (`src/gl/renderer.cpp:277-291`) | -- |
| Variant-program pattern | `fo4_fog.prog` / `pbrm_csm.prog`: `#version` + `#define` + `#include` of the whole shader, swapped by name. An included file loses its `#version` (`src/gl/glcontext.cpp:287-293`) | `src/gl/renderer.cpp:333-349` |

In short: today the far field is lit by a headlight with a fixed ambient. The terrain uses its normal sheet but
not its roughness, metal or AO sheet (the specular bit is off). Objects use their `_n`/`_s` but not the baked
AO or sky visibility. The native view draws no cards at all: trees are meshes.

## 3 What was built (uncommitted until the gates pass; see §6)

**The switch.** `WW_LODL_LIT=1` (environment, like `WW_LODL_AO` / `WW_LODL_CHANNEL`; no menu row). Unset = today's
view, and nothing in the built document or the chosen programs changes. It is refused (and says so in the notes)
while `WW_LODL_AO` or `WW_LODL_CHANNEL` is set: a data view wins. `src/gl/lodlit.h`.

**The light (fixed constants, written to the notes of every lit document):**
- one sun: elevation 35 deg, azimuth 225 deg (from the south-west; the to-sun vector points SW and up), intensity
  3.0, colour (1.00, 0.95, 0.88);
- one sky: zenith (0.34, 0.42, 0.55), nadir (0.12, 0.11, 0.10) linear, mixed by the world normal's up component;
- output through pbrm_default.frag's legacy tonemap, as its Legacy branch does.

**The program.** `res/shaders/lod_lit.prog` = `fo4_default.vert` + `lod_lit.frag`. `lod_lit.frag` includes
`pbrm_default.frag` whole with its `main` renamed out of the way and uses its `Surface`, `directLight`,
`dfgLazarov`, `multiScatter` and `specAlbedo`: the PBR BRDF, no second one. `pbrm_default.frag` is not edited.
Chosen in `Renderer::setupProgram` only for shapes whose shader carries SLSF2 LOD_Landscape or LOD_Objects, only
when the mode is on and no channel view is active (`src/gl/renderer.cpp`).

**Per surface:**
| surface | normal | roughness / metal / F0 | ambient visibility |
|---|---|---|---|
| terrain | the `_msn` sheet (R east, B north, G up), world axes | mask sheet R / G, F0 0.04 | mask sheet B (sky AO, with the objects' occlusion when baked) |
| LOD objects | the `_n` tangent map | `_s` G x glossiness, `_s` R x specular strength = weight, F0 0.04 x specular colour | `.lodi` per-vertex AO x per-vertex sky, in the vertex alpha |
| trees | the tree meshes' own `_n` (the native view draws no cards, §2) | as objects | as objects |

- The terrain's mask sheet is unpacked only in this mode (`LodtSheets::setUnpackMask`) and bound to slot 7
  (`src/btdterrain.cpp`).
- Object visibility: v6 per-vertex AO (else self-AO x placement AO) times v7 per-vertex sky (else the placement's
  sky byte), written to the vertex ALPHA of buckets whose alpha is not opacity; the RGB stays the library colour.
  VERTEX_ALPHA buckets keep their opacity and get visibility 1 (counted in the notes) (`src/lodinative.cpp`).
- The visibility darkens the SKY term only, never the sun.
- Cards stay crisp: alpha test as `fo4_default`, no dithering, no blend change.
- Known overlap: the v6 AO and the v7 sky are both hemisphere visibilities cast in the same scene, so their
  product counts some occlusion twice. Flagged, not tuned.

**Isolation and refuters** (`WW_LODL_LIT_TERM`, `WW_LODL_LIT_RED`): term = all | sun | sky | spec; red =
flipnorth (negate the sheet's north axis), flipup, nospec (specular weight and metal 0), sunmirror (mirror the sun
north-south).

## 4 Gates (numbers; pictures in scratchpad/lit1_20260927/pics/, gates.py in the session scratch lit1/)
Exe: worktree build of ebcc7bc3 (sha1 56e13053), run from a scratch run folder. Baseline: main's exe of
3bc55877 = 422881d4's code (sha1 bb1ae6af). Same run-folder files otherwise. Camera = the 08 camera (view 8,
ortho 16384, 1600x1600 asked, 1600x1624 grabbed), cells -5 -10 2 -3, terrain = AO2's Boston VT sheets + the
installed .lodl copy (ao2/terr_x1), objects = AO2's latest Boston bake (reg_x7).

| gate | result |
|---|---|
| off = today's picture: new exe, `WW_LODL_AO=1`, vs AO2's `08_roads_AO_decal.png` | PASS: file bytes identical (sha1 8d84f4d2 both), 0 of 2,598,400 pixels differ |
| off, new exe vs baseline exe, AO view | PASS: bytes identical (8d84f4d2) |
| off, new exe vs baseline exe, default view (no AO) | PASS: bytes identical (ed61cd30) |
| two lit runs | PASS: bytes identical (e1e98c83) |
| normal refuter (terrain only, top-down view 1, sun term): negate the `_msn` NORTH axis vs mirror the sun north-south | PASS: mean abs difference flipped vs sun-mirrored 0.00 (corr 1.000); normal vs flipped 21.23 (corr 0.576), over 2,559,999 pixels. Flipping the sheet's north axis is exactly the same picture as moving the sun to the other side: the lit slopes swap sides |
| ... the literal "green" of this sheet | This sheet's G is UP, not north (R east, B north, G up). Negating it turns every face away from the sun: lum mean 1.2 (normal 120.3). Reported, not the swap test |
| spec refuter: `WW_LODL_LIT_RED=nospec`, spec-only term | PASS: 1,319,039 of 1,322,474 content pixels are exactly 0. The other 3,435 are background showing through sub-pixel cracks (colour ratio 0.87:0.91:1 = the background's 0.88:0.92:1; 3,303 of them touch the background). Without nospec: 44,950 zero, mean 31.8 |
| whole map OFF, lane exe vs control exe (older copied build), terrain only | PASS: bytes identical (sha1 afb4ecde), 0 of 4,889,600 pixels differ |
| lit notes | terrain mask bound on 20 of 20 sheet tiles; objects: 213,939 vertices, all with the v6 AO stream and the v7 sky stream; AO mean 0.676, sky mean 0.322, product 0.269; 0 VERTEX_ALPHA vertices |

**Launch outage (not a code fault).** 03:25-03:34 and 03:37-about 04:05, every GUI launch of the fresh exes (lane 56e13053, main bb1ae6af) exited rc 0 in 15-25 s: an empty log, no picture. Heavy and light renders alike, lit and OFF alike. A companion 2 MB process sat beside the real one. Meanwhile an older copied exe (the control, b7247c5f) rendered the same whole-map frame in 8 s. No crash dump, no event-log entry. Consistent with the antivirus acting on unknown binaries. Nothing was altered or renamed: I waited and re-probed, and all remaining renders ran at 04:07-04:15.

## 5 Pictures (`E:\Projects\NifskopeWWE-lit1\scratchpad\lit1_20260927\pics\`, untracked)
| file | what |
|---|---|
| `A_boston_off_vs_lit.png` | Boston, the 08 camera: OFF (today, = `08_roads_AO_decal.png`) beside LIT. AO2's latest Boston bake |
| `C_boston_spec_crop_2x.png` | close crop at 2x pixels (the riverbed/water edge, the bridge, the building faces against the SW sun): LIT, specular only, OFF |
| `D_boston_terms.png` | the terms alone: all, sun only, sky only, specular only |
| `G_normal_refuter.png` | terrain only, top-down, sun only: normal, north axis flipped, sun mirrored, up axis flipped |
| `G_spec_refuter.png` | specular only with the specular zeroed, beside specular only |
| `B_whole_commonwealth_off_vs_lit_half.png` | WHOLE COMMONWEALTH, OFF above LIT, the 08 oblique (view 8, ortho 570000), at 1/2 scale. From the INSTALLED bake, read-only; it PREDATES the roads and the AO fixes |
| `whole/whole_obl_lit_labelled.png`, `whole/whole_obl_lit.png`, `whole/whole_obl_off.png` | the same at full size (3200x1528). Parts: `whole/obl_{lit,off}_{T,A,B}.png` + logs |
Whole map: T = terrain only, A = objects of cells x <= -9, B = x >= -8, one camera, composited (skill `ww-whole-map-picture`). 63,724 + 120,707 = 184,431 placements drawn, 0 dropped. Both halves overlap on 0.46% of object pixels. Lit notes: the mask sheet bound on 144 of 144 tiles; objects 6,325,348 + 6,803,847 vertices, all from the v6/v7 streams; AO mean 0.60/0.61, sky 0.36.
Raw renders beside them: `off_*`, `lit_*`, `refn_*` (+ `.log` / `.cam.log`).

What the pictures show:
- Where the specular lands: mostly the terrain (the wet riverbed at the Charles). The LOD object materials
  give almost none: buildings, rocks and bridges are near black in the specular panel. That comes from their
  `_s`/BGSM values, not the wiring (unmeasured beyond the picture; flagged).
- The native view draws no water surface, so the "water edge" is the dry riverbed terrain against the bank.
- No cast shadows: faces away from the sun are lit by the sky only.

## 6 Commits (branch `lit1-20260927`, not pushed, not merged)
- `ebcc7bc3` LOD view: lit mode (WW_LODL_LIT=1) -- one sun, one sky, the PBR BRDF over the baked maps.
- the report commit: this DONE.md, DELIVERABLE_TEXT.md, and the skill `.claude/skills/ww-lod-lit-view/SKILL.md`
  (explicit paths; pictures stay out of git).

## 7 Skill review
- Loaded: see §1.
- Wished for: a page for the lit view itself: the switches, the msn axis order (G = up, so "flip the green"
  is the wrong refuter), the four gates, and the silent-launch control rule. None existed.
- Written: `.claude/skills/ww-lod-lit-view/SKILL.md` (in this worktree; it lands in main with the branch).
  It is not mirrored into `E:\Tools\AISkills`: that is outside this lane's write scope. Overseer: add it there.
