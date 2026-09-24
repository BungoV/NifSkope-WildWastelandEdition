# Lane WATER6 -- the solver consumes what the window stores, and the flow PNG speaks DirectX

Repo `E:\Projects\NifskopeWildWastelandEdition`, branch `main`. **Nothing
committed** (CONSTITUTION 8). Run by lane BUILD10 on 2026-09-10, after WATER4
and WATER5 were built and gated in the same session.

Read first: `CONSTITUTION.md`, `scratchpad/water5_20260910/CHANGE_NEEDED.md`
(C1, C2, C3 -- what the solver owed the window, written by the lane that could
not make the change), `scratchpad/lane_water4_report.md` (the solver),
`scratchpad/lane_water5_report.md` (the window and the PNG codec).

The four things this lane owes, in the director's words:

1. the solver consumes **per-point weights** (C1);
2. the solver consumes **one-point pins** (C2) -- "a one-point pin acts as a
   source/sink";
3. the solver consumes the **imported raster source layer** (C3) -- "authority
   where painted";
4. the flow PNG export/import uses the **DirectX normal-map convention**:
   R = +X, G = +Y toward the image BOTTOM, both centred on 128; B = speed,
   A = confidence. `src/watercurves.cpp` wrote `+green = north`.

---

## 0. The gates, PRE-REGISTERED before any code was written (CONSTITUTION 1)

Written 2026-09-10 16:3x, before the first line of C++ for this lane. Every
number below is what the harness will print; a gate that fails is reported
failed with its number and never re-registered.

They run inside `lodl <copy> --water-mark-selftest` (the marking tool's own
harness, which already carries WATER4's F gates) and are read back BY NAME by
the new spell `tests/spells/water_weights.sh`, so a gate that did not run is a
failure rather than a pass.

| gate | what is measured | passes when |
|---|---|---|
| **X1a the weight FLOOR** | the largest river marked with one centreline stroke carrying NO trailing weights, solved, its flow words hashed; the identical stroke carrying an explicit weight of exactly 1.0 at every point, solved, hashed | the two hashes are **equal** -- weight 1 everywhere is the unweighted solve, byte for byte |
| **X1b the weight SIGNAL** | the same stroke with the weights ramped 1.0 -> 3.0 from its first point to its last, solved, hashed; and the mean SPEED NIBBLE over the body's texels under the last third of the stroke against those under the first third, in both the unweighted and the weighted solve | the hash **differs** from X1a's; the weighted solve's (last third - first third) nibble difference is **greater** than the unweighted solve's, and both numbers are printed |
| **X1c the weight is LOCAL** | in the weighted solve, the fraction of the body's texels whose word changed from the unweighted solve | > 0 and < 100 percent, and the number is printed (a weight that changes every texel of a 25,000-texel body equally is not "where the weight is") |
| **X2a a one-point pin is CONSUMED** | the river marked with a single kind-1 stroke of ONE point, solved: the number of strokes the solver counted and the number of the body's texels whose word moved off the automatic word | strokes >= 1 and moved > 0. **The refuter, run first**: the same file with that pin REMOVED has 0 texels moved, so the gate can fail |
| **X2b it acts as a SOURCE** | over the body's wet texels within four pin widths of the pin, the mean cosine between the solved direction and the outward radial from the pin | > 0.5, printed either way (a source pushes water away from itself; a sink would be < -0.5 and the sentence would say so) |
| **X3a a raster is AUTHORITY where painted** | a raster layer covering a square window of the marked river, every word set to one constant that the solve does not produce, stored as a kind-10 mark: the number of painted texels whose `flowWordOf` is that constant | all of them, and the count is printed |
| **X3b the solve fills the REST** | the body's texels OUTSIDE the painted window whose word equals that constant | 0, and the count of texels outside the window that carry the SOLVED word is > 0 (the floor: the body is not entirely painted) |
| **X3c the raster is not baked in** | the same layer removed, the words read again | every texel returns to the word it had before the layer was added |
| **X4 export -> import** | `water_window.sh`'s W5, re-run on the new convention: the flow map exported as a PNG and imported back, the raster's words against the document's own | 0 differ (unchanged by this lane: both halves move together) |
| **X5a the convention, by hand** | `WaterCurveDoc::rgbaFromWord` for the four cardinal directions | east (dir 0) R=255 G=128; north (dir 64) R=128 G=1; west (dir 128) R=1 G=128; **south (dir 192) R=128 G=255** -- green grows toward the image bottom |
| **X5b the codec round-trips** | every one of the 256 x 16 x 16 = 65,536 words: `rgbaFromWord` then `wordFromRgba` | 65,536 of 65,536 equal (proved in numpy before the C++ was written: 0 failures) |
| **X5c a CHECKED-IN test image** | `tests/fixtures/flowmap_directx_4x4.png`, written by `scratchpad/build10_20260910/make_directx_fixture.py` and committed with the tree, decoded by the shipped `wordFromRgba`: 16 texels whose expected words are documented in this report | all 16 decode to the documented word. This is the only gate that tests the convention against a FILE rather than against our own encoder |
| **X6 the flipped green is still refused** | `water_window.sh`'s W6, re-run: the exported PNG with G mirrored is refused naming the green channel, the layer count does not move, and the unflipped one right after is accepted | all three, and the refusal sentence now says **+green = south** |
| P0-P8, F1-F8, `water_mark.sh`, `water_flow.sh`, `lodl_water.sh`, `lodl_open.sh`, `water_window.sh` | re-run | unmoved except where this lane says so |

**What would make this lane WRONG.** X1a is the refuter for the whole weight
change: if a weight of 1 does not reproduce the unweighted solve byte for
byte, the weight has been folded in somewhere it does not belong and every
existing marked file would re-bake differently. X3c is the refuter for the
raster: a raster that survives its own removal is a bake, not an authority.
X5c is the refuter for the convention: the two halves of a round trip can
agree perfectly while both being wrong, and only a file written by hand from
the written-down rule can catch that.

---

## 1. The method, stated before the code

**C1, per-point weights.** `WaterStroke::extra` carries one float a point for
kinds 0/1 (hook-up H2, lane WATER5). `solveBody`'s `Seg` gains the two
weights of its endpoints; the conductance bump under the stroke, which was
`1 + (kStrokeBoost - 1) q^2` with `q` the quartic falloff, becomes
`1 + (kStrokeBoost * w - 1) q^2` with `w` interpolated along the segment. At
`w = 1` that is the old expression character for character, which is why X1a
can be a byte gate. The same interpolated weight also multiplies `|u|` before
the speed nibble's mean is taken, so a weighted reach reads faster without
the direction being asked to change.

**C2, a one-point pin.** `solveBody` skipped any kind 0/1 stroke with fewer
than two points. A one-point curve now enters as a SOURCE disc of its own
width -- the same treatment a stroke's first point already gets. It is read as
a source rather than a sink because the store already has an `OutletPin`
(kind 5) for a sink, and because a source is what "pour the river in here"
means; the report says so and the gate measures the sign rather than assuming
it.

**C3, the raster layer.** `WaterMarkDoc::flowWordOf` asks the raster layers
BEFORE the solved field and before the automatic word, last layer painted
wins. The layers are decoded once into a cache off the kind-10 marks
(`WaterRasterLayer::fromPayload`, lane WATER5's own decoder -- not a second
one) and the cache is dropped whenever the marks change. The solve is not
touched at all: "authority where painted" is a question about the word the
document WRITES, not about the field it solves.

**The DirectX convention.** `rgbaFromWord` / `wordFromRgba` become
`v = 128 + round(127 c)` and `c = (v - 128) / 127`, with the green channel
carrying `-sin(theta)` so that green grows toward the image BOTTOM -- the
convention every DirectX-era normal map and every flow map written by
Substance, Houdini or a game engine's own exporter uses. Proved in numpy
before the C++: 0 round-trip failures over all 256 directions, against 0 for
the old mapping, so the change costs no precision.

---

## 2. What was changed, file by file

| file | change |
|---|---|
| `src/watermark.h` | `#include "watercurves.h"`, `#define WATERMARK_RASTER_AUTHORITY 1`, and the raster cache (`rasterCache`, `rasterSrc`, `syncRasters()`) |
| `src/watermark.cpp` | `syncRasters()`; `flowWordOf` asks the layers first; `Seg` carries `wa`/`wb`; `weightAt()` reads the trailing floats; a ONE-POINT curve becomes a source disc; the conductance bump and the speed nibble take the weight; `waterWeightGates()` and its call at the end of the self-test; `kDirectXFixtureDirs` |
| `src/watercurves.cpp` | the DirectX codec (`wordFromRgba`, `rgbaFromWord`) and the refusal sentence |
| `src/waterwindow.cpp` | ONE line: the self-test re-reads `win->document()` after it reopens the file (section 5) |
| `tests/spells/water_weights.sh` | NEW, the spell that reads X1..X5 back by name |
| `tests/fixtures/flowmap_directx_4x4.png` | NEW, 112 bytes, written from the rule by `scratchpad/build10_20260910/make_directx_fixture.py` |

`NifSkope.pro` untouched: no new translation unit. `src/watermark.h` gained a
NEW include, so `qmake` ran before `make` and `Makefile.Release`'s block for
`watermark.o` now names `src/watercurves.h`.

## 3. The build

`release/NifSkope.exe` **16:45:53**, 20,007,936 bytes. `style.qss` in step.
bungo had his own window open (pid 20560, no `--port` on its command line, so
his and not a harness's) and the exe was **renamed aside, never killed**; his
window keeps `release/NifSkope_inuse_20560.exe`.

All five objects that include `watermark.h` are newer than it; the exe is newer
than every file this lane changed. `g++ -fsyntax-only` with the real flags:
rc=0, no warnings, on `watermark.cpp`, `watercurves.cpp` and `waterwindow.cpp`.

## 4. The gates, as they ran

| gate | body 2 (the marsh, `water_weights.sh` / `water_mark.sh`) | body 3 (the Charles, `water_flow.sh`) |
|---|---|---|
| **X1a the weight FLOOR** | **green** -- hash `4fcec45d675860cd` both ways over 29,312 texels | green |
| **X1b the SIGNAL** | green -- `892add8859f33dee` against the unweighted hash | green |
| **X1b reads faster where the weight is** | green -- nibble difference **1.28 weighted against 1.10 unweighted** (first third 0.80/0.90, last third 2.08/2.00) | green -- **1.08 against 0.54** (6.85 -> 7.92 against 7.15 -> 7.69) |
| **X1c the weight is LOCAL** | green -- 22,438 of 29,312 texels (76.5 %) | green -- 15,671 of 25,114 (62.4 %) |
| **X2a the REFUTER** | green -- 0 texels move with the pin removed | green |
| **X2 the pin lands on the river** | green | green |
| **X2a a ONE-POINT curve is consumed** | green -- 1 stroke counted, **29,310 of 29,312 texels moved** (it was 0 before) | green |
| **X2b it acts as a SOURCE** | green -- mean cosine **0.742** over 13,759 texels | **RED as registered -- 0.371** over 9,706 texels against 0.5 (section 6) |
| **X3 the layer is stored** | green -- 190 painted wet texels, 198 bytes | green |
| **X3a AUTHORITY where painted** | green -- **190 of 190** | green |
| **X3b the solve fills the rest** | green -- **0** texels outside the layer moved from the layer-free solve, 29,122 did not | green |
| **X3c not baked in** | green -- with the layer removed **0 of 29,312 differ** from the layer-free solve | green |
| **X4 export -> import** | green -- `water_window.sh` W5: **0 differ of 21,754,958** painted texels, on the new convention | -- |
| **X5a the cardinals** | green -- east R255 G128, north R128 G1, west R1 G128, **south R128 G255** | -- |
| **X5b the codec round-trips** | green -- **65,536 of 65,536** | -- |
| **X5c the CHECKED-IN image** | green -- **0 of 16 wrong** | -- |
| **X6 the flipped green is refused** | green -- refused at 0.151 as-is against 1.000 mirrored over 173,568 texels, layer count unmoved, the control accepted right after, and the sentence now says **"+green = south, the DirectX convention"** | -- |

`tests/spells/water_weights.sh` **PASS**, 16 WATER6 gates green against a floor
of 15.

**The neighbours, and what moved in them:**

| harness | before WATER6 | after |
|---|---|---|
| `water_flow.sh` (body 3) | 47 checks, 2 failures | **63 checks, 3 failures** -- the 16 new checks, of which X2b is red on this body |
| `water_mark.sh` (body 2) | 47 / 8 model, 20 / 0 dock | **63 / 8** model, **20 / 0** dock -- all 16 new checks green, the 8 are WATER5's and WATER4's known body-2 reds |
| `water_window.sh` | 46 / 2 | **46 / 2** -- the same two (the dye-pin weight, the body name), and W4/W5/W6 run again after the crash below was repaired |
| `lodl_water.sh` | PASS | PASS |
| `lodl_open.sh` | 23 / 0 | 23 / 0 |

**F1-F8 and P0-P8 did not move**: F2's island bank is still 12.01 / 22.40 and
F5's p99 is still 8.44, to the same two decimals as before the weight change.
That is the evidence that the unweighted solve is untouched, and it is a
stronger statement than X1a alone because it covers the whole file, not one
body.

## 5. What the first run found, and what was done about it

Three things went red on the first run of these gates. None of them was the
change under test; two were the gates and one was an old use-after-free.

**(a) A segmentation fault in the water window's self-test, exit 139.**
`WaterWindow::openFile` does `delete doc; doc = new WaterMarkDoc();`, and
`runWindowSelfTest` captured `WaterMarkDoc * doc = win->document();` ONCE at the
top and reopened the file at gate W3. Everything after that used a freed
object. It had never crashed because `sweep()`'s first act was to read a bool
and a pointer that happened to survive in the freed block; this lane made
`sweep()` begin by comparing two QVector members and the process died straight
after the override check, taking W4, W5, W6 and the check count with it. ONE
line repaired the instrument -- the pointer is re-read after the reopen -- and
no check, assertion or widget was touched. The defect is lane WATER5's.

**(b) X2 measured a pin that was never stored.** The pin was placed at
`axis.pts[size/2]` without asking whether that point is wet. The centreline is
the mean position of each slice's wet texels and on a winding reach that mean
lands on the bank, so `addStroke` refused it in words -- and the gate never read
the refusal, so it reported "a one-point curve is not consumed" about a pin the
document had never seen. The instrument now walks outward from the middle to
the nearest point that IS on the river, and reads `addStroke`'s answer as its
own check.

**(c) X3b called a coincidence an authority leak.** It counted texels outside
the layer whose word EQUALS the layer's constant, and exactly one of the
river's 29,121 outside texels solves to `0xFF40` on its own: the first run read
**1 of 29,121** (and X3c **190 of 191**). The comparison is now against the
LAYER-FREE SOLVE texel by texel, which is what "the solve fills the rest" and
"it is not baked in" mean; the threshold stayed 0 and the first run's numbers
are the ones above.

## 6. X2b on the Charles: red as registered, with its cause

X2b reads **0.742 on body 2** and **0.371 on body 3**, against a registered
0.5. The gate is NOT moved.

The sign is not in doubt -- a sink would read below -0.5 and this reads
positive on both bodies, and X2a shows the pin moving 29,310 of 29,312 texels
where it moved 0 before. What differs is the SHAPE of the fixture: the metric
compares the flow with the straight-line radial from the pin over a disc of
four pin widths (16,384 units = 128 texels), and the Charles bends inside that
disc while the marsh does not. The flow follows the channel; the radial does
not; the cosine falls without anything about the pin changing.

The instrument a later lane should use instead, and why it is better: the NET
FLUX through a ring of wet faces around the pin, normalised by the flux the
pin injects. A source has net outward flux equal to its injection whatever the
channel does afterwards, so curvature cannot bias it, and it would read the
same number on both bodies. That is a change to `WaterFlowGrid`'s
instrumentation, not to the solver, and it is not made here.

## 7. Housekeeping

Nothing committed (CONSTITUTION 8). LF-only throughout, CR 0 before and after
on every file, measured by Python byte count: `src/watermark.h` 21,966 ->
22,821, `src/watermark.cpp` 152,515 -> 172,863, `src/watercurves.cpp` 34,210 ->
35,147, `src/waterwindow.cpp` 95,217 -> 95,462; NEW
`tests/spells/water_weights.sh` and `tests/fixtures/flowmap_directx_4x4.png`
(112 bytes). Every edit was an anchored script under
`scratchpad/build10_20260910/` that prints its count and refuses at anything
but 1 (`w6_patch.py`, `w6_codec.py`, `w6_fix1.py`, `w6_fix2.py`, `w6_fix3.py`).

To reproduce:

```
bash tests/spells/water_weights.sh        # X1..X5 on body 2, PASS
bash tests/spells/water_window.sh         # X4 and X6, 46/2
bash tests/spells/water_flow.sh           # X1..X5 on body 3, plus WATER4's F gates
python scratchpad/build10_20260910/make_directx_fixture.py    # rewrites the test image
```

## 8. Skill review (CONSTITUTION 1a)

**Loaded and used.** `nifskope-ww-resume-pending` -- the read order, the
dependency read-back by object name, "measure the cause and STOP", and section
9's warning that a resume's numbers are PREDICTIONS, which is exactly what
happened to four of them. `nifskope-ww-build-verify` -- make's own exit code as
the gate, the exe renamed aside for bungo's window (his was open at 16:38),
the NEW-include rule that made `qmake` run first, and the syntax pass while a
build could not run. `ww-anchored-hookup` -- WATER5's `hookup.py` was already
written to its contract and `--check` was read off the MARKER, never the
anchor. `ww-texel-picture` -- the dye picture and the DirectX fixture picture.
`nifskope-ww-render-shot` -- absolute paths, one instance at a time, and the
size clamp that the framing correction turned on.
`ww-control-calibration` -- the DirectX mapping was proved in numpy against
the old mapping BEFORE the C++ was written (0 round-trip failures either way,
so the convention costs no precision), and X1a is the floor that makes the
weight change falsifiable.

**Wished for, and recommended to the director.** A short skill, or a section in
`ww-test-harness-add`, on **the gate loop's own grep**: three spells in this
tree read their gates back with `grep -F "<name>" | head -1`, and two of the
three take an INFORMATIONAL line that begins with the same words and report a
green gate red (`water_flow.sh`'s F8 today). The rule is one line --
`grep -aE '^  (ok|FAIL) '` before `head -1` -- and it has now cost two lanes.
`tests/spells/water_weights.sh` is written that way.

Declined: a skill for "consume a CHANGE_NEEDED note". `nifskope-ww-resume-pending`
section 2 already says it, and this lane's only addition is that a note's
RECOMMENDATION (C2 preferred a speed pin) is not the director's ruling (a
source/sink), which is a reading-comprehension rule, not a procedure.

## 9. The pictures

| file | what it shows |
|---|---|
| `scratchpad/build10_20260910/images/directx_convention.png` | the CHECKED-IN test image at texel level, 4 x 4 texels drawn 170 px square, each with its own R and G printed on it, an arrow for the direction, and the direction the shipped decoder reads back. North is DARK green (G = 1) and points up; south is BRIGHT green (G = 255) and points down -- which is the whole of the convention in one picture. The blue cast is B = A = 255 (speed 15, confidence 15), so only R and G carry anything under test |
| `scratchpad/build10_20260910/images/water_window_whole.png` | the window at first open after this lane's build, the whole worldspace fitted (0.12 px a texel); every row still one to a line |
| `scratchpad/build10_20260910/images/water_window_mouth.png` | 2.00 px a texel at the river, the five-point curve with its width band, the source pin and the dye pin, and the summary line naming body 2 "harness river" |

All three were opened and read; the window pair was grabbed from inside the
application, never a desktop capture, with an ABSOLUTE `SHOT` path.
