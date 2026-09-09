#!/usr/bin/env python3
"""CARDPAD -- the two contract pages: the spacing number is the GAP between two
neighbouring silhouettes, and the .lodm records the PER-SIDE half of it."""

def edit(path, pairs):
    b = open(path, 'rb').read()
    cr0 = b.count(b'\r')
    s = b.decode('utf-8')
    for what, old, new in pairs:
        n = s.count(old)
        assert n == 1, '%s / %s: %d occurrences' % (path, what, n)
        s = s.replace(old, new)
    nb = s.encode('utf-8')
    assert nb.count(b'\r') == cr0, '%s: CR moved' % path
    open(path, 'wb').write(nb)
    print('%s: %d -> %d bytes, CR %d' % (path, len(b), len(nb), cr0))


# ==================================================== docs/LODGEN_CARD_SHEETS.md
CS = []

CS.append(('3.1 the law', """### 3.1 The padding, and the mips it buys

Every frame keeps a transparent gutter on each side. It is **per axis** and it
is exactly what the shipped mip chain needs, no more:

```
pad(side) = max( 2, side / 16 ), rounded UP to even
padX = pad(frameW)      padY = pad(frameH)
mips = 1 + log2( min( padX, padY ) )
```

**Where the numbers come from.** A reader sampling inside a frame's UV rect
reaches half a texel past the rect *at the rect's own border*, and that tap
lands in the neighbouring frame. Mip CONSTRUCTION never mixes frames -- the box
filter halves an even frame into an even frame -- so the bleed is at SAMPLE time,
and a level `k` is clean only while its gutter is still a whole texel:
`pad / 2^k >= 1`, hence `1 + log2(pad)` levels and never the one after.

`pad = side/16` is bungo's own rule (2026-09-09): *"8 pixels on 1024x1024, 16 on
2k, and so on"*. An 8x8 grid of 128-texel frames IS a 1024 sheet and gets 8; a
grid of 256-texel frames is a 2048 sheet and gets 16. Applied per axis so the
gutter is the same fraction of a short side as of a long one; floored at 2 so
every card ships at least two clean mips; rounded up to even so `--card-half-aux`
(§3.5), which halves both sides, still lands the aux gutter on a whole texel.

| frame | padX, padY | mips | gutter at the last mip |
|---|---|---|---|
| 16x64 | 2, 4 | 2 | 1 texel |
| 32x32, 32x64 | 2, 2 / 2, 4 | 2 | 1 texel |
| 48x64 | 4, 4 | 3 | 1 texel |
| 96x128 | 6, 8 | 3 | 1.5 texels |
| 128x128 | 8, 8 | 4 | 1 texel |
| 256x256 | 16, 16 | 5 | 1 texel |

**What this replaced** (before 2026-09-09): one gutter `max(4, tileLong/16)` on
all four sides, and a chain that ran *while a frame's shorter side spanned eight
texels*. The two were unrelated, and the pair shipped a BLEEDING level on every
card of 96 texels or more -- measured on the 19-tree Sanctuary library: mip 4,
gutter half a texel, 26/255 of a neighbouring frame's alpha across 16 of 28
frame borders.

`halfW`/`halfH` in the `.lodm` span the full frame, padding included -- **the
quad is the frame** -- and a consumer that insets by the padding shrinks the
object. The silhouette occupies the INNER rect, `frame - 2*pad`, and the `.lodm`
says what the padding is (`card.pad`, `array.pad`) so a reader never has to
re-derive the law.
""", """### 3.1 The gap, the padding, and the mips they buy

**The number is the distance between two neighbouring silhouettes**, not the
margin on one side of a frame. bungo, 2026-09-09, verbatim: *"When I say padding
8 for 1k, it's 8 pixels of distance between two rendered objects."* On a 1024
sheet of 8x8 frames, two trees are 8 texels apart across the border the two
frames share, so each frame keeps HALF of that as its own margin:

```
gap(side) = max( 2, side / 16 ), rounded UP to even
pad(side) = gap(side) / 2                       // on EACH side of a frame
gapX = gap(frameW)   gapY = gap(frameH)
padX = gapX / 2      padY = gapY / 2
inner rect = (frameW - gapX) x (frameH - gapY)
mips = 1 + log2( min( gapX, gapY ) )
```

The inner rect is **15/16 of the frame** wherever a side is a multiple of 32 --
120 of 128, 240 of 256 -- which is his 8-on-1024 and 16-on-2k exactly. A side of
48, 80 or 112 rounds the gap up to even and gives back at most one texel.

**Where the mip count comes from.** Mip CONSTRUCTION never mixes frames -- the
box filter halves an even frame into an even frame -- so the bleed is at SAMPLE
time: a tap on a frame's own UV border reads half of that frame's last texel and
half of the neighbour's first. What has to survive at level `k` is therefore the
SEPARATION between the two silhouettes that meet on the border, `gap / 2^k`, and
the sheet ships every level where that is still a whole texel: `1 + log2(gap)`
levels and never the one after. A 128-texel frame gets 4 (128, 64, 32, 16).

**The sheet's outer border needs only half a gap.** There is no neighbouring
frame beyond it, so padding every frame by `gap/2` is already correct at the
edge and no special case exists: an interior border carries `gap/2` from each of
the two frames that meet on it, an outer border carries `gap/2` and faces the
sheet edge. That holds because the sheet is sampled **clamped** -- a card quad's
UV rect is a sub-rect of the sheet, and neither the DDS nor the `.lodm` asks for
wrapping. Under WRAP the outer border would face the opposite edge's frames and
would need a whole gap; nothing in this format does that.

Floored at 2 so every card ships at least two levels; rounded up to even so the
gap splits into two whole texels of margin.

| frame | gapX, gapY | padX, padY | inner rect | mips | gap at the last mip |
|---|---|---|---|---|---|
| 16x64 | 2, 4 | 1, 2 | 14x60 | 2 | 1 texel |
| 32x32 | 2, 2 | 1, 1 | 30x30 | 2 | 1 texel |
| 32x64 | 2, 4 | 1, 2 | 30x60 | 2 | 1 texel |
| 48x64 | 4, 4 | 2, 2 | 44x60 | 3 | 1 texel |
| 96x128 | 6, 8 | 3, 4 | 90x120 | 3 | 1.5 texels |
| 128x128 | 8, 8 | 4, 4 | 120x120 | 4 | 1 texel |
| 256x256 | 16, 16 | 8, 8 | 240x240 | 5 | 1 texel |

**What this replaced.** Twice, both on 2026-09-09. Before either: one gutter
`max(4, tileLong/16)` on all four sides and a chain that ran *while a frame's
shorter side spanned eight texels* -- two unrelated rules, which shipped a
BLEEDING level on every card of 96 texels or more (measured on the 19-tree
Sanctuary library: mip 4, gutter half a texel, 26/255 of a neighbouring frame's
alpha across 16 of 28 frame borders). Then lane CARDFIT3 read the number as the
PER-SIDE margin, `pad = max(2, side/16)`, which spent twice the texels bungo
asked for and left the inner rect at 7/8 of the frame instead of 15/16. The mip
COUNT is the same under both readings, because the count was always the gap's.

`halfW`/`halfH` in the `.lodm` span the full frame, padding included -- **the
quad is the frame** -- and a consumer that insets by the padding shrinks the
object. The silhouette occupies the INNER rect, `frame - 2*pad`, and the `.lodm`
says both numbers (`card.pad` / `array.pad`, the margin on EACH side; `card.gap`
/ `array.gap`, twice it) so a reader never has to re-derive the law or guess
which quantity a single number meant.
"""))

CS.append(('3.1 dilation depth', """The dilation is `max(8, max(frameW,frameH)/8)` passes deep, which
is at least the padding on every frame shape above.""",
"""The dilation is `max(8, max(frameW,frameH)/8)` passes deep, which
is at least a whole gap on every frame shape above -- so a coarse mip that
averages across the margin still averages the tree's own colour."""))

CS.append(('3.3 aspect ladder', """**Aspect**, from the measured silhouette. Five rungs for the short side —
`1, 3/4, 1/2, 3/8, 1/4` — rounded to a multiple of 4, never below the gutter
floor. The 3/4 rung is measured, not symmetric.

Both are nearest-in-log, which bounds the rounding to about 15%, and the fit
**grows** whichever extent is loose rather than cropping: the cost is air inside
a frame, never a cut silhouette.""",
"""**Aspect**, from the measured silhouette: §3.2, the smallest multiple of 16
texels whose inner rect does not crop it. (It was a five-rung ratio ladder until
2026-09-09; that ladder's floor is the defect §3.2 records.)

The size ladder is nearest-in-log, which bounds its rounding to about 15%, and
both fits **grow** whichever extent is loose rather than cropping: the cost is
air inside a frame, never a cut silhouette."""))

CS.append(('3.4 mips', """`mips = 1 + log2(min(padX, padY))` -- **the padding decides it**, and §3.1 is the
derivation. The count is in the DDS header and in the `.lodm`'s `mips`. It is a
cap on the whole sheet's chain: a sheet mip halves every frame at once, and the
chain stops at the last level whose gutter is still a whole texel, because the
next one bleeds a neighbouring frame into every border sample.""",
"""`mips = 1 + log2(min(gapX, gapY))` -- **the gap decides it**, and §3.1 is the
derivation. The count is in the DDS header and in the `.lodm`'s `mips`. It is a
cap on the whole sheet's chain: a sheet mip halves every frame at once, and the
chain stops at the last level where a whole texel of gap still separates the two
silhouettes that meet on a border, because the next one has them touching."""))

CS.append(('3.5 half-aux', """The divide happens **after** frame dilation, and the padding is rounded up to an
EVEN number of texels precisely so a halving lands its gutter on a whole texel;
frames are multiples of 16, so a halved frame is still even and nothing mixes
across a frame border. The aux sheets' own mip count comes down with their
gutter: `auxMips = 1 + log2(min(padX,padY)/auxDiv)`. Sampling is unaffected — normalised UV reads a""",
"""The divide happens **after** frame dilation, and the gap is rounded up to an
EVEN number of texels precisely so a halving lands its margins on whole texels;
frames are multiples of 16, so a halved frame is still even and nothing mixes
across a frame border. The aux sheets' own mip count comes down with their gap:
`auxMips = 1 + log2(min(gapX,gapY)/auxDiv)`. On the smallest frames -- 16 and 32
texels, where the gap is already at its floor of 2 -- that division reaches 1 and
the aux sheets ship a SINGLE level: a fallback naming itself rather than a silent
bleed. Sampling is unaffected — normalised UV reads a"""))

CS.append(('5 meta line', """class <w> <h>                the frame size class
pad <x> <y>                  the gutter in texels on EACH side of a frame, per axis""",
"""class <w> <h>                the frame size class
gap <x> <y>                  the distance in texels between two neighbouring
                             silhouettes across a frame border, per axis; the
                             margin on each side of a frame is half of it"""))

CS.append(('7 invariant 1a', """1a. `mips == 1 + log2(min(pad[0], pad[1]))`, and therefore **no shipped mip of a
   card sheet bleeds across a frame border**: the gutter at the deepest shipped
   level is at least one whole texel on both axes. The silhouette lives in
   `frame - 2*pad`; `half` still spans the whole frame.""",
"""1a. `mips == 1 + log2(min(gap[0], gap[1]))`, and therefore **every shipped mip of
   a card sheet keeps a whole texel between the two silhouettes that meet on an
   interior frame border**. The silhouette lives in `frame - gap`, which is
   `frame - 2*pad`; `half` still spans the whole frame. The sheet's OUTER border
   carries half a gap and needs no more, because it is sampled clamped and has no
   neighbour beyond it."""))

CS.append(('9 samples', """Two libraries of the same 19 Sanctuary trees, OCT=8 TILE=128, are on disk:
`scratchpad/images_20260909/gen/cards_trees19` (the OLD frame law, exe 19:35:14)
and `scratchpad/cardfit_20260909/cards_after` (this law, exe 20:49:01). The
measurement scripts that read them are in `scratchpad/cardfit_20260909/`.""",
"""Three libraries of the same 19 Sanctuary trees, OCT=8 TILE=128, are on disk:
`scratchpad/images_20260909/gen/cards_trees19` (the five-rung aspect ladder, exe
19:35:14), `scratchpad/cardfit_20260909/cards_after` (this aspect law with the
spacing read as a PER-SIDE margin, exe 20:49:01), and
`scratchpad/cardpad_20260909/cards_gap` (this contract, the spacing read as the
GAP). The measurement scripts that read them are in
`scratchpad/cardfit_20260909/` and `scratchpad/cardpad_20260909/`."""))

edit('docs/LODGEN_CARD_SHEETS.md', CS)


# ==================================================== docs/LODGEN_LODM_FORMAT.md
LM = []

LM.append(('card pad key', """| `pad` | int[2] | `[padX, padY]`, the padding in texels on **each side** of every frame, per axis. The silhouette occupies the inner rect `frame - 2*pad`; `mips` is `1 + log2(min(padX,padY))` by construction. Absent on a set written before 2026-09-09: read `max(4, max(frame)/16)` on both axes, which is what those sheets carry |""",
"""| `pad` | int[2] | `[padX, padY]`, **the margin in texels on EACH side** of every frame, per axis. The silhouette occupies the inner rect `frame - 2*pad`. Absent on a set written before 2026-09-09: read `max(4, max(frame)/16)` on both axes, which is what those sheets carry |
| `gap` | int[2] | `[gapX, gapY]`, the **distance between two neighbouring silhouettes** across a frame border, per axis, in texels -- exactly `2*pad`, and the quantity bungo's number names (*"8 pixels of distance between two rendered objects"*, 2026-09-09). `mips` is `1 + log2(min(gapX,gapY))` by construction. Absent with `pad` present = a set from earlier the same day whose `pad` was written as the whole spacing and whose `mips` was `1 + log2(min(pad))`; absent with `pad` absent = older still, and the same older law applies |"""))

LM.append(('card mips key', """| `mips` | int | stored mips, `1 + log2(min(padX,padY))`: the chain stops at the last level whose **padding is still a whole texel**, because the next one bleeds the neighbouring frame into every border sample |""",
"""| `mips` | int | stored mips, `1 + log2(min(gapX,gapY))`: the chain stops at the last level where **a whole texel of gap still separates the two silhouettes** that meet on an interior frame border, because the next one has them touching |"""))

LM.append(('padding paragraph', """Every frame carries a transparent **padding** of `pad[0]` texels on its left and
right and `pad[1]` on its top and bottom, and every channel under a transparent""",
"""Every frame carries a transparent **margin** of `pad[0]` texels on its left and
right and `pad[1]` on its top and bottom -- so two neighbouring silhouettes are
`gap[0]` / `gap[1]` texels apart across the border they share, and the sheet's
outer border, which has no neighbour and is sampled clamped, carries half of
that. Every channel under a transparent"""))

LM.append(('array pad row', """| `pad` | `cardArray` | int[2] | the padding in texels on each side of a frame, per axis, shared. An array is built from the same PNGs and the same dilation as the per-card sets, so it inherits their padding and their clean mip depth |
| `mips` | `cardArray` | int | mip cap, shared -- `1 + log2(min(pad))` |""",
"""| `pad` | `cardArray` | int[2] | the margin in texels on each side of a frame, per axis, shared. An array is built from the same PNGs and the same dilation as the per-card sets, so it inherits their spacing and their clean mip depth |
| `gap` | `cardArray` | int[2] | the distance between two neighbouring silhouettes across a frame border, per axis, shared -- `2*pad` |
| `mips` | `cardArray` | int | mip cap, shared -- `1 + log2(min(gap))` |"""))

LM.append(('invariant 5', """5. `mips == 1 + log2(min(pad[0], pad[1]))`, so **no shipped mip bleeds across a""",
"""5. `mips == 1 + log2(min(gap[0], gap[1]))`, so **no shipped mip bleeds across a"""))

edit('docs/LODGEN_LODM_FORMAT.md', LM)
