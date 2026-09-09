# Lane MOUNTAINS — why does rebaked Fallout 4 terrain LOD go dark?

Read-only analysis. **Never build. Never launch a GUI. Never edit a file in the
repo.** Your only writes go to `C:\Users\bungo\AppData\Local\Temp\claude\laneb\`.

Write your report to
`C:\Users\bungo\AppData\Local\Temp\claude\laneb\report_mountains.md`
**incrementally, as you go** — append each lens's findings the moment you have
them. A lane that dies with nothing on disk delivered nothing.

You are a full Claude Code session and may spawn your own subagents; they are
billed to this account, which is the point of running here. Fan out across the four
lenses below in parallel, then run an adversarial refutation pass. Do not accept a
finding you have not personally reproduced.

---

## THE QUESTION

bungo asked:

> "In Fallout 4 the mountains outside the playable area have actual colour on them.
> But when rebaked with tools like DynDOLOD and so forth, they go a dark colour
> instead, why?"

He supplied two screenshots from the same viewpoint near Sanctuary Hills, looking
north over the Concord water tower. In the FIRST the distant range is pale, warm
and detailed, with light rock or snow on the peaks and clear tonal variation across
the slopes. In the SECOND, after a rebake, the same range is a flat dark blue-grey
silhouette: caps gone, warmth gone, almost no tonal variation. Nearer terrain and
objects are broadly similar between the two and some object LOD is actually BETTER
in the second, so this is specific to distant terrain, not a global exposure change.

**The change is not only DARKER. It is also DESATURATED and FLATTER.** A good
answer explains all three, or states which of the three it does not explain.

MEASURE. Do not recall folklore. Anything you cannot ground in a measurement or a
file citation must be labelled UNVERIFIED.

---

## RESOURCES

    Vanilla unpacked data:  E:\Tools\Fallout 4\DataUnpacked\Data
      Terrain LOD textures: ...\Textures\Terrain\Commonwealth\
        Commonwealth.<lodLevel>.<x>.<y>.DDS  plus a matching _msn.DDS normal map
      Terrain LOD meshes:   ...\Meshes\Terrain\Commonwealth\*.BTR
      Object LOD:           ...\Meshes\Terrain\Commonwealth\Objects\*.BTO
      Landscape textures:   ...\Textures\Landscape\
    Master ESM:             X:\Programs\Steam\steamapps\common\Fallout 4\Data\Fallout4.esm
    The repo:               E:\Projects\NifskopeWildWastelandEdition

Read-only CLI you MAY run (not a build, not a GUI), from the repo root:

    ./release/NifSkope.exe -no-gui lodgen <esm> --worldspace 3C --dump-land <out>
    ./release/NifSkope.exe -no-gui lodgen --dump-geometry <file.bto|btr>
    ./release/NifSkope.exe -no-gui lodgen --dump-shapes   <file.bto|btr>

`--dump-land` layout is documented at `src/nifcli.cpp:2771`: int32 minX, minY,
cellsX, cellsY; then one uint8 presence flag per cell row-major from the
south-west; then per cell 33x33 int16 heights in units of 8, row 0 south, column 0
west, absent cells zero-filled.

The repo's own measured understanding, reliable, read it:

    docs\LODGEN_ESM_LAYOUTS.md, docs\LODGEN_PARITY.md, docs\LODGEN_BTD_FORMAT.md
    src\esmdata.cpp    (LAND parsing: VHGT ~line 340, VCLR ~line 351)
    src\lodgen.cpp     (terrain texture bake; the VCLR multiply ~line 5164;
                        LAND_SHADER_FLAGS1/2 ~line 59)

**Settled, do not re-derive:**
* VHGT: height = value*8 accumulated along rows, 33x33, SW origin.
* VCLR: 33x33x3 bytes, hand-painted vertex colour, **neutral is 255**, and the
  landscape shader MULTIPLIES it into the ground. Vanilla's LOD bakes inherit it.
  A tool treating 128 as neutral would halve brightness; one treating absent VCLR
  as 0 would black the cell out.

DDS: no PIL plugin opens these. Parse the 128-byte header yourself (plus the
20-byte DX10 extension when fourCC is `DX10`) and decode BC1/BC3 blocks by hand.
Harnesses in `tests\spells\` already do this, e.g. `lodgen_card_arrays.sh`.
Python is available.

---

## ALREADY MEASURED BY THE OVERSEER — do not redo, but DO challenge

A helper is waiting for you at `C:\Users\bungo\AppData\Local\Temp\claude\laneb\lodmean.py`.
It takes the mean colour of a BC3 texture from its 4x4 mip (one block, so the two
RGB565 endpoints averaged are the whole-texture mean to within encoder error).
**Sanity-check it before trusting it** — it is a shortcut, and a coarse one.

1. Level-4 Commonwealth terrain LOD: 2304 textures, 512x512 DXT5, 10 mips,
   x range -96..92, y range -96..92.
2. Mean colour at the 4x4 mip, vanilla shipped diffuse:

        Commonwealth.4.-20.24    lum 80.9  sat 0.192     (near Sanctuary, playable)
        Commonwealth.4.-20.40    lum 81.4  sat 0.272
        Commonwealth.4.-20.60    lum 82.0  sat 0.183     (far, mountains)
        Commonwealth.4.-20.80    lum 70.2  sat 0.176     (far)
        Commonwealth.4.0.0       lum 74.5  sat 0.155     (playable)
        Commonwealth.4.-60.60    lum 82.0  sat 0.183     (far)

   **So vanilla's far LOD textures are NOT brighter or more saturated than the
   playable ones.** If that holds, the diffuse texture is not where the difference
   lives, and the answer is in shading, normals, mesh, or fog. Verify this before
   building on it — it rests on a 4x4-mip shortcut.
3. Far cells are NOT a uniform default fill: md5 of the level-4 diffuse differs for
   every cell sampled at (-20,40), (-20,48), (-20,56), (-20,60), (-20,80),
   (-60,60), (40,-60), (92,92), (-96,-96).
4. `_msn` mean for `Commonwealth.4.-20.24_msn.DDS` is rgb (114.5, 242.0, 106.5),
   i.e. X about -0.10, Y about +0.90, Z about -0.16. **Up appears to be GREEN.**
   If FO4 terrain LOD normals are model-space with up in green, a tool writing the
   usual up-in-blue tangent-space convention would light the terrain wrongly and
   the symptom would be exactly dark, flat and desaturated. **This is the
   overseer's leading hypothesis and it is only one sample — test it properly**
   across many cells and both far and near, and work out the true channel
   convention from the repo's terrain code rather than from one mean.

---

## THE FOUR LENSES

### 1. Corpus — what is actually in vanilla's shipped LOD, and where
Map brightness and saturation over the whole worldspace at level 4. Is there a
boundary between the playable area and the outside? Report numbers on both sides.
Check the `_msn` maps out there: real normals or flat? Do far cells have LOD at
every level (4, 8, 16, 32) or only the coarse ones — a cell present only at 16/32
is drawn from a much coarser texture and would look flatter. Decode properly, not
just at the 4x4 mip, for at least a few cells, and say whether the shortcut in
item 2 above held.

### 2. Source — does the data for those cells even exist?
Use `--dump-land` to establish which cells have a LAND record with VHGT, and
compare against the set of cells with a level-4 LOD texture. Is there a region with
textures but no LAND? For the far mountain cells: do the LAND records carry VHGT,
VCLR, a BTXT base texture, ATXT/VTXT layers? Extend a scratch parser if the CLI
does not report layer assignments. If far cells have only a base texture, what is
it, and what is that texture's own mean colour against vanilla's shipped LOD
texture for the same cell? A large gap there would be the whole answer.

### 3. Shading — maybe the texture is fine and the lighting changed
Do vanilla's far `.BTR` meshes carry vertex colours, and what values? Terrain LOD
vertex colour multiplies into the picture, so a change there is a direct brightness
change. Compare `_msn` between LOD levels — if a rebake derives normals from a
coarser heightmap the mountains lose large-scale shading and read flat. Which
dominates at distance, the mesh normal or the `_msn`? What shader flags and texture
set do vanilla `.BTR`s carry versus what `src/lodgen.cpp` writes (`LAND_SHADER_FLAGS1/2`
near line 59)? And atmospheric perspective: shot one is hazier, shot two crisper and
darker. FO4 fades distant terrain toward a fog colour, so if a rebake changed the
mesh bounds or the LOD level assignment, the same mountains sit at a different point
on the fog curve. Say clearly if that is unverifiable from files alone.

### 4. Tools — which knob causes this, and what should he set
Search this machine for any xLODGen or DynDOLOD install, config, logs or OUTPUT. If
output exists, measure it exactly as lens 1 measures vanilla and report the
difference directly — that is the decisive comparison and it beats everything else
in this brief. Otherwise: explain what each relevant xLODGen control does to output
colour (LOD level list, texture size, quality, brightness/contrast/gamma sliders,
vertex colours, "use existing normal maps", generate-from-landscape-textures versus
reuse-vanilla) and which default differs from what Bethesda shipped. Then test the
widely-repeated claim that Bethesda's Commonwealth terrain LOD textures are not
reproducible by sampling the landscape diffuse textures alone: take one far cell,
work out from its LAND layers which landscape textures cover it, compute the
weighted mean of those textures' own colours, and compare with vanilla's shipped
LOD texture for that cell. Report both numbers.

**Label recalled tool knowledge as recalled. Do not present it as measured.**

---

## OUTPUT

`report_mountains.md`, structured as:

1. **THE ANSWER** — four sentences, in plain language, for someone who wants to fix
   his LOD and not read a paper. Say which of darker / desaturated / flatter each
   cause accounts for.
2. **WHAT TO CHECK OR CHANGE** — a short ordered list, most likely cause first.
3. **THE MEASUREMENTS** — the numbers, with the command or script that produced each.
4. **REFUTED** — what a lens claimed and you killed, and why. If this section is
   empty the refutation pass did not happen.
5. **UNVERIFIED** — what you could not check on this machine, and why.

Guard against these specifically: a mean computed over a whole file including its
mip chain or its header bytes; BC blocks decoded wrongly (sanity-check that a
terrain diffuse comes out earthy, not neon); confusing the LOD LEVEL number with a
mip level, or the file's x/y with cell coordinates — establish the convention from
the repo first; and an explanation that accounts for darker while silently ignoring
desaturated and flatter yet claims to be the whole answer.
