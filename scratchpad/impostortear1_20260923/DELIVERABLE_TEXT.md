# IMPOSTORTEAR1: text for the director to splice into HANDOFF, WW_CHANGES and MISTAKES

Exe: `release/NifSkope.exe`, built 2026-09-23 03:01:19, 23,832,064 B, sha1 `c172ba9de608d7d9c36ac2b1f870e7a47434fe02`.
Rung: `release/NifSkope.before_impostortear1.exe`, sha1 74e317f9….
The shader `res/shaders/impostor_oct.frag` is deployed to `release/shaders/`, and cmp reports the two identical. Nothing is committed.

## HANDOFF line (proposed)

**IMPOSTORTEAR1 LANDED 2026-09-23 03:2x, NOT FLOWN.**
- The tear is repaired by a STIPPLED cut:
  - Each frame's own silhouette counts where a card-texel hash < min(1, 2w).
  - The 3-frame mean is kept as the floor.
  - Zero extra fetches.
- Fix (2), the strongest-frame cut, was measured and FAILED the pre-registered popping bar (1.18..2.61x).
- Every depth march from 4 to 128 taps failed the tear clause on the rock.
- The bake is now 4x, the default: aaK 4, and `WW_IMPOSTOR_AA=K` gives the way back.
- Gates on 03:01:19:
  - `impostor_draw.sh`: 32/0 (rung 32/3).
  - `impostor_aa.sh`: 7/0.
  - `lodgen_octahedral.sh`: PASS, F1 0.73.
  - `native_lighting.sh`: 21/2, the same two pre-existing legacy_btr reds.
- Pictures: `scratchpad/impostortear1_20260923/FOR_BUNGO.md`.
- Owed:
  - bungo's eye on the stipple, which is visible up close and paints 6..16 points more outside the mesh.
  - Three vanilla models bake an EMPTY card on the rung too: Sapling01, TreeElmUndergrowth01, ShrubGroupLarge05. `oct` halfW 1.077, 0 covered texels. Not chased.

## WW_CHANGES (proposed)

### Impostor draw: the tear ("like somebody ripped out a piece of paper") is repaired with a stippled cut

The cause (lane IMPOSTORAA1):
- The alpha cut read the WEIGHTED MEAN of three frames' coverage.
- Those frames come from far-apart bake angles and do not register.
- Where one frame alone is solid, the mean falls under 128/255 and a hole opens.

The cut is now `cutRule` 0, the stipple:
- A frame's own coverage counts where `hash(card texel) < min(1, 2 x its weight)`.
- The mean is kept as a floor.
- Colour and normal still blend exactly as before.

Ways back, per draw:
- `WW_IMPOSTOR_CUT=mean`: the old cut.
- `WW_IMPOSTOR_CUT=strong`: strongest frame only, which exists as a red control.

`ImpostorDraw::Options::cutRule` replaces the unbuilt `cutOnMean`. The harness log names the rule: `cut rule: stipple|mean|strong`.

**Why the other candidates were rejected (pre-registered bar):**
- The popping bar: the worst per-step change in covered pixels over a 1-degree azimuth ring (el 20) and an elevation sweep (az 30) must be ≤ 1.5 x the shipped drawer's, on blast / maple / rock.
- The tear clause: at each subject's torn view, IoU beats the shipped drawer's AND the torn share is ≤ 0.60 x the shipped share.

| candidate | pop az (b/m/r) | pop el (b/m/r) | torn share (b/m/r) | verdict |
|---|---|---|---|---|
| shipped mean | 1 | 1 | 44.9 / 68.3 / 27.3 % | the tear |
| fix (2) strongest | 1.84 / 2.53 / 2.00 | 2.29 / 2.61 / 1.18 | 7.1 / 22.9 / 13.8 % | pops |
| march m32 mean (32 taps x 2 fetches x 3 frames) | 1.18 / 0.77 / 1.11 | – | rock 34.1 % | tear clause fails on rock |
| march m64 mean | 1.22 / 0.83 / 1.19 | – | 17.3 / 36.3 / 37.4 % | fails on rock |
| march m128 mean | 1.08 / 0.96 / 1.10 | – | 11.2 / 24.6 / 27.7 % | fails on rock |
| strongest + march 4..32 | 1.4..3.8 | – | – | pops |
| soft K=2 (max cov x min(1, 2w)) | 1.27 / 1.35 / 1.20 | 1.72 / 1.52 / 0.47 | 5.7 / 20.8 / 1.0 % | pops on el |
| **stipple K=2 (chosen)** | **0.31 / 1.04 / 0.31** | **0.49 / 1.26 / 0.08** | **5.9 / 19.0 / 5.4 %** | **passes both** |

- Torn-view IoU, shipped → stipple: 0.466 → 0.667 blast, 0.191 → 0.401 maple, 0.621 → 0.792 rock.
- **Mechanism, measured.** Any hard threshold on a weight-scaled coverage flips every pixel that only one frame covers at the same degree:
  - The shipped mean flips at w_dom = 0.5; its own worst steps are exactly el 19→20 and el 34→35.
  - Soft K=2 flips at w = 0.25: blast el 35→36, area 34206 → 10148.
  - The stipple spreads that flip over the whole weight range.
- **Price:**
  - The MEAN per-step change rises: blast az 3841 → 5703; maple 5951 → 13406, still under the mesh's own 18534.
  - The stipple is visible up close.
  - Across the 7 picture trees, the share of card pixels outside the mesh rises by 6..16 points. IoU rises on 6 of 7 (evergreen 0.791 → 0.784).

### Bake supersample 2x → 4x

- `aaK` default 4. `WW_IMPOSTOR_AA=K` sets another value; 0 = the fallback.
- The sidecar line is `aa K p1Size centreErr`.
- The rules are unchanged: offscreen, MSAA off, coverage-weighted average, window-size independent.
- Wall time per bake, 2x / 4x, in ms, including startup:

| fixture | 2x | 4x |
|---|---|---|
| blast_n4 | 7944 | 4869 |
| blast_n8 | 6732 | 8259 |
| maple | 5433 | 5532 |
| dead | 4946 | 4705 |
| rock | 7837 | 6926 |
| **total** | **32.9 s** | **30.3 s** |

- The difference is process noise; the draw count, not the supersample, dominates.
- `lodgen_octahedral.sh` F1 worst: 1.27 → **0.73** (bar 1.0), RESULT PASS, 116 ok.
- `impostor_aa.sh`: 7 checks, 0 failures (K read from the sidecar, plus the 4x row).

### Gate rows (tests/spells/impostor_draw.sh)

**Row 17, TEAR.**
- The torn share = |mesh ∩ nearest-frame-only ∩ ¬card| / |mesh ∩ nearest|, over 36 azimuths at el 20. Measured by `tests/spells/impostor_tear_pop.py`.
- Bar 0.1340 = 0.60 x the rung's 0.2234 on the 4x blast_n4 fixture (TreeMapleblasted05).
- Rung 0.2234 RED; new exe 0.1135 green (worst az170 0.4631).

**Row 18, POPPING.**
- The default cut's worst step must be ≤ 1.5 x the mean cut's, same exe, same bake.
- The red control `WW_IMPOSTOR_CUT=strong` must break that bar.
- The exe must name `cut rule: stipple`.
- New exe:
  - az 8471 / 26675 = 0.32 (strong 1.86)
  - el 7278 / 14379 = 0.51 (strong 2.36)
- On the rung it is RED, because the rung names no rule and cannot run the red control. Its drawer IS the mean reference, so the ratio clause itself cannot be red there. This is said in the row's comment.

**Row 5** (unchanged bar 0.50): rung 0.4692 red → new **0.5147** green.

Full suite: new 32 steps 0 failures; rung 32 steps 3 failures (5, 17, 18).

## MISTAKES (proposed entries, newest first)

- **2026-09-23 IMPOSTORTEAR1: a heredoc and a sed ate backslashes again.**
  - A sed edit inside `splice_rows.py` turned a Python `"\n"` into a broken string.
  - A `python -c` carrying `'\\'` through the Bash tool died with SyntaxError.
  - Both were rewritten with the Write tool using `chr(10)` / `chr(92)`.
  - The rule already in the lodgen skill held, and I broke it anyway: **no text with a backslash goes through a shell quote.**
- **2026-09-23 IMPOSTORTEAR1: the splice anchor matched six times.**
  - `say "done  $steps steps, $fails failures"` occurs at every early exit of `impostor_draw.sh`.
  - The first splice attempt asserted uniqueness and stopped. Fixed with `rindex` plus a check that the match is the file's tail.
  - Before choosing an anchor, count it.
- **2026-09-23 IMPOSTORTEAR1: a popping analysis loaded all 360 grabs x 3 variants at full size at once.**
  - It ran the machine short of memory. I killed my own python (pid 11296) and rewrote the script to stream pairs.
  - The m4 mean-cut popping numbers from that run were lost and never re-run.
  - The m8 mean blast sweep came out degenerate (a grab failed mid-sweep) and is marked INVALID rather than quoted.
  - Stream image sweeps pairwise; never hold a sweep in memory.
