# BLENDSEAM1 -- text for the overseer to splice (the lane edited none of HANDOFF / WW_CHANGES / MISTAKES)

Exe: `release/NifSkope.exe` sha1 add1bf84 (24,093,696 B, 21:56:27), only `lodgen.o` rebuilt. Rung:
`release/NifSkope.before_blendseam1.exe` sha1 90e8a57e (= DEFAULTS2's close). Nothing committed. No
NifSkope window was open; the four `NifSkope_inuse_<pid>.exe` copies were left alone.

## HANDOFF entry

- **2026-09-23 BLENDSEAM1 (lane, Opus 5.5)** -- `lodgen_terrain_vt.sh` is green again, 45 / 0, at the
  same bar (V9a-1/-2 still a `cmp` of all four chunks). CAUSE, measured: with the edge blend on, the stock
  chunk colour writer (no `--vt`) fell back to its own colour for a neighbour cell OUTSIDE the chunk
  (`src/lodgen.cpp` stock `nbr()`, paint scoped to the chunk), while the pyramid writer (`--vt`, the one
  that ships) cross-faded into the next chunk's paint through its tile's one-cell ring (`quadColorAt`'s
  `nbr()`, `cells` sized rdim x rdim). 6,718 texels differed over the four V9a chunks, max 25 levels,
  every one within 3 px of the chunk edge; 0 with the blend off. RULING (lane): the pyramid is right --
  the contract (`docs/LODGEN_TERRAIN_VT.md` s2.5a) blends "either side of every 2,048-unit line", the
  chunk-edge "limitation" described the stock writer only, and the shipped writer never had it. FIX: the
  stock writer keeps the ring's paint in its own array (`ringPaint`, filled only with the blend on) and
  its `nbr()` reads it; `cells`, the dominant base, the cover constants and every statistic stay
  chunk-scoped. Shipped (`--vt`) bytes did not move; `--blend-edges off` did not move.

## WW_CHANGES entry

### Edge blend: the chunk's own edge is blended by both colour writers (lane BLENDSEAM1, 2026-09-23)
- With `--blend-edges quadrant` (the default), the stock chunk colour sheet (a bake without `--vt`) now
  cross-fades across the chunk's own edge into the neighbouring chunk's paint, exactly as the pyramid
  sheet always did. The two writers are byte-identical again with the blend on.
- Measured on the four V9a chunks (cells -24 24 -17 31, dim 4): pyramid vs stock 6,718 texels differ ->
  0. Step across the two internal chunk boundaries (boundary step / mean step, x / y): stock was
  1.513 / 1.644 (the blend did nothing there; blend off reads 1.510 / 1.633), now 1.340 / 1.431 on both
  writers. The 14-line interior seam per chunk is unchanged (0.908 / 0.932 / 1.046 / 1.048).
- Unmoved, byte for byte: every file of a `--vt` bake (the `.lodb` differs only in its clock lines);
  every file of a stock bake with `--blend-edges off`. A stock bake with the blend on moves exactly its
  colour DDS files (and their hash lines in the record).
- Docs: `docs/LODGEN_TERRAIN_VT.md` s2.5a (the chunk-edge limitation paragraph replaced by the measured
  fix). Gate: `tests/spells/lodgen_terrain_vt.sh` comment only -- no check changed.

## MISTAKES entries

- **2026-09-23 BLENDSEAM1 -- two progress timestamps typed from feel.** Two progress lines were stamped
  "22:0x" / "22:1x"; `date` at the build's end read 21:56. Corrected in the file and said so there. Rule
  (already standing): read the clock in the same turn as the stamp.

## Findings (measured, not fixed)

1. Only dim-4 chunks were compared (the V9a region). Both writers take the neighbour's base texture
   from their OWN dominant base when a quadrant has no BTXT; for a dim-4 chunk the pyramid's 4 x 4 block
   IS the chunk, so the two agree. At dim 8+ the pyramid's block (4 x 4) and the stock chunk's
   (dim x dim) are different populations; that is blend-independent and not measured here.
2. The DEFAULTS2 terrain checker's `amend_lodb.py` third row ("N0 vs R0 differ") prints FAIL on
   identical stable lines -- for a `--vt` bake the record carries no colour-sheet hash, so nothing there
   can differ. DEFAULTS2 recorded the same reading and counted G1-G3 as PASS; so does this lane.

## Gates

| gate | result |
|---|---|
| RUNG proof: `lodgen_terrain_vt.sh` on 90e8a57e | 45 checks, 2 failures = V9a-1 (tint off) and V9a-2 (tint on), all 4 chunks differ in each |
| `lodgen_terrain_vt.sh` on add1bf84 | **45 checks, 0 failures**, RESULT PASS (V9a-1, -2, -3 ok; ring self-test 13/0) |
| writer agreement (bake4.sh + measure.py, blend on / off) | pyramid vs stock 6,718 -> 0 texels / 0 -> 0 |
| before vs after, per writer | `--vt` 32 files: only `.lodb` clock lines; stock off 25 files: only `.lodb` clock lines; stock on: the 4 colour DDS + their 4 record hash lines |
| seam (DEFAULTS2 G4 instrument) | chunk 0.977, pyramid dim 2 0.992 (rung 1.236 / 1.250) PASS -- unchanged |
| chunk-boundary step (new, measure.py) | stock 1.513 / 1.644 -> 1.340 / 1.431 = the pyramid's |
| DEFAULTS2 terrain G1-G3 (vs before_defaults2 6b8ed793) | G1, G2 all identical but `.lodb`, 13 stable lines identical; G3 moves exactly colour DDS + VT.2 + VT.4 |
| DEFAULTS2 card G5-G7 | RESULT PASS, 0 fails |
| `lodgen_panel_run.sh` | 137 checks, 0 failures |
| `lodgen_byte_gate.sh` PHASES=bc | (b) 132 checks, 0 failures; (c) 15 identical, 2 differ = chunk colour DDS + `.lodi`, the same pre-existing red DEFAULTS2 measured on its own rung |

## Skill review (finished work)

- Loaded: `search-lean`, `nifskope-ww-lodgen`, `nifskope-ww-build-verify`. `ww-test-harness-add` was
  named by the brief but not needed -- no harness was added and no check changed.
- `nifskope-ww-lodgen`, section "The colour sheet has TWO writers": add "They must also agree on the
  NEIGHBOURHOOD, not just the formula: the pyramid's tile grid carries a one-cell ring of paint and the
  stock chunk's did not, so the edge blend differed at the chunk edge only (lane BLENDSEAM1). Any new
  term that reads a neighbour cell reads `ringPaint` on the stock side. The instrument that localises a
  writer disagreement to the chunk edge is `scratchpad/blendseam1_20260923/measure.py` (per-texel diff
  with edge / quadrant-line distances, plus the chunk-boundary step on a 2 x 2 mosaic, north-up)."
- No new skill: the procedure (bake both writers on the V9a region, diff, locate) is the one the skill
  already names; the measure script is the reusable part and is referenced above.

## Files this lane touched (for an explicit-path commit, when bungo says so)

src/lodgen.cpp, docs/LODGEN_TERRAIN_VT.md, tests/spells/lodgen_terrain_vt.sh (comment only).
New: release/NifSkope.before_blendseam1.exe (rung). Patch scripts: fix_blendseam1.py, fix_docs.py (anchored,
CR-checked, CR 0 before and after).
