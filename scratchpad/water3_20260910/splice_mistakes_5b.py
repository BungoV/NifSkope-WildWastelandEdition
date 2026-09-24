# -*- coding: utf-8 -*-
"""splice_mistakes_5b.py -- lane BUILD5b's own entries into MISTAKES.md.

Lane WATER3's two entries are already in the file (checked before writing);
these are five more, four of them found by BUILDING and RUNNING what WATER3
wrote and one by resuming a lane that died on an API rate limit.

`MISTAKES.md` is LF-only (CR 0) and appended to; the CR count is asserted.

    python scratchpad/water3_20260910/splice_mistakes_5b.py --check
    python scratchpad/water3_20260910/splice_mistakes_5b.py
"""
import hashlib
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
check_only = '--check' in sys.argv
PATH = os.path.join(ROOT, 'MISTAKES.md')

MARK = '## 2026-09-10 - a control placed where nobody measured (lane BUILD5b)'

TEXT = """
---

## 2026-09-10 - a control placed where nobody measured (lane BUILD5b)

**What was done.** `lodtWaterMarkSelfTest`'s dry-land control put its stroke at
the worldspace's own corner, with the comment *"the worldspace's own corner: as
dry as this file gets"*. The dock half did the same.

**What was true instead.** On the Commonwealth the corner is OPEN SEA -- body 1,
21,575,619 texels. The tool correctly ACCEPTED the stroke and the control read
as a failure of the refusal it exists to prove. Worse, the accepted stroke then
sat on the sea for the rest of the run and made two later gates fail as well:
the second save grew the file by 66,533 bytes because the sea's flow plane
stopped being uniform (P8), and P3's residue was measured against it.

**How it was found.** By running the harness for the first time, on the first
build. Three of the five failures in that run were this one defect.

**The rule that prevents it.** A CONTROL is a measurement like any other
(CONSTITUTION 4). The dry point is now found by walking the file's own body
plane, the harness prints where it landed and which body sits at the corner, and
a check with a floor asserts a dry point was found at all. Nothing about a
worldspace's shape may be assumed from its bounding box.

---

## 2026-09-10 - a derived field that accumulated (lane BUILD5b)

**What was done.** `WaterMarkDoc::solve()` wrote the body table's flow vector,
flow source, confidence, source and outlet, and set the "user-edited" flag.

**What was true instead.** Nothing ever put them back. The class documents "the
strokes are the SOURCE and the planes are DERIVED", but the table's derived half
was accumulated rather than derived: a body marked once kept its stroke's mean
for the life of the document, and because the flow plane is a pure function of
that table, removing the stroke could not reproduce the file it started from.
Gate P3 measured it at **1,021,405 bytes** -- almost all of it the sea's share
of a plane re-derived from a mean a solve had moved.

Fixing it exposed a second layer of the same mistake: the pristine snapshot was
re-taken by `open()`, and `save()` re-opens the file it has just written, so
after one save the "original" table was the marked one. Residue **480 bytes**,
which is the first residue divided by the number of bodies that had been solved.

**How it was found.** Gate P3, which was written as byte-identity precisely
because that is the only tolerance a pure function admits.

**The rule that prevents it.** A field a solve derives is restored from the
generator's own answer at the TOP of every solve, and the snapshot that holds
that answer must survive every re-open of the document. And a bit that a solve
can set but cannot safely clear -- "user-edited", which the name, class and
colour setters also set -- does not belong to the solve at all.

---

## 2026-09-10 - a view that filtered the data before the model saw it (lane BUILD5b)

**What was done.** `WaterMarkCanvas::layStroke` dropped every point of a drawn
stroke that was not on water, then handed the survivors to `addStroke`.

**What was true instead.** It was buying forgiveness for a drag that starts a
few units off the bank, and paying for it twice: a stroke drawn entirely on land
arrived EMPTY and came back *"that stroke has no points"* -- a sentence that
says nothing about what went wrong, and not the refusal the spec asks for -- and
the file no longer held what the user actually drew. The forgiveness now lives
in the model, which names the body from the first point that lands on water and
reports how many missed.

**How it was found.** The dock half of the harness, on the run after the dry
point was fixed: the model refused correctly and the dock still did not.

**The rule that prevents it.** A view passes the user's input to the model AS
GIVEN. A view that filters it is a second, undocumented model, and the first
thing it costs is the model's own error messages.

---

## 2026-09-10 - nineteen green checks over a dock nobody could use (lane BUILD5b)

**What was done.** The Water Marking dock shipped with its self-test asserting
"settings: 6 on 6 distinct rows", "the map sits outside the scrolling settings",
"the settings themselves do scroll" and fifteen more. All green.

**What was true instead.** `dock.png` shows the dock opening with TWO rows above
the map -- File and Show -- and **0 of those 6 named settings** inside the
visible band. A `QScrollArea` reports a fixed ~100x30 sizeHint whatever widget
it holds, so the `QSplitter` had nothing to open the settings band on and gave
the map everything. Every count was a fact about the LAYOUT; none of them was a
fact about what a person sees.

**How it was found.** Opening the screenshot, which CONSTITUTION 5 requires and
which the count-based skill notes call for in the same words: *counts do not see
a ragged column*.

**The rule that prevents it.** A panel's self-test counts at least one thing
about VISIBILITY, not only about structure: how many of the settings are inside
the scroll area's viewport when the dock opens, with a floor. It now reads 6 of
6 (floor 5), and the number it replaced was 0.

---

## 2026-09-10 - a --check that could not tell applied from unapplied (lane BUILD5b)

**What was done.** Lane BUILD5 died on an API rate limit mid-gate. Its
`PENDING.md` -- written earlier, never updated -- said *"nothing was built and no
existing `src/` file was touched"*. On disk, all three hook-ups WERE applied, the
exe HAD been rebuilt, and the harness had already run and failed 5 checks.
`hookup.py --check`, re-run on resume, reported "ok" for both `nifskope.cpp`
edits, which reads as "not yet applied".

**What was true instead.** The script's anchors are the lines the new text is
inserted AFTER, so they still match exactly once once the edit is in place: a
`--check` that only counts anchors cannot distinguish an unapplied edit from an
applied one, and re-running it would have inserted everything twice. The state
was settled instead by byte counts (`nifskope.cpp` 433,872 -> 434,370 = exactly
the +498 the dry run predicted) and by grepping for the inserted identifiers.

**How it was found.** By checking the file rather than believing the script's
word, because the brief's own description of the tree disagreed with `git
status`.

**The rule that prevents it.** A patch script's `--check` asserts the POST
state as well as the anchors -- "the text I would insert is not already there"
-- so it is safe to re-run and honest to a resuming lane. And a lane that
applies a patch or lands a build updates its `PENDING.md` in the same breath:
a resume file that describes a tree that no longer exists is worse than none.
"""

with open(PATH, 'rb') as f:
    data = f.read()
cr0, lf0 = data.count(b'\r'), data.count(b'\n')
print('MISTAKES.md %s %d bytes  LF %d  CR %d'
      % (hashlib.sha1(data).hexdigest()[:16], len(data), lf0, cr0))
for need in ('a placeholder check sat where a measurement belonged',
             'a lane report written at the end, not incrementally'):
    print('  lane WATER3 entry already spliced: %s -- %s'
          % (need, 'yes' if need in data.decode('utf-8') else 'NO'))
if MARK in data.decode('utf-8'):
    print('REFUSED: these entries are already in the file')
    sys.exit(1)
new = TEXT.encode('utf-8')
if b'\r' in new:
    print('REFUSED: the new text is not LF-only')
    sys.exit(1)
out = data.rstrip(b'\n') + b'\n' + new
cr1, lf1 = out.count(b'\r'), out.count(b'\n')
print('  -> %d bytes  LF %d (+%d)  CR %d' % (len(out), lf1, lf1 - lf0, cr1))
if cr1 != cr0:
    print('REFUSED: CR moved')
    sys.exit(1)
if check_only:
    print('--check: nothing written')
    sys.exit(0)
with open(PATH, 'wb') as f:
    f.write(out)
print('written')
