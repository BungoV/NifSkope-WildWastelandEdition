# IMPOSTORDEPTH1 -- text for the overseer to splice (lane edits none of these files)

Lane folder: scratchpad/impostordepth1_20260923/ (progress.md has every number with its time).
Exe: release/NifSkope.exe 2026-09-23 10:27:11, sha1 e294ae80 (make rc 0). Rung: release/NifSkope.before_impostordepth1.exe (8d87c155).
Not committed. Nothing here is flown or confirmed by bungo.

## HANDOFF entry

**IMPOSTORDEPTH1 (2026-09-23): the depth search on the octahedral card. Every new switch is OFF by default. Not flown.**

Files: res/shaders/impostor_oct.frag (and the copy in release/shaders), src/gl/impostordraw.{h,cpp},
src/impostorpreviewtest.cpp, tests/spells/impostor_trunk.sh, tests/spells/impostor_trunkbar.py.

- New switches, all OFF by default. With them off, the default picture is byte-identical to before (the gate row and two dev-loop controls give 0 px).
  - `WW_IMPOSTOR_SEARCH=N` / `Options::depthSearchSteps`: a march of N steps plus 1 refinement per frame.
  - `WW_IMPOSTOR_COVFILTER=1` / `Options::coverageDecodedFilter`: decodes coverage per texel before the bilinear mix.
  - `WW_IMPOSTOR_SNAP=1` / `Options::snap`: the nearest frame, keeping the parallax and the search.
  - The harness log now names the frames mode, the search and the coverage filter. A BLEND=0 run used to leave no trace.
- **Main finding: the shipped `_n` sheet cannot hold the trunk.**
  - The height is DXT5's B channel, which sits in the 5:6:5 colour block. Measured against the bake's own PNG, the mean error is 2.81 levels (34 units) and the 95th percentile is 8 levels (96 units). Only 28 of 59 heights survive.
  - With `_n` written uncompressed, and nothing else changed, stipple + search 16 + coverage filter PASSES the pre-registered trunk bar at el 0. At el 20 it misses by 4 views of 360 (trunk up to 1.18x the mesh near az 113).
  - On the shipped sheets, every shader variant fails.
  - **Owed (a lodgen job, not started):** keep 8 bits of height. Options: height in the DXT5 alpha block (sway would move to the colour block), BC7, or an uncompressed `_n`. The ruling is bungo's; this lane only measured.
- **Second finding: the coverage cut lost about a quarter texel per edge.** This is not the sheet's fault.
  - Hardware bilinear filters the ENCODED alpha, and the encoding jumps from 0 to 160 at the floor.
  - Even from the frame's own angle, the nearest frame's trunk measured 0.87x the mesh (median), with 84 of 360 views outside the bar.
  - Decoding first gives 0.944x, with 0 views outside.
  - It is `WW_IMPOSTOR_COVFILTER=1`. It costs 4 fetches instead of 1 per coverage read.
  - It is a candidate for default-on, bungo's call. On the shipped sheets it thickens the stipple's doubled trunk (el 0 T1 median 1.15 -> 1.23), so it only makes sense together with 8-bit height.
- Crisp A (the mean cut) + search passes the TEAR bar on 8-bit height but never the trunk bar: it thins the trunk. With the filter at el 20, T1 minimum is 0.687 and 55 of 360 views are outside. **Crisp A + search is therefore not the candidate for the crisp end of the slider.**
- Cost of search N: up to 2N+1 sheet reads per frame, 3 frames per pixel. N=16 means up to about 99 reads per pixel, plus up to 4x on coverage reads with the filter on. The march stops at the first hit, so typical cost is far lower (not timed).
  - Fewest steps that pass at el 0: 16 (8 fails: T1 24 views, T2 20 views).
  - At el 20, 24 and 32 steps are no better than 16.
- Resume points:
  - bungo looks at `gifs/maple_after_8bitheight_*.gif` against `gifs/maple_after_shippedsheets_*.gif` and at `gifs/trunk_strip_after.png`.
  - Then he rules on:
    1. the `_n` format (lodgen);
    2. the coverage filter default;
    3. whether search 16 + stipple becomes the smooth end of the slider once the sheets carry 8-bit height.

## WW_CHANGES entry

### Impostor card: depth search and a sharper coverage edge (lane IMPOSTORDEPTH1, 2026-09-23). All OFF by default.
- `WW_IMPOSTOR_SEARCH=N` (0..64): each frame marches the view ray through the card's depth and lands on the first surface its height puts there, so the three frames agree where a thin trunk is. 0 is the old one-step parallax.
- `WW_IMPOSTOR_COVFILTER=1`: the card's coverage is decoded texel by texel before it is filtered. The hardware used to filter the encoded value, which pulled every edge about a quarter texel inward and drew thin trunks at 0.87 of their width.
- `WW_IMPOSTOR_SNAP=1`: the nearest photo alone, still placed at its own depth. `WW_IMPOSTOR_BLEND=0` remains the flat version.
- New gate `tests/spells/impostor_trunk.sh`: the pre-registered trunk bar (width, doubling, jumping) and the tear bar over two 1-degree orbits, plus the knob floors. Today it reports 19 checks, 6 failures. Those 6 are the trunk rows on today's card sheets, and they stay red until lodgen keeps 8-bit height.

## MISTAKES entries

- **2026-09-23 IMPOSTORDEPTH1: a coarse ray march steps over a thin trunk.** The first search (no in-front jump) at 16 steps LOST the trunk at az 45 (T1 min 0.000), worse than no search.
  - A ray crosses the inside of a trunk w wide only over w/sin(angle), which is shorter than one step at 15-25 degrees between frames.
  - Fix: a covered sample in FRONT of its surface steps onto it when that point is within the next step.
  - Rule: a march over a thin feature needs a fallback for "in front of it". Measure N=64 beside any small N before believing the small N.
- **2026-09-23 IMPOSTORDEPTH1: a shader lane blamed the shader for eight sweeps before measuring its input.** The search moved the trunk numbers by almost nothing on the shipped sheets. The cause was the DXT5 height, measurable in one script against the bake's PNG.
  - Rule: before iterating on a shader that reads a compressed channel as a quantity, decode the DDS and diff it against the bake's own PNG, then run one arm with that sheet rewritten uncompressed (skill ww-shader-devloop-nobuild, section 4).
- **2026-09-23 IMPOSTORDEPTH1: a loose-sheet arm drew blank.** "draw REFUSED: the colour sheet did not bind", because the copied bake had no `textures/data/fo4cslod/cards/` subtree.
  - Rule: copy the whole bake folder, `textures/` included, and replace the sheet in BOTH places.
- **2026-09-23 IMPOSTORDEPTH1: the build chain's "is the exe held" probe matches bungo's renamed window.** WMI reports PID 25584's path as `release\NifSkope.exe` although it runs `NifSkope_inuse_25584.exe`. The skill's chain would have tried `mv release/NifSkope.exe release/NifSkope_inuse_25584.exe` onto the running image.
  - This lane skipped the rename step because the exe was not held.
  - Rule: before renaming, check whether `NifSkope_inuse_<pid>.exe` already exists; if it does, the window is already aside.

## Skill review (finished work)

- **NEW skill written: `E:\Projects\Claude\.claude\skills\ww-shader-devloop-nobuild\SKILL.md`.** It covers:
  - the run folder;
  - the sed const pin with its grep gate;
  - the identity control;
  - the one-sheet uncompressed discriminator arm, including the `textures/data/fo4cslod/cards` trap.
- **nifskope-ww-build-verify: owes a line.** The LOCKED probe matches a window already renamed aside (see MISTAKES above). Suggested text: "if `release/NifSkope_inuse_<pid>.exe` exists, that window is already aside; do not rename again."
- ww-reference-card-diagnose and nifskope-ww-render-shot: used as written; nothing owed.
- **ww-test-harness-add: nothing owed.** The new gate follows the spell shape: its floors fire in the same run, its knob rows are byte-identity, and its red rows name the defect.

## Gates (exe e294ae80, outputs in gates/)

| gate | new exe | rung 8d87c155 |
|---|---|---|
| impostor_trunk.sh (NEW) | 19 checks, 6 failures. The 6 are the trunk rows on the shipped sheets; every knob row and floor passes. | 19 checks, 14 failures. Every knob row fails; the floors hold. |
| impostor_draw.sh (blast_n4 fixture) | 32 steps, 0 failures | -- |
| impostor_aa.sh | 7 checks, 0 failures, PASS | -- |
| lodgen_octahedral.sh | RESULT PASS | -- |
| native_lighting.sh (control) | 21 checks, 2 failures: the 2 legacy_btr reds that were already there | -- |

The new gate leaves 924 MB of orbit PNGs in `release/impostor_trunk_tmp` (it clears them at its next start, like the other gates). Delete it by hand if the space is needed.
