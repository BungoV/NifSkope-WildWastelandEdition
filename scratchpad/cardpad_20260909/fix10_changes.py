#!/usr/bin/env python3
"""CARDPAD -- the WW_CHANGES entry. The file is MIXED CRLF/LF and stays so; the
newest entries at the top are LF, so this one is spliced in binary as LF and the
CR count is asserted unchanged."""

P = 'WW_CHANGES.md'
b = open(P, 'rb').read()
cr0 = b.count(b'\r')

HEAD = b'# NifSkope \xe2\x80\x94 Wild Wasteland Edition: Change Log\n\n'
assert b.startswith(HEAD)

ENTRY = """## 2026-09-09 - the card spacing is the GAP between two trees, not the margin beside one

`src/nifskope_ui.cpp`, `src/lodgen.cpp`, `tools/bake_impostor_cards.sh`,
`tests/spells/lodgen_octahedral.sh`, `docs/LODGEN_CARD_SHEETS.md`,
`docs/LODGEN_LODM_FORMAT.md`.

bungo, correcting the entry above the same evening, verbatim: *"When I say
padding 8 for 1k, it's 8 pixels of distance between two rendered objects."* The
lane before this one read his number as the margin on EACH side of a frame and
spent twice the texels he asked for.

```
gap(side) = max( 2, side / 16 ), rounded UP to even     // between two silhouettes
pad(side) = gap(side) / 2                               // on EACH side of a frame
mips      = 1 + log2( min( gapX, gapY ) )
```

so the inner rect is `frame - gap`, **15/16 of the frame** wherever a side is a
multiple of 32 -- 120 of 128 -- and a 128-texel frame still ships **4** levels
(128, 64, 32, 16), because that count was always the gap's and the gap has not
changed. The SHEET'S OUTER BORDER needs only half a gap and gets exactly that
without a special case: an interior border carries `gap/2` from each of the two
frames meeting on it, an outer border carries `gap/2` and faces the sheet edge.
That holds because a card sheet is sampled CLAMPED (the quad's UV rect is a
sub-rect and nothing asks for wrapping); under WRAP the outer border would need
a whole gap.

Measured over the same 19 Sanctuary trees, OCT=8 TILE=128, both baked by this
tree (`scratchpad/cardfit_20260909/cards_after` -> `scratchpad/cardpad_20260909/cards_gap`),
coverage floor 16/255:

| | before | after |
|---|---|---|
| silhouette / frame, x | 0.250 / **0.729** / 0.875 (min/median/max) | 0.250 / **0.854** / 0.938 |
| silhouette / frame, y | 0.797 / **0.859** / 0.875 | 0.844 / **0.922** / 0.938 |
| inner rect / frame, y | 0.875 on all 19 | **0.9375 on all 19** (15/16) |
| clear texels between two silhouettes, mip 0 | 4 / 10 / 24 | 2 / 7 / 16 |
| narrowest gap at any shipped mip | 2.000 texels | **1.408** texels, and never under 1 |
| total sheet area | 4,915,200 texels | 4,816,896 (-2.0%) |
| frame shapes | 7 | 6 |

Two trees changed frame shape because the wider inner rect let a narrower frame
hold them: TreeMapleblasted04 32x64 -> 16x64 (fill x 0.438 -> 0.875) and
TreeMapleblasted05 32x32 -> 16x32 (0.500 -> 0.875).

**What it costs, stated rather than buried.** His rule keeps a whole texel of
gap at the deepest shipped level, which is HALF a texel of margin on each side,
and a bilinear tap taken exactly on a frame border reaches half a texel. So at
that last level a border tap now picks up some of the neighbour's edge: **13 of
19 sheets, worst 64/255**, against 0 of 19 under the per-side reading. Zero is
bought by shipping one level fewer (`mips = 1 + log2(min(pad))` -- 3 levels on a
128 frame instead of 4), not by more padding. His call; shipped as he stated it.

`.lodm` and sidecar. The meta line is now `gap <x> <y>` (the distance between
two silhouettes); the `.lodm` keeps `card.pad` / `array.pad` as the **per-side**
number and gains `card.gap` / `array.gap`, exactly twice it, so a reader never
has to guess which quantity a lone number meant. Three vintages of card set are
read, each under its own law: `gap` present (this law), `pad` alone (the
per-side reading, capped at `1 + log2(min(pad))`), neither (older still,
`max(4, longSide/16)` per side under that same older cap).

Gates: `tests/spells/lodgen_octahedral.sh` **73 checks / 0 failures / PASS**
(the bleed check is now the gap rule -- the separation across every interior
border at every shipped mip, measured as transparent coverage over the two
texels a border tap reads, narrowest 1.659 on the harness bake; its control,
every silhouette cropped to its own box and filling its cell, measures 0.431 and
fails as it must). `tests/spells/lodgen_impostor_cards.sh` PASS,
`tests/spells/lodgen_identity.sh` PASS (8/0), untouched.

"""

nb = HEAD + ENTRY.encode('utf-8') + b[len(HEAD):]
assert nb.count(b'\r') == cr0, 'CR moved %d -> %d' % (cr0, nb.count(b'\r'))
open(P, 'wb').write(nb)
print('WW_CHANGES.md: %d -> %d bytes, CR %d (unchanged)' % (len(b), len(nb), cr0))
