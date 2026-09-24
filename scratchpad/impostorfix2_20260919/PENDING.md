# IMPOSTORFIX2 -- PENDING

Lane IMPOSTORFIX2, offline simulation lane, 2026-09-19, 14:32 to 15:2x CEDT.
Deliverable: `scratchpad/impostorfix2_20260919/report.md` (complete, sections
0 to 9, plus the ranked list in section 7). Pictures in `images/`.

## STATE: the report is COMPLETE. Nothing in it is landed, built or run.

All six work items are measured and written. The ranked list, the gate rows and
the file:line for each repair are in section 7 of report.md.

## THE SHORT VERSION FOR WHOEVER PICKS THIS UP

1. N=8 instead of N=4: +0.1716 on the one subject that has both bakes. No code.
2. The height fill outside coverage should DILATE 8 texels, not jump to the
   card plane: +0.067 / +0.056 / +0.055 / +0.033 / +0.008 on the five subjects,
   src/lodgen.cpp:2659-2660. This also undoes the rock regression and clears
   the pre-repair number by +0.039.
3. alphaThreshold default 0.0627 -> 0.20: gains on all five, nothing vanishes
   at any threshold up to 0.45 on any view. src/gl/impostordraw.cpp:477-478.
4. Height-consistency rejection at 1 span-step: +0.028 / +0.022 on the bare
   trees, -0.065 ON THE ROCK, so it is per subject-class or not at all.
5. The `_n` height<->sway swap is a FORMAT CONTRACT change and needs bungo's
   ruling. It makes height 20-40x more accurate (+0.038 IoU on both blasted
   trees) and costs sway a factor of 4-8.
6. `tests/spells/impostor_bc_decode.py:26-28` has a BC3 alpha ramp bug,
   always reads LOW, weakens gate row 14. Corrected copy: `bcdec2.py`.

## OWED BY A BUILD LANE, exactly

* `inner` dumped at src/nifskope_ui.cpp:23115 BEFORE `.scaled(...)`, PNG, per
  view, for `matte()` and `channel(9)`, on blast_n4 and maple_n4. Section 1 is
  measured at ratio 4 without it.
* N=8 bakes for maple_n4, dead_n4, rock_n4.
* Harness card AND mesh grabs for blast_n12 and blast_n5,
  `control/<tag>_after_b1/v_az%03d_el%02d_{card,mesh}.png`, same 24 views.

## OWED BY BUNGO

* The alpha-threshold ruling: 0.20 (peak on three of five, gain on all five) or
  0.30 (better on the two blasted trees, worse on the maple and the rock).
* The `_n` channel-swap ruling: it invalidates every baked sheet.
* Whether the height rejection may be per subject-class.

## UNEXPLAINED, needs its own lane

blast_n5's temporal number is WORSE than blast_n4's: 2-degree mean 0.3917
against 0.1943, worst step 0.5633 against 0.4242. Nothing in this report
explains an odd N behaving worse than a smaller even N.

## THE INSTRUMENT, if it is picked up again

`inst.py` (the card), `bcdec2.py` (corrected BC3 decoder), `bcenc.py` (the
encoder, gated exact against the shipped DDS on all-covered blocks), `calib.py`
(registration + the five-subject known-answer control), `s1_downsample.py`,
`s2b_ghost.py`, `s2c_ladder.py`, `s3_rock.py`, `s3b_shift.py`, `s3c_attrib.py`,
`s45_thresh.py`, `s6_swap.py`, `pic_*.py`. Raw output in `out_*.txt`,
registration in `calib.json`.

RUN calib.py FIRST if any fixture changes; every script reads calib.json.
Set `calib.REFH = 448` for anything that renders more than a few hundred views;
it costs at most 0.0013 IoU and is 5x faster (measured, section 2).
