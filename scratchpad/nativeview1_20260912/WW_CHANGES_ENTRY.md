## 2026-09-12 — The viewer opens the native LOD bake: `.lodi` objects, and a `.lodl` lit by its own sheets

Lane NATIVEVIEW1, main tree. bungo, looking at a wall of showcase pictures:
*"why are you showing me .btr still? btr is a legacy thing"*. He was right — the
`.BTR`/`.BTO` pair is the stock engine's far field, and this fork's whole point
is the `.lod` files that replace them. The viewer could not open a single one of
them, so every picture of "the new far field" was a picture of the old one.

Now it can.

### A `.lodi` opens as a document

`File > Open` on a `.lodi` builds a Fallout 4 document, exactly the way a `.btd`
or a `.lodl` does. The file is a worldspace's PLACEMENT table and stores no
triangles; the geometry lives in the `.lodo` library beside it, found by
worldspace stem. Every placement in the chosen region becomes an instance of its
base mesh, welded in world space into one `BSTriShape` per (base, material) — the
same shape a `.BTO` of that chunk carries, with the same shader plumbing and the
same materials, because the point of the picture is that it IS the same far
field.

`WW_LODI_REGION="x0,y0,x1,y1"` chooses cells (default: the file's whole extent),
`WW_LODI_LEVEL=n` chooses the cluster-ladder level (default: finest), and
`WW_LODI_BOXES=1` draws the occluder boxes as wire boxes and nothing else does.

The build does not just draw: it prints what it MEASURED — placements read,
drawn, outside the region, without a mesh — and `WW_LODI_DUMP=<file>` writes one
row per placement it actually put down. That census is what the gate reads,
because nothing can count placements by looking at welded geometry.

### A `.lodl` is drawn LIT when its sheets are beside it

A `.lodl` carries heights and a colour word, and the colour word is a data view:
it is what the file says, not what the world looks like. The land textures live
in the `.lodt` sheet pyramid the same bake writes. So a Height open now looks for
`<worldspace>.VT.<dim>.lodt` beside the file (or at `WW_LODL_SHEETS=<dir>`) and,
finding it, lights the terrain with it — the colour sheet as diffuse, `_msn` as
the normal map, through a real texture set, per sheet tile. Tiles are unpacked
once to loose DDS in a cache directory that is pushed onto the session's resource
roots and never saved.

Nothing found, nothing readable, or any plane other than Height: the build says
so in words and draws exactly the data view it always drew.

One thing had to be learned the hard way and is worth writing down: the sheet's
normal map is an `_msn`, a MODEL-SPACE map, and a shader block that does not say
so lights the land about 40 percent too dark. The bake's own `.BTR` of the same
chunk is the authority for the flags it should carry -- Shader Flags 1
`0x80401000` (`Model_Space_Normals` set, `Specular` clear) and Shader Flags 2 `3`
(`ZBuffer_Write` with `LOD_Landscape`) -- and the sheet branch now sets exactly
those three bits. Measured against the `.BTR` of the same cells: mean absolute
colour difference 50.457 before, 35.821 after, with the two lumas correlating at
+0.6008 (a different chunk: +0.2023; the same chunk mirrored in Y: -0.0242).

`WW_LODL_OBJECTS=<file.lodi>` puts the objects in the same document as the
terrain, so one picture can be the whole region.

### What did not change

Every reader, no writer. No bake output is touched, no default moves, and with
no sheet pyramid beside a `.lodl` the document this build makes is byte-identical
to the one the previous build made — which is the first gate, `cmp` against the
exe from before the change rather than a re-reading of our own output.

### How it was checked

`tests/spells/native_open.sh`, whose right-hand side is the independent decoder
`tests/spells/lodgen_native_decode.py`, on chunk (-20,24) dim 4 of the look bake:

  * **no sheets, nothing changes** -- a height document from this exe is byte for
    byte the one from the exe before the change, three different ways of having
    no sheets; `lodl_open.sh` keeps its 23 checks at 0 failures; and the same
    one-chunk BAKE from both exes produces byte-identical `.BTO`, `.BTR`,
    manifest, `.lodi` and `.lodo`.
  * **the placements are the file's own** -- the manifest's 676 rows inside the
    chunk, the `.lodi` chunk table's 676 and the viewer's own census of 676 all
    agree, with the worst world position off by 0.0046 units against a floor of
    1; an empty region yields 0 and the harness sees 0.
  * **the lit terrain is the sheets** -- see the numbers above; an empty sheets
    directory renders the data view byte for byte, and the lit render is not the
    data view (mean difference 89.427).
  * **the scene against the chunk's own `.BTO`** -- coverage IoU 0.8179 against a
    pre-registered bar of 0.95: **this one is RED**. The refuter fires at 0.0001
    on a different chunk, so the test discriminates; the native scene is simply
    larger, containing 98.7 percent of the `.BTO`'s pixels plus more. Cause
    measured: the native view samples each base's own full-resolution LOD
    material while the `.BTO` samples the bake's downsampled object atlas, so
    alpha-tested canopies keep texels the atlas lost. The bar was not moved.

Whole run: 17 checks, 1 failure, 0 skipped.
