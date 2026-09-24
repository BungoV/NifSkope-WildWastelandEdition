# IMPOSTORLIGHT1 -- text for the overseer to splice (the lane edited none of these files)

## WW_CHANGES.md

**Impostor cards now light like the mesh (lane IMPOSTORLIGHT1, 2026-09-22).**
`res/shaders/impostor_oct.frag` lit a MODEL-space card normal with a
VIEW-space light, `abs( dot( normal, normalize( L ) ) )`. That left the card
lit by the object's model axes, about half as bright as the mesh, and dark
where the mesh is lit.

The normal is now taken to view space first (`nView = mat3(modelViewMatrix) *
normal`), and the card uses the mesh's own diffuse rule:
- light 0, Oren-Nayar times (1 - Fresnel 0.04), one-sided, as in
  `fo4_default.frag`:404/422, whose bake stores the viewer-flipped normal;
- roughness 1, because the sheet's gloss stays unshaded by the 2026-09-19
  ruling.

There is no rebake and no sheet or `.lodm` change.

Card/mesh brightness, before → after:

| subject | before | after |
|---|---|---|
| blast_n4 | 0.475 | 0.953 |
| blast_n8 | 0.481 | 0.954 |
| maple | 0.541 | 0.966 |
| dead | 0.513 | 0.984 |
| rock | 0.877 | 0.967 |

Normal sign agreement went from 40..51 % to 80..99 % on x/y, and the mean
angle from 69..88 deg to 5..22 deg.

Other changes in this lane:
- Debug channel 13 = the lighting normal in view space.
- Harness `src/impostorpreviewtest.cpp` (orbit mode):
  - `WW_IMPOSTOR_MESH_CHANNEL=<n>` sends the MESH half through a LOD channel;
  - `WW_IMPOSTOR_LIGHT=decl,planar` gives a world-fixed side light.
- `tests/spells/impostor_draw.sh`:
  - step 16, rows 16/16a-e (normal sign, brightness, transfer relative to the
    colour sheet, and two instrument floors);
  - `IMPOSTOR_EXE=` to run a rung.
- New `tests/spells/impostor_light_check.py`.

The exe is `release/NifSkope.exe`, 2026-09-22 23:32:40, 23,813,120 B, sha1
89574e81c276621ad4f731507c964725d9d08932. bungo restarted his window at 23:50:13 on this exe
(pid 25584, running release/NifSkope.exe itself: the next build renames it aside).

## HANDOFF.md (top block)

IMPOSTORLIGHT1 is PARTIAL, 2026-09-23 00:1x. The fix is the shader's normal
space (see WW_CHANGES).

Gates on the new exe:
- `impostor_draw.sh`: 30 steps, 0 failures.
  - Row 5 is 0.5082, against the rung's 0.4937 at the same fixture and views.
  - The shortfall was dark pixels read as background: the rung's IoU with the
    true card silhouette is 0.5082, and 3.14 % of card pixels were dropped
    (`scratchpad/impostorlight1_20260922/row5.sh`).
- The same spell on the rung (`IMPOSTOR_EXE=` bf6aa749 plus its shaders) gives
  6 failures: row 5, 16a, 16b, 16c, 16d and 16e.
- `lodgen_octahedral.sh`: PASS.
- `native_lighting.sh`: 21/2 on both the new exe and the rung. The 2 are
  legacy_btr baselines, with frames byte-identical between the two exes. This
  is pre-existing baseline drift, not this lane.

Owed:
1. **The gloss ruling.** On the glossy cube control the card is compressed:
   card/mesh 1.18 at N·L 0.4 down to 0.87 at N·L 1.0. With the sheet's gloss
   it is 0.89..1.02.
2. **The top decile on blast.** The pre-registered "every decile rising" is
   not met at blast_n4/n8's top decile (129 → 129). The cause is the colour
   sheet: mesh albedo 156 against card albedo 110 there.
3. **An unexplained render drift.** The blasted maple's MESH render changed
   between 23:2x and 23:4x with no source file changed. Its colour-sheet rho
   fell from 0.467 to 0.065, while the rock did not move. A persisted setting
   is suspected but not found. bungo's window closed and reopened at 23:50:13,
   which rewrites QSettings, so that is the first suspect. This is the same cause as the gate fixture's
   "flat" colour sheet.

## MISTAKES.md (newest at top)

- **2026-09-22 IMPOSTORLIGHT1: a transfer bar written after seeing the
  numbers.** The lane replaced its own pre-registered relative bar ("lit rho ≥
  colour-sheet rho − 0.10") with an absolute one (rho ≥ 0.30, rise ≥ 10) fitted
  to the res512 results. It then failed on the gate fixture, whose colour sheet
  reads rho 0.071. Restored to the pre-registered form. **Rule:** the bar in
  the gate is the bar in progress.md from before the fix, word for word.
- **2026-09-22 IMPOSTORLIGHT1: a relative OUTDIR.** It was handed to a harness
  script and wrote nothing, with no error, and `cygpath -m` of a relative path
  stays relative. **Rule:** every WW_* output path is built from an absolute
  root.
- **2026-09-22 IMPOSTORLIGHT1: a hazard in `tools/ww_build.sh`.** It renames
  `release/NifSkope.exe` to `release/NifSkope_inuse_<pid>.exe`, and that name
  can already be taken by bungo's running window from an earlier rename. The
  lane built with make directly, because release/NifSkope.exe was not held. The
  script should refuse when the target name exists.
- **2026-09-23 IMPOSTORLIGHT1: the known-answer cube control ran LAST.** It
  should have run first. It exposed the roughness-constant compression after
  the constant was chosen. **Rule:** cube first, then subjects.
- **2026-09-23 IMPOSTORLIGHT1: numbers compared across sessions.** A tree mesh
  render drifted between two runs 20 minutes apart. **Rule:** compare shading
  candidates only inside one session.

## Skill review

- **Loaded:**
  - ww-test-harness-add
  - nifskope-ww-lodgen
  - ww-channel-view-refuter
  - ww-reference-card-diagnose
  - nifskope-ww-render-shot
- **Written:** ww-reference-card-diagnose section 13, "The LIGHTING arm":
  - both normals in one space;
  - shader-folder A/B;
  - the background rule eating dark pixels;
  - the transfer bar relative to the colour sheet;
  - cube per-face grouping;
  - same-session comparisons.
- **Missing:** a skill for "what persisted NifSkope setting changes a harness
  mesh render". It could not be written, because the cause was not found.
