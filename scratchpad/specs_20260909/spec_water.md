# `.lodl` v3 — water bodies, flow and the NifSkope marking tool

> **STATUS, 2026-09-10. Half of this page is BUILT and half is still design,
> and every paragraph now says which.** Lane WATER1 (read-only) wrote the page;
> lane WATER2 built the format, the writer, the reader, the three planes and
> the CLI, and MEASURED several of WATER1's numbers to be wrong; lane WATER3 is
> building the marking tool of §5 and edited this page.
>
> * **BUILT** (WATER2, `src/lodtfile.{h,cpp}`, `src/nifcli.cpp`,
>   `src/btdterrain.{h,cpp}`, gate `tests/spells/lodl_water.sh` 56/0):
>   §2's body rule, §3's whole file layout, §3.7's stroke store (written
>   present and EMPTY), §4.1 and §4.2, and the viewer's `bodyid` / `flow` /
>   `shore` planes.
> * **BUILT** (WATER3, `src/watermark.{h,cpp}`, `src/watermarkpanel.{h,cpp}`):
>   §4.3's propagation, §5's tool and panel, §5.4's harness.
> * **DESIGN, with no code**: §6, the FO4CS consumer's checklist — nothing has
>   read a version-3 file outside this tree.
>
> **CORRECTED BY MEASUREMENT.** WATER1's census is superseded where the two
> disagree: **346 bodies, not 590** (§2), the record-stride refusal is the
> other way round (§3.8), and §7's G5/G6 numbers are restated. Each correction
> names the script that measured it. Numbers in §1 that WATER1's own read-only
> pass produced are kept where nothing later re-measured them and are marked
> where something did.

bungo's words, 2026-09-09 ~20:4x, which this serves:

* *"different water colors for different bodies of water"* — *"or at least an
  ID for them"*
* flow *"calculated"*: *"lakes have no flow if they're not connected to rivers,
  then rivers end up at sea"*
* *"in nifskope, have the player mark the water direction in a smart way"*

---

## 1. What the Commonwealth actually contains (measured, 2026-09-09)

Full tables and scripts: `scratchpad/lane_water1_report.md` and
`scratchpad/water_20260909/`. The six facts the design turns on:

1. **Water is per-CELL and almost entirely inherited.** 36,864 of 36,864 cells
   carry `Has Water`; **98.62%** inherit the default water TYPE and **98.52%**
   the default HEIGHT (450.0, `ExtOceanWater` `00000018`). 15 WATR forms are
   interned in the file; 16 are in play.
2. **16 forms serve the whole worldspace's water.** `ExtLakeWater` alone paints
   15 separate lakes with one colour and one velocity, `ExtOceanWater` 176.
   There is no per-body anything in vanilla — that is the whole gap.
   *(CORRECTED: WATER1 said 590 bodies, 16 `ExtLakeWater` lakes and 405
   `ExtOceanWater` bodies. The shipped classifier gives **346 bodies**, 15 and
   176; see §2. The per-form TEXEL totals did not move — that is the invariant
   no grouping rule can touch.)*
3. **FO4's water is FLAT.** WATER1 measured 589 of its 590 candidate bodies
   carrying exactly ONE water height; under the shipped rule D a body carries
   one height BY CONSTRUCTION (the component key is (height, type)), so the
   fact this states is now a property of the classifier and the measurement
   that matters is the one behind it: a river that steps N times is N bodies.
   *There is no height gradient inside a river to read a direction from.*
4. **Plateaus do not touch.** Only 6 of 804 type-keyed bodies had any
   neighbouring body at a different height: a "step" is a gap of dry land, so
   the drainage relation is **proximity**, not adjacency.
5. **River beds are flat too.** Only 2 of 590 bodies have a bed whose slope
   along their own axis reaches |r| ≥ 0.7 with a ≥ 64-unit drop.
6. **So flow cannot be computed for most water.** MEASURED ON THE SHIPPED FILE
   (`scratchpad/water3_20260910/audit_spec.py`, through the independent decoder,
   never the writer): of the **89** bodies at or above 64 texels — 1 sea, 42
   rivers, 46 lakes — **29** are served by `drain`, **1** by `bed`, **12** by
   `none` (the sea and round lakes with no outlet, which is bungo's own rule and
   not a gap), and **47 are served only by the form's `NAM0`** — the vanilla
   fallback floor, one velocity shared by every body of that form. Those 47 are
   what a human stroke is for. *(WATER1 said 93 bodies, 26 / 1 / 12 / 54.)*

Vanilla's one existing flow signal is `WATR NAM0` *Linear Velocity*, a 3-float
vector per FORM (ExtOceanWater (−0.46, −0.12, 0), ExtRiverCharlesUpper (0.10,
0.42, 0), ExtLakeWater (0.47, 0.17, 0) …). It is the FALLBACK FLOOR of this
design and never the answer.

---

## 2. The body rule (rule D)

Stated once, here, because the writer, the panel and the harness must all run
the same one.

```
wet texel   terrain height (level-0 sample) < the cell's RESOLVED water height,
            in a cell whose flags carry CELL_HAS_WATER.
component   4-connected wet texels with equal water HEIGHT (quantised to 1/8
            world unit) AND equal water TYPE index.
merge       a component whose type is WATER_TYPE_DEFAULT (0xFFFF) is merged into
            the same-height PAINTED component it touches; with more than one
            candidate, into the largest, and the body is flagged AMBIGUOUS.
            THE INHERITING SIDE IS ONLY ABSORBED WHEN IT IS THE SMALLER OF THE
            TWO (the SIZE guard, §2 note below).
bridge      two components at the same height whose shores are within 2 texels
            (256 world units) are merged when their types are equal or one
            inherits, under the SAME size guard.  REFUSED when both are painted
            with different types.  The shore test is EXACT -- a disc scan around
            every texel, never a thinned point cloud.
class       sea    the body reaches the worldspace edge
            river  elongation >= 6, or a lower body within 64 texels
            lake   everything else
```

**CORRECTED, and this is the page's biggest change.** WATER1 measured
804 → 781 → 792 → **590**, 215 bridge merges accepted and 3 refused. Two defects
were found by lane WATER2 and both are measured, not argued
(`scratchpad/water2_20260909/bridge_exact.py`, `bridge_effect.py`,
`bridge_variants.py`, `merge_guard_variants.py`):

* **the shore test was DECIMATED.** WATER1's script compared point clouds
  thinned to at most 4,000 points a body — 4,000 of 21,585,117 for the sea — and
  a thinned point set can only make a minimum distance LARGER, so the error is
  ONE-SIDED and every disagreement is a merge the stated rule requires. Exact:
  **545** pairs within two texels against the decimated test's 218.
* **the merge had no DIRECTION.** With the exact test the biggest body came out
  as 21,587,443 texels of `ExtMarshScumWater` — the Commonwealth's ocean
  carrying a marsh's name — because "equal or one inherits" was read in both
  directions. The SIZE guard above is the fix, and it was chosen on a number:
  it is the only single clause under which "the painted type with the most area"
  and "the majority type counting inherited area" name the SAME form for every
  one of the 346 bodies.

Measured on the Commonwealth, as SHIPPED: 805 components → 793 (rule C) →
**346 (rule D)**, **528** bridge merges accepted, **13 refused**; class sea 1,
river 115, lake 230. Rule D is still the first rule under which the Charles is
ONE body — **body id 3, 25,114 texels, cells −16..−6 × −21..−4** — and under
every other rule it is two or more. Two texels more than WATER1's 25,112, which
is the exact shore test picking up what the thinned one missed.

**The bridge distance is a parameter, not a constant** (`--water-bridge N`,
default 2): at 1 texel the Charles stays split, at 8 texels 41 merges fire and
distinct marshes start to fuse. Every worldspace re-measures it.

**Bodies below 4 texels are noise** — **115 of the 346** are single-sample dips
below the sea plane at the 128-unit sample rate (WATER1 said 288 of 590). They get IDs (an ID plane
cannot have holes) but the writer marks them `TINY` so a consumer, and the
panel's list, can drop them.

---

## 3. The file: `.lodl` version 3

### 3.1 The discipline

Same as version 2's, and for the same reason: **no existing offset moves.**
`0x00..0x9F` is byte-for-byte what version 2 writes, every version-2 section
keeps its offset, and version 3 appends header fields from `0xA0` and its
sections after the block data. A version-2 file upgraded to version 3 differs
only by its version word, a longer header, and bytes appended at the end.

Header size becomes **`0xF8` = 248 bytes** (`LODL_HEADER_V3`).

### 3.2 The new header fields

| offset | type | field |
|---|---|---|
| 0xA0 | uint64 | offset: **body table** |
| 0xA8 | uint32 | body count |
| 0xAC | uint32 | body record bytes (48 for this revision) |
| 0xB0 | uint64 | offset: **body name blob** (UTF-8, NUL-terminated, 0 = none) |
| 0xB8 | uint32 | body name blob bytes |
| 0xBC | uint32 | **body-ID plane** samples per cell edge (0 = no plane) |
| 0xC0 | uint64 | offset: body-ID plane store |
| 0xC8 | uint32 | **flow plane** samples per cell edge (0 = no plane) |
| 0xCC | uint32 | flow encoding (0 = dir8/speed4/conf4, see §3.5) |
| 0xD0 | uint64 | offset: flow plane store |
| 0xD8 | uint32 | **shore-distance plane** samples per cell edge (0 = no plane) |
| 0xDC | uint32 | shore quantum, world units per stored step (32) |
| 0xE0 | uint64 | offset: shore plane store |
| 0xE8 | uint64 | offset: **stroke store** (0 = none) |
| 0xF0 | uint32 | stroke store bytes |
| 0xF4 | uint32 | reserved, written 0 |

The last field ends at `0xF8`, which is the header size declared above — and
the writer refuses if its own assembled header is not the size its version
declares, exactly as version 2 already does.

New `section-present` bits in the existing `0x44` word, beside
`SECT_COLOUR/GROUNDCOVER/AO/WATER`:

```
SECT_BODIES = 1u << 4    body table + body-ID plane
SECT_FLOW   = 1u << 5    flow plane
SECT_SHORE  = 1u << 6    shore-distance plane
SECT_STROKE = 1u << 7    stroke store
```

**A reader checks the bit, not the offset** — the rule the existing document
already states, kept.

### 3.3 The body table

`bodyCount` records of `bodyRecordBytes`, at `bodyTableOffset`, in ID order;
**record `i` is body ID `i + 1`.** ID 0 in the plane means "no body here".

| off | type | field |
|---|---|---|
| 0x00 | uint16 | id (redundant, and checked: `id == index + 1` or refuse) |
| 0x02 | uint8 | class: 0 sea, 1 river, 2 lake |
| 0x03 | uint8 | flags: bit0 user-edited, bit1 flow from a stroke, bit2 colour override present, bit3 merge was ambiguous, bit4 TINY (< 4 texels), bit5 class was set by hand |
| 0x04 | float | water height, world units (the body's one plane) |
| 0x08 | uint32 | WATR form id, RESOLVED — never 0, never 0xFFFF |
| 0x0C | uint32 | area, texels of the body-ID plane |
| 0x10 | int16 ×4 | cell bbox: x0, y0, x1, y1 (inclusive, the file's own cell space) |
| 0x18 | uint16 | source body (flows FROM), 0 = none |
| 0x1A | uint16 | outlet body (flows INTO), 0 = none |
| 0x1C | float ×2 | mean flow, world units per second, X then Y |
| 0x24 | uint8 ×4 | colour override R, G, B, A — **A = 0 means no override** |
| 0x28 | uint8 | flow confidence 0..255 |
| 0x29 | uint8 | flow source: 0 none, 1 the form's NAM0, 2 bed slope, 3 drain, 4 user stroke |
| 0x2A | uint16 | reserved, written 0 |
| 0x2C | uint32 | name offset into the name blob, 0 = unnamed |

48 bytes; **346 bodies = 16,608 bytes** for the Commonwealth, at file offset
`0x2249AE6`.

**Why `bodyRecordBytes` is a field.** A reader whose record is SHORTER than the
file's strides by the file's value and reads the prefix it knows; a reader whose
record is LONGER refuses by name. That is the forward-compatibility rule the
`.lodm` sidecars already use, and it is why a colour or a name can be added later
without a version bump.

### 3.4 The plane store (one container, three planes)

The body-ID, flow and shore planes share ONE container, so there is one
implementation and one gate:

```
uint32 tilesX, tilesY          tiles, one per CELL (tilesX = cellsX)
uint32 tileEdge                samples per tile edge = this plane's samplesPerCell
uint32 bytesPerSample          2 (body id), 2 (flow), 1 (shore)
uint64 directoryOffset         absolute
uint64 dataOffset              absolute
-> directory: tilesX*tilesY * { uint64 offset, uint32 csize, uint32 usize }
-> data:      zlib streams, row 0 SOUTH inside a tile, tiles row 0 SOUTH
```

A tile whose `csize` is 0 is **uniform**, and its `usize` field holds the single
sample value repeated across the tile — the sea's 21.6 M texels then cost 16
bytes a cell instead of an inflate. That is not an optimisation for its own
sake: 99.2% of the Commonwealth's wet area is one body.

Sizes, from the measured Commonwealth: body-ID at 32/cell is 75.5 MB raw, and
20,340 of 36,864 cells are entirely one body, so most tiles are uniform. The
writer REPORTS the compressed size; this spec does not predict it.

**Sample rates are header fields, never constants.** Body ID defaults to the
file's own `samplesPerCell` (32) because the narrowest measured river reach is
1 texel wide at 128 units and any coarser rate loses it. Flow defaults to 32 for
the same reason; 8 is offered and its cost in lost coverage is a number the
writer must print, not a guess.

### 3.5 The flow plane

One uint16 a sample:

```
bits 0..7    direction, 0..255 = 0..2pi measured from +X toward +Y in the
             file's own row-0-SOUTH space (1.41 degrees a step)
bits 8..11   speed, 0..15, times the body's `speedQuantum` = mean flow
             magnitude / 8, so 8 is the body's mean and 15 is ~1.9x it
bits 12..15  confidence, 0..15: 15 = a stroke crosses this sample, 0 = the
             body's fallback (its form's NAM0)
```

Dry samples write 0, which is also "no flow", so the two are the same value on
purpose: a consumer that forgets to test the body-ID plane draws still water,
never garbage.

### 3.6 The shore-distance plane

uint8, `shoreQuantum` (32) world units a step, saturating at 255 = 8,160 units,
measured from a wet sample to the nearest dry one **inside the same body's
plane**. Baked rather than derived because the runtime alternative is a search,
and because the `.lodl` document's own reason for dropping the old `_data` shore
channel ("water is *in this file*, so it is a runtime subtraction") applies to
DEPTH and not to distance.

**Depth stays derived**, exactly as that document says: `body.waterHeight −
height(gx, gy)`. Both sides are in the file; nothing is baked for it.

### 3.7 The stroke store — the SOURCE, not a cache

```
uint32 count
count * {
  uint32 recordBytes        including this field
  uint16 body               the body the stroke was drawn on, 0 = resolve by
                            position at bake time
  uint8  kind               0 stroke, 1 pin, 2 barrier, 3 merge
  uint8  flags              bit0 sets speed, bit1 sets direction,
                            bit2 pins the body id, bit3 disabled
  float  speed              world units per second, when flags bit0
  float  width              world units, the stroke's influence radius
  uint16 pointCount
  uint16 reserved
  pointCount * { float worldX, float worldY }
}
```

**Points are WORLD coordinates**, not texels, so a stroke survives a re-bake at
a different sample rate, a different bridge distance, or a heightmap change.
The planes are DERIVED from the `.lodl` plus this store; the store is what a
user's work lives in and the only part of the file a panel writes.

### 3.7b Dye (lane WATER4, **BUILT AND RUN 2026-09-10 by lane BUILD10** (`release/NifSkope.exe` 15:52:46; 47 checks / 2 failures on the Charles, and both failures are the two gates the lane pre-registered as expected red))

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

**Refusal 1 — an older reader handed a version 3 file.** It already exists and
must keep working: `src/lodtfile.cpp` refuses with *"unsupported version %1
(this reader knows %2..%3)"* against `LODL_VERSION_MIN`/`LODL_VERSION`. Two
things must be done when `LODL_VERSION` becomes 3:

* the header-size ternary that runs BEFORE the version check
  (`const qsizetype hdrBytes = ver >= 2 ? LODL_HEADER_V2 : LODL_HEADER_V1;`)
  becomes a table, or a version-3 file is measured against a 160-byte floor by
  the very reader that is about to refuse it — harmless today, a misparse the
  day someone relaxes the refusal;
* **FO4CS pins `kVersion = 1u`** (`FarFieldLodtFormat.h`, lane LODT1) and
  already refuses version 2. Version 3 does not make that worse, and the
  zero-effort fallback is unchanged: `WW_LODL_VERSION=1` or `=2`.

**Refusal 2 — a version 3 reader handed a file it cannot serve.** Each is a
NAMED refusal, never a silent zero:

* a section bit set with a zero offset, a zero sample rate, or a directory that
  does not fit the file → *"section <name> is declared present but its
  <offset/rate/directory> is empty"*;
* `bodyRecordBytes` **SHORTER** than this reader's record → *"the body table's
  records are N bytes; this reader knows M"*. **CORRECTED: this page had the
  refusal the wrong way round**, which would have made the stride field useless.
  §3.3 states the rule correctly and the reader implements §3.3: a LONGER record
  is the forward-compatible case (stride past the fields you do not know), a
  SHORTER one is missing fields the reader needs and is refused by name;
* a body-ID plane sample naming an ID past `bodyCount` → refuse, do not clamp;
* `id != index + 1` in the body table → refuse.

**The module and its fallback** (CONSTITUTION rule 10). Water bodies are a
module with its own switch (`--water-bodies` / panel tick); with it off the
writer emits version 2 and the file is byte-identical to today's. With it on but
a plane refused, the consumer's floor is the vanilla path it has today: the
per-cell water height and type, and the form's own `NAM0` for flow. The output
NAMES the serving arm — the body record's `flow source` byte is exactly that.

---

## 4. The algorithms

### 4.1 Classification (writer)

1. Build the wet mask from the level-0 height plane and the per-cell table.
2. Label components on (height, type) — a run-length labeller; there is no
   scipy in this tree and `scratchpad/water_20260909/ccl.py` is the reference
   implementation, with its own five known-answer controls.
3. Merge inheriting components into painted ones (§2), flag ambiguity.
4. Bridge same-height components within `--water-bridge` texels, refusing
   painted-vs-painted.
5. Apply the stroke store's `barrier` (split) and `merge` strokes — **user
   strokes run AFTER the automatic rule and always win.**
6. Class per §2; assign IDs by descending area, so ID 1 is the sea and the
   list a user sees is sorted where it matters. **BUILT as stated**: id 1 is the
   21,575,619-texel sea, id 2 the largest marsh, id 3 the Charles.

### 4.2 Flow (writer)

Per body, in this order; the first that answers sets `flow source`:

1. **stroke (4)** — the body carries stroke or pin constraints.
2. **drain (3)** — a body with a LOWER body within 64 texels whose contact
   point lies within 45° of the body's principal axis (measured: 22 of 27
   candidates pass). Direction = along the axis, toward the contact.
3. **bed (2)** — the terrain under the water falls monotonically along the axis:
   |r| ≥ 0.7 and ≥ 64 units of drop (measured: 2 bodies).
4. **form NAM0 (1)** — the WATR form's Linear Velocity, the vanilla floor.
5. **none (0)** — a lake with no outlet: zero flow, which is bungo's own rule
   (*"lakes have no flow if they're not connected to rivers"*).

`mean flow` in the body record is that direction times the speed; a sea is
always 0 unless a stroke says otherwise.

### 4.3 Propagation inside a body

> **REBUILT by lane WATER4 (2026-09-10), **BUILT AND RUN 2026-09-10 by lane BUILD10** (`release/NifSkope.exe` 15:52:46; 47 checks / 2 failures on the Charles, and both failures are the two gates the lane pre-registered as expected red).** The harmonic
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
stroke samples are Dirichlet conditions (direction as a unit vector, so
averaging is done on the VECTOR and never on the angle), every other wet sample
of the body is solved to the average of its 4-connected neighbours inside the
same body, iterated to a fixed tolerance, then normalised. Confidence is a
second harmonic fill with 15 at the constraints and 0 nowhere — it decays with
distance, so a consumer can fade to the body's mean where nobody marked.

Two properties this buys, both testable:

* it never leaves the body — the mask is the domain, which is what makes the
  panel's isolation harness (§5.4) a real gate;
* with no constraints it is constant, equal to the body's mean flow, so the
  automatic case costs no solve at all.

---

## 5. NifSkope: marking the direction

### 5.1 Where it lives

The `.lodl` viewer already meshes a scene from the file and its plane picker
now offers `bodyid`, `flow` and `shore` beside `waterheight` and `watertype`
(lane WATER2, `src/btdterrain.cpp`).

**DIVERGENCE, AS BUILT.** This page said *"the water plane itself becomes the
canvas"* — the tool drawing on the meshed 3-D water surface. What shipped draws
on a **TOP-DOWN MAP inside the dock**, and the 3-D viewer only SHOWS the result
by reading the planes back. Two reasons, the first of them the honest one:

* the 3-D viewport is `src/glview.cpp`, which another lane held open while this
  was written, and lane WATER3's brief forbade touching it;
* it is the better canvas anyway. A river reach is eleven cells long, its
  direction is a fact about the MAP, and orbiting a terrain mesh to draw a line
  down it is precisely the gesture Blender itself replaces with a 2-D editor
  (the UV and Image editors) whenever the thing being edited is flat.

Marking in the 3-D viewport is therefore still OPEN, and it is a `glview.cpp`
lane, not a redesign: the model (`src/watermark.{h,cpp}`) takes world
coordinates and knows nothing about either canvas.

Body ID renders as a categorical colour (the ID hashed to a hue, so that two
touching bodies cannot come out the same colour), which is also the picture that
answers bungo's *"or at least an ID for them"* on sight.

### 5.2 The interaction — Blender's grease pencil is the reference

CONSTITUTION rule 10: uncertain about a design, follow Blender's equivalent and
state the divergence.

| Blender GP | here | built? | divergence |
|---|---|---|---|
| Draw: click-drag lays a stroke | **Stroke** (kind 0): a polyline on the map; its tangent is the flow direction | yes | drawn on the top-down map, not the 3-D surface (§5.1) |
| a stroke's pressure sets thickness | the **Width** row sets the influence radius in world units | yes | no tablet pressure: the panel's field is the only source, so the value is reproducible from the file alone |
| Eraser | **Erase**: click removes the stroke under the cursor | yes | Blender's eraser has a radius and rubs part of a stroke away; a stroke here is ONE constraint and half a constraint is not a smaller one |
| Line/Arc tools | **Pin** (kind 1): click, drag out an arrow = one point + one direction | yes | Blender has no single-sample constraint; this is ours |
| — | **Source pin** (kind 4) and **Outlet pin** (kind 5): the head and the mouth; the PAIR is the path between them | yes | ours, and it is bungo's *"a source pin + outlet pin = a path"*. The straight segment is the constraint; the harmonic fill bends it to the banks |
| — | **Still water** (kind 6): this body has no flow | yes | ours, and it is bungo's *"lakes have no flow if they're not connected to rivers"*. It is a MARK in the store, not a bit in the table, so it survives a re-derivation |
| Cutter (knife) | **Barrier** (kind 2): a stroke across a body splits it in two | **no** | the split needs the classifier re-run, which needs the plugin and the heightfield, not just the `.lodl`. The kind is reserved and the store carries it; nothing acts on it yet |
| Join | **Merge** (kind 3): a stroke between two bodies joins them | **no** | same reason |
| strokes live in a GP object | strokes live in the `.lodl`'s stroke store, in world coordinates | yes | the file is the document |
| pan and zoom | middle-drag pans, the wheel zooms about the cursor | yes | none — and the wheel over a NUMBER field still does nothing unless the field has focus, which is also Blender's rule |

A stroke is drawn on ONE body: the body under the first point. Points that
leave that body are kept (the store is what the user drew) and IGNORED by the
solve (the mask is the domain). The panel says so in words when it happens —
*"3 of 41 points fell outside body 3 and will be ignored by the solve; the
stroke is stored as drawn"* — rather than silently trimming. A stroke whose
FIRST point is dry is refused outright, in words, and stored nowhere: a
constraint that names no body constrains nothing, and storing it would leave the
next reader to work out why the planes did not move.

### 5.3 The panel (`nifskope-ww-panel-style`, and its self-test counts)

**AS BUILT it is its OWN dock, not a section of the LOD Generation panel**
(`src/watermarkpanel.cpp`, in the Workspaces dropdown beside the other manager
docks, and hiding them the way they hide each other). The reason is the file
rule again — the LOD panel is `src/lodgenmanager.cpp`, another lane's file —
and, as with the canvas, it is the better shape: this dock stays open while its
own map is being drawn on, and the LOD panel's job is a bake that runs for
minutes.

`wwHeading` for every section (never a `QGroupBox` title), one
`label | field` `QGridLayout` a section, one setting a row, one `labelW` for the
page, whole-word labels with the explanation in the tooltip:

| section | row | control | notes |
|---|---|---|---|
| Landscape file | File | `QLineEdit` + Browse | the version-3 `.lodl` being marked |
| | Show | `QComboBox` + `wwMatchFieldStyle` | Body ID / Flow / Shore distance — which plane the map paints |
| Marking | Tool | `QComboBox` + `wwMatchFieldStyle` | Stroke / Pin / Source pin / Outlet pin / Erase |
| | Speed | `QDoubleSpinBox` + `wwMakeScrubField` | world units per second |
| | Width | `QDoubleSpinBox` + `wwMakeScrubField` | world units, the influence radius |
| Selected body | Class | `QComboBox` + `wwMatchFieldStyle` | Automatic / Sea / River / Lake — sets flags bit5 |
| | Water form | `QComboBox` + `wwMatchFieldStyle` | the forms the FILE interned, plus the worldspace default — zero-authoring: no list of water types is written down anywhere in this tree |
| | Colour | tick + colour button | writes the body record's RGBA; unticking clears it (A = 0) |
| | Flow | tick, "Still water" | bungo's lake rule, stored as a `ZeroFlow` mark |
| | Name | `QLineEdit` | the name blob |

and a folding **Bake** section (arrow folds, the fold persists in `QSettings`):

| row | control |
|---|---|
| Flow samples per cell | `QComboBox` 8 / 16 / 32 |

**Dropped from this page's list, with the reason.** The Body IDs / Flow / Shore
distance ticks and the Bridge gap belong to the WRITER, not to the marking tool:
this dock edits an EXISTING file's strokes and re-derives its flow plane, while
which planes exist at all and how components bridge are decided when the file is
generated (`--water-bodies`, `--water-bridge`). Rows that could not do what they
said would be worse than no rows.

Pinned outside the scroll area: the **summary**, carrying the sentence Save will
act on — *"Write 1 stroke(s) into Commonwealth.lodl (346 bodies, flow at 32
samples a cell)"* — the selected body's own line under it (*"body 3 - river -
25,114 texels - plane 1300.0 - water form 001c4995 - flow from a stroke"*), and
the last solve's numbers; or the ONE reason Save cannot run, e.g. *"No landscape
file is open"*, in `danger`. `refreshSummary()` owns the buttons' enabled state,
because a greyed button with no sentence beside it is a broken button. Then the
action bar: Reload, Solve, Save.

Every colour through `wwSkinColor`; muted hints `textMuted`; a refusal `danger`;
the wheel guarded (`wwGuardWheel`, applied by both helpers) so scrolling the
panel never changes a value.

### 5.4 The harness — `WW_WATER_MARK_TEST` (`tests/spells/water_mark.sh`)

Renamed from this page's `WW_WATER_TEST`, which reads as the writer's own
control (`--water-selftest`); this one tests the TOOL. Two halves, because both
can fail on their own: the MODEL, headless
(`lodl <copy.lodl> --water-mark-selftest`), and the DOCK (`WW_WATER_MARK_TEST=1`,
with `WW_WATER_MARK_SHOT=<png>` for the grab).

The gate that matters is **a stroke changes exactly the body under it**, and it
is written with the floor on the other side (CONSTITUTION rule 4):

1. Open the real Commonwealth `.lodl`, solve with no strokes, keep the flow
   plane as the baseline.
2. Add one stroke down the middle of body **3** (`ExtRiverCharlesUpper`,
   25,114 texels, cells −16..−6 × −21..−4). Re-solve.
   *(CORRECTED: this page said body 233, WATER1's id under the 590-body rule.
   Ids are assigned by descending area and the rule changed, so the Charles is
   now id 3. Audited before the harness was written —
   `scratchpad/water3_20260910/audit_spec.py`.)*
3. **Assert**: the number of texels whose flow word changed and whose body ID
   is NOT 3 is **0**, of the **21,754,958** texels that name a body.
4. **The floor**: at least 60% of body 3's own texels changed, and its
   `flow source` byte moved 3 → 4. A test that only checks "nothing else moved"
   passes on a solver that does nothing.
5. **The refuter, run first**: place the same stroke on body **2**
   (`ExtMarshDarkWater`, 29,312 texels — WATER1's body 136, whose 29,305 grew by
   the same exact-shore-test texels) and watch step 3 go RED for 3 — i.e. show
   the test failing before trusting it pass.
6. Undo: after removing the stroke and re-solving, the flow plane is
   byte-identical to step 1's. **As built the gate is stronger and cheaper: the
   whole FILE is byte-identical**, because the re-derivation is a pure function
   of the body-ID plane and the body table, and the two planes marking cannot
   change (body ID and shore) are copied through verbatim with their absolute
   offsets rebased.
7. **Added, and everything above rests on it: the two IDENTITY gates.** The
   marking tool re-implements the writer's body-table encoder and its plane
   packer (the file rule again — `src/lodtfile.cpp` was another lane's). A twin
   is only safe while something proves the two agree, so the harness re-encodes
   the table and re-derives the WHOLE flow plane on an unmarked file and
   compares both against the bytes the writer put there. If either drifts it
   goes red before any behaviour is believed.
8. **Added: one stroke sends the river to the sea**, which is bungo's own
   sentence as a measurement. The mouth is found IN THE FILE — the river texel
   nearest a texel of the body it drains into — the stroke is oriented to end
   there, and the gate is that the flow plane's MEAN direction over the whole
   body has a positive component toward the mouth. The before and after angles
   are both printed.

Panel-style counts, in the DOCK half, with the floors as built: plain spin
boxes without the `wwScrubbed` stamp 0 (of ≥ 2), `QGroupBox` 0 against
weight-600 headings ≥ 4, selectors without the matched drop-down rule 0 (of
≥ 5), check boxes carrying " - " 0 and without a tooltip 0 (of ≥ 2), six
settings on six distinct rows (measured on the laid-out panel's GEOMETRY, not on
its layout class), Save and the map and the summary outside the scroll area with
the settings inside it, the Bake fold opening and closing, the refusal sentence
with no file open against the Save sentence with one, and `SHOT=<png>` grabbed
in the state the test leaves — a river selected and marked.

### 5.5 The water window (lane WATER5, BUILT 2026-09-10)

bungo: *"just make it open a new popup window that can be set to full screen
and you can drag that shows the flowmap"*, *"Add all the tools needed to mark
the rivers and solve it and export import there, into that new window"*,
*"allow me to save the curves as some type of a file"*, and on the dock's map,
*"do you draw it on that tiny map?"*

`src/waterwindow.{h,cpp}` is a top-level window (not a dock) opened from the
dock's **Water window** button and from Workspaces; the dock's own canvas is
HIDDEN and stays only as the model's hands for `water_mark.sh`. The map draws
the whole worldspace at first open and zooms to texel level (measured: 0.121
px a texel fitted, 2.00 px a texel at the river's mouth). Rows, one to a row,
in three bands with the map, the summary and the action bar outside the
scrolling settings: Landscape file (File, Show), Curves (Tool, Speed, Width,
Point weight, Reverse / Finish / Delete), Selected body (Class, Water form,
Colour, Still water, Dye at mouth, Name), Dye (Dye colour, Dye fade), and a
folding Files section (Save curves / Load curves, Export PNG / Import PNG,
Flow samples per cell). **Solve** calls `WaterMarkDoc::solve()` as WATER4
wrote it, after the curves are mirrored into the stroke store.

**Two homes, one source.** The curves are saved as `<Worldspace>.water.json`
beside the land file and MIRRORED into the `.lodl` stroke store when the land
file is saved. The json is versioned, in WORLD units, and carries no plane, so
loading it onto a regenerated land file and pressing Solve re-derives the same
flow words (gate W4: hash equal, 29,312 of 29,312 texels moved).

```
{ "format": "ww-water-curves", "version": 1,
  "worldspace": "Commonwealth", "landFile": "Commonwealth.lodl",
  "cells": [minX, minY, maxX, maxY], "bodySamples": 32, "units": "world",
  "dye": { "halfDistance": 8192 },
  "curves": [ { "kind": "curve"|"pin"|"sourcePin"|"outletPin"|"dyePin",
                "body": 2, "enabled": true, "speed": 0.5, "width": 4096,
                "colour": [r, g, b, a],            // dyePin only
                "points": [[x, y, weight], ...] } ],
  "bodies": [ { "id": 2, "name": "...", "class": "auto"|"sea"|"river"|"lake",
                "form": null, "colour": [r, g, b], "still": false,
                "dyeMouth": false, "dyeStrength": 1 } ],
  "rasters": [ ... ] }
```

**The flow map as a PNG.** Export writes the flow plane at the file's own
body-plane grid plus a 16-bit body mask; a wet texel's alpha is floored at 1
so water can be told from land by alpha alone, and the round trip is exact
(gate W5: 0 of 21,754,958 painted texels differ). Import stores the image as a
RASTER SOURCE LAYER (a kind-10 stroke record). The green channel's meaning is
lane WATER6's section below; the refusal that catches a map written the other
way round is gate W6 and it fires with both agreement numbers in the sentence.

**What was RED on the first build** (lane BUILD10, both reported and not
cured): a dye pin's per-point weight is not written by `writeTo`, and the
FIRST named body in a file this document writes reads back nameless because
`encodeTable` gives it name offset 0 while the reader spells 0 "no name".

---

## 6. The FO4CS reader's checklist

What a consumer does per water draw, in order, with the fallback named at each
step:

1. `sectionFlags() & SECT_BODIES`? If not: **fall back** to the version-2 path —
   per-cell water height and type, one tint per form. Nothing else in this list
   runs.
2. Sample the **body-ID plane** at the fragment's world position (nearest, never
   filtered — IDs are not interpolable). ID 0 = no water here; do not draw.
3. Look the body up in the **body table**. Its `water height` is the plane's
   height; the per-cell height need not be read at all.
4. **Tint**: if `colour override A != 0`, use it. Otherwise resolve the body's
   `WATR form` through the engine's own loaded form (shallow/deep/reflection
   colours, fog and the noise layers all live in `WATR DNAM`, whose field order
   is in the xEdit FO4 definitions and whose 201-byte layout was confirmed
   against all 42 records in `Fallout4.esm`). **The form is the fallback, the
   override is the answer.**
5. **Depth** = `body.waterHeight − terrainHeight(gx, gy)`; both from this file,
   nothing baked. Shallow/deep blending uses the form's `Color Shallow Range` /
   `Color Deep Range` as vanilla does.
6. **Shore**: sample the shore plane, `value * shoreQuantum` world units,
   saturating at 255. Absent → skip foam; never synthesise it.
7. **Flow**: sample the flow plane. Direction = `(bits 0..7) * 2pi / 256`, speed
   = `(bits 8..11) * body.speedQuantum`, confidence = `bits 12..15`. Where
   confidence is 0, cross-fade to `body.meanFlow`; where the whole plane is
   absent, use `body.meanFlow`; where the body table is absent, use the form's
   `NAM0`. Three floors, each named in the output.
8. **Fog and underwater**: from the form, per body, so two lakes with different
   forms fog differently even at the same height — which the per-cell path
   already allowed and the per-body path preserves.

Row 0 is SOUTH in every plane in this file, including the three new ones; the
`HeightMap` DDS is north-up. A consumer converting to a north-up texel space
mirrors Y. That mismatch has already cost this tree one consumer.

---

## 7. Lane plan and pre-registered gates

Two lanes, in this order, one file each (CONSTITUTION rule 1: one lane per
file). Gates are written HERE, before either starts.

### Lane WATER2 — the writer (`src/lodtfile.cpp`, `src/lodtfile.h`)

Body rule, flow solve, the three planes, the body table, the stroke store, the
version bump, the CLI (`--water-bodies`, `--water-bridge N`,
`--water-flow-samples N`, `--water-report <file>`).

| gate | pass condition |
|---|---|
| G1 byte identity, module OFF | the Commonwealth `.lodl` written with `--water-bodies` off is byte-identical to the current 35,953,294-byte file |
| G2 the fallback | `WW_LODL_VERSION=2` writes those exact bytes with the module ON |
| G3 refusal 1 | the version-2 reader opening a version-3 file fails with *"unsupported version 3 (this reader knows 1..2)"* — and the header-size ternary is a table |
| G4 refusal 2 | each of the four named refusals in §3.8 is provoked on a hand-corrupted file and produces its own sentence, not a zero |
| G5 the census reproduces | **RESTATED, and MET as restated.** WATER1's pre-registered numbers (590 bodies, 93 at ≥ 64, 215 accepted / 3 refused) were measured under a decimated shore test and are withdrawn; §2 carries the audit. What the shipped classifier gives, and what the gate now pins: **346 bodies, 89 at ≥ 64 texels, 1 sea / 115 river / 230 lake, 15 painted forms + the default, the Charles ONE body — id 3 — of 25,114 texels over cells (−16..−6, −21..−4), 528 bridge merges accepted and 13 refused.** The INVARIANT that crossed the rule change unmoved, and which is why this is a grouping difference and not a reading difference: **13 of 15 per-form texel totals identical to WATER1's, form for form**; the two that moved (`ExtOceanWater` 21,594,553 → 21,583,510 and `ExtLakeWater` 12,960 → 24,003) are exactly the 11,043 texels the SIZE guard took back |
| G6 the known-answer control | **RESTATED, and MET as restated.** The expectation belongs to the rule under test: a body carries ONE plane by construction, so the control's stepped river IS sixteen bodies, not one with sixteen surfaces. The geometry was NOT changed. `lodl <any> --water-selftest` builds the synthetic worldspace in memory and asserts **19 bodies: 1 sea (73,728 texels, the WORLDSPACE's own form), 16 river steps, 1 lake (16,384), 1 puddle (16)**, with the type-blind refuter printed beside it (18 bodies: the sea and the river's tidal step fuse). It FAILED on its first run and named a real defect in rule C — §2's missing direction, in the merge the Commonwealth happens never to exercise |
| G7 an INDEPENDENT decoder | `tests/spells/lodl_open_authority.py` grows a version-3 decoder that shares no code with `src/lodtfile.cpp`, and cross-checks every body record and 10,000 random plane texels. The writer is never its own witness |
| G8 the stroke round-trip | strokes written, read back, and re-written are byte-identical; a re-bake with the same store reproduces the same planes byte-for-byte |
| G9 cost, REPORTED not gated | compressed size of the three planes, and the write time against today's 7.4 s Commonwealth |

### Lane WATER3 — the panel and the tool (NEW files `src/watermark.{h,cpp}`, `src/watermarkpanel.{h,cpp}`)

The Water section, the five tools, the body picker, the summary, and the
harness — named `WW_WATER_MARK_TEST` as built, because `WW_WATER_TEST` reads
as the writer's own control and this one tests the TOOL.

**File plan changed, deliberately.** This page said `src/lodgenmanager.cpp`.
Lane BUILD4 was compiling `src/lodgen.cpp` and `src/nifskope_ui.cpp` while this
lane ran, so every line of new code went into NEW files that compile alone
(`g++ -fsyntax-only` with the real flags) and the touches to existing files were
made only after that build finished and were kept to registration and dispatch.

| gate | pass condition | as built |
|---|---|---|
| P0 identity (ADDED) | on an unmarked file, the tool's body-table encoder and its flow-plane packer reproduce the WRITER's own bytes exactly | the first two checks of the model half; everything else rests on them |
| P1 isolation | §5.4 steps 1-4: 0 texels outside the body changed, ≥ 60 per cent inside it did | measured by a WHOLE-PLANE sweep, not over the marked body's bounding box — "only the bbox could have moved" is the claim under test |
| P2 the refuter fires | §5.4 step 5 shows the test RED before it is trusted green | the neighbour is marked FIRST and the river's own changed-texel count printed as 0 |
| P3 undo | §5.4 step 6, byte-identical | strengthened to the WHOLE FILE, not the plane |
| P4 survives a re-bake | a stroke laid at sample rate 32, re-baked at 8, still points the same body the same way (angle within 5°) | **the mechanism shipped, the gate is NOT in the harness.** `--water-flow-samples` is the WRITER's switch and the writer does not read the stroke store; what ships instead is `WaterMarkDoc::setFlowRate` (the Bake section's row), which re-derives the flow plane at 8 or 16 from the same world-coordinate strokes. Wiring the angle comparison into `water_mark.sh` is owed |
| P5 panel style | the counts in §5.3 with their floors, in `WW_WATER_MARK_TEST` | the dock half |
| P6 the picture | `SHOT=<png>` dock grab, and a flow render of the Charles region through the render hook, before and after a stroke | the grab is in the harness; the before/after pair is owed with the build (`Fallout4.exe` was up) |
| P7 dry land (ADDED) | a stroke whose first point is dry is refused in words and stored nowhere | both halves check it |
| P8 round trip (ADDED) | save, reopen, save again is byte-identical, and the strokes come back OUT OF THE FILE | the model half |

### Lane WATER4 — the potential-flow solve and the dye (**BUILT AND RUN 2026-09-10 by lane BUILD10** (`release/NifSkope.exe` 15:52:46; 47 checks / 2 failures on the Charles, and both failures are the two gates the lane pre-registered as expected red))

Pre-registered in `scratchpad/lane_water4_report.md` section 0 before any
code; run by `lodl <copy> --water-mark-selftest` (which now carries them) and
read back by name in `tests/spells/water_flow.sh`:

| gate | pass condition | prototype (numpy, `scratchpad/water4_20260910/flow_proto.py`) | **the C++, run 2026-09-10 (lane BUILD10), body 3** |
|---|---|---|---|
| F1 continuity | a channel that narrows to half its width: speed ratio 2.00 +- 5 percent; flux through 10 sections within 3 percent | 2.0000; 1.6e-10 | 2.0000 (460 iterations, residual 9.3e-10); 1.63e-10 -- **green** |
| F2 island | parts and rejoins (halves within 2 percent); mass balance 1e-6 at every cell; straight-bank normal < sin 1 deg; island bank direction vs the analytic cylinder mean < 5, max < 15 deg | 0.5000 / 0.5000; 3e-12; 3.9e-4; **12.0 / 22.4 deg -- FAILS as registered**, R-independent (8, 16, 32 all ~12), one ring in 4.0, at 2R 0.8: the face-averaged velocity at a STAIRCASE bank cell, not the solve | 0.5000 / 0.5000 (526 it, 9.6e-10); 3.05e-12; 3.93e-4 over 416 texels; **12.01 / 22.40 deg over 48 bank texels -- RED as registered, the prototype's own number** |
| F3 lake, no outlet | speed exactly 0 | 0 | 0 exactly, 0 iterations -- **green** |
| F4 lake, one outlet | 0 texels point away; every streamline (Pollock's, on the face fluxes) reaches it | 0 of 3225; 40 of 40 | 0 of 3,225 (mean cosine 0.914, 252 it); 40 of 40 -- **green** |
| F5 the Charles | 0 seam-bounded patches; p99 adjacent jump < 5 deg; seams < 0.5 percent; cos to the mouth > 0.9; R stated | patches 0; **p99 7.0 at 8 passes (FAILS as registered by 2 deg)**; seams 0.26 percent; R 0.79 | patches **0** (39 before); **p99 8.44 -- RED as registered**; seams 0.422 percent; cos 1.000; R 0.794. The independent decoder reads the same numbers off the saved file. On the held-out body 2: 9.84 / 0.840 percent / cos 0.763 |
| F6 plume | 1/8 length and direction predicted before the dye, measured after, within 10 percent / 9 deg | 98.2 vs 96 texels; 0.0 vs 0.0 deg | 98.2 vs 96 texels; 0.0 vs 0.0 deg -- **green** |
| F7 dye pin | 1/2 at L, 1/8 at 3L, 0 upstream | 0.5000; 0.1250; 0 | 0.5000; 0.1250; 0 -- **green** |
| F8 cost | the Charles under 1.0 s, residual < 1e-8 | numpy 1.5 s at 1,565 CG iterations (the C++ is what is gated) | **0.319 s**, 1,493 iterations, residual 9.26e-10 -- **green** (`water_flow.sh` calls it red: its grep matches the informational line above the ok line) |
| dye round trip | the plane is written only with a dye mark, reads back through `LodtFile::dyeWordAt`, and undo is byte-identical | C++ only | 8,649 of 8,649 sampled texels agree; save-reopen-save byte-identical; undo 0 bytes differ -- **green** |

Neither lane commits without bungo's word, both write their reports
incrementally, and both end with the finished-work skill review.

---

## Provenance

Written by lane WATER1 (design), corrected by lane WATER3 against the code lane
WATER2 landed. Anchors are quoted beside every line number and every number was
re-derived from its anchor by `scratchpad/water3_20260910/anchors.py` as the last
step, per the `ww-contract-provenance` procedure.

| file | sha256 (16) | bytes | lines |
|---|---|---|---|
| `src/lodtfile.cpp` | `10ad5f58262d8620` | 140,908 | 3,638 |
| `src/lodtfile.h` | `128dcd3f4c011a08` | 20,346 | 415 |
| `src/watermark.cpp` | `4bf11c1eeab01923` | 70,693 | 2,004 |
| `src/watermark.h` | `f433ccea8160f410` | 13,344 | 281 |
| `src/watermarkpanel.cpp` | `3d9d825e4ffcea56` | 44,604 | 1,220 |
| `docs/LODGEN_BTD_FORMAT.md` | `98ff8f12d520ae03` | 72,264 | 1,303 |
| `tests/spells/lodl_open_authority.py` | `b7be45375288ea90` | 9,877 | 239 |

The format, as WATER2 built it:

| claim | line | anchor |
|---|---|---|
| the version constants, now 1..3 | lodtfile.cpp 56 | `constexpr quint32 LODL_VERSION = 3;` |
| the header size is a TABLE, and the version refusal runs before it | lodtfile.cpp 74 | `static inline qsizetype lodtHeaderBytes( int version )` |
| version 3's header is 0xF8 | lodtfile.cpp 63 | `constexpr qsizetype LODL_HEADER_V3 = 0xF8;` |
| the default water-type sentinel | lodtfile.cpp 101 | `constexpr quint16 WATER_TYPE_DEFAULT = 0xFFFFU;` |
| refusal 1's text | lodtfile.cpp 2791 | `unsupported version %1 (this reader knows %2..%3)` |
| the writer's own version refusal and the `WW_LODL_VERSION` fallback | lodtfile.cpp 1716 | `if ( qEnvironmentVariableIsSet( "WW_LODL_VERSION" ) )` |
| section-flag bits 4..7, the water sections | lodtfile.h 42 | `constexpr quint32 LODL_SECT_BODIES      = 1u << 4;` |
| the body record, 48 bytes | lodtfile.h 57 | `struct LodtWaterBody` |
| the water module's own switches | lodtfile.h 83 | `struct LodtWaterOptions` |
| the plane container's packer, and the uniform tile | lodtfile.cpp 433 | `static QByteArray lodtPackPlane( int tilesX, int tilesY, int tileEdge,` |
| the flow word the writer puts on an unmarked body | lodtfile.cpp 1246 | `word = quint16( dir | ( 8 << 8 ) );` |
| the stroke store, written present and EMPTY | lodtfile.cpp 1159 | `/* The stroke store is written EMPTY and present: a count of zero.` |
| the reader's version-3 accessors | lodtfile.h 266 | `bool waterBody( int id, LodtWaterBody & out ) const;` |
| the raw stroke store, as read | lodtfile.h 288 | `QByteArray strokeStore() const { return strokes; }` |

The marking tool, as WATER3 built it:

| claim | line | anchor |
|---|---|---|
| the header fields this tool patches | watermark.cpp 36 | `constexpr qsizetype kHdrV3      = 0xF8;` |
| the packer, the TWIN of lodtPackPlane, and why a twin is safe | watermark.cpp 108 | `QByteArray packPlane( int tilesX, int tilesY, int tileEdge, int bytesPerSample,` |
| an absolute plane offset is rebased, never memcpy'd | watermark.cpp 172 | `QByteArray rebasePlane( const QByteArray & bytes, quint64 oldBase, quint64 newBase )` |
| the section ORDER the tool refuses to rearrange | watermark.cpp 376 | `"the water sections are ordered body 0x%1, stroke 0x%2, "` |
| the stroke store codec | watermark.cpp 434 | `QByteArray WaterMarkDoc::encodeStrokes() const` |
| the body table encoder, byte for byte the writer's | watermark.cpp 458 | `QByteArray WaterMarkDoc::encodeTable() const` |
| a stroke on dry land is refused in words | watermark.cpp 822 | `"that stroke starts on dry land, so it names no body of water; "` |
| the constrained harmonic fill, red-black SOR | watermark.cpp 884 | `bool WaterMarkDoc::solve( WaterMarkSolve * out, QString * error )` |
| confidence: the stated departure from 4.3 | watermark.cpp 1108 | `* spec asks for "a second harmonic fill with 15 at the constraints and` |
| the automatic word, reproduced from the table | watermark.cpp 1251 | `quint16 WaterMarkDoc::automaticWord( quint16 id ) const` |
| the whole-plane sweep the isolation gate measures on | watermark.cpp 1298 | `bool WaterMarkDoc::sweep( const std::function<void( int, int, quint16, quint16, quint16 )> & cb,` |
| the save: prefix verbatim, tail re-derived, original renamed aside | watermark.cpp 1489 | `bool WaterMarkDoc::save( QString * error )` |
| the two identity gates | watermark.cpp 1430 | `bool WaterMarkDoc::flowRepackMatches( qint64 * differingBytes, QString * error ) const` |
| the harness | watermark.cpp 1665 | `bool lodtWaterMarkSelfTest( const QString & path, QString * text, QString * error )` |
| the stroke kinds, 0..6 | watermark.h 66 | `enum Kind { Stroke = 0, Pin = 1, Barrier = 2, Merge = 3, SourcePin = 4, OutletPin = 5,` |
| the still-water mark is a STROKE, not a table bit | watermark.h 59 | `*  kind 6, a one-point mark saying this body is still -- a STROKE and not a bit` |
| the dock, and its one line in somebody else's file | watermarkpanel.cpp 1182 | `void waterMarkInstall( QMainWindow * mw )` |
| the panel's rows and its summary sentence | watermarkpanel.cpp 618 | `void refreshSummary()` |
| the canvas, and the Blender divergences | watermarkpanel.cpp 158 | `class WaterMarkCanvas final : public QWidget` |
| the dock's self-test counts | watermarkpanel.cpp 991 | `void runSelfTest( QMainWindow * mw, QDockWidget * dock, WaterMarkPanel * panel )` |

Still true from the older documents:

| claim | line | anchor |
|---|---|---|
| the plane list a `--info` prints, and where `waterheight`/`watertype` come from | lodl_open_authority.py 134 | `def plane_keys(self):` |
| depth is a runtime subtraction, shore was dropped for it | LODGEN_BTD_FORMAT.md 965 | `**shore proximity**` |
| ROW 0 IS SOUTH in every grid | LODGEN_BTD_FORMAT.md 214 | `**ROW 0 IS SOUTH**` |
| FO4CS pins `kVersion = 1u` | LODGEN_BTD_FORMAT.md 27 | `A CONSUMER EXISTS, AND IT KNOWS VERSION 1 ONLY.` |

Measured inputs, with their own stamps:

| input | stamp |
|---|---|
| `Commonwealth.lodl`, version 2 (the writer's own bytes with the module off) | 35,953,294 bytes |
| `Commonwealth.lodl`, version 3 | 38,612,038 bytes, `scratchpad/water2_20260909/out/Terrain/`, mtime 2026-09-10 00:50, 346 bodies, body table at 0x2249AE6, stroke store 4 bytes (a count of zero), planes at 32/32/32 samples a cell |
| the body census every number in 1 and 2 was re-derived from | `scratchpad/water3_20260910/census_v3.txt` (`lodl --water-census`) and `audit_spec.py` through the INDEPENDENT decoder `scratchpad/water2_20260909/lodl_v3_authority.py` |
| `Fallout4.esm` | `X:\Programs\Steam\steamapps\common\Fallout 4\Data\Fallout4.esm`, 42 WATR records, WRLD `0000003C` |
| `WATR DNAM` field order | xEdit `wbDefinitionsFO4.pas`, `wbRecord(WATR, 'Water'` — 201 bytes, and all 40 full records measured at exactly 201 (2 truncated at 188, the `SetOptionalFrom(4)` tail) |
