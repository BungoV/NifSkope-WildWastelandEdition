#!/usr/bin/env python3
"""Lane CARDFINAL's WW_CHANGES.md entry, spliced in BINARY under the title.

`WW_CHANGES.md` is a MIXED file -- 19,020 CR against 23,677 LF -- and the rule is
to match neighbours and never normalise (CONSTITUTION 8). The head of the file is
LF-only, so this entry is LF-only and the CR count must not move.
"""
import sys

P = 'WW_CHANGES.md'
b = open(P, 'rb').read()
CR0 = b.count(b'\r')

MARK = b'# NifSkope \xe2\x80\x94 Wild Wasteland Edition: Change Log\n\n'
if b.count(MARK) != 1:
    print('title marker matched %d times' % b.count(MARK))
    sys.exit(1)

ENTRY = """## 2026-09-09 - impostor cards: one mip fewer, and every frame in its own place

Two rulings of bungo's, agreed together ("I agree on all these"), in
`src/nifskope_ui.cpp` (the bake) and `src/lodgen.cpp` (the sidecar reader, the
`.lodm`, the card-array packer). Built 23:41:00, four gates green, nothing flown.

**1. SHIP ONE MIP FEWER.** `mips = log2(gap)`, not `1 + log2(gap)`; `auxMips =
log2(gap/auxDiv)`; both floored at one level. His words: *"so the deepest shipped
level still has a full texel of margin per side (128 frame, gap 8 -> 3 levels
128/64/32)"*.

A tap taken exactly ON a frame's UV border reads half of that frame's last texel
and half of the neighbour's first, so what it picks up OF THE NEIGHBOUR is
decided by the MARGIN INSIDE EACH FRAME, `gap/2^(k+1)` -- not by the gap. The
previous cap shipped the level where that margin is half a texel, which is
exactly a border tap's reach. Measured on the same 19-tree library under both
caps, changing nothing else:

| | the old cap | the new cap |
|---|---|---|
| shipped mips, min / median / max | 2 / 2 / 4 | 1 / 1 / 3 |
| narrowest gap at any shipped mip | 1.408 / 1.957 / 2.000 texels | 2.000 / 2.000 / 2.000 |
| worst neighbour alpha a border tap picks up | 0 / 5 / **64** per 255 | **0 / 0 / 0** |
| **sheets that bleed at any shipped mip** | **13 of 19** | **0 of 19** |

The two OLDER sidecar vintages do not move: each wrote a PER-SIDE number whose
gap is twice it, and `log2(2*pad) = 1 + log2(pad)` is the chain they were built
for. `octMipUnit` is now the GAP under all three readings, which is what makes
that arithmetic rather than luck.

**2. PER-FRAME POSITIONING, shipped.** Every frame now shifts its OWN silhouette
to its own centre, and the shift is written into the `.lodm` as
`card.frameOffset` -- two numbers per frame, in model units, along that view's
own right and up axes, frame `(i,j)` at index `j*oct+i` -- with the same key per
LAYER on a `cardArray`. The sidecar gains `frameoff <i> <j> <ox> <oy>` (one line
a frame), `frameclamped <n>` and `framefit <sx> <sy> <ux> <uy>`.

At one scale about one centre, each view's silhouette box sits at its own offset
inside the frame, so the frame had to hold the UNION of all N^2 boxes. Centring
each view in its own frame means the frame holds only the WIDEST SINGLE VIEW.
The scale does not change (`half` is still one pair, so the tree is the same size
in every view, his rule of the same afternoon), the quad is still the whole frame
(so a three-frame blend still blends one shape), and `center` still means what it
meant. Over the 19 trees, OCT=8 TILE=128:

| | before | now |
|---|---|---|
| what ONE view fills of its frame, x, min / median / max | 0.188 / **0.646** / 0.938 | 0.250 / **0.750** / 0.938 |
| the same, y | 0.812 / **0.891** / 0.938 | 0.906 / **0.938** / 0.938 |
| a frame's silhouette off its own centre, worst texels | 15.0 x, 20.5 y | **7.0 x, 4.5 y** |
| total sheet area | 4,816,896 texels | **4,358,144 (-9.5%)** |
| frame shapes (one card array each) | 6 | 7 |
| frames whose crop had to be clamped | n/a | **0 of 1,216** |

From the bake's own `framefit` line, the frame is narrower than a fixed centre
needs by a median **24.8% on x** and **7.7% on y**. Five bases dropped a frame
rung on it; `00121550` BurntTreeUpright02 went from 48x64 to 32x64 with its
single view filling **0.938** of the frame where it filled 0.646. One base,
`000531b3`, lost a texel of width because the aspect widening's binding axis
flipped when its inputs shrank -- measured and stated, not hidden.

**A reader that ignores `frameOffset`** draws every quad at `center`, which is
what a set from before this law carries, and the tree steps sideways as the mesh
hands over. Absent means absent: a layer from an older set carries no key.

**Contracts:** `docs/LODGEN_CARD_SHEETS.md` 3.1, 3.4, 3.5, the new 3.6 and
invariants 1a/1b; `docs/LODGEN_LODM_FORMAT.md` `card.mips`, `card.frameOffset`,
the `cardArray` layer, 3.1 and invariant 5. The `--card-half-aux` figure in 3.5
is corrected from 46.4% to **42.9%** -- the same saving under a chain that lost a
level, not a smaller saving.

**Gates**, all green on the 23:41:00 exe: `lodgen_octahedral.sh` **85 ok / 0**
(two real bakes; new: the per-frame centring with the control that undoing the
shift makes it worse, the offsets against the picture with the control that a
sheet of zeros could not reach the extent, zero border alpha at every shipped
mip with the stripped-margin control at 255/255, and the aspect ladder as its own
law rather than as a texel budget), `lodgen_card_arrays.sh` **33 ok / 0** (the
fixtures now state their own `gap`, and the half-aux saving is the exact byte
count the class, the gap and `auxDiv` account for), `lodgen_impostor_cards.sh`
**12 ok / 0**, `lodgen_identity.sh` **8 ok / 0**.

**Pictures:** `scratchpad/cardfinal_20260909/pics/cardfinal_0003a28b.png`
(TreeHero01) and `cardfinal_000393cd.png` (TreeBlasted02) -- four panels each,
before above and now below, mip 0 left and each library's own deepest shipped mip
right, on the border that bleeds worst, texel grid and checkerboard, red where a
covered texel touches the border. TreeHero01: **56/255 -> 0**.

"""

b = b.replace(MARK, MARK + ENTRY.encode('utf-8'), 1)
open(P, 'wb').write(b)
b2 = open(P, 'rb').read()
print('CR %d -> %d (must not move); LF %d' % (CR0, b2.count(b'\r'), b2.count(b'\n')))
if b2.count(b'\r') != CR0:
    sys.exit(1)
