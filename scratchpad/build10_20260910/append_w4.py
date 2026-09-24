#!/usr/bin/env python3
"""Lane BUILD10 -- append the Build section to lane WATER4's report.

Append-only: the lane's own text is not rewritten.  LF-only, CR asserted 0.
"""
import os

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
P = os.path.join(ROOT, "scratchpad", "lane_water4_report.md")

TEXT = """
---

## Build (BUILD10, 2026-09-10)

`release/NifSkope.exe` **15:52:46**, `release/style.qss` in step. `make -j2`
had **nothing to do**: an earlier build lane had already compiled this lane's
five changed sources into the running exe, and the consistency sweeps say so
rather than the exit code -- `watermark.o` 14:04:17, `watermarkpanel.o`
15:52:17, `lodtfile.o` 14:04:09, `nifcli.o` 14:34:24, `btdterrain.o` 14:03:56,
`lodvfile.o` / `lodgenmanager.o` all newer than `src/watermark.h` (04:33:22)
and `src/lodtfile.h` (04:10:41), and the exe newer than every one of them.
`Makefile.Release`'s dependency blocks name both changed headers for all seven
objects (the `awk` walk, not `grep -A`), so no hand patch and no qmake run was
owed. No new translation unit, so `NifSkope.pro` was not touched.

`tasklist` printed no `Fallout4.exe` and no `NifSkope.exe` before the build and
before every launch.

### The gates

| harness | result | notes |
|---|---|---|
| `water_flow.sh` (the Charles, `WW_WATER_MARK_BODY=3`) | **47 checks, 2 failures** | the two failures are EXACTLY the two pre-registered as expected red |
| -- F1 continuity / flux | green | ratio 2.0000 (460 it, 9.3e-10); flux deviation 1.63e-10 |
| -- F2 parts / mass / straight bank | green | 0.5000 + 0.5000; 3.05e-12; 3.93e-4 over 416 texels |
| -- **F2 island bank vs the analytic cylinder** | **RED as registered** | mean **12.01**, max **22.40** deg over 48 bank texels against 5 / 15 -- the prototype's number to two decimals |
| -- F3 / F4 | green | 0 exactly; 0 of 3,225 point away, mean cosine 0.914; 40 of 40 streamlines |
| -- F5 patches / seams / mean | green | **0 patches** (the disc fill had 39); seams 0.422 % (2.97); cos 1.000, R 0.794 |
| -- **F5 p99** | **RED as registered** | **8.44** deg against 5 (prototype predicted 8.4) |
| -- F6 / F7 | green | plume 98.2 vs 96 texels, 0.0 vs 0.0 deg; 0.5000 / 0.1250 / 0 |
| -- F8 cost | green in the log | **0.319 s**, 1,493 iterations, residual 9.26e-10 |
| -- the dye chain | green | 17,218 texels, source body 3, painted on body 1; 230.7 within L/2; 34 beyond 3 L; 8,649/8,649 read back; P8 byte-identical; P3 undo 0 bytes differ |
| -- the independent decoder | green | `disc_metric.py` on the saved file: 0 patches, p99 8.44, seams 0.422 % -- the same numbers the app printed |
| `water_mark.sh` | model **47 / 8**, dock **20 / 0 PASS** | the model half runs on **body 2**, not the Charles -- see below |
| `lodl_water.sh` | **RESULT PASS**, 33 `ok` lines | the writer did not change; it prints no count, so the count is mine |
| `lodl_open.sh` | **23 checks, 0 failures, PASS** | unmoved |

Skipped, with the reason: every harness that reads no `.lodl` and builds no
water panel (collision, block list, impostor, atlas, arrays, merge, VT,
render_shot) -- this lane changed `watermark.*`, `watermarkpanel.cpp` and the
`.lodl` reader only, and `render_shot.sh` was exercised in substance by the two
picture renders below.

### The six reds that are not the two registered ones

**Four of them are `water_mark.sh`'s model half measuring a different body.**
That run picks body **2** (the marsh, 29,312 texels) as "the river"; the F5
gates and the dye gates were registered on body **3** (the Charles). On body 2
the numbers are p99 9.84, seams 0.840 %, cos 0.763 -- close to this lane's own
held-out prediction for the marsh (11.3 / 1.0 % / R 0.53), so the method is
behaving as it was measured to behave, on a body whose gates were never
registered. Its four dye reds have ONE cause and the instrument states it
itself: *"body 2 and the body it drains into (19) do not touch, so its dye has
no mouth to leave by"* -- 0 receiving fields, therefore 0 dyed texels, therefore
four gates with nothing to measure. Not fixed here (rule: a verdict, not a
cure); the candidates for the director are (a) pin `WW_WATER_MARK_BODY=3` in
`water_mark.sh` as `water_flow.sh` already does, or (b) let the dye gates
report `n/a` by name when the receiving field is empty.

**Two are defects in `water_flow.sh` itself**, found by running it for the
first time (`nifskope-ww-resume-pending` section 9 predicts exactly this):

1. the gate loop does `grep -F "F8 the solve" | head -1`, and the harness prints
   an INFORMATIONAL line `F8 the solve: 0.319 s, ...` immediately BEFORE
   `  ok   F8 the solve of body 3 runs under 1.0 s ...`. The grep takes the
   informational line, sees no leading `  ok `, and calls a green gate red;
2. `flow gates green: 17 (floor 18)`. There are 19 `F[1-8]` gates and the lane
   pre-registered TWO of them as expected red, so 17 is the arithmetic the lane
   itself predicts. The floor and the prediction were registered inconsistently.

Neither was re-pinned and neither was repaired: both are one-line changes to
`tests/spells/water_flow.sh`, and the brief for this build says report, never
re-pin.

### The pictures (`scratchpad/water4_20260910/images/`)

| file | what it shows |
|---|---|
| `charles_flow_pair_v4.png` | the generator's single direction beside the potential-flow solve, ONE framing. The framing is proved, not asserted: the BEFORE render taken today is byte-identical to `water2_20260909/images/charles_flow.png` (30,905 bytes), and `make_pair_v4.py` refuses if it is not |
| `charles_dye_mouth.png` | the dye plane at the Charles's mouth at texel level, drawn from the file's bytes through WATER2's independent decoder (there is no `dye` plane key in `src/btdterrain.*` to render). Every dyed texel names body 3 as its source and lies on body 1 |
| `flow_channel_f1.png` | gate F1's synthetic channel with the speed as brightness: the narrows is visibly twice as bright, and the caption's ratio is re-derived by the script |

**The framing needed a correction that is worth writing down.** WATER3's
`make_pair.py` docstring says the framing is "1500x1000". It is not: the render
hook honours `WW_RENDER_SIZE`'s WIDTH exactly and takes 59 px of window chrome
off the HEIGHT, so `1500x1000` renders **1500x941** and does not reproduce the
baseline. The baseline is 1507x941, i.e. `WW_RENDER_SIZE=1507x1000`; with that,
the BEFORE render is byte-identical. Three probe renders found it.

### What is still owed

* the two registered reds are the METHOD's, measured, and stand as reported;
* the two `water_flow.sh` defects and `water_mark.sh`'s body choice are the
  director's to route;
* nothing was committed (CONSTITUTION 8).
"""


def main():
    b = open(P, "rb").read()
    assert b.count(b"\r") == 0, "the report is not LF-only"
    assert b.count(b"## Build (BUILD10") == 0, "already appended"
    t = TEXT.encode("utf-8")
    assert t.count(b"\r") == 0
    open(P, "wb").write(b + t)
    print("appended %d bytes -> %d, CR 0" % (len(t), len(b) + len(t)))


if __name__ == "__main__":
    main()
