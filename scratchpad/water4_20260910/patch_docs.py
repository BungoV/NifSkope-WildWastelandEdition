# -*- coding: utf-8 -*-
"""patch_docs.py -- lane WATER4's documents: docs/LODGEN_BTD_FORMAT.md (the
dye plane, the three new stroke kinds, the 0xF4 word), spec_water.md (4.3 as
rebuilt, 3.7 kinds, the dye section, the F gates), WW_CHANGES.md (a LF-only
entry spliced in BINARY at the top; the CR count asserted unchanged),
MISTAKES.md (LF-only, newest at the top).  Every anchor matches exactly once."""
import sys
sys.path.insert(0, 'scratchpad/water4_20260910')
from splice import splice   # noqa

BUILT = '**BUILD PENDING** -- written and syntax-checked (`g++ -fsyntax-only` with the real `Makefile.Release` flags, rc=0 on all four files), NOT compiled, NOT run; the numbers below that come from the numpy prototype say so'

# ---------------------------------------------------------------- contract --
D = 'docs/LODGEN_BTD_FORMAT.md'
splice(D, [
    ('| **0xF4** | uint32 | **v3 only** — reserved, written 0 |\n',
     '| **0xF4** | uint32 | **v3 only** — the **dye plane** store offset, 32 bits (lane WATER4); 0 = none, and the generator always writes 0. A reader tests bit 8 of `0x44`, never this word |\n', 'replace'),
    ('into a store that already exists.\n\n### Refusals, by name\n',
     '''into a store that already exists.

### The dye plane (lane WATER4) — carried water

bungo, 2026-09-10: *"a factory that's releasing toxic sludge into a river, or
river flowing into an ocean and the river and the ocean may have slightly
different color"*.

A fourth plane in the SAME container format as the three above, at the FLOW
plane's rate, **4 bytes a sample**, referenced from the version-3 header's
reserved word at `0xF4` as a 32-bit absolute offset under a new section bit:

```
SECT_DYE = 1u << 8     dye plane
uint32 sample:
  bits  0..15   the SOURCE: 1..32767 = a body id (this is that body's water,
                carried past its mouth; the consumer takes that body's colour
                -- its override if A > 0, else its WATR form's);
                0x8000 | n = the n-th DyePin record of the stroke store, in
                store order, counting enabled DyePin records only (the pin
                carries its RGBA); 0 = no dye
  bits 16..23   weight 0..255 (255 = undiluted)
  bits 24..31   written 0
```

The version stays 3 and no existing offset moves: the container is
self-describing (its own rate and sample size are in its head), so one word is
all it needs. A file past 4 GB cannot carry one; the marking tool refuses by
name rather than truncating the offset.

**Only the marking tool writes it, and only while the store carries a dye
mark** (a DyePin, kind 7, or a DyeMouth, kind 9). The generator never writes
one; an unmarked file, and a file whose dye marks were removed, carry no dye
plane and the word at `0xF4` is 0 again -- which is what keeps the marking
tool's undo gate byte-identical. Its weight is the steady advection-decay of
the dye along the potential flow inside the receiving body (`u . grad c =
-|u| c ln2 / L`), `L` the half-distance in world units (default 8,192 = two
cells; a DyeKnob record, kind 8, holds another in `width`), solved exactly by
one pass in descending potential. `src/watermark.cpp`, `WaterMarkDoc::solveDye`
and `WaterFlowGrid::dye`. Reader: `LodtFile::dyeWordAt`, `dyePlaneSamples`,
`dyePlaneOffset`. ''' + BUILT + '''.

### Refusals, by name
''', 'replace'),
    ('  uint8  kind               0 stroke, 1 pin, 2 barrier, 3 merge\n  uint8  flags              bit0 sets speed, bit1 sets direction,\n                            bit2 pins the body id, bit3 disabled\n  float  speed              world units per second, when flags bit0\n  float  width              world units, the stroke\'s influence radius\n  uint16 pointCount\n  uint16 reserved\n  pointCount * { float worldX, float worldY }\n}\n```\n\n**Points are WORLD coordinates**, not texels, so a stroke survives a re-bake at\na different sample rate, a different bridge distance, or a heightmap change. The\nthree planes are DERIVED',
     '''  uint8  kind               0 stroke, 1 pin, 2 barrier, 3 merge,
                            4 source pin, 5 outlet pin, 6 still water (WATER3);
                            7 dye pin, 8 dye knob, 9 dye at mouth (WATER4)
  uint8  flags              bit0 sets speed, bit1 sets direction,
                            bit2 pins the body id, bit3 disabled
  float  speed              world units per second, when flags bit0;
                            a dye pin's / dye-mouth's STRENGTH 0..1
  float  width              world units, the stroke's influence radius;
                            a dye knob's HALF-DISTANCE
  uint16 pointCount
  uint16 reserved
  pointCount * { float worldX, float worldY }
  kind 7 only: uint8 R, G, B, A   the dye's colour, after the points
}
```

A reader that does not know a kind skips it by `recordBytes`; a DyePin record
is 24 + 8 n bytes, every other kind 20 + 8 n.

**Points are WORLD coordinates**, not texels, so a stroke survives a re-bake at
a different sample rate, a different bridge distance, or a heightmap change. The
three planes are DERIVED''', 'replace'),
])

# -------------------------------------------------------------------- spec --
S = 'scratchpad/specs_20260909/spec_water.md'
splice(S, [
    ('### 4.3 Propagation inside a body\n\nThe per-texel field is a **constrained harmonic fill on the body\'s mask**:\n',
     '''### 4.3 Propagation inside a body

> **REBUILT by lane WATER4 (2026-09-10), ''' + BUILT + '''.** The harmonic
> fill below was what WATER3 built and BUILD5b measured; bungo saw its picture
> and said *"That stroke doesn't look smooth at all, it's like overlapping
> circles more like"* -- measured as 39 seam-bounded constant-direction
> patches of radius 12-15 texels (the stroke's half-width is 16) with 20-45
> degree seams, because every stroke segment's tangent was HELD over a
> capsule of the half-width and the fill only solved the slivers between
> (`scratchpad/lane_water4_report.md` section 1). His design, agreed: *"Could
> this maybe use a bit of some simulation though?"*
>
> **As rebuilt:** a POTENTIAL FLOW on the body's mask. `div( k grad phi ) = S`
> with `k` the water depth (body height minus the file's own level-0 terrain
> height, floored at 8 units) times a smooth quartic bump (x4 on a stroke, x1
> at its half-width -- the stroke's soft preference); no-flux at every bank
> face (the five-point stencil only reaches a neighbour inside the mask); `S`
> = +1 spread over the SOURCE texels, -1 over the SINK texels: outlet / source
> pins first (a disc of the stroke width round the pin), then the body
> table's own `outlet` / `source` contacts (every body texel within 2.5
> texels of the other body, or the nearest band within 64), then -- only when
> the body has none -- the strokes' first and last points (a disc each); a
> sink with no source gets a UNIFORM source (rain), and the reverse; a body
> with neither keeps the writer's constant and the note says why. Velocity
> `u = -grad phi` (flux over depth): continuity is a property, a half-width
> narrows doubles the speed. If the strokes' tangents disagree with the
> contact-driven flow under them (mean cosine < 0) the stroke wins and the
> solve is re-run from its ends. Solved by Jacobi-preconditioned conjugate
> gradient on the compacted wet set, balanced per connected piece, relative
> residual 1e-9, cap 20,000; a body whose bbox exceeds 2^20 texels (the sea)
> is solved in a WINDOW round its constraints (margin 256 texels) with the
> window's cut edges as an open far field. The written DIRECTION is the
> solve's, continued into slack water (< 2 percent of the mean speed) and the
> bank texels by the harmonic fill, then low-passed by 8 in-mask 3x3 vector
> averages -- the prototype measured a staircase bank's raw direction jumping
> 22-25 degrees (p99) within three texels of it; the SPEED nibble is
> `round( 8 |u| / mean |u| )`, saturating at 15. Confidence stays the geodesic
> distance-to-mark decay. Lakes: a still-water mark is still zero; a lake with
> an outlet contact and no source flows to it under rain. `WaterFlowGrid` and
> `WaterMarkDoc::solveBody` in `src/watermark.{h,cpp}`; gates F1-F8 in section
> 7 and `tests/spells/water_flow.sh`.

The per-texel field WAS a **constrained harmonic fill on the body's mask**:
''', 'replace'),
    ('### 3.8 The version bump, and the two refusals\n',
     '''### 3.7b Dye (lane WATER4, ''' + BUILT + ''')

bungo: *"a factory that's releasing toxic sludge into a river, or river
flowing into an ocean and the river and the ocean may have slightly different
color"*. Three new stroke kinds -- **DyePin (7)**: one point, a colour (4
RGBA bytes after the points; the record is 24 + 8n) and a strength in
`speed`, whose plume runs DOWNSTREAM; **DyeMouth (9)**: a one-point mark on a
river, "this river's water tints the body it drains into"; **DyeKnob (8)**:
the one knob, the half-distance in `width`, at most one per file, default
8,192 units -- and a fourth plane, the DYE plane, `uint32 = source | weight
<< 16` at the flow plane's rate, referenced from the version-3 header's
reserved word at `0xF4` under `SECT_DYE = 1 << 8`, so the version stays 3 and
no offset moves (`docs/LODGEN_BTD_FORMAT.md`, "The dye plane"). Written only
while a dye mark exists. The receiving body gets a DYE-ONLY field cut round
the mouth (the mouth as the source, the cut edges as the far field; its flow
words and its record are NOT touched, so an unstroked sea keeps its zero
flow); the weight is the steady advection-decay `u . grad c = -|u| c ln2 / L`
solved exactly in one pass in descending potential. Panel rows: Tool "Dye
pin", "Dye colour", "Dye fade" (Marking); "Dye at mouth" (Selected body);
Show "Dye". Ice in winter from the shore plane is READER-side and belongs in
section 6's checklist, not here.

### 3.8 The version bump, and the two refusals
''', 'replace'),
    ('| P8 round trip (ADDED) | save, reopen, save again is byte-identical, and the strokes come back OUT OF THE FILE | the model half |\n',
     '''| P8 round trip (ADDED) | save, reopen, save again is byte-identical, and the strokes come back OUT OF THE FILE | the model half |

### Lane WATER4 — the potential-flow solve and the dye (''' + BUILT + ''')

Pre-registered in `scratchpad/lane_water4_report.md` section 0 before any
code; run by `lodl <copy> --water-mark-selftest` (which now carries them) and
read back by name in `tests/spells/water_flow.sh`:

| gate | pass condition | prototype (numpy, `scratchpad/water4_20260910/flow_proto.py`) |
|---|---|---|
| F1 continuity | a channel that narrows to half its width: speed ratio 2.00 +- 5 percent; flux through 10 sections within 3 percent | 2.0000; 1.6e-10 |
| F2 island | parts and rejoins (halves within 2 percent); mass balance 1e-6 at every cell; straight-bank normal < sin 1 deg; island bank direction vs the analytic cylinder mean < 5, max < 15 deg | 0.5000 / 0.5000; 3e-12; 3.9e-4; **12.0 / 22.4 deg -- FAILS as registered**, R-independent (8, 16, 32 all ~12), one ring in 4.0, at 2R 0.8: the face-averaged velocity at a STAIRCASE bank cell, not the solve |
| F3 lake, no outlet | speed exactly 0 | 0 |
| F4 lake, one outlet | 0 texels point away; every streamline (Pollock's, on the face fluxes) reaches it | 0 of 3225; 40 of 40 |
| F5 the Charles | 0 seam-bounded patches; p99 adjacent jump < 5 deg; seams < 0.5 percent; cos to the mouth > 0.9; R stated | patches 0; **p99 7.0 at 8 passes (FAILS as registered by 2 deg)**; seams 0.26 percent; R 0.79 |
| F6 plume | 1/8 length and direction predicted before the dye, measured after, within 10 percent / 9 deg | 98.2 vs 96 texels; 0.0 vs 0.0 deg |
| F7 dye pin | 1/2 at L, 1/8 at 3L, 0 upstream | 0.5000; 0.1250; 0 |
| F8 cost | the Charles under 1.0 s, residual < 1e-8 | numpy 1.5 s at 1,565 CG iterations (the C++ is what is gated) |
| dye round trip | the plane is written only with a dye mark, reads back through `LodtFile::dyeWordAt`, and undo is byte-identical | C++ only |
''', 'replace'),
])

# --------------------------------------------------------------- WW_CHANGES --
W = 'WW_CHANGES.md'
b = open(W, 'rb').read()
cr = b.count(b'\r')
head = b'# NifSkope \xe2\x80\x94 Wild Wasteland Edition: Change Log\n\n'
assert b.startswith(head), 'the change log head moved'
entry = ('''## 2026-09-10 - potential flow inside each water body, and dye (lane WATER4) -- ''' + BUILT + '''

bungo, 2026-09-10, on the marking tool's after-picture: *"That stroke doesn't
look smooth at all, it's like overlapping circles more like."* Measured
(`scratchpad/water4_20260910/disc_metric.py`, through WATER2's independent
decoder): on the Charles after one stroke, **39 seam-bounded constant-direction
patches** of equivalent radius 12-15 texels (the stroke's half-width is 16),
seams of 20-73 degrees, p99 of the adjacent angle difference **40.78 degrees**,
2.97 percent of adjacent pairs across a seam. The cause, by line: the fill held
every stroke SEGMENT's tangent over a capsule of the half-width
(`src/watermark.cpp` 1077-1097 as built by BUILD5b) and the SOR solved only the
slivers between. His design, agreed: *"Could this maybe use a bit of some
simulation though?"*

`src/watermark.{h,cpp}` (the solver core `WaterFlowGrid`, the solve rebuilt as
`solveBody` + `solveDye`, the dye API, the dye plane packer, the flow gates and
F5/F8 and the dye round trip in the selftest), `src/watermarkpanel.cpp` (Tool
"Dye pin", rows "Dye colour" and "Dye fade", tick "Dye at mouth", Show "Dye"),
`src/lodtfile.{h,cpp}` (`LODL_SECT_DYE`, the dye plane read from the 0xF4 word,
`dyeWordAt`), `tests/spells/water_flow.sh` (NEW), `docs/LODGEN_BTD_FORMAT.md`,
`scratchpad/specs_20260909/spec_water.md`, `MISTAKES.md`,
`scratchpad/water4_20260910/`, `scratchpad/lane_water4_report.md`.

**The method.** Potential flow on the body's mask: `div( k grad phi ) = S`, k
the water depth from the file's own terrain (floored) times a smooth quartic
bump under a stroke; no-flux banks by construction; sources and sinks from
pins, then the table's own outlet/source contacts, then the strokes' ends, a
uniform "rain" when one side is missing; `u = -grad phi`, so a half-width
narrows doubles the speed (F1: 2.0000) and the flow parts round an island and
rejoins (F2: 0.5000 / 0.5000) by property, not by rule. Jacobi-PCG on the
compacted wet set, per-piece balanced, residual 1e-9. The written direction is
continued into slack water and bank texels and low-passed by 8 in-mask 3x3
vector averages; the speed nibble is the solve's own. **Dye:** a fourth plane
(uint32 source | weight << 16, the flow plane's rate) under a new section bit
from the version-3 header's reserved word -- the version stays 3, no offset
moves -- written only while a DyePin (7, with RGBA) or DyeMouth (9) mark
exists; the weight is the exact one-pass steady advection-decay in descending
potential (F7: 0.5000 at L, 0.1250 at 3L; F6: the plume's length predicted
before the dye 96, measured after 98.2 texels).

**Gates, pre-registered before the code** (`lane_water4_report.md` section 0)
and run so far only in the numpy prototype: F1, F3, F4, F6, F7 green; **F2's
island-bank gate FAILS as registered** (12.0 mean / 22.4 max against 5 / 15:
the face-averaged velocity at a staircase bank cell, R-independent, one ring
in 4.0 degrees); **F5's p99 FAILS as registered** (7.0 against 5 at 8 passes;
patches 0 of 39, seams 0.26 percent of 2.97). Neither gate was moved. The C++
has not run: P0-P8, `water_mark.sh`, `lodl_water.sh`, `water_flow.sh`, the
render-hook picture pair and the dye picture are all owed to the build
(`scratchpad/water4_20260910/PENDING.md`).

''').encode('utf-8')
assert b'\r' not in entry
out = head + entry + b[len(head):]
assert out.count(b'\r') == cr
open(W, 'wb').write(out)
print('%s: %d -> %d bytes, CR %d' % (W, len(b), len(out), cr))

# ----------------------------------------------------------------- MISTAKES --
M = 'MISTAKES.md'
b = open(M, 'rb').read()
assert b.count(b'\r') == 0
anchor = b'## 2026-09-10 \xe2\x80\x94 the brief\'s "no headers" was typed into the plan without an `ls` (lane NATIVE0b)\n'
assert b.count(anchor) == 1
entries = '''## 2026-09-10 — the "half-distance" knob was coded as an e-fold (lane WATER4)

**What was done.** The pre-registered gate F7 says a dye pin's weight is 1/2 one
half-distance L downstream and 1/8 at 3 L. The prototype's dye pass decayed by
`exp(-s/L)`.

**What was true.** That is an e-fold, not a half-distance: 0.368 at L, 0.050 at
3 L. The gate went red on its first run and said so.

**How it was found.** By running the pre-registered gate, which is the point of
registering it: the number was fixed before the code and the code was wrong
against it, not the other way round. Fixed to `0.5^(s/L)` in the prototype and
written that way in the C++.

**The rule.** A knob's NAME is a contract: "half-distance" means 1/2 at L, and
the gate is written from the name before the formula is typed.

## 2026-09-10 — a tracer seeded outside the cell it tested (lane WATER4)

**What was done.** Gate F4's streamlines were seeded at `x + 0.5` where `x` came
from `np.linspace`, after checking `wet[int(y), int(x)]`. Three of forty seeds
landed on DRY texels and the gate read "37 of 40 reached the outlet".

**What was true.** The flow was right; the instrument started three particles on
land. Fixed by seeding at `int(x) + 0.5`: 40 of 40.

**How it was found.** By tracing the three failures step by step instead of
reading the count -- the first step of each was "DRY at (59,13)".

**The rule.** CONSTITUTION 4: when a gate is red, read the instrument before the
thing it measures; a count is not a result. Twice this session the instrument
was the defect (the e-fold above), once the code.

## 2026-09-10 — six vexing parses, again (lane WATER4; the same entry lane WATER2 wrote)

**What was done.** `std::vector<double> z( size_t( n ) )` and two more like it
in the new solver -- a function declaration, and `g++ -fsyntax-only` reported
fourteen errors on the first USE of each name.

**What was true.** Lane WATER2 wrote this exact entry ("six most-vexing parses in
one file") and its fix ("give a single-argument container construction its fill
value, or use `resize`"). Repeating an entry that is already in the file is its
own entry (CONSTITUTION 2).

**How it was found.** The syntax pass, first try.

**The rule.** Read `MISTAKES.md` for the area BEFORE writing in it (CONSTITUTION
3, step 5); every `vector<T> v( n )` in this tree gets its second argument.

## 2026-09-10 — a heredoc ate a script and the next tool call refused (lane WATER4)

**What was done.** A 120-line Python patch script was written through a bash
heredoc. The heredoc terminator arrived with a CR behind it, bash reported
"here-document at line 1 delimited by end-of-file", the file was written
TRUNCATED mid-string, and the next Write refused because the file had "not been
read".

**What was true.** `nifskope-ww-lodgen`'s editing-trap section and lane WATER2's
ledger both already say: write the patch as a FILE, never as a heredoc. This is
the fourth time the tree has paid for it.

**How it was found.** The Python `SyntaxError: EOF while scanning triple-quoted
string literal` on the run that followed.

**The rule.** Every file over a few lines goes through the Write tool; a heredoc
is for a command, never for content.

'''.encode('utf-8')
out = b.replace(anchor, entries + anchor)
assert out.count(b'\r') == 0
open(M, 'wb').write(out)
print('%s: %d -> %d bytes, CR 0' % (M, len(b), len(out)))
