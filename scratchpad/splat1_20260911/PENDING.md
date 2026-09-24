# SPLAT1 -- BUILD PENDING (phase B not reached)

Written 2026-09-11 16:0x. `date +%H:%M` at the time of writing is in the DONE
line below; no timestamp here is typed from memory.

## What is finished and on disk

PHASE A is COMPLETE and needs nothing rebuilt. It is read-only: no exe was run,
no bake was made, nothing under `src/`, `res/`, `tests/`, `tools/`,
`NifSkope.pro` or `docs/` was touched.

* Report: `scratchpad/lane_splat1_report.md`, sections 0-5.
* Picture: `scratchpad/splat1_20260911/images/speckle_diagnosis.png`.
* Instruments and logs: `scratchpad/splat1_20260911/` (`splatlib.py`,
  `offline_bake.py`, `s0_selftest.py` 12/12, `s1_spectra.py`, `s2_sampling.py`,
  `s2b_tiling.py`, `s2c_engine_tiling.py`, `s3_candidates.py`, `s4_predict.py`,
  `make_pictures.py`; `logs/`).
* Ledger text: `MISTAKES_ENTRIES.md` (3 entries), `WW_CHANGES_ENTRY.md`,
  `HANDOFF_BLOCK.md`, `CONTRACT_AMENDMENT.md`.

## Why phase B did not run

The brief gated it on `scratchpad/cards_agg_20260911/DONE` AND
`scratchpad/nifparse1_20260911/DONE` existing AND no `scratchpad/*/BUILDING`.
At every poll (`logs/poll2.log`) CARDS-AGG still held `BUILDING` and neither
`DONE` existed. NO CODE WAS WRITTEN, so there is nothing half-applied and
nothing to revert. The game was DOWN at every check (`Fallout4.exe` absent).

## The resume, exactly

1. Re-read `scratchpad/lane_splat1_report.md` section 4 (the verdict) and 4.2
   (the change) and section 5 (the reds). Nothing else needs re-deriving.
2. Wait for the gate: both DONE markers, no BUILDING, `Fallout4.exe` down.
3. Take the rung ONCE: copy the current `release/NifSkope.exe` to
   `release/NifSkope.before_splat1.exe` and record its time and size.
4. The change, ONE thing, both bake paths together (section 5 red 2 -- the
   colour path alone would put the mask sheet on different ground from the
   colour beside it):
   * `src/lodgen.cpp` `constexpr float TILE = 2048.0f` at 6335 and 7623 becomes
     a value carried on the bake options, default **341.3333**.
   * CLI `--land-tiling <units>`; `--land-tiling 2048` is the exact way back and
     is what the byte-identity gate uses.
   * `tests/spells/lodgen_terrain_model.py`'s own `TILE = 2048.0` takes the same
     value, or the ring-0 gate measures the old law against the new bake.
5. Markers `scratchpad/splat1_20260911/BUILDING` then `DONE`; one build plus
   counted relinks; bungo's own window renamed aside, never killed
   (`nifskope-ww-build-verify`).
6. Rebake ONLY the two tiles, headless, into this lane's own out-dir, one
   NifSkope instance at a time, never his installed `Data\Terrain`, never the
   full Commonwealth (`nifskope-ww-lodgen`).

## The gates, PRE-REGISTERED here before the build

| id | gate | the number to beat |
|---|---|---|
| S1 | local variance of the rebaked colour sheet vs vanilla, both tiles | (-20,24) 76.22 -> predicted 12.93 against vanilla 19.81; (-20,20) 75.26 -> predicted 14.60 against vanilla 29.39. PASS = inside 20% of the prediction. |
| S2 | whole-tile mean abs RGB vs vanilla, this lane's definition, both tiles | 16.33 -> ~15.0 and 19.51 -> ~20.2. **This gate is NOT an improvement gate**: the tiling fix does not close the grading. PASS = it does not get WORSE by more than 1.0. |
| S3 | `--land-tiling 2048` byte-identical to the rung, every file compared | all files identical |
| S4 | `_msn` byte-identical at BOTH tiling values | no tiling term reaches the normal sheet (section 5 red 3) |
| S5 | the harnesses the change reaches | `lodgen_terrain.sh` 26/0, `lodgen_terrain_vt.sh` 41/1 (V9b red on the rung too), `lodgen_roads.sh` 11/0, `lodgen_ground_cover.sh` 29/5, `lodgen_terrain_pbrm.sh` 14/0, `ui_align.sh` 11/0, `water_ui.sh` at its baseline |
| S6 | the picture `images/cmp_speckle_fixed.png` -- vanilla / ours before / ours after, the SAME 128x128 texels at (216,128) of chunk (-20,24), 4x nearest neighbour, local variance burned into each panel | `make_pictures.py` already picks that crop by the metric; reuse it |
| S7 | exe newer than every changed source; dependent objects rebuilt; `res/style.qss` and `release/style.qss` byte-identical | |

S5's `lodgen_terrain_vt.sh` and `lodgen_terrain.sh` contain byte-identity checks
against stored expectations. **Expect them to move**, because the law moves; the
gate is that they move only where the tiling reaches, and `--land-tiling 2048`
reproduces the old bytes exactly (S3). Any spell that hard-codes a colour
expectation needs its expectation re-taken WITH the reason written beside it, not
silently.

## Why the gate will not open on its own, measured rather than assumed

At 16:15 (`date`), after 12 polls over 13 minutes plus an earlier run
(`logs/poll.log`, `logs/poll2.log`):

* `scratchpad/cards_agg_20260911/DONE` -- absent. `BUILDING` present, stamped
  **15:48**. Its own `PENDING.md` (15:55) says, first line of its state section,
  "**THE BUILD HAS NOT RUN YET**" and then lists "The first three actions on
  resume".

  **CORRECTED AT 16:21 -- CARDS-AGG IS ALIVE, NOT DEAD.** An earlier version of
  this file said its newest file was `aggpicture.py` at 16:02 and that nothing
  had changed since, and concluded it had ended with a stale marker as WATER8
  did. That was wrong: it wrote `lodm_3a.md` at **16:16**, `lodi_4_6.md` at
  **16:17** and `gate/` at **16:20**. It is still working and it may yet build
  and write `DONE`. What is true is only this: at 16:21 the gate was still
  shut (no `DONE` on either lane, `BUILDING` still up), and SPLAT1's own session
  ends here. **Do not treat CARDS-AGG as finished on this lane's word --
  re-check its markers.**
* `scratchpad/nifparse1_20260911/DONE` -- absent. That lane wrote
  `PENDING.md`, `HANDOFF_BLOCK.md`, `MISTAKES_ENTRIES.md` and
  `WW_CHANGES_ENTRY.md` at 15:43-15:47 and stopped.

So the brief's gate -- both DONE, no BUILDING -- cannot be reached by waiting,
and SPLAT1 ends BUILD PENDING on the evidence rather than on the poll limit.

## The recommendation: ONE build for three lanes

SPLAT1, CARDS-AGG and NIFPARSE1 are all code-or-change-pending on the same
`src/lodgen.cpp` and the same single build slot, which is precisely the case
`nifskope-ww-resume-pending` is written for. The order that avoids a second
build:

1. NIFPARSE1's hook-up (it owns the model/loader files),
2. CARDS-AGG's (it owns `lodgenaggregate.*`, `lodifile`, `nativeemit`, and it
   has already MOVED every line number in `lodgen.cpp`),
3. SPLAT1's tiling value LAST, re-anchored against the file as it then stands
   (`anchors.txt`'s pass re-run, not its numbers reused),

then one qmake + make, then each lane's gates in its own out-dir, sequentially,
one NifSkope instance at a time.

---

## FINAL STATUS, 18:20 -- SUPERSEDED BY LANE RESUME3. DO NOT APPLY THIS CHANGE.

The gate poll ran its full **120 polls** and closed at **18:18:33** with
"GATE NEVER OPENED" (`logs/poll2.log`). What it recorded:

* `scratchpad/cards_agg_20260911/DONE` -- **appeared.** That lane landed, which
  is why the 16:2x correction above ("CARDS-AGG IS ALIVE, NOT DEAD") was worth
  making: the first draft would have told the director a live lane was finished.
* `scratchpad/nifparse1_20260911/DONE` -- **never appeared** (0 of 120 polls).
* A NEW lane, `scratchpad/resume3_20260911/`, has held `BUILDING` since
  **16:45:32** and still holds it.

**RESUME3 now owns SPLAT1's fix.** Its own resume file lists, as step **R4**,
"the tiling change -- written and `--check` green, NOT applied, NOT built", and
`scratchpad/resume3_20260911/tiling.py` implements exactly this lane's verdict:
default **341.3333**, `--land-tiling` with `--land-tiling 2048` as the exact way
back, **all fourteen `TILE` uses** (colour, mask, emissive, both bake paths),
citing VA 0x1403A74C6 / 0x1403A7620 and lane SPLAT1.

So SPLAT1's phase B is **not owed and must not be run from here**: RESUME3 holds
the build slot and owns `src/lodgen.cpp`, and two lanes in one file is the thing
the constitution forbids. This file stays as the MEASUREMENT behind that change
-- the verdict, its controls, the predicted numbers (local variance 76.22/75.26
-> 12.93/14.60; colour error 16.89 -> 15.02 and 21.73 -> 20.16) and the gates
S1-S7, which RESUME3's build should be checked against.
