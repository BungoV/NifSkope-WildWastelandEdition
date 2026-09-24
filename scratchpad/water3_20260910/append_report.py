# -*- coding: utf-8 -*-
"""append_report.py -- the Build section onto scratchpad/lane_water3_report.md.

LF-only file; the CR count is asserted unchanged.

    python scratchpad/water3_20260910/append_report.py --check
    python scratchpad/water3_20260910/append_report.py
"""
import hashlib
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
check_only = '--check' in sys.argv
PATH = os.path.join(ROOT, 'scratchpad', 'lane_water3_report.md')
MARK = '## Build (BUILD5/BUILD5b)'

TEXT = """
---

## Build (BUILD5/BUILD5b)

Lane BUILD5 applied the three hook-ups and built, then died on an API rate limit
with its harness red and its `PENDING.md` still saying nothing had been built.
Lane BUILD5b resumed from the tree, not from the note.

### The state BUILD5b found, settled by bytes and not by the resume file

| claim in `PENDING.md` | what was on disk |
|---|---|
| "nothing was built" | `release/NifSkope.exe` 02:26:06, newer than every source in `NifSkope.pro` |
| "no existing `src/` file was touched" | all three hook-ups applied -- `src/nifskope.cpp` 433,872 -> 434,370 bytes, exactly the +498 the dry run predicted; `src/nifcli.cpp` at its predicted 261,671 |
| the harness "has never executed" | it had, at 02:32, **5 failures** |

`hookup.py --check` re-run on resume says "ok" for both `nifskope.cpp` edits,
which reads as "not applied". Its anchors are the lines the new text goes AFTER,
so they keep matching once the edit is in. Recorded in `MISTAKES.md`.

### The five failures, and what each one was

| gate | measured | cause |
|---|---|---|
| P1 floor | 11,271 of 29,312 = 38.5%, floor 60 | the harness's stroke ran down the body's BOUNDING-BOX DIAGONAL and kept every point on ANY water: 13 of its 16 points were not on the river, and a bbox diagonal is within a degree of the body's own mean (55.5 against 54.84), so the stroke asked the plane for what it already said |
| P7 dry land | refused for the wrong reason | the control was placed at the worldspace corner, which on the Commonwealth is open SEA |
| P8 round trip | 38,613,382 -> 38,679,915 bytes | a cascade of P7: the accepted sea stroke made the sea's flow plane stop being uniform on the second save |
| P3 undo | 1,021,405 bytes differ | THE ONE REAL DEFECT -- `solve()` accumulated the body table's derived fields |
| dock P7 | "that stroke has no points" | the canvas filtered dry points out before the model saw them |

Four patch scripts, each refusing unless every anchor matches exactly once and
the CR count is unchanged: `gatefix.py`, `gatefix2.py`, `gatefix3.py`,
`gatefix4.py`, plus BUILD5's own `p4_gate.py` (gate P4, written and unapplied).
Three builds, `make` exit code gating each; `g++ -fsyntax-only` with the real
`Makefile.Release` flags before every one of them.

### The gates, on `release/NifSkope.exe` 2026-09-10 03:38:56

| gate | result | the number |
|---|---|---|
| P0 repack identity | PASS | body table and flow plane both re-encode to the writer's own bytes, 0 differ |
| P1 isolation | PASS | 0 texels outside the marked body changed |
| P1 floor | PASS | 25,110 of 25,114 = 100.0% of body 3's own (was 38.5% before the centreline) |
| P2 refuter, run FIRST | PASS | a stroke on the neighbour moves 25,110 of its own and 0 of the river's |
| P3 undo | PASS | **0 bytes differ** over 38,612,038 |
| P4 re-bake | PASS | marked at 32 samples a cell, re-written at 8: mean **112.59 -> 112.38, moved 0.21 degrees**, tolerance 5. The tolerance is BUILD5's pre-registered 5 and the measured move is 24x inside it; the floor beside it asserts the header really says 8, that 32 != 8, and that the body has samples at both rates (29,309 at 32, 1,819 at 8) |
| P5 panel style | PASS | 20 dock checks: 2 scrub fields / 0 plain, 0 group boxes / 4 headings, 5 selectors / 0 unmatched, 2 check boxes / 0 dashed / 0 untipped, 6 settings on 6 distinct rows, three bands, the fold |
| P6 the pictures | PASS | `images/charles_flow_pair.png` and `dock.png`, below |
| P7 dry land | PASS | refused in words, at a dry point found in the file at (-305152, -313344) |
| P8 round trip | PASS | save, reopen, save byte-identical, 38,619,353 both times |
| "rivers end up at sea" | PASS | the mouth is found IN THE FILE; the marked plane's mean points at it, cos = **1.000** on body 3 |
| `water_mark.sh` | **PASS** | 21 model checks + 20 dock checks, 0 failures |
| `lodl_water.sh` | PASS | RESULT PASS, unmoved |
| `lodl_open.sh` | PASS | 23 checks, 0 failures |
| `lodgen_terrain.sh` | PASS | 26 checks, 0 failures |
| `lodgen_identity.sh` | PASS | RESULT PASS |
| `render_shot.sh` | PASS | **82 checks, 0 failures**; section 7 (the pinned camera) all green -- the 512-unit cube spans 376.91 px against the 376.75 the projection predicts, at eye 500, 1000 and 2000, and the census `upp` agrees with the arithmetic to six figures |

### The pictures

`scratchpad/water3_20260910/images/charles_flow_pair.png` -- the Charles (body 3,
cells -16..-6 / -21..-4), the FLOW plane, ONE framing, two files. The framing is
lane WATER2's unchanged, and the proof is that this lane's "before" render is
**byte-identical** to `water2_20260909/images/charles_flow.png`: the only thing
that differs between the halves is the file. Measured through WATER2's
independent decoder (`flow_mean.py`), over all 25,114 samples of body 3:

* **before** -- mean 115.31 degrees, **1 distinct direction**, concentration
  R = 1.000. The drain rule found the body it flows into and painted that single
  vector over every texel;
* **after one stroke toward the mouth** -- mean 111.31 degrees, **99 distinct
  directions**, R = 0.807. The hue turns with the reach.

The mean barely moves and that is the point: the stroke did not re-aim the
river, it gave it a SHAPE. Every other body in the frame -- the lake, the sea,
the puddle -- is pixel-for-pixel unchanged, which is gate P1 in a picture.

`scratchpad/water3_20260910/dock.png` -- the dock as a person sees it. It obeys
the house style: four `wwHeading` sections and no group boxes, one setting a row
with one label column, scrub fields for both numbers, matched chrome on all five
selectors, tooltips instead of dashed labels, the Bake section folding, the three
bands with the map, the summary and the buttons pinned, and the refusal sentence
in words above the map. Two divergences, both deliberate and both stated: the
map's palette is a hash of the body id rather than the skin table (346 bodies
cannot come out of a twenty-entry palette), and dry land inside the map is drawn
at a literal (24,26,30) -- that one is a genuine miss and is listed as owed.

### What is still NOT done

Everything section 4 of this report lists, minus the picture pair and gate P4,
which are now delivered. Still open: Barrier and Merge strokes do nothing; the
writer does not read the stroke store, so `lodgen --water-bodies` still discards
a user's marks; the plane packer is a TWIN of `lodtPackPlane`; the 3-D viewport
cannot be marked on; `EsmWorld` has no `WATR` accessor; and the map's dry-land
literal should be a skin token.

**Nothing is committed** (CONSTITUTION 8). **His open NifSkope window needs a
restart** -- the exe under it is from before 03:38:56.

### Skill review (BUILD5b)

**Loaded and used.** `nifskope-ww-resume-pending` (the read order, qmake before
make, the dependency read-back BY OBJECT NAME -- which is how `watermark.o`,
`watermarkpanel.o` and `nifcli.o` were each checked against the header this lane
changed -- and the exe-newer sweep over every changed file rather than one).
`nifskope-ww-build-verify` (make's own exit code as the gate, the stylesheet
`cmp`, and its "a successful build is not a consistent one" section, which is
the object-versus-header check above). `nifskope-ww-panel-style` (the judgement
of `dock.png`, and the visibility count that was missing from it).
`nifskope-ww-render-shot` (the switches, one instance at a time, and the reason
a leftover instance had to be identified by its command line before being
killed). `ww-texel-picture` (the caption arithmetic: the first sheet's number
line ran past its cell and read as the neighbour's number; the fix asserts each
caption fits the panel it belongs to).

**A skill that should have existed, and now does not need to be invented twice.**
`WW_RENDER_SHOT` silently writes NOTHING when given a RELATIVE path: the grab
runs, `release/ww_camera_pin.log` records it, the process exits 0, and
`QImage::save` fails without a word. Two renders were lost to it. That belongs
as a line in `nifskope-ww-render-shot`'s switch table -- **recommended
amendment, for the director to apply to both skill trees**: *every `WW_*` output
path is ABSOLUTE; a relative one is saved relative to the process's working
directory and fails silently, and the tell is a `grab` line in
`release/ww_camera_pin.log` with no file on disk.*

**Declined, with the reason.** "Diagnose a red gate as harness or as tool" is
the whole of this lane's work and it does not compress into a procedure: each of
the five had to be read back to its own cause. The general rule it followed is
already CONSTITUTION 4.
"""

with open(PATH, 'rb') as f:
    data = f.read()
cr0, lf0 = data.count(b'\r'), data.count(b'\n')
print('lane_water3_report.md %s %d bytes  LF %d  CR %d'
      % (hashlib.sha1(data).hexdigest()[:16], len(data), lf0, cr0))
if MARK in data.decode('utf-8'):
    print('REFUSED: the Build section is already there')
    sys.exit(1)
new = TEXT.encode('utf-8')
if b'\r' in new:
    print('REFUSED: not LF-only')
    sys.exit(1)
out = data.rstrip(b'\n') + b'\n' + new
print('  -> %d bytes  LF %d  CR %d' % (len(out), out.count(b'\n'), out.count(b'\r')))
if out.count(b'\r') != cr0:
    print('REFUSED: CR moved')
    sys.exit(1)
if check_only:
    print('--check: nothing written')
    sys.exit(0)
with open(PATH, 'wb') as f:
    f.write(out)
print('written')
