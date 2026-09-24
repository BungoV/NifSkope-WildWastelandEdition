# -*- coding: utf-8 -*-
"""splice_changes.py -- lane BUILD5b: the WATER3 entry into WW_CHANGES.md.

`WW_CHANGES.md` is a MIXED file (CR 19,020, LF 24,424) and the 2026-09 entries
at its top are LF-only, so the splice is done in BINARY, the new text is LF-only
and the CR count is asserted unchanged (CONSTITUTION 8).

The text is NOT lane WATER3's `WW_CHANGES_ENTRY.md` verbatim: that entry says
"NOT BUILT, NOT RUN, NOTHING PROVEN" and names the picture and gate P4 as owed,
all of which lane BUILD5/BUILD5b has since settled with numbers.

    python scratchpad/water3_20260910/splice_changes.py --check
    python scratchpad/water3_20260910/splice_changes.py
"""
import hashlib
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
check_only = '--check' in sys.argv
PATH = os.path.join(ROOT, 'WW_CHANGES.md')

ANCHOR = '## 2026-09-10 - the cell owns its own boundary rows\n'

ENTRY = """## 2026-09-10 - marking water direction by hand (lanes WATER3, BUILD5, BUILD5b)

bungo, 2026-09-09: *"in nifskope, have the player mark the water direction in a
smart way"*, *"lakes have no flow if they're not connected to rivers, then
rivers end up at sea"*, *"different water colors for different bodies of
water"* -- *"or at least an ID for them"*.

`src/watermark.{h,cpp}` and `src/watermarkpanel.{h,cpp}` (NEW, ~3,700 lines),
`src/nifskope.cpp` and `src/nifcli.cpp` (the three hook-ups), `NifSkope.pro`,
`tests/spells/water_mark.sh`, `scratchpad/specs_20260909/spec_water.md`.
**BUILT and GATED**: `release/NifSkope.exe` 2026-09-10 03:38:56, harness
**21 model checks + 20 dock checks, 0 failures**, and the four sibling
harnesses that share the reader or the Workspaces menu are unmoved
(`lodl_water` PASS, `lodl_open` 23/0, `lodgen_terrain` 26/0, `lodgen_identity`
PASS, `render_shot` 55/0).

**A Water Marking dock** (in the Workspaces dropdown) over a new model. The
worldspace is drawn from above, one stored plane at a time -- Body ID as a
hashed hue, Flow as direction-hue, Shore as a ramp -- and a drag along a river
is a CONSTRAINT on its direction. Strokes, pins, a source/outlet pin pair that
means "the path between these", a still-water mark for a lake nothing feeds,
per-body class, water form, colour override and name. `lodl <file>
--water-mark-selftest` is the headless half.

**The strokes are the SOURCE and the planes are DERIVED.** The store is written
in WORLD coordinates (spec 3.7), so a stroke survives a re-derivation at another
sample rate; nothing in the tool edits a plane. `save()` rewrites the file from
the body table onward: table, name blob and stroke store re-encoded, the flow
plane re-derived tile by tile, and the body-ID and shore planes copied through
VERBATIM with their absolute offsets rebased -- because nothing a marking tool
does can move a body's shape. The original is renamed `.bak-watermark`.

**What the marking actually does to the plane, measured through lane WATER2's
independent decoder** (`scratchpad/water3_20260910/flow_mean.py`, body 3, the
Charles, 25,114 samples): before, the whole reach carries **one** direction,
115.31 degrees, angular concentration R = 1.000 -- that is the drain rule's
single vector painted over every texel. After one stroke down the river toward
its mouth: **99 distinct directions**, mean 111.31 degrees, R = 0.807. The flow
bends with the banks. Pictures: `scratchpad/water3_20260910/images/`.

**The gates, all with a floor on the other side.** P0 identity: a file nobody
marked re-derives to the writer's own bytes, 0 differ over 37,748,736 texels
(pre-measured in Python before a line was compiled, floor 25,114). P1
isolation: a stroke on body 3 changes 0 texels outside it and 25,110 of its own
25,114 (100%), with the refuter run FIRST so the isolation check is seen able
to fail. "Rivers end up at sea": the mouth is found IN THE FILE and the marked
plane's mean points at it, cos = 1.000. P3 undo and P8 round trip: both
byte-identical, 0 bytes differ. P4 re-bake: marked at 32 samples a cell,
re-written at 8, the body still points the same way -- **moved 0.21 degrees**
against a tolerance of 5, with a floor asserting the file really was rewritten
at 8. P5: the house-style counts, twenty of them. P7: a stroke on dry land is
refused in words, at a dry point FOUND in the file.

**Four defects the first gated run caught, and what each one was.** (1) The
"dry land" control was placed at the worldspace's own corner -- which on the
Commonwealth is open SEA, body 1, so the tool correctly accepted the stroke and
the control read as a failure; the dry point is now measured out of the file.
(2) `solve()` wrote the body table's derived fields -- flow vector, flow source,
confidence, source, outlet -- and nothing ever put them back, so removing a
stroke could not reproduce the file it started from: gate P3 failed by
**1,021,405 bytes**, the sea's share of a flow plane derived from a mean a
solve had moved. The derived fields are now re-derived from the table as the
generator wrote it, on every solve, and `solve()` no longer sets the
"user-edited" bit it cannot clear. (3) The canvas dropped every point that was
not on water before the model saw the stroke, so a stroke drawn entirely on land
came back "that stroke has no points"; it now hands the stroke over AS DRAWN and
the model names the body from the first point that lands on water. (4) The dock
opened with **0 of its 6 named settings** inside the visible band -- a
QScrollArea reports a fixed ~100x30 sizeHint whatever it holds, so the splitter
gave the map everything -- while all nineteen dock checks stayed green, because
"6 settings on 6 distinct rows" is a fact about the layout and not about what a
person sees. Only the screenshot saw it (CONSTITUTION 5). The splitter now opens
each band at its own height, and the count that would have caught it ships with
a floor: 6 of 6.

**The spec's pre-registered gate numbers did not survive their audit**
(`ww-spec-gate-audit`). `spec_water.md` marked "body 233, the Charles, 25,112
texels" and refuted on "body 136": under the rule lane WATER2 shipped, ids are
assigned by descending area over 346 bodies, so the Charles is **body 3**
(25,114 texels, same cells) and the marsh is **body 2** (29,312). The harness
hard-codes neither -- it picks the largest river and the largest other non-sea
body and prints which, and `WW_WATER_MARK_BODY=<id>` names one for a picture at
a framing that already exists.

**`spec_water.md` is current**, 557 -> 735 lines: 346 bodies everywhere, the
SIZE-guarded merge clause, 3.8's stride refusal reversed (it had the
forward-compatibility rule backwards), 7's G5 and G6 restated with the numbers
that hold, 5 rewritten as built, and a provenance footer rebuilt from scratch
whose 38 line numbers were re-derived from their anchors.

**Divergences, stated.** The spec said the 3-D water plane would be the canvas;
it is a top-down map in the dock, because `src/glview.cpp` was another lane's
file and because a river reach eleven cells long is a fact about the map.
Blender's grease pencil is the reference for the gestures; the eraser removes a
WHOLE stroke (a stroke is one constraint), there is no tablet pressure (the
Width row is the only source, so a stroke is reproducible from the file), and
pan/zoom are Blender's. The map's own palette is a hash of the body id rather
than the skin table, because 346 bodies cannot come out of a twenty-entry
palette; dry land is drawn at a literal (24,26,30) and that one IS a miss.
Confidence is not the spec's second harmonic fill -- that converges to 15
everywhere -- but a geodesic distance inside the body's mask, halving every
stroke width, which is what the spec's own sentence asked for.

**Owed, and named:** Barrier and Merge strokes do nothing (they need the
classifier re-run, so they belong to the writer); the writer does not read the
stroke store, so a full re-bake from the ESM (`lodgen --water-bodies`) still
discards a user's marks -- the sharpest owed item here; the plane packer is a
TWIN of `lodtPackPlane` rather than a call, kept safe only by the identity gate,
and retiring it into shared code is a `src/lodtfile.cpp` lane; the 3-D viewport
cannot be marked on; `EsmWorld` still has no `WATR` accessor.

"""

with open(PATH, 'rb') as f:
    data = f.read()
cr0, lf0 = data.count(b'\r'), data.count(b'\n')
print('WW_CHANGES.md %s %d bytes  LF %d  CR %d'
      % (hashlib.sha1(data).hexdigest()[:16], len(data), lf0, cr0))

anchor = ANCHOR.encode('utf-8')
n = data.count(anchor)
if n != 1:
    print('REFUSED: %d matches for the anchor' % n)
    sys.exit(1)
if b'marking water direction by hand' in data:
    print('REFUSED: an entry with this title is already spliced')
    sys.exit(1)

new = ENTRY.encode('utf-8')
if b'\r' in new:
    print('REFUSED: the new text is not LF-only')
    sys.exit(1)
out = data.replace(anchor, new + anchor, 1)
cr1, lf1 = out.count(b'\r'), out.count(b'\n')
print('  -> %d bytes  LF %d (+%d)  CR %d' % (len(out), lf1, lf1 - lf0, cr1))
if cr1 != cr0:
    print('REFUSED: CR moved %d -> %d' % (cr0, cr1))
    sys.exit(1)
if b'\x00' in out:
    print('REFUSED: a NUL appeared')
    sys.exit(1)
if check_only:
    print('--check: nothing written')
    sys.exit(0)
with open(PATH, 'wb') as f:
    f.write(out)
print('written')
