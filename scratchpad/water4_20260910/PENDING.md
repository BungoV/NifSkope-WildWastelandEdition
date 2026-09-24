# Lane WATER4 — BUILD PENDING (paste-able resume)

The one gate check (2026-09-10, after all code was written): `scratchpad/water4_20260910/GO`
did NOT exist (the director creates it after bungo's timed bake), `tasklist |
grep -i -E "Fallout4|NifSkope"` printed nothing (rc=1). Per the brief, no
build, no run, no poll. Everything below is written, `g++ -fsyntax-only`
clean with the real `Makefile.Release` flags (rc=0 on `src/watermark.cpp`,
`src/watermarkpanel.cpp`, `src/lodtfile.cpp`), and UNRUN.

Read first: `CONSTITUTION.md`, `scratchpad/lane_water4_report.md` (sections 0
and 2 are the gates and what the prototype predicts for them),
`scratchpad/lane_water3_report.md` "Build" section (the harness chain that
already exists), skill `nifskope-ww-resume-pending`.

## What is on disk (all LF-only, measured by Python byte count)

| file | change |
|---|---|
| `src/watermark.h` | 15,060 -> 21,377 bytes: kinds 7/8/9, `colour[4]`, `WaterFlowGrid`, the dye API |
| `src/watermark.cpp` | 82,104 -> 151,755 bytes: the solver core, `solveBody` + `solveDye` replace the harmonic fill, the dye plane codec and packer, the flow gates and F5/F8 and the dye round trip inside `lodtWaterMarkSelfTest` |
| `src/watermarkpanel.cpp` | 47,081 -> 51,314: Tool "Dye pin", rows "Dye colour" / "Dye fade", tick "Dye at mouth", Show "Dye", the dye plane painted, `WaterMarkDyeFadeSpin` in the one-row list |
| `src/lodtfile.h` / `.cpp` | 20,346 -> 21,491 / 140,908 -> 141,680: `LODL_SECT_DYE = 1 << 8`, the dye store read from the 0xF4 word, `dyeWordAt`, `dyePlaneSamples`, `dyePlaneOffset` |
| `tests/spells/water_flow.sh` | NEW, 114 lines |
| `docs/LODGEN_BTD_FORMAT.md`, `scratchpad/specs_20260909/spec_water.md`, `WW_CHANGES.md`, `MISTAKES.md` | the dye plane, 4.3 as rebuilt, the F gates, four ledger entries -- every one says BUILD PENDING |

No `NifSkope.pro` change: no new translation unit. `src/nifcli.cpp` untouched:
the flow gates run inside the existing `--water-mark-selftest`.

## Step 1 — the gate, then the build

```
tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?          # must print rc=1
cd /e/Projects/NifskopeWildWastelandEdition
bash scratchpad/water3_20260910/syn.sh src/watermark.cpp src/watermarkpanel.cpp src/lodtfile.cpp   # rc=0 today
MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc 'export PATH="$PATH:/e/Tools/GIT/cmd"; cd /e/Projects/NifskopeWildWastelandEdition && make -j2 > scratchpad/water4_20260910/build.log 2>&1; rc=$?; grep -E "error:|Error [0-9]" scratchpad/water4_20260910/build.log | head -20; echo BUILD-RC=$rc; exit $rc'
```

`watermark.h` changed and is included by `watermark.cpp`, `watermarkpanel.cpp`
and `nifcli.cpp`; `lodtfile.h` changed and is included by many more. No new
`#include` crosses into `lib/`, so qmake need not re-run, but READ THE
DEPENDENCY BACK per the resume skill (`grep -n "watermark\.h" Makefile.Release`
and the `awk` walk) before trusting the objects. Then the exe-newer sweep over
all five changed `src/` files, not one.

## Step 2 — the harnesses, in this order

```
bash tests/spells/water_mark.sh      > scratchpad/water4_20260910/gate_water_mark.txt 2>&1; tail -3 scratchpad/water4_20260910/gate_water_mark.txt
bash tests/spells/water_flow.sh      > scratchpad/water4_20260910/gate_water_flow.txt 2>&1; tail -3 scratchpad/water4_20260910/gate_water_flow.txt
bash tests/spells/lodl_water.sh      > scratchpad/water4_20260910/gate_lodl_water.txt 2>&1; tail -2 scratchpad/water4_20260910/gate_lodl_water.txt
bash tests/spells/lodl_open.sh       > scratchpad/water4_20260910/gate_lodl_open.txt 2>&1; tail -2 scratchpad/water4_20260910/gate_lodl_open.txt
```

What each must print, and what the prototype PREDICTS will be red:

* `water_mark.sh`: model checks >= 14 and PASS, dock checks >= 16 and PASS.
  The model half now carries ~30 more checks (the flow gates, F5, F8, the
  dye). **Predicted red as registered: "F2 island bank direction" (12.0 mean
  / 22.4 max against 5 / 15) and "F5 the 99th percentile" (8.4 against 5).**
  Neither gate is to be moved; report the numbers. If a red is anything else,
  it is the C++ diverging from the prototype: compare against
  `python scratchpad/water4_20260910/flow_proto.py` line by line.
* `water_flow.sh`: reads the same gates back by name plus the independent
  decoder on the marked file (patches must be 0).
* `lodl_water.sh`: 56 checks, PASS, unmoved (the writer did not change; the
  reader gained one section it never sees on a generator file).
* `lodl_open.sh`: 23/0.

## Step 3 — the pictures (`nifskope-ww-render-shot`, ABSOLUTE paths)

The marked file the harness leaves: `scratchpad/water4_20260910/work/flow.lodl.bak-watermark`
(after `water_flow.sh`, it carries the Charles stroke AND the DyeMouth mark).

1. The flow pair at WATER2's framing (`WW_LODL_REGION=-16,-21,-6,-4,0`,
   `WW_LODL_PLANE=flow`, top view, flat, 1500x1000) -- exactly
   `scratchpad/water3_20260910/make_pair.py`'s two renders, with the AFTER
   file replaced by `work/charles_marked_v4.lodl`; the BEFORE render must
   again be byte-identical to `water2_20260909/images/charles_flow.png`.
   Write `images/charles_flow_pair_v4.png` with captions from `flow_mean.py`
   and `disc_metric.py` (both numbers, before and after).
2. The dye plane at the Charles mouth: `src/btdterrain.{h,cpp}` has NO `dye`
   plane key (not this lane's file), so the picture is a TEXEL picture from
   Python: `lodl_np.py` reads the dye store (`d.dyeStore`, 4 bytes a sample);
   crop the 5 L window round the mouth (the harness prints the mouth texel),
   paint weight over the body-id hash, caption the mean weight within L/2 and
   the max beyond 3 L. Save `images/charles_dye_mouth.png`.
3. The synthetic channel with its speed: the F1 gate's numbers are printed by
   the harness; the picture is `flow_proto.py`'s F1 grid drawn with the
   speed as brightness (write a 20-line `make_channel.py` beside it).
4. Open every PNG.

## Step 4 — the four documents stop saying BUILD PENDING

`WW_CHANGES.md` (the WATER4 entry at the top), `MISTAKES.md` (add what the
build found), `docs/LODGEN_BTD_FORMAT.md` (the dye section's BUILT line),
`scratchpad/specs_20260909/spec_water.md` (4.3, 3.7b, the F gate table:
replace the prototype column with the C++ numbers), and the lane report's
section 5 (gates) and 6 (pictures). Line endings by Python byte count;
`WW_CHANGES.md` is mixed (CR 19,020) and its top entries are LF-only.

Then `touch scratchpad/water4_20260910/DONE`.
