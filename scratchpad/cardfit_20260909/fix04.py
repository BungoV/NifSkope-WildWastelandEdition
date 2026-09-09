#!/usr/bin/env python3
"""CARDFIT3 fix 04 -- the WW_CHANGES entry, spliced in binary.

WW_CHANGES.md is a MIXED file (19,020 CR against 23,402 LF) and stays so
(CONSTITUTION 8). The newest entries at the top are LF-only, so this one is
written LF-only and the rest of the file is not touched.
"""
P = 'WW_CHANGES.md'
b = open(P, 'rb').read()
cr0, lf0 = b.count(b'\r'), b.count(b'\n')

HEAD = b'# NifSkope \xe2\x80\x94 Wild Wasteland Edition: Change Log\n\n'
assert b.startswith(HEAD), b[:60]

entry = """## 2026-09-09 - the impostor frame law: the tree fills the frame, the padding is the mip chain's, and the card does not move

`src/nifskope_ui.cpp`, `src/lodgen.cpp`, `tools/bake_impostor_cards.sh`,
`tests/spells/lodgen_octahedral.sh`, `tests/spells/lodgen_impostor_cards.sh`,
`docs/LODGEN_CARD_SHEETS.md`, `docs/LODGEN_LODM_FORMAT.md`,
`scratchpad/handoff_fo4cs/samples/make_samples.sh`.

bungo, three messages, the rule this entry implements, verbatim: *"biggest,
texture aspect ratio, maximizing the size of the geometry on each render, so
that there is still a little bit of padding, 8 pixels on 1024x1024, 16 on 2k,
and so on"*; *"maximizing the tree's size in each row and column, with enough
pixel padding so that there's no mip map bleeding into other rows and
columns"*; *"the tree must be positioned correctly, so that when a 3d tree
transitions to an imposter, the tree won't change position"*.

**MEASURED FIRST, on the 19-tree Sanctuary library** (`OCT=8 TILE=128`, 1,216
frames). The number bungo's rule is about is the UNION of the 64 views'
silhouette boxes, because one scale serves every frame and everything outside
that union is wasted in all 64:

| union box / frame | min | median | max |
|---|---|---|---|
| x, before | 0.125 | 0.500 | 0.875 |
| x, after | **0.250** | **0.729** | 0.875 |
| y, before | 0.562 | 0.812 | 0.875 |
| y, after | **0.797** | **0.859** | 0.875 |

and the sheets came out **2.5% SMALLER** doing it. The frame's own centre was
never the problem and is not touched: the union box sat within ONE texel of the
frame centre in x and 3.5 in y before the change.

**Cause, in order of cost.** (1) The short side came off a five-rung ratio
ladder floored at `max(32, 2G+4)`; a 1:5.5 tree asks for a rung below a quarter
and there was none, so TreeBlasted05 filled **4 texels of a 32-texel frame**.
(2) The gutter was a fraction of the LONG side applied to both, so a 32-texel
short side spent 25% of itself on margin where a 128 side spent 12.5%. (3) The
mip cap had nothing to do with the gutter at all.

**The padding law, derived from his numbers rather than fitted to them.** A
reader sampling inside a frame's UV rect reaches half a texel past the rect AT
the rect's own border, and that tap lands in the next frame. Mip CONSTRUCTION
never mixes frames (an even frame halves into an even frame), so the bleed is at
SAMPLE time and a level is clean only while its gutter is a whole texel:
`pad / 2^k >= 1`, hence `mips = 1 + log2(pad)` and never one more.
`pad = side/16` IS his two numbers -- an 8x8 grid of 128-texel frames is a 1024
sheet and gets 8, a grid of 256-texel frames is a 2048 sheet and gets 16 -- now
applied PER AXIS, floored at 2 so every card ships two clean mips, and rounded
UP TO EVEN so `--card-half-aux` lands the aux gutter on a whole texel.

**That closes a bleed that was shipping.** Measured with a control (the same
sheet with the padding stripped, which must bleed and does, on 19 of 19 bases up
to 127/255): before, every card of 96 texels or more shipped a mip whose gutter
was half a texel -- **26/255 of a neighbouring frame's alpha across 16 of 28
frame borders** on the 128x128 sheets, 15/255 on 10 of 28 for 96x128. After: **0
on every border of every shipped mip of all 19.**

**The aspect** is now the smallest multiple of 16 texels whose inner rect does
not crop the measured silhouette -- it grows the frame until the tree fits, so
the loose axis gets air and the binding one is never cut. Seven frame shapes
over the 19 trees against four; a card array still groups by family, grid and
frame. The measurement margin on the silhouette came down from 4% to **1%**:
pass one measures in viewport pixels at the bound fit, whose error is about one
viewport pixel = 0.24% of a half-extent, and 4% was seventeen times that.

**THE CONTRACT NOW SAYS WHERE THE CARD STANDS.** `card.center` always was the
offset from the object's PIVOT to the card's centre -- the bake points its camera
at that one model-space point in all N^2 views -- but nothing said so, and a
reader that treated it as zero would drop TreeHero01's card **1,070 units**, most
of the tree. `docs/LODGEN_LODM_FORMAT.md` 3.1 now states it and the reasoning.
New key `card.pad` / `array.pad` (int[2], the padding per axis on each side), so
a reader never re-derives the law that produced the sheet.

**Gated against a SECOND reader, never against the bake.** The transition gate
reads each model's own declared per-shape `Bounding Sphere` fields through
`-no-gui dump` and merges them, and requires `card.center` to be at least four
times closer to that centre than the pivot is. Measured 6.4x / 12.3x / 19.7x on
TreeHero01, TreeMapleForest2 and TreeBlasted01 (delta 14.1% / 8.0% / 4.9% of the
model's own radius against 90.9% / 97.8% / 96.7% for the zeroed control). It is
a discrimination test and not a one-texel test on purpose: the renderer
recomputes each shape's bound from the VERTICES, so the file's declared spheres
can bound `center` but cannot confirm it to a texel.

**The card quads' `_fs.DDS` is DXT5 now, and carries its alpha.** It went out as
DXT1 with `pfflags 0x4` and no `DDPF_ALPHAPIXELS`, which is why every card quad
in a chunk drew as an OPAQUE SQUARE (lane IMAGES5). Vanilla's own alpha-tested
tree LOD textures -- `Textures/LOD/Trees/MapleBranchesLOD_d.dds` and
`ElmBranchesLOD_d.dds` -- are DXT5 with `dwFlags 0x000A1007`, `pfflags 0x4`,
`caps 0x401008`, and those four numbers are what the gate checks, not a guess at
a format. The sheet doubles, 1,014,600 -> 2,029,072 bytes for a 3014x501 card.

**Two defects found in the render hook while building the gate, and fixed.**
`WW_RENDER_DIST` had NO effect -- `setDistance` sets `Dist` and the
orthographic half-height is `Dist/Zoom`, and the auto-fit leaves `Zoom` where it
framed the bound sphere, so one tree photographed at half-heights 1874 and 7496
produced two BYTE-IDENTICAL PNGs. It reads the value back and corrects now, the
way the impostor bake always did. And `WW_RENDER_CLEAN=1` photographs the model
alone -- no viewport grid, no axis lines, no node markers -- because a script
measuring a silhouette cannot tell a grid line from a twig, and the grid alone
put the measured bounding box at the full width of the window.

**Still open, and named:** the hook is not yet a metric camera.
`WW_RENDER_CENTER` has no effect on the framing (two renders 500 units apart are
byte-identical), and the scale is not proportional to `WW_RENDER_DIST` -- a
512-unit cube written by `-no-gui new --cube --size 512` spans 547, 107 and 25
px at `WW_RENDER_DIST` 500, 1000 and 2000. Until that is fixed no picture from
this hook can carry a world-unit measurement, which is why the transition gate
is geometric and not photographic.

Gates: `lodgen_impostor_cards.sh` 14/0 (the DXT5 header and a live alpha block
are new). `lodgen_octahedral.sh` gained the frame law as multiples of 16, the
padding checked per axis and exact on all four sides, the silhouette FILLING the
inner rect's long axis to within 2%, cross-frame bleed at every shipped mip with
the padding-stripped control that must fail, and the dilation under the
transparent texels.

"""
out = HEAD + entry.encode('utf-8') + b[len(HEAD):]
open(P, 'wb').write(out)
n = open(P, 'rb').read()
assert n.count(b'\r') == cr0, (n.count(b'\r'), cr0)
print('WW_CHANGES.md: CR unchanged at %d, LF %d -> %d' % (cr0, lf0, n.count(b'\n')))
