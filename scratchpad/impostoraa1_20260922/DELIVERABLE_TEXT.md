# IMPOSTORAA1: text for the director to splice (HANDOFF / WW_CHANGES / MISTAKES)

The lane did not commit anything. It did not touch HANDOFF.md, WW_CHANGES.md or MISTAKES.md.

- Exe: `release/NifSkope.exe`, built 2026-09-23 00:21:11, 23,830,016 B, sha1 74e317f9.
- Rung: `release/NifSkope.before_impostoraa1.exe`, sha1 89574e81.
- Source changed: `src/nifskope_ui.cpp` only, via `scratchpad/impostoraa1_20260922/patch_aa.py`.
- New files: `tests/spells/impostor_aa.sh` and `tests/spells/impostor_cube_coverage.py`.

## WW_CHANGES (proposed)

**Impostor bake: the 2x offscreen arm (lane IMPOSTORAA1, the ruled design).**

Before this change, each octahedral view was a photograph of the window. That photograph:
- was framed on the window (the rung's framebuffer was 1024x525, with a 1.09x crop);
- carried whatever MSAA the user's setting gave the window;
- was resized with `SmoothTransformation`.

So two machines baked two different sheets, and the MSAA resolve averaged the normal, height and material channels across silhouette edges.

Now each view renders into its own FBO:
- GL_RGBA8, no MSAA, no sRGB;
- exactly 2x the frame's inner size;
- panned in the view plane onto that view's own silhouette centre;
- box-filtered 2:1.

The box filter works per channel:
- **Coverage** is the mean of the 4 samples.
- **Colour, height, material, mask and emissive** are coverage-weighted: premultiply, box, un-premultiply.
- **Normal** is the same weighted sum of the decoded vectors, renormalised.

Pass one reads its silhouette boxes from a fixed 1024-square offscreen matte.

The contract:
- The sheet bytes keep their meaning, so there is **no version bump**.
- The sidecar gains one line: `aa 2 <pass-one px> <worst centre miss, texels>`, or `aa 0 <WxH>` on the fallback.
- `WW_IMPOSTOR_AA=0` restores the window photograph byte for byte.
- `WW_IMPOSTOR_WINDOW=WxH` sets the bake window's size, for the window-independence gate.
- The legacy front/side cards (`legacy 512`) are still window photographs. They were not in the ruling.

### Gate: `tests/spells/impostor_aa.sh` (6 checks, about 39 s)

It bakes the 512-unit cube at N4, tile 512.

| exe | result | fact 1 | fact 2 |
|---|---|---|---|
| New | **6/0 PASS** | Bake vs the ideal 2x2 estimator: 1.71 levels (bar 4). Edge error 41.88 against the design ceiling of 42.45. Floor: AA=0 reads 71.09. | All four oct sheets are byte-identical between a 560x560 and a 1400x1000 window. Floor: AA=0 differs on 3 of 4 sheets. |
| Rung 89574e81 | **6/3 FAIL** | 71.09 against the bar of 4. Edge error 65.98 against a ceiling of 15.92. | The window floor reads 0 sheets differing, because the rung has no window knob. |

### Five subjects rebaked at cardRes 512, the same drawer on both

| subject | photograph IoU at every bake direction, rung -> new | bake seconds, rung -> new | crevice highpass card/mesh, rung -> new |
|---|---|---|---|
| blast_n4 | 0.8889 -> 0.8966 | 7 -> 18 (first run, cold start) | 0.40 -> 0.55 |
| blast_n8 | 0.8798 -> 0.8971 | 16 -> 9 | — |
| maple_n4 | 0.4861 -> **0.5689** | 8 -> 6 | — |
| dead_n4 | 0.7692 -> 0.8137 | 7 -> 6 | 0.62 -> 0.71 |
| rock_n4 | 0.9531 -> 0.9675 | 8 -> 7 | **0.54 -> 0.68** (resolution ceiling 0.66) |

- Total bake time is unchanged: 46 s on the rung and 46 s on the new exe.
- `impostor_draw` row 16a, card vs mesh normal mean angle: 11.1° -> 6.1°.
- Maple's frame half-width grew from 412 to 440. The offscreen pass one sees twigs the window matte missed, so the maple's texel pitch is 7% coarser.

## CONTROLS (no bar lowered)

**`impostor_aa.sh`:** 6/0 PASS.

**`impostor_draw.sh`** on blast_n4 res512: 29 PASS / 1 FAIL / 3 SKIP on both sheet sets.
- Row 5 (silhouette IoU, floor 0.50) is red on both: 0.4664 on the rung sheets and 0.4699 on the 2x sheets. The red comes from the ruled cut; IMPOSTORFIN1 read 0.4979 on its smaller fixture.
- Row 15 (photograph): 0.8889 -> 0.8966.

**`native_lighting.sh`:** 21/2 on the NEW exe and on the RUNG alike. The two red rows are `legacy_btr_top` and `legacy_btr_obl` against their baselines. They are pre-existing and not this lane's.

**`lodgen_octahedral.sh`:** 115 ok, 1 FAIL. The red row is **F1**: "every frame of the cube spans its predicted texels within 1 at half coverage", worst 1.27. This is a **conflict between the brief's design and the bar, and a ruling is owed.**

`f1probe.py` measures the same instrument on several inputs (texels):

| input | span error |
|---|---|
| the new bake | 1.32 |
| the ideal 2x2 point estimator, ties counted | 1.32 (the bake reproduces it exactly) |
| the ideal 2x2 point estimator, ties not counted | 1.58 |
| the area truth | 0.68 |
| 3x3 or 4x4 supersampling | 0.68 |
| the rung (MSAA plus bilinear) | 0.70 |

So a 2x box cannot meet a 1-texel bar on a phase-aligned cube edge. The options are:
- 3x or 4x supersampling (about 2.25x or 4x the fill of 2x);
- rebasing F1 on the design ceiling.

The bar was not lowered.

## THE TEAR ("like somebody ripped out a piece of paper"): the cause is in the DRAW, not the bake

**Pictures.** Each picture shows, at the worst of four el-20 views: the app mesh, the card as drawn and the nearest frame; the reference blend, the blended coverage with the cut, and the TORN map; then, for each of the 3 contributing frames, raw texels, coverage and height.
- `scratchpad/impostoraa1_20260922/pics/tear_{blast_n4,maple_n4,rock_n4}.png` (rung sheets)
- `pics/tear_after2x_*.png` (2x sheets)

**Measured.** "Torn" means a texel that one frame alone keeps (coverage at or above 0.502) but the blend drops.

| subject | torn share, rung sheets | torn share, 2x sheets | agreement of all 3 frames | blended coverage in torn pixels | best single frame in torn pixels |
|---|---|---|---|---|---|
| blast_n4 (upper trunk) | 72.3% | 71.6% | 0.132 | 0.342 | 0.966 |
| maple_n4 (mid trunk) | 80.1% | 81.0% | 0.028 | 0.319 | 0.882 |
| rock_n4 (holes) | 35.0% | 35.9% | 0.129 | 0.397 | 0.995 |

**The mechanism.** The fragment shader (`res/shaders/impostor_oct.frag`) takes coverage as the weighted **mean** of 3 frames, then applies the 0.502 cut.
- At N=4, a view 20° up blends the 0° rim frame (w 0.50) with the 63.4° top-ring frame (w 0.32) and a rim neighbour (w 0.18).
- The one-step, card-plane parallax cannot move a thin trunk, or a rock edge seen from 63°, by more than its own width. So the frames do not register, and each one puts the trunk in a different place.
- The mean then falls below the cut where any single frame is solid.

Neighbour-vs-self registration at the bake directions (IoU):

| set | one-step (shipped) | no parallax | 8 steps |
|---|---|---|---|
| blast_n4 | 0.221 | 0.134 | 0.203 |
| maple_n4 | 0.183 | 0.119 | 0.170 |
| blast_n8 | 0.459 | 0.288 | 0.461 |

For blast_n4, same-ring pairs read 0.275 and cross-ring pairs 0.148.

**Refuted as causes:**
- `frameOffset`: transposed is worse.
- `depthSpan`: scaling it x0.5, x2 or x4 is worse.
- 8-bit heights: using the PNG heights gives 0.215.
- The bake: the tear is unchanged on the 2x sheets.

**Ranked repairs (NOT applied; the director rules).** Scores are torn-view IoU vs the mesh, as blast / maple / rock. The shipped drawer reads 0.462 / 0.170 / 0.646.
1. **Ray-march each frame's depth hull** instead of the one-step parallax, with a thickness of about 24–48 units. The 3-frame blend is kept, so there is no new popping.
   - Scores 0.650 / **0.389** / **0.794**.
   - Neighbour registration for blast_n4 rises from 0.275 / 0.148 to 0.445 / 0.354 (same / cross ring).
   - Cost: about 100–200 taps per frame, per fragment. It needs the thickness bound: without one it reads 0.092.
2. **Take the cut's coverage from the dominant frame**, keeping the colour blend. This is a one-line change to the shader.
   - Scores **0.705** / 0.340 / 0.778.
   - Its popping cost is unmeasured. A fine-orbit temporal run is owed (the reference-card skill, section 9) before it could ship.
3. **Raise N.** N8 neighbour registration is 0.459 against 0.221 at N4. It does not move the apparent-size knee.

All three are measured at one worst view per subject, in the numpy reference card. The registration check (nearest-only vs the mesh) reads blast 0.705, maple 0.340 and rock 0.778.

## MISTAKES (proposed entry)

**2026-09-23, IMPOSTORAA1: a control spell run with no subject reads as a real red.**
- `impostor_draw.sh` without `IMPOSTOR_LODM` refuses with rc 1 in 1 s, and my controls summary first listed it beside the genuine reds.
- Every control needs its fixture spelled on the command line.
- Also: "IMPOSTORFIN1's row 5 fixture" is its OWN small fixture (frames 48x128), not the res512 set. Comparing 0.4979 with 0.4664 compares fixtures, not exes.
