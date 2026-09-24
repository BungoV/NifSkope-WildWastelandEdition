---

## 10. Aggregate sheets — one card set per forested CELL

**Contract version: aggregate sets carry `kind: "aggregate"` `.lodm` v1.**
**Status: SHIPPED, gated, never flown in a game.** Composite:
`lodgenAggregateBuild()` in `src/lodgenaggregate.cpp`; the card library is read
through `lodgenAggregateCards()` and the sheets are written by
`lodgenAggregateWrite()`, both in `src/lodgen.cpp`; the `.lodi` rows are
`docs/LODGEN_NATIVE_LODO_LODI.md` §4.6. Switch: `--aggregate` (off by default,
and off is byte-identical).

bungo, 2026-09-11 08:3x, *"1 sounds good"*, over his own description: *"At ring
3 the bake takes each cell's trees, places their cards with the same rotation
and mirror the repetition breaking would give them, photographs the whole
cluster from the horizon views, and writes one aggregate sheet per cell."*

### 10.1 On disk

```
Data\Textures\Lodgen\Aggregate\<ws>\<cellX>_<cellY>_agg_d.DDS      legacy  BC3
                                     <cellX>_<cellY>_agg_n.DDS              BC3
                                     <cellX>_<cellY>_agg_gsaos.DDS          BC3
                                     <cellX>_<cellY>_agg.lodm       kind "aggregate"

                                     <cellX>_<cellY>_agg_bc.DDS     pbr     BC3
                                     <cellX>_<cellY>_agg_n.DDS              BC3
                                     <cellX>_<cellY>_agg_rmaos.DDS          BC3
```

**THREE sheets, not four: there is no emissive.** A forest emits nothing, and
the `.lodm` says so with `emissiveScale` 0 (§4 of
`docs/LODGEN_LODM_FORMAT.md`). A reader binds black.

### 10.2 The grid is a RING, not a hemisphere

A card's `oct` is frames per side of a hemi-octahedral grid over a whole
hemisphere of directions. **An aggregate is photographed at the HORIZON only**,
because that is where ring 3 is, so its grid is `views × 1`: `views` azimuths in
ONE ROW, frame `v` at pixels `[v·frameW, (v+1)·frameW) × [0, frameH)`, with

```
eye(v)   = ( cos φ, sin φ, 0 )          φ = 2π·v / views
right(v) = ( −sin φ, cos φ, 0 )
up(v)    = ( 0, 0, 1 )
```

**8 azimuths by default, and the reason is the cell's own shape.** A cell is a
square footprint: its projected width swings from 4,096 units face-on to 5,793
corner-on — 41 percent, with a period of 90°. Eight azimuths at 0/45/90/… sample
both extremes exactly, so the two hardest views are photographed rather than
interpolated. The alternative considered and refused was the hemi-octahedral
grid's outer RING, which is the whole horizon at `4N−4` frames (28 at N = 8):
3.5× the sheet, and its azimuths are not uniform — `z = 1 − max(|u|,|v|)`, so
the boundary is walked in equal steps of `x+y` rather than of angle and the
frames bunch toward the diagonals. The cost of the coarse step is that a reader
blending two neighbouring frames is up to 22.5° from either; §10.6 is what that
costs, measured.

### 10.3 The frame law is the card law, unchanged

Long side = the run's aggregate tile (`--aggregate-tile`, default 64); short
side = the smallest multiple of 16 whose INNER rect is not narrower than the
measured silhouette; `gap(side) = max(2, side/16)` rounded up to even;
`pad = gap/2`; `mips = max(1, log2(min(gap)))`; `half` spans the WHOLE frame,
padding included, and every channel is dilated out from the silhouette frame by
frame. §3.1–§3.4 are the derivations and none of them moved.

What the aggregate measures instead of a model's silhouette is the CELL's:

```
the silhouette box of view v = the union of the cell's tree QUADS in that view
```

computed analytically from each tree's own `half`, `center` and `frameOffset` —
no render, because a card quad is a rectangle of known world size at a known
place. The frame then holds the WIDEST SINGLE VIEW and each view shifts its own
silhouette to its own centre, which is §3.6's law applied per azimuth and is
what `aggregate.frameOffset` carries.

### 10.4 The photograph is an orthographic COMPOSITE, and here is why

The obvious implementation is to put the cell's cards in a scene and photograph
them through the same two-pass matte hook the per-tree cards use. That was
refused, with numbers:

1. **It cannot bake the worldspace.** The hook photographs one model per process
   and sleeps 1,200 ms a card. 2,631 forested cells × 8 views = 21,048
   photographs, over seven hours of sleep alone, against an object stage that
   measures 4.1 s on a nine-chunk region.
2. **There is nothing left to photograph.** Since 2026-09-10 every card sheet is
   orthographic and metric (§3.7): `half`, `center` and `frameOffset` are world
   lengths that describe the sheet beside them. An orthographic composite of
   orthographic sheets is a RESAMPLE, exact up to the resample; a re-render adds
   a second round of pixel quantisation on top of the first.
3. **It is deterministic.** Two runs of one region give byte-identical sheets,
   which is what `--aggregate` off being byte-identical is measured against, and
   it holds no GL context, no window and no instance slot.

A card whose sidecar does not say `projection ortho` is **refused by name** and
its trees stay per-tree: a foreshortened set's extents describe no picture, so
compositing it would put the tree in the wrong place at the wrong size.

**The resample, exactly.** For each tree and each view, the frame whose own
direction best matches `R^T · eye(v)` is chosen (`R` the DRAWN rotation, tree
yaw folded in), the mirror flips the frame and the sign of its right offset, and
every source texel is splatted into the aggregate frame with the weight
`sampleArea / texelArea`. Trees are composited **back to front** by depth along
the eye axis, straight `over` compositing on every channel.

**The area weight is not a detail.** The first run normalised each tree's layer
by the NUMBER of samples that landed in a texel, so a tree covering a tenth of a
coarse texel composited as if it covered all of it, and the aggregate's
silhouette carried **94 percent more coverage mass** than the same cluster
composited finely — against a measured ceiling of 1.4 percent. Gate A3's own
CEILING arm is what found it.

### 10.5 The channels

The card channel contract of §4, with three statements of its own:

| channel | law |
|---|---|
| colour RGB, A | the trees' own colour and coverage, `over`-composited back to front. The sheet is written under THE COVERAGE CONTRACT of §4 — floor 16, test 128, base 160 — so a consumer's own 0.5 test selects exactly the coverage the composite measured |
| normal R, G | the trees' own view-space normals, `over`-composited. The aggregate's azimuth is within half a frame step of each source frame's own, so the two view spaces are the same basis to within that; it is an approximation and it is named |
| normal B (height) | **the composite's own depth, re-encoded.** Each source texel's world depth is `treeDepth − (B_src − 0.5)·depthSpan_src·scale`, and the aggregate writes `0.5 + behind / depthSpan_agg`, with `depthSpan_agg = 3 × max(boundRadius, 1024)`. Same decoder, same meaning, and it is what the far shadows are cast from (bungo, 08:2x) |
| normal A (sway) | **each texel keeps the sway weight of the tree it came from.** It is NOT re-derived from the aggregate frame's own bottom row: the aggregate's bottom row is the lowest point in the whole cell, so a tree standing on a hill would sway at its trunk. Same rule, applied where the rule means something |
| mask RGBA | the trees' own, `over`-composited |
| emissive | **absent.** `emissiveScale` 0 |

### 10.6 Gates

`scratchpad/cards_agg_20260911/aggpicture.py` is the calibrated picture gate and
`aggfixture_gate.sh` the standalone layout gate. The measure of the picture gate
is the silhouette **MASS** — the integral of coverage over the card, in world
units squared — because it is threshold-free and resolution-free, which a
thresholded mask is not: a canopy at 64 texels is mostly partial coverage, so a
hard alpha test measures the test (`ww-silhouette-compare` §3).

| arm | what it is | what it must read |
|---|---|---|
| known answer | the reference against itself | 0 |
| known answer 2 | the reference box-filtered to the subject's own pitch | ~0, because a box filter preserves mass |
| CEILING | the same, read back on the common grid | the best a frame of that size can do |
| SUBJECT | the shipped tile-64 aggregate | at or near the ceiling |
| FLOOR | a DIFFERENT cell's aggregate, same view, same grid | far outside the tolerance |

The reference is the SAME cluster of the SAME cards composited at
`--aggregate-tile 1024` — 8.2 units a texel against 23 units a texel inside a
tree's own card frame, so it resolves the cards better than the cards resolve
themselves.

