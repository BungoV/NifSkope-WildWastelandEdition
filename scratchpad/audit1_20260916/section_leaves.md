## L. bungo's row, 2026-09-17: "thick leaves on LOD textures. Thicker than on the vanilla bakes"

Not fixed. No ledger entry claims it, and nothing in this lane fixes it, because
what makes our leaves thicker is a **default**, not a defect: since the native
library landed, a default FO4CS bake builds every object's rung 0 from the
**near model** and therefore from the **near leaf texture**, while vanilla's
object LOD draws the MNAM LOD model and its dedicated, thinned LOD texture.
Everything below is measured on this exe and on the shipped game files; the last
sub-section names the one-switch way back and what it would cost.

Every number is the **alpha-test coverage**: the fraction of a surface's texels
that pass the alpha test the consumer actually runs. A leaf is a cut-out, so
that fraction IS its thickness on screen. The readers are new and read only the
compressed bytes -- `scratchpad/audit1_20260916/leaf_alpha.py` (BC1
punch-through: a block stored `c0 <= c1` makes index 3 transparent, every other
index opaque, and a `c0 > c1` block is wholly opaque; BC3: the eight-bit ramp),
`leaf_chain.py` (the writer's own 2x2 box filter), `find_tile.py` (normalised
cross-correlation, to find a source's tile inside a baked atlas),
`leaf_pairs.py`, `leaf_table.py`, `dds_png.py`, `leaf_picture.py`.

### L.1 What the threshold is, and it is the one bungo already had fixed

`src/lodgen.cpp:2161` throws the source's own alpha threshold away and forces
**128** with a comment naming this same complaint ("near-tree materials test at
65-80, which at LOD distance passes far more canopy texels than vanilla's chunks
do ... bungo diagnosed the denser-canopy difference as exactly this cutoff"), and
`src/nativeemit.cpp:1337` carries that byte into the `.lodo` material row.
MEASURED on this lane's Sanctuary bake: every one of the twenty foliage material
rows reads `a=128`, and 137 of 769 distinct material strings are alpha-tested.
So that half is intact and is not the mechanism.

### L.2 The mechanism, proven by running the exe both ways

A default FO4CS bake of Sanctuary (region -20 24 .. -9 35, dim 4) against the
same bake with `--library mnam`:

| | `.lodo` bytes | triangles | materials | foliage materials named |
|---|---|---|---|---|
| default (`--library near`) | 225,399,755 | 7,572,082 | 787 | 8 `Materials\LOD\Trees\*` **and 12 near ones**: `Landscape\Trees\MapleAtlas01/02`, `MapleAtlas02_Tree`, `TreeElmAtlas`, `TreeSMarsh`, `TreesGlowingSea`, `BlastedForestBurntTreeAtlas`, `BlastedForestDestroyedTreeAtlas`, ... |
| `--library mnam` | 8,764,388 | 226,863 | 136 | 8 `Materials\LOD\Trees\*` and **no near one** |

The near rows come with the near models, measured off the cluster table: 2,990
triangles of `MapleAtlas02.BGSM` on `Landscape\Trees\TreeHero01.nif`, 1,915 of
`BlastedForestBurntTreeAtlas.BGSM` on
`Landscape\Trees\BlastedForestBurntTreeUpright01.nif`, 1,437 of
`TreeElmAtlas.BGSM` on `Landscape\Trees\TreeElmFree01.nif`, and so on -- full
near `.nif` files, not `LOD\` ones. The switch is `src/nativeemit.cpp:1207-1214`:
`--library near` (the default) is `[ near MODL, MNAM 0, MNAM 1, MNAM 2 ]` and
pushes vanilla's LOD slots one rung down.

That the placements themselves are vanilla's dim-4 LOD placements is measured
too: the `.lodi` header's `slotInstances` reads **[3526, 0, 0, 0]** -- every
placement came from MNAM slot 0, the slot vanilla draws at this ring. The
library is what changed under them, not the placement set.

### L.3 The numbers: the near texture against the LOD texture vanilla uses

Alpha-test coverage at 128, per mip, on six foliage pairs. "near" is the texture
a default bake's rung 0 names; "LOD" is the texture vanilla's object LOD (and our
own rungs 1-3) names. Both columns are the file's OWN stored mip chain.

| near texture (ours, rung 0) | mip 0 | mip 3 | mip 5 | LOD texture (vanilla) | mip 0 | mip 3 | mip 5 | mip-0 ratio |
|---|---|---|---|---|---|---|---|---|
| MapleAtlas01_d 4096x2048 | 0.7175 | 0.7704 | 0.8396 | MaplePostWarSet01LODlv2_d 256 | 0.1230 | 0.0977 | 0.0312 | **x5.83** |
| MapleAtlas02_d 4096x2048 | 0.3780 | 0.3989 | 0.4403 | MaplePostWarSet02LODlv2_d 256 | 0.1003 | 0.0732 | 0.0000 | **x3.77** |
| ElmAtlas_D 4096x2048 | 0.5525 | 0.5762 | 0.6117 | ElmSet02LODlv2_d 256 | 0.1528 | 0.1016 | 0.0625 | **x3.62** |
| ElmPreWarAtlas_d 4096x2048 | 0.6198 | 0.6339 | 0.6714 | ElmSet01LODlv2_d 256 | 0.1251 | 0.0791 | 0.0000 | **x4.95** |
| BlastedForestBurntTreeAtlas_d 4096x2048 | 0.5013 | 0.5006 | 0.4966 | BlastedForestSet01LODlv2_d 256 | 0.1946 | 0.1885 | 0.0938 | **x2.58** |
| BlastedForestDestroyedTreeAtlas_d 4096x2048 | 0.3966 | 0.3964 | 0.3920 | BlastedForestSet01LODlv2_d 256 | 0.1946 | 0.1885 | 0.0938 | **x2.04** |

Two readings, and the second is the one that shows on screen:

* at every mip the near texture passes **2.0 to 5.8 times** as many texels as the
  LOD texture the same tree has beside it;
* the two chains go OPPOSITE WAYS. The LOD texture thins with distance (Maple
  0.1230 -> 0.0312 by mip 5, gone by mip 6). The near texture **fattens** (Maple
  0.7175 -> 0.8396 at mip 5 and 0.8828 at mip 7 in its own chain, 0.9043 under
  ours). A leaf card whose texels each cover a big footprint stops being a
  cut-out and fills in **solid**. The branch-card region of `MapleAtlas01_d`
  alone (the 512x512 cell at 0,0) reads 0.4681 at mip 0 and 0.4912 at mip 4.

The picture is `images/leaf_sidebyside.png`: the LOD texture, vanilla's baked
tile, and the near texture at two mips, all thresholded at 128, with each panel's
coverage on it.

### L.4 What is NOT the mechanism (each refuted with its own numbers)

* **Our mip filter.** `lodgen.cpp:4690-4705` (arrays) and `:4549` (atlas) average
  alpha `(acc[3]+2)>>2`. Run against the source's own stored chain on the same
  files, ours tracks it: MapleAtlas01 mip 7 ours 0.9043 vs source 0.8828 (+2.4%),
  ElmAtlas mip 7 ours 0.5156 vs source 0.6484 (**-20%**, ours thinner). It is not
  a fattener, and a **default bake writes no atlas and no arrays at all** -- the
  Sanctuary FO4CS bake's nineteen files are `.lodo`, `.lodi`, `.lodj`, `.lodb`
  and the manifests, so this chain is not even in play unless `--arrays` or
  `--atlas` is spelled.
* **The colour dilation** (`lodgen.cpp:12269`). It writes RGB only --
  `sheet[i] = (sheet[i] & 0xFF000000U) | ...` keeps the alpha byte -- so it
  cannot move a silhouette.
* **Our atlas being the thick one.** Where the stock-target atlas does hold the
  same LOD tile, ours is the THIN one. Vanilla's `Commonwealth.Objects.DDS` holds
  these LOD textures 1:1 at 256x256 on a 256 grid (found by cross-correlation:
  MaplePostWarSet01LODlv2 at 2560,1024 ncc 0.819; ElmSet02LODlv2 at 2048,1024 ncc
  0.703; ElmSet01LODlv2 at 3840,768 ncc 0.613; MapleTrunksLOD at 2560,1280 ncc
  0.980; ElmTrunksLOD at 2304,1024 ncc 0.981; BlastedForestTrunksLOD at 3840,512
  ncc 0.984). Vanilla's BC1 re-encode FATTENS the tile against its own source --
  Maple 0.1230 -> 0.1258 at mip 0 and 0.0820 -> 0.2031 at mip 4 -- so ours reads
  **x0.98 of vanilla at mip 0 and x0.40 at mip 4** on that tile, x0.83 -> x0.50
  on ElmSet02, x0.93 -> x0.42 on ElmSet01. Trunk tiles are solid on both sides
  (1.0000 at every mip), which is the floor that says the comparison can move.
* **The impostor card coverage floor** (16/255, WW_CHANGES ~5685). Cards need
  `--impostors <tree>`, which a default bake does not pass, and this bake wrote
  none.

### L.5 The smallest fix, and why this lane did not make it

The way back already exists and is one switch: **`--library mnam`**, which builds
the ladder from vanilla's own MNAM slots. Measured above: it removes every near
foliage material, and it takes the Sanctuary `.lodo` from 225.4 MB to 8.8 MB.
Making it the DEFAULT is a default change and a design ruling, which this lane is
forbidden. It is bungo's call, and it is not free: the near model at rung 0 is
the whole point of the library (it is what puts real geometry where vanilla has
billboards), so `mnam` trades the thick canopy back for vanilla's silhouette
everywhere, not just on trees.

Three narrower options, in rising cost, for whoever takes the ruling:

1. **Per-family**: keep `near` for everything except bases whose material carries
   `LODO_MAT_TREE` (`src/lodofile.h:184`, already set -- measured `f=4` on all
   twenty foliage rows), which take their MNAM slot instead. Small, and gateable:
   after such a bake NO material row may be both TREE-flagged and named under
   `Landscape\`; the refuter is that the same assertion fails on today's bake,
   where twelve such rows exist.
2. **Texture substitution**: keep the near geometry but point a TREE material at
   the base's MNAM-0 LOD texture. Cheap on paper, wrong in fact -- the near
   model's UVs address the near atlas, and the LOD texture has a different
   layout. Refuted; do not do this.
3. **Coverage-preserving mips for the near leaf texture** once `--arrays` is in
   play (rescale each level's alpha so the pass fraction matches mip 0). It would
   hold Maple at 0.7175 instead of 0.9043 -- worth about a fifth of the fattening
   -- and WW_CHANGES ~5685 records the same law REFUSED on the impostor sheets
   (worst silhouette -9.1%, a solid rectangle fattened by a whole texel). It does
   nothing for a default bake, which writes no arrays.

**The gate this row is owed either way** (a guard on the half that IS fixed, and
it does not need a ruling): every TREE-flagged material row in a `.lodo` must
carry `alphaThreshold == 128`. It passes today on both bakes above; the refuter
is one doctored byte in the material table, which turns it red.
