## HANDOFF text

**CSM1 (2026-09-24): cascaded sun shadows in the PBR renderer. LANDED, bungo has not seen it yet.**
Exe `release/NifSkope.exe` 20:36:11, sha1 `53637ec69b804e34e635ce6f2b371f678ec47a8d`.
Commits: a4df29e, 786d0f5, 0b0ceba, 2dd262e, 488a99a, 47b2cad, 71f96c1.
bungo's open window predates this exe; he must restart it to get shadows.

What was built:
* Three cascades at 800 / 3000 / D, where D is the shadow distance (3000 by default, set by
  `WW_CSM_DISTANCE`). At the default D, the third cascade is empty.
* The spec's split, fit and texel snapping.
* D16 shadow maps, drawn with `glPolygonOffset(6,12)` and back faces culled. Receiver offsets are
  0.275 / 1.0.
* The 16-tap Poisson filter, blended cascade selection (B = 100) and the distance fade.
* The night light follows the sun's arc. The moon is visual only.
* A new **Cascaded Shadows** row in the Scene window, in the "shadows" box. It ships OFF, it is
  saved between sessions, and the harnesses force it. The hour row moves the shadows live.

Gates:
* The gate is `bash tests/spells/pbr_csm1_gates.sh --out <ABS dir> [--red <r>]`, judged by
  `tests/spells/pbr_csm1_gates.py`.
* Result: 25 gates, 25 PASS. All 14 reds end FAIL, each on the gates it targets (flipsun, nofloor,
  nosnap, onecascade, wrongsplit, noblend, nobias, bigbias, nofade, diffonly, factorhalf,
  kernelmut, nolive, nosave).
* With shadows OFF, output is byte-identical to `release/before_csm1`:
  * the PBR fixture: 6/6, 0 px differ;
  * the legacy zero set (`pbr_shade_ab.sh`): 10 cases, 0 failures.
* Regressions all PASS: FOG1 64/0, WX1 71/0, R1 48/0, R2a, R2b, R3 15/0, R4 23/0.

Pictures are in `scratchpad/csm1_20260924/`: `CSM1_pictures.png` and `pics/pic_*`. They show
shadows on and off at 08:00, 12:00 and 16:30. The cascade-colour pictures are red A, green B and
blue C: `pic_cascades` shows A|B and `pic_cascades_far` shows B|C (D=8000).

Divergences from the spec, all deliberate:
* The ground is drawn as a caster.
* B = 100 at both seams.
* No u16 wrap on depth.
* Legacy (non-PBR) shapes cast shadows but do not receive them.
* Alpha-tested shapes cast solid shadows.
* Orthographic views get no shadows.
* The camera's near and far planes follow the scene bounds, so no single framing shows all three
  cascades. At view 8, the 3000 seam is barely in view.
* A face the sun grazes darkens itself under the bias law.
* A wall the sun sees edge-on shows a lit sliver at its base, because the slope bias pushes it
  behind the ground.
* The last two are the law, not defects.

Env switches (diagnostics only, no menu row):
* `WW_CSM_ECHO=<abs path>`: the cascade state of the frame that was grabbed.
* `WW_CSM_PROBE=1..5`.
* `WW_CSM_FORCE=<0..1>`.
* `WW_CSM_RED=<name>`.
* `WW_CSM_DISTANCE`, `WW_CSM_MAP`.

Still owed:
* bungo's eye on the row at morning, noon and evening.
* A concave fixture, so that a gate can see a sun-facing part shadowed by its own model. The duct
  is convex, so the place gate forces the shadow factor instead.

## WW_CHANGES text

### Cascaded sun shadows (lane CSM1, 2026-09-24)
The lookdev sun and the night light now cast shadows in the PBR renderer. There are three
cascades, split at 800, at 3000 and at the shadow distance. They are texel-snapped so they do not
swim as the camera moves. A 16-tap Poisson filter softens them, and they blend across the cascade
seams. A **Cascaded Shadows** row in the Scene window turns them on; it is off by default. The
hour row moves the shadows live. With the row off, the picture is unchanged, byte for byte. Models
and the preview ground cast shadows. Older non-PBR shapes cast shadows but do not receive them.
Orthographic views get none.

## MISTAKES text

**2026-09-24, lane CSM1.**
* **I broke the chain rule four times.**
  * A heredoc wrote the two shader files.
  * A heredoc appended to `progress.md`.
  * `sed` edited a comment in `src/gl/sunshadow.h`.
  * A heredoc Python patch changed the judge.

  I checked every output byte for byte (0 CR), and nothing was corrupted. But the rule is
  Write/Edit only. Fix: patch through the Write/Edit tools, no exceptions.
* **The first foot gate read the wrong camera and failed 9 times on correct code.** It read the
  camera from the PBRM census row. That row is written at a shape's first draw, before the render
  hook pins the camera. Fix: the feature now echoes the grabbed frame itself (`WW_CSM_ECHO`).
* **The judge had three defects of its own, none of them in the renderer.**
  * It first used a hull test with no bias, and called the slope-bias sliver at the base of
    edge-on walls a defect.
  * It then counted ground past the camera's far plane as acne (158k px).
  * It then asked a convex fixture for sun-facing pixels in shadow, which a convex fixture cannot
    have.

  Fix: the judge models the bias law and the far plane, and forces the shadow factor to test the
  places it is applied.
* **`tools/ww_build.sh` twice renamed the lane's own previous exe onto
  `NifSkope_inuse_23560.exe`.** Its lock check matches bungo's pid by the exe's original path.
  His process was not touched, and `release/before_csm1` kept the pre-lane exe. But the file name
  now lies about what the file holds. The script should rename to a unique name, never onto an
  existing one.
* **The first cascade-colour picture (dist 2400) showed only cascade B.** The camera's near plane
  was 864, beyond the 800 split. Fix: two framings, whose near and far planes are read from the
  echo.

## Skill review

* **Loaded:**
  * nifskope-ww-pbr-shade-ab: the zero set, the R gates, the FOG1 conventions.
  * nifskope-ww-build-verify: the build chain, and telling whose NifSkope is running.
  * nifskope-ww-render-shot: the camera pin, absolute paths, the size floor.
  * ww-test-harness-add: the Scene-row harness leg and its floors.
  * search-lean.
* **Wished had existed:** a procedure for judging a shadow-map render against an analytic model:
  where to read the camera that was drawn, how the caster bias moves an edge, and how to frame
  acne shots. I worked all of it out from first principles, three judge defects deep.
* **Written:**
  * `E:\Projects\Claude\.claude\skills\ww-shadow-map-judge\SKILL.md`. It covers echoing the
    grabbed frame, modelling the bias law instead of a hull, convex fixtures and down-sun
    framing, and a red for every gate.
  * Section 2e of the repo skill `nifskope-ww-pbr-shade-ab`: the CSM1 gates, their reds, and
    their traps (commit 47b2cad).
* **Declined:** a skill for the `ww_build.sh` rename quirk. It is a script defect, not a
  procedure. It goes in the MISTAKES text and should be fixed in the script.
