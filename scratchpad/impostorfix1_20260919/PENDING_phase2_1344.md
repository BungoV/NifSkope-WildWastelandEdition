# IMPOSTORFIX1 -- state at 2026-09-19 13:44 (phase 2 start, build + exe slot held)

## Where phase 1 got to (all offline, all on the fixture .DDS)

A numpy reference card written from `docs/LODGEN_IMPOSTOR_SPEC.md`, reading the
same BC3 `.DDS` the viewer reads, scores **0.358** mean over the 24 orbit views.
The viewer measured **0.3546**. The GLSL/C++ draw is therefore a faithful
transcription of the spec; the fault is in the SHEET.

- **GUILTY -- height on partially covered texels. 0.358 -> 0.428.**
  Height is averaged down over a background that is the FAR PLANE and is never
  un-premultiplied or refilled (colour is). A texel at 30% coverage decodes to
  +765 world units, max +1536, against a card half-width of 135. 61.6% of all
  card ink comes from texels that were transparent before the parallax step.
  Measured fix in the reference: dilate height from fully covered neighbours.
- **GUILTY (minor) -- BC1-blue 5-bit height + oversized depthSpan. 0.358 -> 0.382.**
  `depthSpan` 3072 vs a measured object depth of 590 (99.5th pct of fully
  covered texels, pre-compression). One BC1-blue block step = 99 world units =
  0.73 of the card half-width.
- **CLEARED -- `frameOffset`.** Independent known answer (the trunk base is one
  world point, so its place across the 12 horizon frames must trace a sinusoid
  in azimuth): shipped law rms 2.16 world units, not applied 7.16, opposite
  sign 14.51. The shipped subtraction is right. DO NOT RE-LITIGATE.
- **CLEARED -- step clamping (hurts: 0.319/0.323), coverage decode, mips,
  frame pick.**

Scripts: `refcard.py` (the reference), `bcdec.py` (BC1/BC3 decoder, validated:
0.014 mean error against the albedo PNG on covered texels), `ablate.py`,
`counterfactual.py`, `strip2.py`, `texelpic.py`.

TRAP, already paid for once: `*_oct_albedo.png` / `*_oct_normal.png` are dumped
BEFORE the dilate-and-flood pass. They are NOT the pre-compression twin of the
`.DDS`. Only COVERED texels are comparable between them.

## Phase 2 work list (coordinator, 13:44)

1. Known-answer control: shoot the mesh at the EXACT bake directions, score
   single un-blended frames, set the floor from the measurement.
2. Repair the bake: partial-coverage height; depthSpan fitted; height precision
   (format contract -- RULING OWED, ship the in-spec option too).
3. Re-bake fixtures, re-run `tests/spells/impostor_draw.sh`, raise the floor,
   gate rows that fail on exe 88d6abb3, bake-time self-check on height range.
4. Regenerate orbit strips / GIFs / distance strip + `00_before_after.png`. LOOK.
5. Numbers before -> after, exe facts, WW_CHANGES + HANDOFF text, the skill,
   MISTAKES.

## Exe / rungs

Inherited `release/NifSkope.exe` 23,346,176 B 13:32:07 sha1 88d6abb3...
Rung `release/NifSkope.before_impostorfix1.exe` to be taken ONCE before the
first build. Never touch another rung.

## Bake directions the control needs (N=4, 16 frames)

12 sit on the horizon (elev 0) at azim 0, 26.57, 63.43, 90, 116.57, 153.43,
180, 206.57, 243.43, 270, 296.57, 333.43. Four sit at elev 63.43 at azim
0, 90, 180, 270. NO render on disk is at any of them (every one is elev 15/45).

## Progress log

- 13:44 phase 2 opened, PENDING written, nothing built yet.

## 13:52 -- THE KNOWN-ANSWER CONTROL, and it found a defect in the INSTRUMENT

Run: `control_run.sh <tag> <lodm> <mesh> <views> <blend>`, views from
`bakeviews_n4.txt` (the 16 N=4 bake directions) via the new
`WW_IMPOSTOR_ORBIT_VIEWS`.

**First run scored 0.7455 -- because the MESH grab had a GRID in it.**
`Scene::drawGrid` (src/gl/glscene.cpp:632) returns early in orthographic mode
unless `GLView::axisAlignedViewState()` names a face view, so at azim
0/90/180/270 elev 0 AND NOWHERE ELSE a screen-plane lattice is painted. The
card grab runs with the scene suppressed and never gets it, so the lattice
joins the union and never the intersection. Mesh coverage 0.0746 at those four
against 0.0250 at the other twelve; IoU 0.31-0.35 against 0.86-0.91. It looks
exactly like a defect in the card. Fix: `scratchpad/impostorfix1_20260919/fix01_grid.py`
clears `Scene::ShowGrid` beside `Scene::ShowAxes`. Exe 13:51:55.
(The old 24-view orbit was NOT contaminated: elev 15/45 is never axis-aligned.)

**Then the control passes. blast_n4, 16 bake directions, BLEND=0:
mean 0.8823, range 0.8563 .. 0.9107, all 16 counted.**
A single un-blended frame photographed from its own bake direction IS a
photograph of the mesh. THE DRAW IS NOT THE DEFECT. Floor from measurement:
0.85.

## 13:55 -- the decomposition (same exe, same lodm, same mesh)

| views | blend/parallax | IoU |
|---|---|---|
| 16 bake directions | OFF | **0.8823** |
| 16 bake directions | ON  | **0.6507** |
| 24 orbit (12 az x el 15/45) | OFF | 0.4401 |
| 24 orbit | ON | 0.3546  (reproduces the shipped number exactly) |

At a bake direction `pickFrames` gives the other two frames weight 0 and the
shader `continue`s, and the camera is orthographic so `ray` is antiparallel to
`frameFwd` and the parallax is a MATHEMATICAL NO-OP. It costs **0.23**.
That is the height channel, in the viewer, at the one place it cannot be
blamed on anything else. Phase 1's GUILTY verdict confirmed live.

NEXT: repair the bake (partial-coverage height, fitted depthSpan), re-bake,
re-measure all four rows. The gate is row 2 rising toward row 1.
