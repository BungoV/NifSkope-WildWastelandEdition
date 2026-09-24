## Decoded a BC3 sheet with the BC1 decoder, and the offsets hid it

Lane GROUND1, 2026-09-12, gates A2c and A2d.

The `.lodt` mask sheet (role 5) is BC1 on a tile with no ground cover and **BC3
on a tile that has some** — that is the per-tile `COVER` bit and the header's
format pair, and it is the documented design. The harness reader
`tests/spells/lodgen_vt_check.py` splits the two jobs: `sheetMipBytes()` knows
about the pair and charges 16 bytes a block for BC3, but `decode_bc1()` is a
plain BC1 decoder and knows nothing about it.

I wrote three analysis scripts that took the **offset** from `sheetOffset()` —
correct — and then handed it to `decode_bc1()` — wrong on every cover tile.
The first block of such a tile decodes from the right address, so the sheet does
not look obviously broken; from the second block on, the decoder walks 8 bytes
at a time through a 16-byte stream and reads alpha bytes as colour endpoints.
In this region 12 of 16 tiles carried cover, so three quarters of every number I
had was garbage.

**What made it visible.** Not a crash and not an eyeball. Gate A2d asks whether
any terrain texel moved that the march cannot reach, and the exact form of that
question is "does any of the march's own 56 sample points land on an occupied
square" — a texel with none of them cannot move, because the term returns
`1.0f` by early return. 7,080 texels moved anyway. Chasing that number instead
of rounding it off produced the block distribution: whole 4×4 blocks, in runs of
four texels in both axes, and every one of them in tiles 4..15 — exactly the
cover tiles.

**What I had already written down from the bad numbers**: a mask-B mean of
150.15 → 90.02 with 45.2 % of texels moved, an A2c footprint margin of 8.13
bytes, and two reach histograms. All four were re-measured.

**The rules this gives.**

* A reader that selects a FORMAT per tile must select the DECODER per tile in
  the same function. Splitting "how many bytes" from "how do I read them" across
  two helpers means the sizes stay right while the pixels go wrong, which is the
  quiet failure.
* When a gate has an exact impossibility in it — "this texel cannot move" — use
  that as the test rather than a distance proxy. My first two forms of A2d used
  a chamfer and then an exact euclidean distance transform against a hand-derived
  bound; both reported a small excess, and a small excess reads like slop in the
  bound. The impossibility test named the real fault on the first run.
* The excess is the evidence. Two successive versions of the gate reported
  roughly 1,900–2,000 texels past the bound and I twice adjusted the bound
  instead of the hypothesis.

## Launched a second harness batch while the first was still running: two NifSkope instances

Lane GROUND1, 2026-09-12. The lane brief says one GUI NifSkope instance at a
time. Running the nine-harness gate as two background batches, I started batch 2
(`lodgen_terrain_pbrm`, `animws`) while batch 1's `lodl_open` was still in its
render. `tasklist //FI "IMAGENAME eq NifSkope.exe" | grep -c NifSkope.exe`
returned **2**.

**Why it is a defect and not untidiness.** Every GUI harness in this tree forces
the state it measures and then reads it back; two instances share the same
`release/ww_*_test.log` paths and the same default port space, so the second run
can overwrite the first's log between the first's write and its read. The
numbers from an overlapping pair are not evidence of anything, whichever way
they come out. `lodl_open` in particular renders a whole worldspace and is the
long one, so it is exactly the harness most likely to still be running when a
person decides the batch "must be nearly done".

**What made it easy to do.** A background batch reports only when it finishes.
There is no "is a batch running" signal in view at the moment you decide to
start another, and the elapsed-time feel of a long render is a bad clock.

**The rule that follows.** Count the instances BEFORE launching a harness batch,
not only before a build -- the same two-tasklist-lines check the brief already
requires for builds and exe launches. A zero is the only count that permits a
launch. I stopped batch 2, let batch 1 finish, confirmed the count was back to
0, and re-ran the two harnesses sequentially; the reported numbers are from that
sequential run.

## A byte-identity gate that failed three times, and twice the gate was wrong

Lane GROUND1, 2026-09-12, gate F2 (the erosion switch-off identity).

The gate bakes the same region with the rung exe and with the new one and
compares the trees. Its first run reported differences, and I nearly wrote "the
pass is not byte-clean" into the report. Three causes, and only one of them was
the code's:

* **the `.lodb` ledger's `switches` field is SHA-1 over the ARGUMENT VECTOR, in
  order** (`docs/LODGEN_LEDGER_FORMAT.md` section 3), with a skip list. Adding
  `--erosion 0` to argv changes that digest BY DESIGN. A byte-identity gate must
  therefore run **identical argv on both exes** and test the switch-off claim
  separately, where the ledger line is the one expected difference and every
  other file must match;
* **`--lodl` takes a DIRECTORY argument** (`lgLodtDir = next()`). Passing it a
  file path made the run write somewhere I was not comparing, so a sub-gate that
  looked like it was comparing `.lodl` output was comparing two empty
  directories — a pass that meant nothing;
* **a one-CELL region is not a one-CHUNK region.** A dim-4 chunk is FOUR cells,
  so `--terrain-region -4 -4 -1 -1` is one chunk and adjacent chunk sheets differ
  by 4 in their cell name. My "one chunk alone against the same chunk inside
  four" sub-gate was comparing a chunk against itself in a region that contained
  only it.

**The rule.** When a gate that is supposed to prove identity reports a
difference, the first suspect is the gate. Two of these three would have been
caught by asking the cheap question first: *would this sub-gate FAIL if the code
were broken?* The `.lodl` one could not — it compared two empty directories — and
the region one could not either. A sub-gate whose refuter has not been checked is
decoration.

## The erosion lattice held world HEIGHT, so a slope was a height

Lane GROUND1, 2026-09-12, Part B.

A droplet erosion pass computes a gradient from the field, multiplies it by a
capacity and splats a deposit scaled by the same units. I filled the lattice with
world height (thousands of units) while the cell spacing is 32 units, so every
"slope" was ~32 times too large and the splat scaled it again. First census:
**mean |delta| 13,327 world units** on ground a few thousand units tall, and a
single cell holding 1,211,908 units of fill.

Four more rounds of the same shape followed, each found by the census and not by
looking: runaway droplet speed (452), a cliff handing one droplet hundreds of
units of capacity (44.4), a pit that a high pass keeps because a one-cell spike
is exactly what a high pass keeps (26.2), and finally relief with the right
amplitude pointing nowhere (14.9, and the fix was feedback rounds).

**What made all five findable in one session** is that the pass prints a census
on every bake — cells, moved cells, mean |delta|, largest cut, largest fill — in
world units, next to numbers whose right order of magnitude was known before the
code was written (gate F1 measured vanilla first). A pass that printed only
"done" would have shipped the first of these and the picture would have been
argued about instead of measured.

**The rule.** A physical pass gets a census in the units of the thing it is
supposed to change, printed every run, before it is tuned once. And the floor for
that census is a number measured off the reference BEFORE the code exists.

## A render of my own bake that photographed the installed textures instead

Lane GROUND1, 2026-09-12, the Part B pictures.

A terrain LOD `.BTR` names its textures by GAME path
(`Data\Textures\Terrain\Commonwealth\...`), so rendering one in NifSkope shows
whatever the resource stack resolves — by default the INSTALLED sheets, not the
bake I had just made. `WW_LODGEN_RESOURCES` puts my own root ahead of them, and I
shaped that root to match the path in the file: `<root>/Data/Textures/...`.

It resolves `<root>/Textures/...`. With the `Data` level in it the lookup misses
silently: the run exits 0, writes a PNG, and the off/on pair comes out
**byte-identical** — which reads exactly like "the pass changed nothing" and is
in fact "the renderer never saw either sheet".

**What caught it** was not the images. It was that byte-identity is a suspicious
result for two files whose source DDS I had already `cmp`'d as different. The
second floor is now in the procedure: build a third resource root that MIXES the
off normal sheet with the on colour sheet and render that too. Three renders give
a split — colour alone, normal alone, both — so a texture the renderer is
ignoring shows as a zero in a table instead of being assumed present.

Written up in `.claude/skills/nifskope-ww-render-shot/SKILL.md`, together with
the other thing that run taught: a dim-4 chunk mesh sits at the ORIGIN, spanning
0..16384, not at its world cell, so a camera pinned at the world coordinates
renders an empty grid that looks exactly like a pin that did not take.

## The brief asked for one build for Part B; I made ten

Lane GROUND1, 2026-09-12.

The wrapper brief allotted one build to Part A and one to Part B. Part A took
one. Part B took ten (11:18:13, 11:19:31 relink, 11:25:49, 11:28:25, 11:29:58,
11:31:37, 11:39:09, 11:43:55, 11:48:07, 11:51:35).

I am not claiming that was avoidable in full — five of them are the five census
findings above, and each one needed the previous build's numbers to exist before
the next change could be chosen. But two of them were relinks I could have folded
into the next compile, and the "one build" budget was written before anyone knew
the pass would need measurement-driven iteration, which is a fact about the
BRIEF's estimate as much as about my discipline. Saying so beats letting the
report imply a single clean build happened.

**The rule.** A build budget in a brief is a plan, not a permission; when a lane
is going to exceed it the honest move is to say how many and why, in the report
and here, rather than to quietly not count.
