## Built the fix the brief named before measuring whether it could move the picture

Lane TERRAINFMT1's brief said our far-terrain chunks read blue-purple because our
colour sheet is DXT1 where vanilla's is DXT5, and item 2 was to write the DXT5
path. That was done: the corpus was measured, the writer was written, it was
built, and the sheets it writes match vanilla's container exactly.

Then item 3 rendered it. Our chunk with the old sheets and our chunk with
`--sheet-format vanilla`, same camera, same mesh: **0 pixels differ.** Mean
distance to vanilla's own render, 131.95 both ways. The cast was in our `.BTR`'s
vertex colours the whole time -- `src/lodgen.cpp:952` packs the terrain identity
channels (R = material class, G = wetness, B = occlusion, A = shore) into the
vertex colour slot, and a consumer that multiplies albedo by vertex colour gets
a blue world. `WW_RENDER_FLAT=1`, vertex colours alone: vanilla
237.3/237.3/237.3, ours 50.9/49.3/202.1. One render, and it would have said so
before a line of the writer existed.

**A brief that names the cause is a hypothesis.** The render proof was item 3 of
7 and it belonged at item 0. The cost was a build and two gates spent proving a
switch does what it says on a container nobody was looking at. The procedure for
not repeating it is now a skill, `.claude/skills/ww-render-arm-isolate` -- four
renders: the reference, the before, the candidate, and a mix arm that proves the
inputs are read at all.

## `lodgenBlendVanillaDetail` adds the east detail to north and the north detail to east

`src/lodgen.cpp:7083`, on the non-default `--land-detail-source vanilla-blend`
path. FOUND, NOT FIXED -- this lane had spent its one build and a fix would need
another, and the path is off by default.

`lodgenTerrainMsnPixel` packs `east << 16 | up << 8 | north` (R = east, G = up,
B = north). `lodgenBlendVanillaDetail` unpacks the opposite way:

```cpp
float e = float( p & 0xFFU ) / 255.0f * 2.0f - 1.0f;          // bits 0-7 = NORTH
float n = float( ( p >> 16 ) & 0xFFU ) / 255.0f * 2.0f - 1.0f; // bits 16-23 = EAST
e = qBound( -1.0f, e + dE[i], 1.0f );
n = qBound( -1.0f, n + dN[i], 1.0f );
```

It then writes them back into the same physical slots, so nothing is visibly
transposed -- the channels stay where they were and the sheet still decodes. The
damage is that **`dE` is added to the north component and `dN` to the east
one**: the detail gradients land on swapped axes, so the added relief leans the
wrong way. The unit-length recompute is `e*e + n*n`, symmetric, so it cannot
catch it either.

Two things let this hide. The packing law is stated in a comment eighteen
hundred lines above the reader, not next to it; and a swapped pair of axes on a
detail term is exactly the kind of defect that still produces a plausible sheet.
The general rule: **a function that unpacks a word someone else packed names the
channels from THAT function, quoted, in the same file** -- and a gate for a
directional term asks for an ASYMMETRIC input (relief along east only) and
checks which channel moved, because a symmetric fixture passes either way.

## Imported a module that measures at import time -- twice in one lane

`f3_struct.py` did `from f3_fit import tiled, RES`, and `f3_fit` runs a 22-texture
fit at module level. The import re-ran the whole fit. Then `f3_picture.py` did
`from f3_struct import tiled, hp` and re-ran that in turn.

**The same trap had already been caught once earlier in the same lane** --
`f5_written.py` importing `f5_cache` -- and the fix taken then was to COPY the
function into the second file with a comment saying why. Copying is what let it
happen again: the second copy is not a shared home, so the third script had
nothing to import but a measuring module.

Fixed properly by creating `f3_common.py` holding `rough`, `hp`, `corr`,
`phase_twin`, `tiled` and the constants, with a docstring stating that this
module exists so nothing imports a module that measures at import time, and
refactoring all three scripts onto it.

The rule: **a measurement script gets a `main()` or an `if __name__ ==
'__main__':` guard from the first line it is written**, and shared statistics go
in a module whose import does nothing. The tell that it has happened is a script
printing another script's output before its own.

## Two process errors worth one line each

**`local` assigns left to right, and the right-hand side is expanded first.**
`local n="$1" c="$2" m="$3" d="$H/roots/$n/..."` in `mkroots.sh` died with
`n: unbound variable` under `set -u`: `$n` in the fourth assignment is expanded
before `local` has assigned the first. Each variable that references an earlier
one goes on its own `local` line.

**A page-level caption is not covered by the panel cell rule.**
`ww-texel-picture` section 3 sizes each panel cell so a panel label cannot clip,
and `pairpage.py` did that correctly -- then its page caption, one long line
naming the camera and the mask, ran off the right edge of a canvas sized from
the tiles. Wrap the page caption to the page width, measure the wrapped height
and grow the canvas by it. It was only visible by opening the PNG, which is
section 5 of that same skill.
