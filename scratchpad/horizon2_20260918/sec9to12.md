
## 9. Text for the two files this lane does not touch

`WW_CHANGES.md` and `HANDOFF.md` were not edited. Here is their text.

### 9.1 A paragraph for `WW_CHANGES.md`

> **Far-LOD terrain shadows: the horizon march no longer smears every occluder
> across its azimuth bin.** `LodgenHorizonField::maxAlong` picked its mip level
> against the width of the 22.5-degree bin and took a 2×2 block at every tap, so
> each stored byte was the *maximum over the whole sector* rather than the
> skyline in that direction -- while the viewer blends the two bins either side
> of the sun, which is only meaningful for a directional sample. The march now
> takes one square a tap, spaced half a square, with the footprint derived from
> the segment rather than the bin, and the step ladder grows 1.3× instead of 1.5×
> (about 33 steps to 4096 units, 50 s vs 47 s per chunk). Measured against a new
> third witness built from the raw inputs -- the BTD heightmap read along a
> 1-degree pencil plus every placement as an exact world box, ten receivers on
> chunk 4.4.-12 -- the sheet's one-sided bias falls from **+8.59 to +0.87
> degrees** and its mean error from 11.90 to 8.15 (the 128-unit lattice itself
> owns 7.33 of that and no march over it can do better). In the viewer, chunk
> 4.4.-12's terrain horizon drops from 14.47..87.18 deg (mean 64.37) to
> 4.32..86.82 (mean 56.44), and the chunk that reported **0.0% lit** at a
> 15-degree sun now reports 5.3%. The way back (`--no-terrain-horizon`,
> `--lodi-v7`) is byte-identical in all 23 payload files. `tests/spells/
> lodgen_horizon.sh` gains **G6**, which scores the sheet against that witness and
> carries the pre-fix sheet as a red control that must fail on a real file --
> because G3's in-bake reference calls the same `maxAlong` the march calls and
> moved with it, which is why a 97% agreement floor sat at 51.69% for a day
> without anyone being able to say which of the two was wrong.

### 9.2 A LANDED block for `HANDOFF.md`

> **LANDED 2026-09-19 -- HORIZON2 -- the terrain horizon's sector-max footprint**
> * Exe `release/NifSkope.exe` 2026-09-18 23:47:33, 22,949,376 B, sha1
>   `b349f807426be700ed2ff9b54ee23e4fab3ba037`. Rung:
>   `release/NifSkope.before_horizon2.exe` (the 21:59:46 exe, sha1
>   `e578b76f9d7a2d011363e4300a2c94967e14d605`).
> * One source file: `src/lodghorizon.h` (untracked, 25,134 B). Three edits: one
>   square a tap at half-square spacing; footprint from the SEGMENT
>   (`(dEnd - d) / LODGEN_HORIZON_MAX_TAPS`, 64 taps) instead of
>   `binWidthFraction * d`; `growth` 1.5 -> 1.3.
> * Two tracked test files added: `tests/spells/lodgen_horizon_witness.py` and
>   `…_witness.json`; `tests/spells/lodgen_horizon.sh` gains G6 (427 -> 486 LF).
> * Nine documentation corrections across `LODGEN_NATIVE_LODO_LODI.md`,
>   `LODGEN_TERRAIN_VT.md`, `LODGEN_CENSUS.md`, `FO4CS_IMPROVED_LOD_PLAN.md` §9.
>   Seven `MISTAKES.md` entries.
> * Report: `scratchpad/horizon2_20260918/lane_horizon2_report.md`.
> * **Still red, with their numbers, not re-tuned:** G3 terrain
>   `horizonRefuteWorst` 51.69% -> **89.01%** against a 97% floor -- but the
>   in-bake reference calls `maxAlong` (`src/lodghorizonrefute.h:107,112`) and
>   moved too (`MeanRefDeg` 63.565 -> 58.995, toward the third witness), so that
>   floor scores agreement between two instruments, not correctness. G3 objects
>   `vhorRefuteWorst` 97.73% -> **95.96%** (all at A120E60) is a NEW red against a
>   floor the object stream used to pass; there is no `--horizon-growth` switch,
>   so separating growth from footprint needs a code change.
> * **Owed:** a `qmake` run (`Makefile.Release` carries no dependency on
>   `src/lodghorizon.h`, so make exits 0 having compiled nothing); a third witness
>   for the OBJECT stream's per-vertex horizon, which has none; cleanup of
>   `binWidthFraction`, now set by three callers and read only by the refuter's
>   bound; and the ruling on whether 16 bins is still the right count now that
>   each byte is directional.

## 10. Rows for bungo

| what he sees | before | after |
|---|---|---|
| Downtown chunk at a low sun (15 deg) | the whole chunk in shadow -- **0.0%** of it lit | **5.3%** lit; the streets facing the sun open up |
| How high the sheet thinks the skyline is | mean **64.4 deg** -- as if every direction had a tower in it | mean **56.4 deg**, and a raw measurement of the same ground says 43.3 |
| Which way the error pointed | always **too much** shadow, +8.6 deg everywhere | +0.9 deg, and it now falls on both sides |
| A gate that could catch this | none -- the checker shared its code with the thing it checked | **G6**, built from the heightmap and the building placements, with the old sheet kept as the proof it can fail |
| Bake cost | 47 s a chunk | **50 s** a chunk |
| Turning it off | `--no-terrain-horizon` / `--lodi-v7` | identical bytes to before, all 23 payload files |

## 11. MISTAKES entries written

Seven, at the top of the root `MISTAKES.md` (8,894 -> 9,080 LF, 541,310 ->
553,785 bytes, LF-only before and after, byte splice on a unique anchor):

1. **The refuter's "independent reference" calls the same function the march
   does** -- `src/lodghorizonrefute.h:107,112` call
   `LodgenHorizonField::maxAlong`. This is the brief's "a MISTAKES entry for
   whichever witness shared the bug", and the witness is named: the in-bake
   reference. The rule: name every function a producer and its reference share
   BEFORE pre-registering a floor between them, and read a reference whose own
   aggregates move when the producer is edited as a regression signal, not a
   score.
2. **A "resolution invariant" that errs on the safe side, with its price never
   measured** -- "errs on the safe side" is a claim with units; a one-sided bias
   does not average out.
3. **I read the shipped sheet with the WRITER's channel shifts, and blamed the
   bake** -- my instrument was wrong, not the bake; a decoder is validated
   against the shipped consumer. Carries the second finding: the in-bake refuter
   reads back from in-memory planes and can never verify the file.
4. **My leading hypothesis was refuted by my own measurement** -- SLAB1's
   wall/ceiling law made the error worse, 7.33 -> 10.90 deg. Write the
   measurement that refutes your favourite first.
5. **An elimination pass that scored every candidate against the defect's own
   convention** -- the first sweep found nothing because it graded candidates on
   how well they reproduced the sector maximum. The yardstick comes from the
   CONSUMER.
6. **`make rc=0` on a build that compiled nothing** -- `Makefile.Release` carries
   no dependency on a header added after qmake ran; the tell is the exe's mtime.
7. **A control that mixed the two changes it was controlling for** -- the frozen
   prediction was cast at growth 1.5 while the exe shipped 1.3.

## 12. The finished-work skill review

**Skills that carried weight, unchanged.** `nifskope-ww-lodgen` (the build
incantation, the exe lock, the `.lodt` reader traps), `ww-texel-picture` (every
framing and caption in section 7), `ww-sheet-diff` (the way-back byte comparison
in 4.7), `ww-population-refuter` (the 576-texel population in 4.4 rather than a
hand-picked rectangle -- the mean over a population is what made the +8.5-degree
bias visible where any single receiver looked merely noisy), and
`feedback_measure_dont_eyeball`, which is the whole lane: the defect had been
looked at and reasoned about for a day, and it moved the moment someone built an
instrument that could disagree.

**Skills that needed a delta, written out in 8.2.**
`nifskope-ww-build-verify` (a header with no Makefile dependency at all),
`ww-test-harness-add` (a floor between a producer and a reference that calls it),
`nifskope-ww-lodgen` (the file's channel order is not the packing order),
`ww-control-calibration` (a control is a comparison at identical settings).

**The skill this lane wanted and did not have -- proposed, for the director.**
`ww-third-witness`: building a raw-input witness for a baked field. Its shape,
from this lane: (1) pick the receivers from the OUTPUT's own addressing -- texel
centres, not round world numbers, so the comparison needs no interpolation;
(2) compute the truth from the SOURCE assets by a different mechanism than the
bake uses -- here a 1-degree pencil over the BTD heightmap plus placements as
exact world boxes, sharing no line with `src/lodghorizon.h`; (3) measure the
FLOOR the bake's own data structure imposes before setting any bar (the 128-unit
lattice costs 7.33 deg and no march can beat it), and put that number in the
script's docstring next to the bar it justifies; (4) freeze it as JSON with
`what`/`how`/`why` provenance beside the checker; (5) add a fixture-rot check so
a fixture that quietly loses its content fails; (6) require the checker to FAIL
on a real pre-fix artefact via `--expect-fail`, never on a mutation; (7) carry
the alternative truth column (`trueSectorMaxDeg`) so a future lane can argue the
convention without regenerating anything. That recipe is why this lane has a
gate that can fail, and it is worth having before the next field is baked.

**What this lane could not do and is owed:** the object stream's per-vertex
horizon has no third witness, and building one for 53,396 vertices is the same
recipe at a different scale. Until it exists, `vhorRefuteWorst` is the same kind
of number `horizonRefuteWorst` was.
