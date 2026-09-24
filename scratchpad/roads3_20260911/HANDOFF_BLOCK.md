- ROADS3 **BUILT AND GATED**. `Fallout4.exe` was up at 03:57:20 (PID 22908) so
  the lane was written BUILD PENDING; it had exited by **04:08:58**, the build
  was spent after a fresh `tasklist`, and `tools/ww_build.sh` returned
  **BUILD-RC=0 on the first try — one build, zero extra relinks**. New
  `release/NifSkope.exe` **2026-09-12 04:10:38, 21,489,152 B**, md5
  `fe65cc978f3896881140c2eea57c69c6`. Rung kept aside as
  `release/NifSkope.before_roads3.exe`, md5
  `6af74b4b4667ce50c4506a2d42a04fdf`, **equal to the exe this lane found at
  launch**; the two are the same size and different bytes. The old exe was
  renamed aside at link time, never killed; no window of bungo's was touched.
  Markers: `scratchpad/roads3_20260911/DONE` in, `BUILDING` gone;
  `PENDING.md` is superseded and says so at its top. Report
  `scratchpad/lane_roads3_report.md` (sections 0-7 and `## DONE`). Entry text
  `scratchpad/roads3_20260911/WW_CHANGES_ENTRY.md`. **Four MISTAKES entries NOT
  appended by the lane** — `scratchpad/roads3_20260911/MISTAKES_ENTRIES.md`, the
  director splices. Contract amended in place: `docs/LODGEN_TERRAIN_VT.md`
  section 1a (new 1a.5d with its build results, plus 1a.4, 1a.6, 1a.7, 1a.8),
  LF-only, byte-counted. `src/lodgen.h`, `src/lodgen.cpp` and `src/nifcli.cpp`
  changed; backups beside the lane (`*.bak`) and the edit re-runnable as
  `patch_road_opacity.py` + `patch_usage_synopsis.py`. Nothing committed,
  nothing stashed. **bungo's open NifSkope window DOES need a restart** — it is
  still on the 03:06:21 exe.

  **THE GATES, ON THE BUILT EXE.** F2 byte identity: the new exe with no flag,
  with `--road-opacity 1`, and with `--roads-legacy` reproduces the rung **9/9
  and 10/10 files identical in all three arms on both chunks**, every file
  compared, not a sample. The compare is shown able to fail in the same run:
  `--road-opacity 0.326` moves 3 files on (-20,20) and 4 on (-8,8), **all of
  them colour** — the `_msn`, the `_data`, the `.bto`, the `.lodl` and the
  `.lodm` are byte-identical at every setting. F4 chain at GRADE1's baselines
  row for row: `lodgen_roads` **11/0** (R5 floor 0.1354, after 0.3435, reference
  0.4039, bars 0.2708/0.3231 both cleared; centreline colour error 38.04 →
  22.34), `lodgen_terrain` **26/0**, `lodgen_terrain_vt` **41/1**,
  `lodgen_ground_cover` **29/5**, `lodgen_terrain_pbrm` **14/0**,
  `lodgen_native` **0 failures in all seven sections**, `lodl_open` **23/0**,
  `lod_generation` **116/0**. Zero NifSkope processes left running.

  **THE ONE RED ROW IS NOT THIS LANE'S, AND THERE IS A CONTROL FOR IT.**
  `lodgen_terrain_vt` holds its 41/1 count but the failing check is **V9c**
  where the historic red row was V9b. The rung exe was re-run through the same
  harness (`EXE=release/NifSkope.before_roads3.exe`,
  `logs/f4_vt_RUNG_control.txt`) and fails V9c with **digit-for-digit identical
  numbers** — E/W seam 188.074, interior 13.243, ratio 14.20, edge step 14.348.
  So it predates ROADS3. **NEW RED: V9c wants a lane** — the E/W chunk-seam step
  is 14.20x the interior against a bar of 3.20, and the interior control itself
  (13.243 / 12.182) is outside its own 1.20..2.20 window.

  **WHAT VANILLA'S FAR ROAD IS.** Not a painted ribbon: a **wash that follows
  the ground under it**. Road luminance regressed on the mean luminance of the
  non-road texels within 8 texels — vanilla **+0.714** on (-20,20) and **+0.755**
  on (-8,8), our own unpainted ground **+0.637** / **+0.565**, ours **+0.339** /
  **+0.209**. Floors both sides: the field translated reads −0.009 mean (worst
  0.298) over 5 draws, a known-answer fixed paint +0.000, a known-answer
  ground+4 +1.000. Vanilla's road stands **+4.29** and **+4.40** levels over its
  surround on two tiles a biome apart; **ours stands +29.96 and +3.84** — right
  on (-8,8) to 0.56 of a level, **25.67 levels too contrasty on (-20,20)**,
  because our paint is a fixed material colour (99.05 / 106.68) while our ground
  swings 60.96 → 102.12.

  **THE BRIEF'S OWN LAW WAS REFUSED BY ITS OWN FLOOR.** Fitting
  `vanilla = a·ourPaint + (1−a)·ourGround` per texel leaves **17.6 %**
  unexplained against a shuffled-ground floor of **18.2 %** on (-20,20), and
  **52.6 %** against **51.5 %** on (-8,8) — worse than the floor. Ceiling
  (vanilla against its neighbouring shipped sheet) 18.1 % / 39.2 %. The spread
  of `a` (sd 1.414 / 1.789, p10 0.258 / −2.000) is not something an opacity can
  do, and an alignment control over ±3 texels buys only 0.005, so the mask does
  name vanilla's road and the fit still fails. Nothing was built on it.

  **THE HUE IS NOT THE DEFECT — ROADS2's owed number, delivered, and null.**
  Road minus surround on the opponent axes: vanilla +2.30 / −2.14 and ours
  +3.35 / −3.25 on (-20,20); vanilla +4.10 / −3.07 and ours +1.96 / −1.24 on
  (-8,8). Same sign, same direction, every gap under 3 levels of 255. On the
  road texels themselves at Sanctuary vanilla's b_y is −12.62 against our
  −12.39, saturation 0.162 against 0.161. The difference bungo sees is
  brightness.

  **VANILLA KEEPS NO ROAD-TEXTURE DETAIL** — residual-vs-detail correlation
  **+0.0275** against a phase-twin floor of 0.0270 / 0.0644 on (-20,20) and
  **+0.0162** against 0.0124 / 0.0163 on (-8,8), best-fit strength negative.
  **That measurement is now OVERRULED as a default-picker**: bungo looked at both bakes on 2026-09-12 and ruled *"--road-detail 1 is always
  on, do not ever use road detail 0, that looks terrible"*, and
  `docs/LODGEN_TERRAIN_VT.md` 1a.5d paragraph 7 carries the overrule. Vanilla
  really does keep no road-texture detail -- that is why the flag exists -- and
  it is not what chooses ours. Nothing in this lane's own change depends on it:
  ROADS3 never touched `--road-detail`. **The edge WIDTH is refused as unresolvable**: a 4.29-level
  rise under a 6.59-level local SD, SNR **0.65**.

  **THE TWO-TONE IS THE OPPOSITE WAY ROUND FROM THE GUESS.** Vanilla reaches
  full value one texel in and runs flat; ours ramps 79.86 → 86.48 → 93.45 over
  three texels, plateaus ~96.5, then climbs to 105.5 in the core — a **darker
  outer band around a brighter core** (the alpha-blended skirt over a far darker
  ground, the wider trunk material inside). Biggest step inside the road: ours
  **3.88** against vanilla's **1.31** on (-20,20); on (-8,8) ours **1.25**
  against vanilla's **4.43**, i.e. already smoother than vanilla there.

  **WHAT SHIPS: `--road-opacity A`, default 1.0**, scaling the road
  plane's alpha into the composite at both writers (`lodgen.cpp:7944` and
  `:9261`), with the multiply **branched over at 1.0** so the off value is the
  rung's bytes by construction, the ground-cover suppression deliberately left
  on the UNSCALED coverage, `roadOpacity` added to the census, and
  `--roads-legacy` restoring 1.0 unless the caller named a value.

  **THE DEFAULT DID NOT MOVE, AND THAT IS ARITHMETIC.** Every candidate was
  simulated on the rung's own sheets with the generator's own composite before
  any code existed. On **(-8,8) no opacity can meet the brightness gate at
  all** — the composite can only land between our ground 102.12 and our paint
  106.68 and vanilla's road is 94.59, **7.53 levels outside** the reachable
  interval. On **(-20,20) the gates are mutually exclusive by 22 levels**:
  absolute level wants a = 0.83, the rise wants 0.326, the step wants ≤ 0.25,
  the local SD wants ≥ 0.75. Those 22 levels are the **ground's** (ours is 19
  levels darker than vanilla's there — GRADE1's per-cell content difference,
  which TILING2 told this lane in writing not to chase with the road pass). A
  per-texel **ground-relative mode was built, simulated and rejected with its
  numbers** (Sanctuary road 25 levels below vanilla with a rise of **−1.8**,
  more than half its texels clamping to 0; downtown local SD halved to 0.48 of
  vanilla's).

  **BUNGO'S CALL — the table, both tiles; BAKED rows marked:**
  a = 1.000 BAKED → (-20,20) 99.05 / rise +29.96, (-8,8) 106.68 / +3.84;
  a = 0.830 BAKED → **92.43 (−0.09 vs vanilla's 92.52)** / +23.34, 105.89 /
  +3.05;
  a = 0.500 priced → 80.01 / +10.91, 104.40 / +1.56;
  a = 0.326 BAKED → 73.09 (−19.43) / **+4.01 against vanilla's +4.29**, 103.39 /
  +0.55;
  a = 0.250 priced → 70.48 / +1.39, 103.26 / +0.42.
  Plainly: **0.326 stops the Sanctuary road reading as a stripe and costs 19
  levels of darkness on it**, because it is borrowing against a ground error.
  Ground first, road second, is the honest order.

  **PICTURES** `scratchpad/roads3_20260911/images/cmp_road_wash.png` (two
  chunks × four columns: vanilla | the rung | a = 0.326 | a = 0.83, each with
  its road level, rise, local SD and step burned in) and `cmp_road_profile.png`
  (cross-road luminance profile, vanilla / rung / ground / both candidates, with
  the per-texel opacity the wash would need on the right-hand scale). **Every
  panel in both pictures is a real bake or Bethesda's own sheet — nothing is
  simulated.** How good the pre-build pricing turned out to be, measured rather
  than guessed: it agrees with the bake on the AGGREGATES to **0.29** and
  **0.22** of a level and differs **per texel** by 1.58 levels on average, 7.34
  at p99 and **15.28** at worst, because the bake goes through 8-bit
  quantisation and BC1 and the simulation does not.

  **THE SKIRT IS GEOMETRY AND THE KNOB CANNOT REMOVE IT.** Correlating road
  luminance with the road mesh's own interpolated vertex alpha (ROADS2's
  `seam.py`, re-run on these bakes): vanilla **+0.001**, ours **−0.792** at
  a = 1, **−0.694** at 0.83, **−0.436** at 0.326 — and our unpainted ground's own
  floor on the same texels is **−0.325**. Opacity walks it toward the ground's
  floor, never to vanilla's zero. The feathered-boundary gradient (54 texels,
  vanilla 4.242) reads 3.979 / 3.352 / 2.979 at those three settings, inside
  vanilla's at all of them.

  **SKILLS.** Added `.claude/skills/ww-simulate-before-build/SKILL.md` (apply a
  candidate knob's arithmetic offline to already-baked sheets and read the whole
  gate table off it before spending a build — the four conditions under which
  that is legitimate, the three things it cannot show, and the labelling rule).
  Amended `.claude/skills/ww-control-calibration/SKILL.md` with "a floor that
  returns NaN is not a weak floor, it is no floor". Both mirrored into
  `E:\Projects\NifskopeWWE_ui\.claude\skills\` **additively** — nothing in that
  tree was deleted or overwritten, because lane UINOTES1 is live in it.

  **REDS CARRIED.** (1) our ground on (-20,20) is 19 levels darker than
  vanilla's (68.69 vs 83.52) and is the actual reason that road reads wrong;
  (2) the object-path material fix-up at `src/lodgen.cpp:6811` still keys on the
  LAST `materials/` in a path — this lane confirmed the
  `materials/c:/projects/fallout4/...` lines in the bake log come from
  `lodgenLoadModel`'s object path and NOT from the road pass (road census:
  `roadRefusedNoTexture 7` of 319 shape tiles, `roadTexels 27509`), so GRADE1's
  red 3 narrows to the object path; (3) the ground-cover plane is empty on these
  chunks (`dwReserved1 = 0`), which is why the grass tint is inert.

  **OWED:** bungo restarts his NifSkope window (new exe 04:10:38); nobody has
  looked at any of this in the game — the default's bytes did not move so there
  is nothing new to see at the default, and the two candidate settings have not
  been flown. `scratchpad/lodui1_20260911/BAKE_INSTRUCTION.md` needs **no
  change**: no default moved, so the instruction it carries still bakes the same
  bytes.
