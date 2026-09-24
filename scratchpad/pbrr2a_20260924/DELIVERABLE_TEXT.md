## HANDOFF text

### PBRR2A -- Scene window + Studio mode, exposure, view transforms (2026-09-24 06:15) -- DONE, NOT COMMITTED

The exe is release/NifSkope.exe, sha1 b985beac5e4af5a7385ff2312cf16cda5296e6d7. The rung is release/before_pbrr2a (exe 4d30baa9, PBRLODFIX1).
Only this lane has changed its files, but the shared tree holds many other uncommitted changes. Commit by explicit path list:
- NifSkope.pro
- src/gl/scenelighting.h and src/gl/scenelighting.cpp (new)
- src/ui/scenewindow.h and src/ui/scenewindow.cpp (new)
- src/scenetest.cpp (new)
- src/gl/gltex.h
- src/gl/gltexloaders.cpp
- src/gl/renderer.cpp
- src/gl/glproperty.cpp
- src/nifskope_ui.cpp
- res/shaders/pbrm_default.frag
- tests/spells/pbr_r2a_fixtures.py, tests/spells/pbr_r2a_gates.sh and tests/spells/pbr_r2a_gates.py (new)
- tests/fixtures/pbr_r2a_data (generated; the spell regenerates it)

**What landed**

The Scene window:
- One non-modal Qt::Tool window, owned by the main window, so it stays above NifSkope only. It moves to any monitor.
- It opens from View > Scene and from a "Scene" button on the viewport toolbar.
- It remembers its geometry (key Settings/Scene Window/Geometry).
- It is a flat Name|Value tree in the skinVars palette, label and control only.
- Sections are Mode / Weather / Sky / Ground / Fog / Effects.
- Mode rows are live:
  - Lighting (Legacy/Studio);
  - Exposure (EV, -10..+10);
  - View Transform (Standard / AgX / Khronos PBR Neutral; the default is PBR Neutral);
  - PBR Route View.
- Exposure and View Transform are greyed in Legacy.
- The rows of the other sections are present but disabled until their stage fills them.

Settings and defaults:
- Exposure and View Transform are remembered (Settings/Render/Scene/...).
- The Lighting mode is never remembered: it always starts Legacy (Q7).
- The PBR display default stays Legacy (Q9). Studio only shows on shapes that the PBR program draws.

**PBR Route View came from the RENDER menu** (Render > PBR Route View). It is now a row of the Scene window. The Render-menu entry has been removed.

The light angles are NOT in the Scene window. They already have live controls in the Lighting panel (LIGHTANGLES1), and the sun row belongs to R2b.

The PBR program:
- Studio is linear throughout:
  - the sun is in linear units (E = lightSourceDiffuse * pi);
  - exposure scales by 2^EV before the view transform;
  - the view transform is then applied, then the sRGB encode, which is done in the shader.
- Base and emissive are decoded as sRGB whatever the DXGI tag. An sRGB-tagged texture is sampled with the hardware decode skipped (GL_EXT_texture_sRGB_decode), so both tags take one path and render byte-identically.
  - The skip is undone at the start of the next shape's program setup, because the legacy programs share the texture object.
  - Without that extension, the sampler decodes and the uniform baseIsSrgbTex covers it.
- Legacy mode on the PBR program:
  - it computes in linear with the lights un-squared;
  - it encodes with the legacy Hable plus sqrt (design doc s5.2).
  - This changes the look of PBR-served shapes under Legacy lighting only. Nothing that the legacy programs draw has moved (the zero set).

The Studio cube:
- ONE scene-wide cube: FO4's default textures/shared/cubemaps/mipblur_defaultoutside1.dds, until R2b. It can be overridden with WW_STUDIO_CUBE=<data path>.
- It goes through the SFCubeMapCache prefilter (m = r*(10-4r)) plus a 32 px irradiance cube, and is sampled on its own two units.
- In Legacy mode, both units hold the grey 1x1 cube.

Divergence from design doc s4: the new inputs are per-program uniforms of pbrm_default (sceneMode, sceneExposure, viewTransform, baseIsSrgbTex, emissiveIsSrgbTex, hasStudioCube, StudioCube, IrradianceMap, studioProbe). They do not use unusedUniform1/2, and the legacy programs are untouched.

**L1 finding (vanilla, no fix in this lane)**

mipblur_DefaultOutside1.dds has a legacy header:
- BGRA8, dwCaps 0x40FE08, dwCaps2 = 0: the cube bits are in dwCaps, not dwCaps2.

The legacy loader's cube test reads byte 113 (dwCaps2). So does gli (CubemapFlags = caps2). Both therefore see the file as 2D. In the harness report, legacy bindCube returned true with cube binding 0 and 2D binding 6. The legacy path binds FO4's default env cube as a 2D texture, and the cube unit stays empty.

The Studio loader normalises this header: cube bits taken from dwCaps or dwCaps2, then rewritten as a DX10 _SRGB format.

A later lane should decide whether the legacy path gets the same normalisation. It would move legacy pixels, so it needs a ruling.

**Harness backstop change (src/nifskope_ui.cpp, NifSkope::wwPlaceHeadlessWindow)**

The backstop forced every top-level window shown during a WW_* run to WW_WINDOW_AT plus WW_RENDER_SIZE. As a result, the "reopens where it was" gate measured the backstop instead of the window.

A window that sets the property wwOwnGeometry now keeps its geometry, but only when that geometry lies wholly on a non-primary screen. Opacity 0 and no-activate still apply. Only the Scene window sets the property.

**Gates (exe b985beac; runs in scratchpad/pbrr2a_20260924/)**

| Gate | Result | Red |
|---|---|---|
| Zero set, pbr_shade_ab vs before_pbrr2a | 10/10 PASS (3 empty-by-viewer as standing) | red shader BITES (4 of 4) |
| ev: EV+1 doubles the linear value (Standard) | median 2.0049, 99.7% inside 1.9..2.1, 316063 samples | ev BITES (1.4147) |
| grey: linear 0.5 on screen | 99.25% at 188 +-1, mode 188 | sqrt encode BITES (mode 180) |
| srgbtag: sRGB twin vs UNORM twin | max\|d\| 0, under Studio and under Legacy; flat base differs by a mean of 34.3 | srgbtag BITES (max 67) |
| cube: uniform cube L=0.50289 | prefiltered 10 levels and irradiance 32 px, both RGB9_E5, max\|v-L\| 0.00102 (<= 1/255); the vanilla cube loads with its picture | cubedecode BITES (0.235) |
| window (WW_SCENE_TEST) | 17 checks PASS: menu+toolbar, moved route view, Tool/non-modal, live Mode/EV/View, close from the menu and reopen from the button at the same geometry | nolive BITES |
| restart | geometry 2500,200,420,600, EV 2 and View Standard came back; the mode starts Legacy | nosave BITES |
| Neighbour pbr_r1_gates | 48/48 PASS | |

The zero set's first run had one no-picture launch (bgsm_duct old_a, the rung's first launch). The case was re-run with --only and passed with px=0. gates1's ev0 had the same no-picture timeout.

**Machine state**

E: is full: 99 MB free at 06:1x. This lane holds about 115 MB (the rung plus scratch). The zero-set red run failed to write 2 logs, but its verdict stands. No further build is possible until space is freed.

There are four release/NifSkope_inuse_*.exe files from earlier lanes. They were left alone.

**Next**

R2b: the Weather/Sky/Sun rows, a per-scene cube instead of the one default cube, and a sun row in the Scene window.

## WW_CHANGES text

### Scene window: Studio lighting, exposure and view transforms for PBR materials (lane PBRR2A, 2026-09-24)

- **New Scene window.** Open it from View > Scene or from the Scene button on the viewport toolbar.
  - It is a separate window that you can move to any monitor, and it stays above NifSkope.
  - It remembers its size and position.
  - Its sections are Mode, Weather, Sky, Ground, Fog and Effects. Only Mode is active in this release; the other rows arrive in later stages.
- **Lighting: Legacy / Studio.**
  - Legacy is the look NifSkope has always had, and it is the mode every session starts in.
  - Studio lights PBR materials physically: linear light, FO4's default outdoor reflection cube (prefiltered the way the game does it), and exposure plus a view transform.
- **Exposure (EV)** and **View Transform** (Standard, AgX, Khronos PBR Neutral) act in Studio and are remembered between sessions.
- **PBR Route View** moved from the Render menu into the Scene window.
- PBR textures are read as sRGB colour whether their DDS is tagged _SRGB or not, so a texture re-saved with the other tag looks identical.
- Nothing drawn by the classic FO4 shaders has changed (checked pixel for pixel).

## MISTAKES text

### 2026-09-24 PBRR2A

- **Typed a progress timestamp instead of reading the clock.**
  - What happened: a progress.md entry said 04:50 when `date` said 04:42.
  - Fix: corrected in place.
  - Rule, as standing: run `date` in the same turn as every stamp.
- **Wrapped tools/ww_build.sh in `timeout 590` while the link took longer.**
  - What happened: the link took over 10 minutes because other builds were running on the machine. The timeout killed the script, but not its MSYS2 child. BUILD-RC=0 arrived from the orphan, and the exe-newer and copies-in-step gates never ran.
  - Fix: I re-ran ww_build.sh as a no-op, which printed them.
  - Rule: never wrap ww_build.sh in a timeout shorter than a link. Run it in the background and wait on its own output.
- **Edited NifSkope.pro through a bash heredoc with Python byte literals holding backslashes.**
  - What happened: the heredoc mangled the escapes, and the replacement assertion failed on bytes that were really there.
  - Fix: the house rule, a script file written with the Write tool. It applied at once.
- **Wrote the Scene window's geometry gate without checking what else moves top-level windows in a WW_* run.**
  - What happened: the headless backstop (NifSkope::wwPlaceHeadlessWindow) re-placed the window on every show, so the gate first measured the backstop.
  - Rule: before gating a window's geometry, grep for code that places windows on Show in the harness path.
- **Accepted "3/255 is filtering noise" as the first reading of the srgbtag gate.**
  - What happened: the brief says "render identically". The difference was a real path difference (hardware decode before the filter vs shader decode after it), and it was removable.
  - Fix: skip the hardware decode.
  - Rule: when a gate says identical and the reading is small but non-zero, find the mechanism before loosening the bar.

## Skill review

- **nifskope-ww-build-verify:** add that the link can take over 10 minutes when other builds share the machine. Never put `timeout N` around ww_build.sh; run it in the background and wait on its printed "exe newer than the sources" line, not on BUILD-RC. A BUILD-RC without the two follow-up lines means the script was killed.
- **ww-test-harness-add:** add that in any WW_* run, NifSkope::wwPlaceHeadlessWindow re-places every top-level window on Show (WW_WINDOW_AT plus WW_RENDER_SIZE). A tool window whose own geometry is under test sets the property wwOwnGeometry. It then keeps its geometry when that geometry lies wholly on a non-primary screen.
- **nifskope-ww-pbr-shade-ab:**
  - A "no picture" FAIL is a launch that timed out, not a pixel verdict. Re-run that case with `--only <case>` before reading it as a regression. It has been seen on the first launch of a freshly placed exe.
  - Check `df -h /e` before the run, because a full disk truncates logs mid-run.
- **ww-analytic-fixture-gate** (pattern used here, no edit needed): a constant-colour input through the prefilter, with every texel of every level read back, plus a FO4-quirk header as the input so the normaliser is exercised too.
- A new skill is not warranted. pbr_r2a_gates.sh follows the pbr_r1_gates.sh shape: a shot or harness per leg, reds by pin or by a sabotaged release copy, and the judge prints "RED CONTROL ... BITES".
