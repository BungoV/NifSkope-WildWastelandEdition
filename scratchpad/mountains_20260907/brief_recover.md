# Lane RECOVER — give the out-of-bounds cells their materials back

Pure Python. **You cannot run `release/NifSkope.exe`** — the sandbox refuses it
from a B session. Do not retry it. If you need something only the exe can
produce, write the exact command into your report under NEEDS-OVERSEER and carry
on. Never build, never launch a GUI, never edit a file inside the repo. Writes go
only to `C:\Users\bungo\AppData\Local\Temp\claude\laneb\`.

Write `report_recover.md` **incrementally**, and write your data products to disk
as you go. A lane that dies with nothing on disk delivered nothing — that already
happened once tonight.

You may spawn your own subagents; they bill to this account, which is the point.

---

## THE GOAL, and why it is worth doing

Fallout 4's Commonwealth is 192x192 cells. **Only 3,955 of 36,864 carry any
painted material** — 10.7%, a blob spanning x -36..32, y -41..32. Every other
cell has heights but no BTXT base and no ATXT layers. Bethesda nevertheless
shipped a real, detailed LOD diffuse for all of them, so that data exists as
pixels but not as materials.

Consequence: anyone who regenerates terrain LOD gets nothing to sample out there.
Our own generator writes flat grey (measured: 28 of 36 level-32 tiles at
luminance standard deviation **0.00**).

bungo's plan, and the end product: **a .esp mod** that assigns real LTEX
materials to those cells, recovered by matching their vanilla LOD colour against
the painted area where we know both colour and material. Then anyone can rebake
correct far LOD — and, the reason this matters most, **anyone running a landscape
texture replacer gets the far terrain rebaked from their new textures too**,
instead of frozen in vanilla's pixels.

That last point sets your accuracy target: **you are matching material identity,
not colour.** Getting the colour right with the wrong LTEX means a replacer moves
the distant terrain the wrong way. Judge yourself on materials.

---

## SETTLED FACTS — measured, do not re-derive, but say so if one looks wrong

All files below are in `C:\Users\bungo\AppData\Local\Temp\claude\laneb\`.

**`layers.txt`** — every cell, from the MASTER (not from any regenerated bake;
defining the painted area from our own output would be circular). One line a cell:

    L cx cy land vclr base=q0,q1,q2,q3 layers=n0,n1,n2,n3 ltex=<distinct> blend=<q0>|<q1>|<q2>|<q3>

`base` is the BTXT LTEX form id per quadrant, 00000000 = none. `layers` is the
ATXT count per quadrant. `blend` is the real payload: per quadrant, each layer as
`ltexid:meanOpacity` separated by `;`, with the BTXT base listed first at 1.000,
or `-` for an empty quadrant. Mean opacity is over that quadrant's 17x17 alpha
grid. Quadrant order is **0 BL, 1 BR, 2 TL, 3 TR** (`docs/LODGEN_ESM_LAYOUTS.md`).
Example real line:

    L -26 -41 1 0 base=00000000,00000000,00000000,00000000 layers=0,0,1,1 ltex=000ecbdf blend=-|-|000ecbdf:0.287|000ecbdf:0.297

Totals: 36,864 cells with LAND, 3,955 painted (3,517 with a base, 3,937 with
layers), 2,362 with VCLR, **100 distinct LTEX** worldspace-wide.

**`land.bin`** — every cell's 33x33 VHGT grid. Layout: int32 minX, minY, cellsX,
cellsY; then one uint8 presence flag per cell row-major from the south-west; then
per cell 33*33 int16 heights in units of 8, **row 0 south, column 0 west**.
Here: 192x192 from (-96,-96).

**Vanilla LOD textures**: `E:\Tools\Fallout 4\DataUnpacked\Data\Textures\Terrain\Commonwealth\`
`Commonwealth.<level>.<x>.<y>.DDS` plus `_msn`. Level 4 has 2,304 tiles covering
cells -96..95, each 512x512 BC3 with 10 mips. A level-4 tile spans 4x4 cells, so
one cell is 128x128 texels at mip 0.

**TILE ORIENTATION — I measured this, it is load-bearing, and two weaker tests
failed to resolve it before this one did.** Tile `Commonwealth.4.X.Y` covers cells
X..X+3 and Y..Y+3. **Texture row 0 is the NORTH edge and column 0 is the WEST
edge**, so cell `(X+dx, Y+dy)` is sub-block column `dx`, row `3-dy`. Decided by
predicting each cell's surface normal from VHGT and matching it against the
`_msn`: mean error 0.0719 for this orientation against 0.2401, 0.2405 and 0.2740
for the other three, over 6,080 cells.

**`_msn` channel convention**: R = X east, G = up, B = Y north, each 0.5+0.5
encoded. (Model-space, up in GREEN, not the usual up-in-blue.)

**The vanilla diffuse is plain albedo.** No baked sun — the best-fitting light
direction per tile is weak, |r| <= 0.50, and its azimuth scatters 75/315/180/315/
30/330 degrees across tiles, where a real baked sun would agree. No baked AO. The
alpha channel is a constant 255 and carries nothing. So the colour you match on is
material colour, which is what makes this whole approach sound.

**VCLR multiplies into the ground and its neutral is 255**, not 128. 2,362 cells
carry it and they are essentially all painted ones. **Divide it out before
matching**, or you fold the artist's hand-painted dirt shading into the material
choice. Check whether unpainted cells carry VCLR at all before assuming.

**The far palette is small.** Sampling 48 far tiles against 48 painted ones at
6-bit quantisation: **99.97% of out-of-bounds texels land on a colour bin that
already occurs in the painted area**, 100% within one bin (4/255 per channel).
The far palette is 127 bins against 635 painted, and concentrates hard — 3 bins
are 50% of all far texels, 9 bins are 80%, 17 bins are 95%. The dominant ones are
one family of warm dark browns, luminance 60-73, saturation 0.11-0.22.

**`dds.py`** — a verified BC1/BC3/BC5 decoder. `DDS(path)` exposes `width`,
`height`, `mips`, `decode(mip) -> (w, h, rgba_bytes)`. It self-checks: file size
equals header plus the summed mip chain on every terrain texture tested. Use it;
no PIL plugin opens these.

---

## WHAT TO BUILD

### 1. The training set
For every **painted** cell, pair its vanilla LOD diffuse colour with its known
blend. Work per quadrant, since `blend` is per quadrant — a cell is 128x128 texels
at level 4, a quadrant 64x64. Divide VCLR out of the colour first. Save this table
to disk before going further.

### 2. The model, and be honest about its ambiguity
Learn colour -> blend. Start simple and measure before adding machinery: a nearest
neighbour in colour over the training set, returning that sample's blend, is the
baseline and may well be enough given the palette is 17 bins.

**The question I could not answer and you must:** is the mapping unique? A colour
bin in the painted area may correspond to several unrelated blends. Measure it —
for each of the dominant far colour bins, how many distinct blends does that
colour correspond to among painted samples, and how much do they disagree? If one
colour maps to three materials, say so and say which you picked and why. A
confidence number per assignment is worth more than a clean-looking table.

### 3. Holdout validation — this is what makes it trustworthy
Hold out a share of the **painted** cells. Recover them using only their vanilla
diffuse colour, then score against the blend you actually have. Report:
* top-1 accuracy on the dominant LTEX per quadrant;
* how often the recovered blend's predicted colour lands within a few units of
  the true colour;
* and the failure cases, by LTEX, with a guess at why.
Hold out spatially contiguous regions, not random cells — random holdout leaks,
because a cell's neighbour is nearly itself.

### 4. Apply it
Produce assignments for all **32,909 unpainted cells**, per quadrant. Write
`recovered.txt` in a format the .esp writer can consume directly — one line a
cell, quadrant by quadrant, LTEX form ids with weights, plus your confidence:

    R cx cy q0=<ltex:w;...> q1=... q2=... q3=... conf=<0..1>

Start with a **BTXT-only** recovery: one flat base texture per quadrant, no ATXT
layers. That is 32 bytes of new data a cell and it is probably enough for terrain
this far away. If your holdout says flat-per-quadrant is materially worse than a
blend, say so with the number, and produce the blended version as a second file.

### 5. A picture
Write a PNG of the recovered material map over the whole 192x192 extent, one
colour per dominant LTEX, painted cells drawn in their true material for
comparison. There is a working PNG writer in `mosaic.py` in this folder.

---

## OUTPUT

`report_recover.md`:
1. **Does it work** — three sentences and the holdout number.
2. **The ambiguity finding** — is colour -> material unique, and where is it not.
3. **The recovered palette** — which LTEX you assigned out of bounds, how much
   area each covers, and what each one actually is (resolve the form id to its
   texture path; `src/esmdata.cpp` shows how LTEX -> TNAM -> TXST resolves).
4. **Method and measurements**, each with the command that produced it.
5. **REFUTED** — what you tried that did not survive. If empty, you did not check.
6. **NEEDS-OVERSEER** — exact commands you want me to run.

Files: `training.txt`, `recovered.txt`, `recovered_map.png`, plus your scripts.

Be blunt about what does not work. A recovery that is wrong in a plausible-looking
way is worse than none, because it will ship to other people as a mod.
