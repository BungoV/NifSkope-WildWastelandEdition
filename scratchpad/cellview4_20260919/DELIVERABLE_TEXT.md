# CELLVIEW4 -- the blended ground, the bare quads, the black shape

Lane CELLVIEW4, 2026-09-19, launched 19:30, written through 20:4x (clock read
with `date`, never from elapsed-time feel).

This is the lane's deliverable text. It is under the brief's **fallback name**:
the tool harness in this session refuses to let an agent write a file named
`report.md`, so the brief's stated alternative was used. Nothing else about it
changed.

## 0. What this lane is, and what it is not

CODE-ONLY. It compiled nothing, built nothing, started no exe. Everything below
is either **a measurement of file bytes**, **a labelled SIMULATION**, or **code
that has never been through a compiler**. The status line is at the bottom and
it is `BUILD PENDING`.

Two honesty notes before anything else:

* `g++` in this lane's shell exits 1 with **zero bytes of stderr**, even for
  `int main(){return 0;}`. The lane's first two "clean syntax passes" were
  therefore artifacts of swallowed output, not successes. They were withdrawn.
  `src/cellsplat.cpp` and `src/cellsplat.h` are **unverified by any compiler**
  and the director's step 0 is a syntax pass in a shell where the compiler
  speaks (`PENDING.md`).
* This text was owed **inside the lane's first ten tool calls** and was not
  written until the end. That is a process failure, it is in the root
  `MISTAKES.md` alongside the technical one, and it means a crash before now
  would have left only scripts behind.

Deliverables on disk:

| what | where |
|---|---|
| the design header | `E:/Projects/NifskopeWildWastelandEdition/src/cellsplat.h` |
| the builder | `E:/Projects/NifskopeWildWastelandEdition/src/cellsplat.cpp` |
| the gate (starts nothing) | `E:/Projects/NifskopeWildWastelandEdition/tests/spells/cell_splat_compare.py` |
| the ONE hook-up script | `E:/Projects/NifskopeWildWastelandEdition/scratchpad/cellview4_20260919/hookup.py` |
| its dry run onto copies | `E:/Projects/NifskopeWildWastelandEdition/scratchpad/cellview4_20260919/hookup_dryrun.py` |
| the director's commands | `E:/Projects/NifskopeWildWastelandEdition/scratchpad/cellview4_20260919/PENDING.md` |
| the simulation | `E:/Projects/NifskopeWildWastelandEdition/scratchpad/cellview4_20260919/splat_sim.py` |

**THE TARGET PICTURE:**
`E:/Projects/NifskopeWildWastelandEdition/scratchpad/cellview4_20260919/images/sim_m20_7_blended.png`

beside today's rule rebuilt the same way, from the same record, by the same
script, in the same frame:
`E:/Projects/NifskopeWildWastelandEdition/scratchpad/cellview4_20260919/images/sim_m20_7_mosaic.png`
(and their difference, `sim_m20_7_diff.png`).

**These two are SIMULATIONS.** They are Python compositing raw diffuse texels
out of the shipped `.dds` files at the opacities the LAND record stores. They
are not a render: no lighting, no vertex colour, no tone map. What they are
good for is telling two *rules* apart, and that is exactly what the gate asks
of them.

---

## 1. Ground blending

### 1.1 The data path

`Fallout4.esm` -> `EsmWorld::land` (`src/esmdata.cpp`) -> `EsmLand` ->
`cellBuildSplat` (new) -> `Bucket`s in `src/cellview.cpp` -> `emitBucket` ->
BSTriShapes in the NIF the cell viewer hands the renderer.

A LAND record carries, per cell, four quadrants (0 BL, 1 BR, 2 TL, 3 TR). Each
quadrant has at most one `BTXT` base texture and any number of `ATXT`+`VTXT`
layer pairs. `ATXT` is `<IBBh`: LTEX formid, quadrant, one unknown byte, and an
**int16 layer index**. `VTXT` is a list of `<HHf`: position, unknown, opacity,
where `row = posn // 17` and `col = posn % 17` over a 17x17 grid.

The tree reads the LTEX form and the quadrant and **stops**. The layer index --
the order the engine composites that quadrant's layers in -- is thrown away at
`src/esmdata.cpp:306-335`. The hook-up adds three bytes of reading there and an
`int index` to `EsmLandLayer`, because compositing in the order layers happen to
sit in the record is a guess, and a builder should be able to tell a known order
from a guess. It can: `cellSplatLegend()` shouts **"WITHOUT THE PAINT ORDER"**
when `WW_CELLSPLAT_LAYER_INDEX` is undefined, and the gate's own failure text
tells the reader that a score between 76% and 90% means exactly that.

### 1.2 The mesh and texture representation

The 33x33 land vertex grid maps onto the four 17x17 quadrant grids **with the
middle row and column SHARED** -- local index is `grid - 16`, not `grid - 17`.
This is the fact the whole design rests on, and it is worth stating plainly:

> **every corner of every 128-unit quad has a stored opacity. Nothing is
> interpolated by us, and neighbouring quads read the same stored number at the
> edge they share.**

So there is no seam to invent a rule for and no resampling to defend.

The representation is therefore **geometry, not texture units**:

* one quad per **contributing pass** per 128-unit cell quad;
* pass 0 is the quadrant's `BTXT`, opaque;
* then one pass per `ATXT` layer that is non-zero at any of the four corners,
  in paint order, alpha-blended;
* the layer's own VTXT opacity travels in the **alpha byte of the vertex colour
  the file already writes**. `OutVert::chan` goes from 3 floats to 4; the NIF
  vertex format does not change at all, because `emitBucket` already writes a
  `ByteColor4` and already puts 1.0 in its alpha.

**One texture unit per shape. One pass. No new shader, no render-target
change, no second geometry path.** The rasteriser interpolating vertex alpha
across the quad *is* the engine's bilinear blend, and `result = lerp(result,
layer, opacity)` per pass *is* the engine's ordered alpha-over.

What makes the order come out right in NifSkope's draw path:
`Scene::drawDeferredShapes` calls `secondPass.alphaSort()`, and
`compareNodesAlpha` returns **block order** when both nodes are presorted and
depth order otherwise; `presorted` is set from a root `BSOrderedNode`, which
`Node::drawShapes` then pushes onto every child. So the root block type changes
from `NiNode` to `BSOrderedNode` and bucket emit order becomes paint order.
Depth is already safe: `src/gl/renderer.cpp` ~890 disables depth writes for
translucent shapes.

**THE REFUTER, and it is a real one.** That root change re-sorts **every**
transparent shape in the scene, not just the ground. If tree cards or glass
come back sorting worse in the downtown picture, this is the cause, and the
repair is to give the ground its own `BSOrderedNode` child rather than making
the scene root one. `PENDING.md` step 5 picture 3 exists to look for that.

### 1.3 The vertex budget against the 12M cap

`CELL_MAX_TOTAL_VERTS = 12000000` (`src/cellview.cpp:44`). The mosaic costs a
fixed 4096 verts per cell (1024 quads x 4). A splat costs
`4 x quadsEmitted`, and the multiplier is `quadsEmitted / quadsTotal` -- the
mean number of passes a quad carries.

It is **measured, not guessed**, and it is measured twice: `cellSplatLegend()`
prints it for whatever the viewer actually built, and `cellSplatCountVerts()`
computes it **before a single vertex is allocated**, so the refusal happens
ahead of the allocation instead of after it. Past the cap the blend refuses by
name in the census line and the hard-edged mosaic draws the rectangle -- a
fallback, not a crash, and not a toggle.

### 1.4 The simulation, and what it says

`splat_sim.py` reads the real LAND for Sanctuary **-20,7**, resolves each LTEX
to its TXST diffuse, loads the shipped `.dds`, tiles at
`fLandTextureTilingMult = 341.3333` world units, and composites twice: once by
today's mosaic rule and once by the engine's. Per-quad block means of the two:

```
mean |blended - mosaic| over the cell   0.0183
worst quad                              0.5026
quads where the rules disagree (>0.02)  249 / 1024
```

(Per *pixel* the same pair reads 0.0345 mean and 509/1024 quads over 0.02.
These are two different reductions of one picture and the lane mixed them up
once already -- the gate's docstring now carries both and says which it uses.)

---

## 2. The 24 still-bare quads

### 2.1 What they carry in the record bytes

Measured over **all 36,864 LAND records** in the Commonwealth by
`bare_quads.py`, which replays `src/cellground.cpp`'s exact rule and then says
why each bare quad is bare in the record's own terms:

| reason | quads |
|---|---|
| `no-btxt-no-layers` -- quadrant has neither BTXT nor ATXT | 33,810,944 |
| `no-btxt-zero-here` -- has ATXT layers, all read 0.0 at this corner | 195,961 |
| `no-btxt-null-form` -- its only layers name LTEX form 0 | 512 |
| `btxt-null-form` -- has a BTXT whose LTEX form is 0 | **0** |

Quadrants with no BTXT at all: **134,985 of 147,456**; of those, **132,074**
have no ATXT either.

Sanctuary -20,7's 24 are **all** `no-btxt-zero-here`, **all in quadrant 3**,
which carries **no BTXT** and **four layers, every one with a real LTEX form**.
So the record is not silent there. Paint exists in that quadrant; every layer
simply reads 0.0 at those particular corners.

### 2.2 What the game draws there

The Commonwealth WRLD names **no** default land texture. Its subrecords are
`CNAM DATA DNAM EDID FULL ICON MNAM NAM0 NAM2 NAM3 NAM4 NAM9 NAMA ONAM WLEV
XLCN XWEM ZNAM` (`wrld_fields.py`), and `DNAM` is the 8-byte land/water HEIGHT
pair, not a texture -- which is what `wbDefinitionsFO4.pas` calls it too. There
is no INI key for one either. This corroborates CELLVIEW3 independently.

So the honest answer is: **the record does not say, and nothing upstream of it
says.** What the engine puts there is a question about the engine's default
material, which this lane has no file fact for and will not invent one for.

### 2.3 The proposed rule

Do not paint them. Count them, and let the four-corner rule shrink them.

A mosaic quad asks **one** corner which texture wins. A splat quad carries a
weight at **all four** and interpolates, so it is blank only when the quadrant
has no BTXT **and** every layer reads 0.0 at **all four** corners. That is not
a new rule -- it is the same rule the blend already needs.

On -20,7: **24 -> 6**, and the six are
`(25,27) (27,18) (28,25) (30,16) (31,16) (31,17)`.
Corpus-wide over the 3,955 cells that paint anything at all:
**308,601 -> 243,344**.

The remainder stay bare and stay **counted in the census line**. `cellsplat.h`
records this as one of the lane's three refusals: bare quads are counted, never
invented.

---

## 3. The solid-black shape downtown

### 3.1 The name, model, shape and material

| | |
|---|---|
| reference | REFR **`0x00066245`** |
| base | **`0x00000034`**, STAT -- the editor's XMarkerHeading |
| model | **`markerxheading.nif`**, at the meshes ROOT |
| shape | one `BSTriShape`, 40 verts, 18 triangles |
| vertex descriptor | `0x0002900003020004` -- flags `0x29` = VERTEX\|NORMAL\|COLORS, **no UV, no tangent**, 16-byte verts |
| material | `BSEffectShaderProperty` with an **empty Name** and an **empty Source Texture**; `NiAlphaProperty` flags **4333** |

Placement check, done independently of the picture: predicted screen position
**(1368.9, 548.2)** against a measured blob centre of **(1364.5, 547.5)**;
exactly one reference in the cell covers those pixels; **497 pixels of pure
rgb(0,0,0)**; the 4x crop (`images/blackshape_crop_x4.png`) shows a clean arrow
silhouette with no shading variation at all, so the albedo is zero rather than
the lighting being wrong.

### 3.2 The likeliest cause -- and what was refuted

**Refuted, twice: "it is black because it has no tangents."**
(a) `black_probe.py` found **18** tangent-less instances on screen in the same
picture, every one of them with mean luminance 0.33-0.53 and a black fraction
of 0.00, including pillars and skeletons the same size as the arrow.
(b) Mechanism: the placeholder normal `#FFFF8080n` decodes through
`TexCache::texLoadColor` (`src/gl/gltexloaders.cpp:987-1019`) to
R8G8B8A8_SNORM `(0,0,+1)` -- a **flat** normal that never touches the tangent
frame at all.

**Still a candidate, not decisive: the untextured effect shader.** 179 shapes
from 51 models weld into this arrow's exact bucket key `|1|128|0|0|0`, yet the
picture shows **one** black blob. The refuter is that those bucket-mates are
largely interior pieces hidden from a top-down camera, so their absence from
the picture is not evidence that the bucket renders fine. Both readings survive
the files, so **the black cause does not go in the hook-up script.**

### 3.3 The repair that IS decisive

`isMarkerModel()` (`src/cellview.cpp:200`) tests
`\marker`, `marker_`, `endsWith("markerx.nif")` and `\editor\`. **Every one of
those needs a backslash, or the exact suffix.** A marker sitting at the meshes
root -- `markerxheading.nif`, `markercocheading.nif`, the `markers\...` subtree
-- has no leading backslash and is drawn as an ordinary static.

That is decisive from the files alone and independent of why it is black: an
editor marker should not be in the picture at all. Measured in cell 5,-11,
exactly **three** models take the wholly-material-less branch and **all three
are markers**. The hook-up adds one clause, `m.startsWith("marker")` -- `m` is
already lowercased on the line above -- and nothing else. Per the brief, the
black-cause repair is **not** included.

This is a repair, so it ships without a toggle.

---

## 4. The hook-up script and its `--check`

One script, `scratchpad/cellview4_20260919/hookup.py`, written with the Write
tool. Thirteen edits over four files. `--check` is the default, writes nothing,
and prints **counts**, never "ok". `--apply` refuses unless every anchor matches
exactly once. Every anchor is **read out of the file** -- only a short prefix is
typed and the file supplies its own tabs and its own trailing comment. The
already-applied marker is the string `lane CELLVIEW4` in the target files, which
is **not** any anchor. **No anchor is in `src/lodgen.cpp`.**

Three things the lane got wrong in the script and fixed before running it:

1. The inserted splat block contains a line reading `if ( spec.terrain ) {`,
   which is also another edit's anchor. Taking the first hit would have edited
   the wrong line. Fixed two ways: the narrowing edit now runs first, **and**
   `apply_to()` re-counts against the text in hand and refuses at apply time if
   an anchor stopped being unique.
2. The CRLF arithmetic was wrong for `replace` edits (off by one per edit). Now
   `crAfter == crBefore + linesAdded` on a CRLF file and `crAfter == crBefore`
   exactly on an LF file, asserted per file.
3. `#include "cellsplat.h"` was missing from the table entirely. Caught by
   grepping the dry-run copies, not by reading the table.

`hookup_dryrun.py` runs the same table through the same `apply_to()` onto
**copies**, so the apply is demonstrated without `src/` being touched: brace
and paren deltas match what the inserted text carries, whole-file balance stays
`+0`, CR stays 0.

### The `--check`, run last, quoted whole

```
CELLVIEW4 hook-up -- --check
file               mode     count anchor (from the file)
NifSkope.pro       replace  1     '\tsrc/cellground.cpp \\'
        note: the anchor carries a backslash or a quote; it is compared as BYTES read from the file, never retyped
NifSkope.pro       replace  1     '\tsrc/cellground.h \\'
        note: the anchor carries a backslash or a quote; it is compared as BYTES read from the file, never retyped
src/esmdata.h      replace  1     '//! One additional splat layer on a cell quadrant: 17x17 opaciti'
src/esmdata.h      replace  1     '\tfloat opacity[17][17];      //!< [row][col] over the quadrant, '
src/esmdata.cpp    after    1     '\t\t\t\tlayer.ltex = ltex;'
src/cellview.cpp   replace  1     '#include "cellground.h"\t\t// lane CELLVIEW2'
        note: the anchor carries a backslash or a quote; it is compared as BYTES read from the file, never retyped
src/cellview.cpp   replace  1     '\tfloat chan[3] = { 1.0f, 1.0f, 1.0f };'
src/cellview.cpp   replace  1     '\t\t\t\t\tByteColor4( FloatVector4( o.chan[0], o.chan[1], o.chan[2], '
src/cellview.cpp   replace  1     '\t\t\tnif->set<int>( iAlpha, "Flags", 4844 );'
        note: the anchor carries a backslash or a quote; it is compared as BYTES read from the file, never retyped
src/cellview.cpp   replace  1     '\tQModelIndex iRoot = nif->insertNiBlock( QStringLiteral( "NiNode'
        note: the anchor carries a backslash or a quote; it is compared as BYTES read from the file, never retyped
src/cellview.cpp   replace  1     '\t\t|| m.endsWith( QLatin1String( "markerx.nif" ) )'
        note: the anchor carries a backslash or a quote; it is compared as BYTES read from the file, never retyped
src/cellview.cpp   replace  1     '\t\tif ( spec.terrain ) {'
src/cellview.cpp   after    1     '\t\tQVector<Bucket> groundBuckets;'

NifSkope.pro       carries the marker 'lane CELLVIEW4' 0 time(s)
src/cellview.cpp   carries the marker 'lane CELLVIEW4' 0 time(s)
src/esmdata.cpp    carries the marker 'lane CELLVIEW4' 0 time(s)
src/esmdata.h      carries the marker 'lane CELLVIEW4' 0 time(s)
NifSkope.pro       CR before 0, after 0, 2 line(s) added, 2 edit(s)
src/cellview.cpp   CR before 0, after 0, 96 line(s) added, 8 edit(s)
src/esmdata.cpp    CR before 0, after 0, 6 line(s) added, 1 edit(s)
src/esmdata.h      CR before 0, after 0, 8 line(s) added, 2 edit(s)

13 of 13 anchors match once. NOTHING WAS WRITTEN (--check).
```

---

## 5. The gate row -- **NOT RUN**

`tests/spells/cell_splat_compare.py`. It starts nothing; the director supplies
the shot. Both ends were measured by feeding each known answer in as the shot,
before any C++ existed:

```
GATE  cell_splat_compare -20,7   NOT RUN
  known answer  shot = blended (a correct build)   249/249  100.0%  PASS
  known answer  shot = mosaic  (today's build)     187/249   75.1%  FAIL
  the line                                                   90.0%
  margin above the failing case                              15 points
  reduction     32x32 per-quad block means, one gain+offset per channel
                fitted over the whole cell, scored only on the 249 quads
                where the two rules disagree by more than 0.02
```

Two things this row is honest about. A mosaic build does **not** score near
zero -- the two pictures agree over most of the cell and three quarters of even
the disagreeing quads land nearer the blended target by chance, which is why
the line is at 90% and not 50%, and why moving it below 76% would make the gate
**unable to fail**. And a score between 76% and 90% is a distinct diagnosis:
the layers composite, in the wrong order.

---

## 6. The doc text, for the director to splice

The lane wrote neither file.

### WW_CHANGES.md

```
### Cell view: the ground BLENDS (lane CELLVIEW4, 2026-09-19) -- BUILD PENDING

The painted ground was a hard-edged mosaic: each 128-unit quad took the one
texture that won at its own south-west corner. It now draws the quadrant's base
texture and then every painted layer over it, each at the opacity the LAND
record stores at that quad's four corners -- which is what the game does.

New files `src/cellsplat.h` and `src/cellsplat.cpp`. No new texture units, no
second pass, no shader change: the layering is in the geometry, one quad per
contributing layer, and the weight rides in the alpha byte of the vertex colour
the cell writer already wrote. The scene root became a BSOrderedNode so the
transparent pass sorts by block order, which is paint order.

`src/esmdata.cpp` now reads the ATXT layer index -- the engine's paint order --
which the tree had been discarding. Without it the builder says so out loud in
its census line rather than assuming.

The vertex cost is counted BEFORE anything is allocated: past the 12,000,000
cap the blend refuses by name in the census line and the mosaic draws the
rectangle.

Also: editor markers at the meshes root were being drawn as ordinary statics,
because every test in isMarkerModel() needed a leading backslash. That is the
solid black arrow in the downtown picture -- REFR 0x00066245, markerxheading.nif.
One clause fixes it. WHY it renders black is a separate question the lane could
not settle, and no guess about it shipped.

Gate: tests/spells/cell_splat_compare.py (starts nothing). NOT RUN.
```

### HANDOFF.md

```
* **CELLVIEW4 (cell view: the ground blends) -- DELIVERED, BUILD PENDING.**
  New `src/cellsplat.{h,cpp}`: the quadrant's BTXT then every ATXT layer in
  paint order, per-vertex VTXT opacity in the vertex colour's alpha, one pass
  and one texture unit. `scratchpad/cellview4_20260919/hookup.py` carries the
  13 edits over `NifSkope.pro`, `src/esmdata.{h,cpp}` and `src/cellview.cpp`;
  `--check` says 13 of 13 anchors match once, nothing written, no anchor in
  `src/lodgen.cpp`. **THE C++ HAS NEVER BEEN COMPILED** -- g++ in that lane's
  shell exited 1 with no stderr even on a trivial file, so a syntax pass is the
  director's step 0 (`scratchpad/cellview4_20260919/PENDING.md`).
  Target picture `scratchpad/cellview4_20260919/images/sim_m20_7_blended.png`
  (a SIMULATION, python over the real LAND record). Gate
  `tests/spells/cell_splat_compare.py`, pre-registered at 90% with both ends
  measured (blended 100.0% PASS, mosaic 75.1% FAIL), NOT RUN.
  Carried: the root is now a BSOrderedNode, which re-sorts EVERY transparent
  shape by block number -- look at tree cards and glass in the downtown
  picture, and if they got worse, give the ground its own ordered child
  instead of the scene root. Carried: the 6 quads on -20,7 that stay bare
  under the four-corner rule are counted, not invented -- the Commonwealth
  WRLD names no default land texture (measured, subrecord list in section 2).
  Carried: WHY markerxheading.nif renders solid black is unsettled; the
  tangent theory was refuted twice and the untextured-shader theory has a live
  refuter, so only the decisive `isMarkerModel` repair shipped.
  Note: the lane's deliverable text is `DELIVERABLE_TEXT.md`, not `report.md`
  -- the harness refused the latter name.
```

---

## 7. MISTAKES

**Written by the lane** into the root `MISTAKES.md`, at the top, by byte splice
so the file's CRLF survived:

```
CR before 10517, after 10550 (+33); LF before 10517, after 10550 (+33)
spliced 2084 bytes at offset 309
```

The entry is *"a byte scan was used as evidence about a format this tree has a
reader for"*: `tangent_census.py` guessed at `BSVertexDesc` by scanning each
block for a plausible u64 and reported flags `0x02F` / vertex size 40 for
`markerxheading.nif`, whose real descriptor is `0x0002900003020004` -- flags
`0x29`, size 16. It had locked onto a bounding-sphere float pair, and a whole
theory was built on the number before anyone checked it. Rebuilt field-exact
from the declared NIF order with a self-check against a file whose numbers were
known independently; 153 spurious "refusals" then turned out to be 12 trailing
bytes on `BSMeshLODTriShape`, and with a per-type tail table the refusals went
to zero. The second half of the entry is the late deliverable text.

The splice script is `scratchpad/cellview4_20260919/mistake_splice.py` and it
refuses if the top of the file already names CELLVIEW4.

## 8. Skill text earned

Not a new skill -- an **addition to `ww-anchored-hookup`**, earned by a bug the
skill as written does not catch. Text for the director to paste into the skill
in **both** trees, at the end of section 5:

```
5c. AN EDIT CAN EAT THE NEXT EDIT'S ANCHOR.

Inserted text is source too, and a block you insert can contain a line that is
a later edit's anchor. The table was resolved against the file as it was, so
the count printed by --check says 1 and the apply then quietly edits the copy
you just inserted instead of the one you meant.

Two defences, and use both:
  * count again AT APPLY TIME, against the text in hand, and assert exactly
    one match. Never "take the first hit" -- that is the failure, not the fix.
  * order the table so an edit that NARROWS a line runs before the edit that
    inserts a block mentioning it.

And demonstrate the apply without applying it: run the same table through the
same apply function onto COPIES in the scratchpad, then check each copy's CR
delta and its brace/paren delta against what the inserted text carries. A
hook-up script whose --apply has never produced a byte is not evidence that it
works. (Found by lane CELLVIEW4, 2026-09-19: an inserted block carried its own
`if ( spec.terrain ) {`, and the same dry run caught a missing #include the
table had simply never had.)
```

---

**BUILD PENDING**
