- SPLAT1 PHASE A LANDED, PHASE B **BUILD PENDING**, NOTHING BUILT, NOTHING
  CHANGED. Read-only lane: no exe run, no bake made, nothing touched under
  `src/`, `res/`, `tests/`, `tools/`, `docs/` or `NifSkope.pro`; no commit; the
  game was down at every check. Report `scratchpad/lane_splat1_report.md`;
  resume `scratchpad/splat1_20260911/PENDING.md`; entry text, mistakes (3) and
  the contract amendment in the same directory, for the director to splice.

  **THE ANSWER TO HIS QUESTION.** He asked whether the terrain bakes use the
  landscape textures at their correct scale. **They do not.** The bake stretches
  every landscape texture to **2,048 world units a repeat**; the engine's own
  repeat is **341.3333** -- exactly **6x** smaller. `fLandTextureTilingMult`
  = 1.5 in `Fallout4.exe` 1.10.155 (one code reference, VA 0x1403A74C6),
  `uv = vertexIndex * mult/4 = 0.375` over the 17x17 quadrant grid, 128 world
  units a vertex, so 128/0.375 = 341.3333 -- twelve repeats a cell.
  `scratchpad/splat1_20260911/s2c_engine_tiling.py` re-derives every address
  from the shipped binary. At the correct scale one repeat is 10.7 texels of a
  32-unit far sheet, which is the "very tiny repeating pixel sized patterns" he
  described, and the footprint-matched tap over it lands near the texture's own
  mean -- smooth, like vanilla's.

  **THE MIP IS NOT THE DEFECT.** It already picks the footprint mip (5.00 for
  all 20 layer textures, all 2048x2048 with 12 mips) and an exact box mean over
  the footprint is 0.50 units SMOOTHER, not 52. The director's first candidate
  is refuted by its own number.

  **THE NUMBERS.** 3x3 local variance of luminance, 512x512 sheets at 32 world
  units a texel; instruments gated 12/12 first (`s0_selftest.py`), codec floor
  1.52, phase-randomised twin and a smooth/checker known-answer pair beside
  every row.

  | | (-20,24) | (-20,20) |
  |---|---|---|
  | vanilla | 19.81 | 29.39 |
  | ours as shipped | **76.22** | **75.26** |
  | offline re-bake at TILE 2048 (the model's control) | 71.95 | 61.71 |
  | **offline re-bake at TILE 341.333** | **12.93** | **14.60** |

  Ruled out with their own numbers: the grass tint (chunk (-20,24) has NO cover
  plane -- `_data.DDS` is DXT1 with no `WWCV` stamp -- so the tint touched none
  of it, and that is the tile with the LARGER excess), VCLR (moves the sheet by
  0.02 and 0.05 of a 52 and 32 excess; and the cells carrying no VCLR show the
  larger excess), the BC1 codec (1.52), the 17x17 blend (0% on one tile, 33% on
  the other, and it goes away with the tiling too).

  **A CONTROLLED NEGATIVE.** Bethesda's sheet contains no landscape-texture
  pattern at ANY of six candidate repeats -- every correlation inside its own
  phase-twin floor -- while the same correlation reads **+0.91 / +0.88** against
  OUR sheet, where one certainly is. Their fine detail is objects, roads and
  paint.

  **SAID PLAINLY: THIS DOES NOT CLOSE THE COLOUR ERROR.** Whole-tile mean
  absolute RGB against vanilla goes 16.89 -> 15.02 and 21.73 -> 20.16 of 255.
  The remaining 16-20 is the GRADING (ROADS1's x0.82-0.83), and "splat
  calibration vs vanilla grading" stays OPEN. Nobody should read this lane as
  closing it.

  **PICTURE FOR BUNGO:**
  `scratchpad/splat1_20260911/images/speckle_diagnosis.png` (1078x1324) --
  vanilla | ours as shipped | ours re-baked offline at 341.333 | the difference
  x4, the same 128x128 texels at (216,128) of chunk (-20,24) chosen by the
  metric, 4x nearest neighbour, local variance burned into every panel
  (18.58 / 89.21 / 13.29).

  **WHY PHASE B DID NOT RUN.** Its gate was CARDS-AGG's DONE and NIFPARSE1's
  DONE with no `BUILDING` marker anywhere. CARDS-AGG held `BUILDING` at every
  poll and neither DONE appeared (`logs/poll2.log`). **No code was written, so
  nothing is half-applied.** The change when it runs is ONE value -- `TILE` at
  `src/lodgen.cpp` 6335 and 7623 -> a bake option defaulting to 341.3333, CLI
  `--land-tiling`, `--land-tiling 2048` the exact way back -- and it must move
  the COLOUR and the MASK/EMISSIVE paths together (the same `TILE` is read at
  6680, 6688, 7661, 7665, 7699, 7703, 7718, 7722), or the roughness sheet ends
  up describing different ground from the colour beside it. `_msn` comes from
  VHGT and must not move at all. Gates S1-S7 pre-registered in `PENDING.md`.

  **THREE REDS FOR THE DIRECTOR.** (a) `docs/LODGEN_TERRAIN_VT.md` 2.5's VCLR
  range does not reproduce: it says 249..255 over cells -20..-17 x 24..27;
  measured, 11 of 16 cells carry a VCLR and the range is **203..255** (and
  170..255 on the other region). Its conclusion survives, its number does not.
  (b) The contract cites the bake for the tiling constant -- a guess became law
  by restatement (MISTAKES entry). (c) When the tiling moves, every stored
  colour expectation in `lodgen_terrain.sh` / `lodgen_terrain_vt.sh` moves with
  it; re-taking them silently would hide the very change under test.

  **RESTART: no.** No exe was built or run by this lane.

  **AND THE COUNTERWEIGHT, IN THE SAME BREATH.** Correcting the tiling does not
  move our spectrum TOWARD vanilla's -- it moves it away. Vanilla keeps 16% of
  its power at 2-4 texels and 6% below 2; at the correct tiling we keep 2.6% and
  0.2%. That is consistent with the controlled negative above: vanilla's fine
  detail is content we do not bake (objects, rubble, road edges, paint), not
  ground-texture grain. So today our sheet has the WRONG fine detail at the
  wrong amplitude from a source vanilla does not have; after the fix it has
  almost none, which is honest for a sheet built from the splat alone, and the
  missing content is TERRAIN-AO1 / ROADS1 / objects, already open. Report both
  or the fix reads as more than it is.

  **THE PHASE-B GATE WAS STILL SHUT WHEN THIS LANE ENDED (16:21).** No `DONE`
  on either lane; CARDS-AGG's `BUILDING` stamped 15:48 still up; its own
  `PENDING.md` says "THE BUILD HAS NOT RUN YET". NIFPARSE1 wrote its four
  handoff documents 15:43-15:47 and has been quiet since -- that one looks
  ended. **CARDS-AGG IS NOT: it was still writing at 16:16, 16:17 and 16:20**,
  so it may yet build and write `DONE`; an earlier draft of this block said it
  had gone quiet at 16:02 and that was wrong (MISTAKES entry). SPLAT1 ends BUILD
  PENDING because its own session ends here, NOT because CARDS-AGG is finished. **THREE LANES NOW SHARE ONE
  BUILD SLOT AND ONE FILE** (`src/lodgen.cpp`): the order that avoids a second
  build is NIFPARSE1's hook-up, then CARDS-AGG's, then SPLAT1's one tiling value
  LAST with the anchor pass re-run -- `nifskope-ww-resume-pending`, spelled out
  in `scratchpad/splat1_20260911/PENDING.md`.

  **CLOSED 18:20 -- RESUME3 OWNS THE FIX, SPLAT1 OWES NOTHING.** The gate poll
  ran all 120 polls and closed 18:18:33 "GATE NEVER OPENED": CARDS-AGG's `DONE`
  DID appear (so the 16:2x correction above was the right call), NIFPARSE1's
  never did, and `scratchpad/resume3_20260911/` has held `BUILDING` since
  16:45:32. RESUME3's step R4 is this lane's change, written and `--check`
  green: default **341.3333**, `--land-tiling` with 2048 the exact way back,
  **all fourteen `TILE` uses** (colour, mask, emissive, both paths), citing
  SPLAT1's addresses. SPLAT1 therefore does NOT build: RESUME3 holds the slot
  and owns `src/lodgen.cpp`. Check its build against SPLAT1's pre-registered
  gates S1-S7 in `scratchpad/splat1_20260911/PENDING.md` -- above all S1 (local
  variance 76.22/75.26 -> ~12.93/14.60 against vanilla 19.81/29.39), S3
  (`--land-tiling 2048` byte-identical to the rung) and S4 (`_msn` byte-
  identical at BOTH values; no tiling term may reach the normal sheet).
