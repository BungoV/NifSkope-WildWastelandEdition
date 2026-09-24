# Lane BUILD10 -- three water lanes landed, 2026-09-10

**`release/NifSkope.exe` 16:45:53** (20,007,936 bytes), `style.qss` in step,
nothing committed (CONSTITUTION 8). **bungo's open window predates all of it:
his next launch of `release\NifSkope.exe` is the one that has it.**

Three lanes were built and gated in order, one build each, and a fourth (new)
was written, built and gated on top:

| step | lane | state |
|---|---|---|
| 1 | **WATER4** -- the potential-flow solve and the dye | built (already in the 15:52:46 exe; `make` had nothing to do and the object sweep says why), gated, pictures, documents. `GO` and `DONE` written |
| 2 | **WATER5** -- the water window | 12 hook-up anchors applied, `qmake` then `make`, exe 16:23:22, gated, both window pictures, documents. `DONE` written |
| 3 | **WATER6** -- the solver consumes weights, one-point pins and rasters; the flow PNG speaks DirectX | written, built (16:41:23, then 16:45:53 after two gate repairs), gated, picture, documents |

## The gate tables

**WATER4** (`water_flow.sh`, the Charles, `WW_WATER_MARK_BODY=3`): **47 checks,
2 failures** -- and both failures are the two the lane PRE-REGISTERED as
expected red: F2's island bank (mean 12.01 / max 22.40 deg against 5 / 15) and
F5's p99 (8.44 against 5). Everything else green, including the whole dye
chain, and the independent decoder reads the same 0 patches and the same 8.44
off the saved file. `water_mark.sh` 47/8 model + 20/0 dock, `lodl_water.sh`
PASS (33 ok), `lodl_open.sh` 23/0.

**WATER5** (`water_window.sh`): **46 checks, 2 failures**, floor 24. W1 (house
style) all green with every floor firing, W2 byte-identical json, W4 the same
flow-word hash on a regenerated file, W5 **0 differ of 21,754,958**, W6 the
flipped green refused and the control accepted.

**WATER6** (`water_weights.sh`, NEW): **PASS**, 16 of 16 against a floor of 15.
X1a's byte floor holds (`4fcec45d675860cd` twice), X2a moves 29,310 of 29,312
texels where a one-point pin moved 0 before, X3a/b/c are 190/190, 0 and 0,
X5b is 65,536 of 65,536 and X5c reads the checked-in image 16 of 16.

**After WATER6, the neighbours:** `water_flow.sh` 63/3, `water_mark.sh` 63/8 +
20/0, `water_window.sh` 46/2, `lodl_water.sh` PASS, `lodl_open.sh` 23/0.
**F2's island bank is still 12.01 / 22.40 and F5's p99 is still 8.44** -- to the
same two decimals as before the weight change, which is the evidence that the
unweighted solve is untouched across the whole file.

## The five reds that are yours to route, none of them landed

1. **X2b on the Charles: 0.371 against a registered 0.5** (0.742 on body 2).
   The gate asks whether the water round a one-point pin points away from it,
   measured against the STRAIGHT-LINE radial over four pin widths; the Charles
   bends inside four widths and the cosine falls without the pin behaving
   differently. The sign is not in doubt. The better instrument is named in
   `lane_water6_report.md` section 6: the net flux through a ring around the
   pin, which curvature cannot bias. Not moved, not re-pinned.
2. **`tests/spells/water_flow.sh` calls a green gate red.** Its loop does
   `grep -F "F8 the solve" | head -1` and takes the INFORMATIONAL line printed
   above the ok line. One line: `grep -aE '^  (ok|FAIL) '` before `head -1`.
   `water_weights.sh` is written that way.
3. **The same spell's floor of 18 green F-gates is unreachable** while two of
   its 19 F-gates are pre-registered as red (19 - 2 = 17). A floor and a
   prediction in the same document contradict each other.
4. **`water_mark.sh` runs WATER4's F5 and dye gates on body 2**, the marsh,
   while they were registered on body 3. Four of its eight reds have one stated
   cause: body 2 and the body it drains into do not touch, so its dye has no
   mouth and 0 texels are dyed. Candidates: pin `WW_WATER_MARK_BODY=3` as
   `water_flow.sh` does, or let the dye gates report `n/a` by name.
5. **Two defects in the water window, both WATER5's:** a DYE PIN's per-point
   weight is never written (`writeTo` fills `extra` for `Stroke` and `Pin` only)
   and **the FIRST named body in any file `WaterMarkDoc` writes reads back
   nameless** (`encodeTable` gives it name offset 0 while `LodtFile::bodyName`
   spells 0 "no name"). The second one affects any file the marking tool saves
   with names, not just the harness.

**One thing WAS repaired, and it was an instrument, not a product:** the water
window's self-test held a DELETED document (it captured `win->document()` once
and reopened the file at W3). It had never crashed because `sweep()` began by
reading a bool that happened to survive the free; WATER6 made `sweep()` begin by
comparing two QVector members and the harness died with **exit 139**, taking W4,
W5, W6 and the check count with it. One line, no check or widget touched.

## The pictures (all opened)

```
scratchpad/water4_20260910/images/charles_flow_pair_v4.png     the generator's word beside the solve
scratchpad/water4_20260910/images/charles_dye_mouth.png        the dye plane at the mouth, texel level
scratchpad/water4_20260910/images/flow_channel_f1.png          gate F1's channel, speed as brightness
scratchpad/water5_20260910/images/water_window_whole.png       the window, whole worldspace
scratchpad/water5_20260910/images/water_window_mouth.png       the window, 2 px a texel at the river
scratchpad/build10_20260910/images/directx_convention.png      the checked-in DirectX test image
scratchpad/build10_20260910/images/water_window_*.png          the same two after WATER6's build
```

The Charles pair's framing is PROVED, not asserted: `make_pair_v4.py` refuses
unless today's BEFORE render is byte-identical to
`water2_20260909/images/charles_flow.png`. Getting there corrected a framing
error worth knowing: **`WW_RENDER_SIZE` honours the WIDTH exactly and takes 59
px of chrome off the HEIGHT**, so WATER3's "1500x1000" renders 1500x941 and does
NOT reproduce the 1507x941 baseline. `nifskope-ww-render-shot` has been amended
with the four measurements, and `ww-test-harness-add` has gained section 2b
(the gate loop's own grep, and floors that must be arithmetic on the gates
expected green) -- the rule behind reds 2 and 3 above.

## Documents written

`WW_CHANGES.md` (WATER4's entry rewritten, WATER5's rewritten, WATER6's new at
the TOP; 1,529,845 -> 1,540,610 bytes, **CR 19,020 unchanged**),
`docs/LODGEN_BTD_FORMAT.md` (the dye plane's BUILT line),
`scratchpad/specs_20260909/spec_water.md` (3.7b, 4.3, the F-gate table with a
C++ column, and a new **5.5 The water window** with the json schema),
`MISTAKES.md` (eleven entries, LF-only, CR 0 -> 0), the three lane reports each
with a `## Build (BUILD10)` section, and the new
`scratchpad/lane_water6_report.md`.

## Left in the tree

* `release/NifSkope_inuse_20560.exe` (16:23:22 = WATER4 + WATER5, no WATER6) and
  `release/NifSkope_inuse_41116.exe` (16:41:23 = WATER6 before its two gate
  repairs). **`tasklist` shows no NifSkope running now**, so both may be
  deleted; the first is the only rollback rung to "before WATER6" that does not
  need a rebuild, which is why it was not deleted here. Backups keep their own
  names.
* `sx_tmp.sh` at the repo root is NOT this lane's and was not touched.
* A live lane is editing `src/hkxmodel.{h,cpp}` and `src/hkxmodeltest.cpp`
  (16:05, mid-session). They are not in `NifSkope.pro`, so this lane's two
  `qmake` runs and three builds did not compile them -- but the next lane to run
  `qmake` after their hook-up lands should know both happened.

## Restart

**Yes.** `release\NifSkope.exe` 16:45:53. His open window (if he reopens one
before reading this) carries the older image.
