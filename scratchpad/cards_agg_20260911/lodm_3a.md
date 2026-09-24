---

## 3a. `kind: "aggregate"` — one forested CELL's whole tree cluster on one card set

bungo, 2026-09-11 08:2x → 08:3x, verbatim: *"At ring 3 the bake takes each
cell's trees, places their cards with the same rotation and mirror the
repetition breaking would give them, photographs the whole cluster from the
horizon views, and writes one aggregate sheet per cell. The ring 3 instance list
then holds one placement per cell instead of one per tree. At the ring 2 to 3
border the per-tree cards cross-fade into the cell card. Same sheet format, same
sway rule from the height channel."* — and *"1 sounds good"*.

Written beside the sheets as
`Data\Textures\Lodgen\Aggregate\<worldspace>\<cellX>_<cellY>_agg.lodm`; the
three sheets are the same stem with the family's suffixes and `.DDS`. **The path
is DERIVED from the worldspace and the cell and is not stored in the `.lodi`
row**, so a reader can never be handed a path that disagrees with the cell it
came from (zero-authoring, CONSTITUTION 10).

**THREE sheets, not four.** Colour + coverage, normal + height + sway, and the
mask. There is **no emissive sheet**: a forest emits nothing, and
`emissiveScale` is written as **0**, which is exactly how a set says so in one
number (§2.2). A reader binds black. This is the one place an `aggregate`
differs from a `card` in its texture set, and it is stated here rather than left
to a missing file.

`aggregate` object:

| key | type | meaning |
|---|---|---|
| `cell` | int[2] | `[cellX, cellY]`, the exterior cell this set stands for. With the worldspace it is the set's whole identity: the file's own path is derived from these two numbers |
| `views` | int | **azimuths photographed at the HORIZON**, one elevation band. 8 by default. This is NOT `oct`: a card's `oct` is frames per side of a hemi-octahedral grid, and an aggregate has no grid — it has a ring |
| `grid` | int[2] | `[views, 1]`, the sheet's frame layout: the frames sit in ONE ROW, frame `v` at pixels `[v·frameW, (v+1)·frameW) × [0, frameH)`. Stated rather than implied, so a later elevation band is a `grid` change and not a reinterpretation |
| `frame` | int[2] | `[frameW, frameH]` in texels. The long side is the run's aggregate tile, the short side the smallest multiple of 16 whose inner rect is not narrower than the measured silhouette — `docs/LODGEN_CARD_SHEETS.md` §3.2, unchanged |
| `pad`, `gap` | int[2] | the margin on each side of a frame, and twice it — the distance between two neighbouring silhouettes across a frame border. The card law, unchanged |
| `mips` | int | `max(1, log2(min(gap)))`, the card law, unchanged |
| `half` | float[2] | the quad's half extents in **world units**, spanning the WHOLE frame. ONE pair for every view, exactly as a card's is |
| `center` | float[3] | the card's centre in **world** coordinates — the cell's own centre in X and Y, and the mid-height of the cluster in Z. Unlike a card's `center`, which is an offset from a pivot, an aggregate has no pivot: it is a placement in the world |
| `depthSpan` | float | world units the height channel spans: `units = (B − 0.5) × depthSpan`, 0.5 = the card plane. Same law, same decoder |
| `boundRadius` | float | the tree cloud's radius about `center`, for the projected-size test that selects the aggregate |
| `trees` | int | **how many tree placements were photographed into this sheet.** It must equal the `coveredCount` of this cell's row in the `.lodi` — that equality is the count-identity gate, and it is stated in two files on purpose so one can check the other |
| `frameOffset` | float[2·views] | where each view's quad sits relative to `center`, along that view's own right and up axes, in world units: view `v` at `[2v]` and `[2v+1]`. The card law of `docs/LODGEN_CARD_SHEETS.md` §3.6, applied per azimuth |
| `identity` | string | **`"per-aggregate"`, always.** The far-shadow pass keys on the identity index (bungo 08:4x), and once a cell's trees are one card they are ONE caster: the aggregate carries its own index, `0x80000000 \| aggregateIndex` in the `.lodi` row, and NOT the dominant tree's. The sheet therefore carries no identity channel, and this word says that is deliberate rather than missing |
| `projection` | string | `"ortho"`. An aggregate is composited from orthographic card sheets by an orthographic resample, so its `half`, `center` and `frameOffset` are metric. A value other than `ortho` is not written by any generator |
| `coverage` | object | `{floor, test, base}`, the colour sheet's coverage contract, identical in meaning to a card's (`docs/LODGEN_CARD_SHEETS.md` §4). This bake writes `16 / 128 / 160` |

### 3a.1 What a reader does with one

```
quadCentre = center + frameOffset[2v] * right(v) + frameOffset[2v+1] * up(v)
```

spanning ±`half`, with `right(v) = (−sin φ, cos φ, 0)`, `up(v) = (0, 0, 1)` and
`φ = 2π·v/views` — the azimuth of the view the frame was photographed from, with
the eye direction `(cos φ, sin φ, 0)`. Between two azimuths a reader blends the
two neighbouring frames; the frames are a RING, so view `views−1` blends back
into view 0.

**When to draw it instead of the trees.** The `.lodi` header carries
`aggSwitchPx` and `aggBandRatio`
(`docs/LODGEN_NATIVE_LODO_LODI.md` §4.6): the aggregate takes over when the
CELL's projected width falls to `aggSwitchPx`, and the per-tree cards cross-fade
out over `aggSwitchPx … aggBandRatio × aggSwitchPx`. Which instances to stop
drawing is not a guess either — the `.lodi` row names them, one u32 each.

### 3a.2 The three things an aggregate does NOT inherit from a card

1. **`oct` does not apply.** An aggregate has `views` and `grid`, and a reader
   that looks for `oct` on an aggregate finds nothing. The hemi-octahedral
   mapping is a hemisphere's worth of directions; an aggregate is photographed
   at the horizon only, because that is where ring 3 is.
2. **`center` is WORLD, not an offset from a pivot.** A card stands in for a
   placed object and is drawn at `pivot + center`; an aggregate stands in for a
   CELL and is drawn at `center`.
3. **There is no emissive sheet and no `textures.emissive` key.** See above.

### 3a.3 What the sway channel carries, and why it is not re-derived

bungo asked for *"the same sway rule from the height channel"*. The rule
(`h²·(0.35 + 0.65·r)`, `h` up from the view's own bottom row) is measured
**per tree, on the tree's own card**, and carried through the composite
unchanged — it is not recomputed from the aggregate frame's bottom row. Deriving
it from the aggregate would make a tree standing on a hill sway at its trunk,
because the aggregate's bottom row is the lowest point in the whole cell and not
the base of that tree. Each texel keeps the sway weight of the tree it came
from, which is the same rule applied where the rule means something.

