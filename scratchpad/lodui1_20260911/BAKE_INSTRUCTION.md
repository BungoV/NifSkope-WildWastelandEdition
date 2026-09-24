# Baking the whole Commonwealth from the LOD Generation panel

One page. Written by lane LODUI1 on 2026-09-11 against
`release/NifSkope.exe` of that day. Restart NifSkope first if your window is
older than **13:13:51** — the panel below only exists in that build.

---

## 1. Open it

**Workspaces > LOD Generation.** The dock is on the right; its strip has two
tabs, **LOD** and **Water**. Stay on LOD.

## 2. Set the six things that matter

| row | what to put |
|---|---|
| **Source** | `Specified` if you point at plugins yourself, `Mod Organizer 2` if you launched NifSkope from MO2 (then the plugin list is MO2's and is read-only). |
| **Worldspace** | `Commonwealth (0000003c)` |
| **Output mod** | a NEW folder under your mods folder, e.g. `E:\Projects\Fallout 4 Mods\mods\Generated LOD`. It IS a Data folder; enable it in MO2 afterwards. **Never your installed `Data\Terrain`.** |
| **Target** | **FO4 Community Shaders** |
| **Chunk range** | press **Whole worldspace** (it fills -96..95 both ways), and leave **Chunk size** on `all rings (4+8+16+32)`. |
| **Impostor cards** | the directory `tools/bake_impostor_cards.sh` wrote, or leave it empty for no cards. **Trees only** stays ON. |

Everything else can stay at its default.

## 3. What the FO4CS target offers, and what it writes

Under FO4 Community Shaders the panel offers **five outputs**, and the legacy
rows (`.btr` terrain chunks, `.bto` object chunks, the terrain-texture bake,
the object atlas, chunk textures from the pyramid) are not shown at all:

* **Landscape file (.lodl)** — `Terrain\Commonwealth.lodl`
* **Shadow heightmap** — `Textures\Terrain\Commonwealth\Commonwealth.HeightMap.dds`
* **Terrain virtual texture (.lodt)** — `Terrain\Commonwealth.VT.*.lodt` plus its `.lodm` index
* **Native object files (.lodo/.lodi)** — `Terrain\Commonwealth.lodo` and `Terrain\Commonwealth.lodi`
* the **`.lodm` material sidecars** that ride with the texture arrays and the card arrays

**One thing it also writes that is not on that list:** the object chunk files
`meshes\terrain\Commonwealth\*.BTO`. They are not an offered choice any more —
they are what the `.lodo`/`.lodi` pair, the texture arrays, the card arrays, the
shape merge and the far-ring cut are all built from, and they are what FO4CS's
Improved LOD module reads today. The summary line says so before you press
anything. **If you want the FO4CS target to stop writing them, say so and it
becomes a one-line change plus a lane for whatever then feeds the arrays.**

## 4. Press Generate, and read the line under the summary

When the run ends the pinned bar prints, under the "Will write to ..."
sentence:

```
stage times: landscape N s, meshes N s, textures N s, impostors N s
```

* **landscape** — the `.lodl` and the shadow heightmap
* **meshes** — the terrain and object chunk builders, the shape merge, the
  far-ring cut, and the `.lodo`/`.lodi` pair
* **textures** — the terrain pyramid, the chunk sheets, the object texture
  arrays and the atlas
* **impostors** — the card arrays

A stage that did not run reads exactly `0.0 s`.

## 5. How long, honestly

**These are measured on the ONE-CHUNK Sanctuary region `(-20,24)`, dim 4, and
the whole-Commonwealth figures below them are an EXTRAPOLATION, not a
measurement.** Nobody has run the full bake.

Measured, 2026-09-11, on the 13:13:51 exe:

| stage | Sanctuary, 1 chunk, dim 4 | notes |
|---|---|---|
| landscape | **1.1 s** | the shadow heightmap at 4096; the whole-worldspace `.lodl` is a separate, much larger job and is NOT in this number |
| meshes | **3.8 s** | includes the full `.lodo`/`.lodi` write, which walks the WHOLE worldspace census, not the one chunk |
| textures | **0.2 s** | the object texture arrays for one chunk |
| impostors | **0.0 s** | no card library was set |

**The extrapolation, and why it is soft.** The full bake is 3,060 chunks over
the four rings against this one, so the chunk part of *meshes* and the whole of
*textures* scale with that count, while the `.lodo`/`.lodi` write and the
heightmap are paid ONCE however big the range is. On those measurements the
honest range is **one to three hours** for the whole Commonwealth, dominated by
the terrain pyramid if you tick it (the panel's own estimate for the pyramid is
**about 1.59 GB over 12,276 tiles** and its bake time has never been measured).
Treat that as an order of magnitude, not a promise, and read the real numbers
off the result line when it finishes.

## 6. What to send back

1. **The result line** — the four stage times, verbatim.
2. **The progress bar's last message** — it carries the chunk count, the merge,
   the far-ring cut, the arrays and the native census in one sentence. Hover the
   result line and the whole message is the tooltip.
3. **The file sizes** of:
   * `Terrain\Commonwealth.lodl`
   * `Terrain\Commonwealth.lodo` and `Terrain\Commonwealth.lodi`
   * `Textures\Terrain\Commonwealth\Commonwealth.HeightMap*.dds`
   * the count and total size of `meshes\terrain\Commonwealth\*.BTO`
4. Anything the panel turned **red** in the summary line.

## 7. If you want the cards first

The card library is baked outside the panel:

```
CANDIDATES=trees OCT=8 TILE=512 bash tools/bake_impostor_cards.sh \
    "<Fallout4.esm>" -96 -96 95 95 "<card directory>"
```

`CANDIDATES=trees` is what the panel's **Trees only** row means when it is ON;
`CANDIDATES=missing` is what it means when it is off. `CANDIDATES=all` was
retired and refuses by name. `TILE=512` is the new resolution — at `OCT=8` that
is a sheet **4096 texels on a frame's long side** (a thin tree gets a narrower
frame from its own silhouette: the maple measured 2176 x 4096). Then point the
panel's **Impostor cards** row at that directory and set **Card frames** to
`8 x 8` and **Card resolution** to `512 px` so the run and the library agree —
the panel refuses in words if they do not.

---

## Appended 2026-09-11 by lane BAKEPERF1 — the measured numbers, and no speed-up yet

LODUI1's §5 above is unchanged; this is the update its estimate was waiting for.

**There is no speed-up to report.** bungo asked whether the bake could use the
whole machine. The machinery to do it now exists and is switched **off**,
because building NIF documents on worker threads faults in this tree
(`NifItem::deleteChildItems()` under `BaseModel::~BaseModel()`, five runs out of
five); serialising the parse makes it survive and then it is **1.8x slower**
than one thread, because parsing is the part that cannot overlap. The default
bake is byte for byte the bake §5 describes. Details:
`scratchpad/lane_bakeperf1_report.md` §3.5–§3.9.

**Real region bakes, measured on the 14:48:52 exe** (§5's numbers above were one
chunk; these are whole regions, everything on, `--arrays --merge --native`):

| region | chunks | landscape | meshes | textures | wall | peak memory |
|---|---|---|---|---|---|---|
| Sanctuary, cells −20 24 −9 35 | 9 | 0.0 s | ~8.5 s | ~6 s | **~17 s** | 1.75 GB |
| downtown Boston, cells 0 −12 19 7 | 25 | 0.0 s | ~47 s | ~61 s | **~113 s** | 3.75 GB |

Run-to-run spread on the same region and the same exe is about 30 percent, so
treat one decimal place as noise.

**What that says about the whole Commonwealth.** 3,060 chunks against these 25,
at roughly **4.5 s a chunk** on the Boston region (which is dense — Sanctuary is
about 1.9 s a chunk), gives a chunk-pass range of **1.6 to 3.8 hours**, plus the
`.lodl`, the shadow heightmap and the terrain pyramid, which are paid once and
are not in that number. That is the same order of magnitude §5 guessed at, now
with region measurements under it rather than one chunk.

**Memory for the full bake is still an OPEN QUESTION, deliberately.** The peak
went 1.75 GB at 9 chunks to 3.75 GB at 25. Two points do not determine that
curve, and extrapolating them straight to 3,060 chunks gives a number nobody
should act on. Before the full run, bake one ~100-chunk region and read the
`bake census:` line's peak — the run prints it now.

**Two new switches, neither of which you need to touch.** `--threads N` is the
general fan-out budget and defaults to the machine; **`--chunk-threads N`
defaults to 1 and should stay there** until the parser is fixed. The result line
in the panel now carries the census under the stage times: threads, chunk
threads, chunk jobs, chunk workers and the peak working set.

---

## Added 2026-09-11 19:3x by lane RESUME3 -- two things above are now out of date

**The tiling default changed, so every sheet you bake from now on differs from
the ones baked before today.** The landscape textures were tiled at 2,048 world
units a repeat; the engine's own repeat is **341.3333** (= 128 / 0.375, from
`fLandTextureTilingMult` 1.5 in Fallout4.exe 1.10.155), so the old sheets were
six times too coarse and printed a visible grain. Measured on chunk (-20,24):
local variance **76.22 -> 12.34** against vanilla's 19.81, and the mean colour
difference from vanilla **16.33 -> 14.33** of 255. You do not need to do
anything -- it is the default. `--land-tiling 2048` reproduces the old sheets
byte for byte if you ever want to compare.

**`--chunk-threads` is now SAFE and still should stay at 1.** The paragraph
above says to leave it at 1 "until the parser is fixed". The parser was never
the problem; the crash was the generator's own log line, and it is fixed.
`--chunk-threads 16` is now clean -- 20 consecutive runs on each of two regions,
every output file byte-identical to the one-at-a-time bake. **Leave it at 1
anyway, and here is why, measured:**

| region | `--chunk-threads 1` | `--chunk-threads 16` |
|---|---|---|
| Sanctuary, 9 chunks | **17.9 s**, 1.75 GB | 40.5 s, 5.80 GB |
| downtown Boston, 25 chunks | **128 s**, 3.76 GB | 156 s, **25.1 GB** |

Sixteen chunk workers is 2.3x slower on a small region, 1.2x slower on a big
one, and 25 GB of the machine's 31.8. The time goes backwards in the TEXTURE
stage (5.8 -> 31.9 s on Sanctuary), because each worker keeps its own texture
cache and they all re-read the same landscape diffuses.

`--chunk-threads 0` now means "as many as free memory holds, up to the core
count", and the result line says which limit decided it:
`chunk threads N bound by default|asked|cores|memory`. A run held back by memory
now says so instead of just looking slow.

**Expected time for the whole Commonwealth is unchanged from the estimate
above** (1.6 to 3.8 hours for the chunk pass), because that estimate was already
for the one-at-a-time bake, which is still the default and is still the
configuration to run it in. The tiling change costs nothing: the six bakes taken
today at both tiling values ran within noise of each other.

**And the memory question for the full bake is still open**, for the same reason
as before: two points do not determine the curve. The 100-chunk region has still
not been baked.

---

## Added 2026-09-11 23:5x by lane TILING3 -- the sheets now reuse vanilla's own

**Restart NifSkope if your window is older than 23:26:29.**

**The far-terrain colour and normal sheets changed, and the change is the
default.** You do not need to do anything; this is here so the output is not a
surprise.

* A terrain chunk that has a **shipped vanilla `_msn`** now gets **vanilla's
  `_msn`, byte for byte**. Our normal bake is skipped for that chunk. On the
  Commonwealth that is every chunk, because Bethesda ships a complete grid.
* A chunk whose cells carry **no land paint at all** -- the out-of-bounds ground
  around the playable map -- gets **vanilla's colour sheet, byte for byte**. That
  colour was baked outside the Creation Kit and cannot be rebuilt from the plugin,
  so the old behaviour was inventing ground, not reconstructing it.
* Every other chunk keeps our own colour composite and gains a small **crevice
  darkening** taken from vanilla's fine relief -- rills and drainage where vanilla
  has them.
* **The `_msn` files are now larger**, because Bethesda's are BC5 and ours were
  DXT1: 349,680 bytes instead of 174,888 per dim-4 chunk sheet. Budget for it on a
  whole-Commonwealth bake: 2,304 dim-4 sheets is about **400 MB** where it used to
  be about 200 MB.
* Nothing else moved: `_data`, the `.btr`/`.bto` files, the object manifest and
  every `.lodt`/`.lodm` are byte-identical to yesterday's.

**Where vanilla is read from.** `--vanilla-lod-root`, default
`E:\Tools\Fallout 4\DataUnpacked\Data`. It reads **loose files only** and never
the resource stack on purpose -- otherwise a second bake would read our own
installed output and "reuse vanilla" by copying its own previous run. **If your
unpacked vanilla Data lives somewhere else, pass that path**, or the reuse simply
will not happen and the bake will say so in its census.

**How to check it did what it says.** The run's `report` line now carries a census:

```
landDetail 1 vanillaRoot <path> msnCopied N msnOurs N colCopied N colOurs N
chunksLayered N chunksLayerless N chunksLayerlessNoVanilla N chunksShaded N landShade -3.242
```

* `msnOurs` large and `msnCopied` 0 means the vanilla root is wrong.
* `chunksLayerlessNoVanilla` above 0 means some out-of-bounds ground had no vanilla
  sheet to borrow and fell back to our composite. On the Commonwealth this has
  always read 0.

**To turn the whole thing off**, `--land-detail-source none` reproduces the
previous build's bytes exactly. `--land-detail-source vanilla-blend` is for
terrain you have reshaped: it puts vanilla's fine detail over YOUR normal instead
of replacing it. `--land-shade 0` keeps the sheet reuse and drops the crevice
darkening.

**The tiling repeat is still there, and there is now a switch for it that is
OFF.** `--land-sample stochastic` breaks the land textures' repeat with a smooth
deterministic warp and restores the grain with a mip bias. On the two test tiles
it took the repeat from 1.037 to **0.183** (vanilla reads 0.201) with the grain at
111 % of vanilla's on one tile, and 1.261 to 0.593 on the other -- so it fixes one
of the two outright and improves the other without clearing vanilla's own
threshold. Over seven sheets it passed **6 of 7**, which is why it is not the
default. It is deterministic, seamless across chunk borders and identical at any
`--chunk-threads` value. **Try it on a region and look at it; the call is yours.**

---

## Added 2026-09-12 05:0x by the director -- bungo's ruling on the road look

"--road-detail 1 is always on, do not ever use road detail 0, that looks terrible" (bungo, 2026-09-12 05:0x). Until the default flip is built (queued as INCR1 item 0), add **`--road-detail 1`** to every bake command in this file: it prints the road's own diffuse in the far sheet instead of one flat colour per material. The flat mode was ROADS2's default and is rejected. Nothing else in the command lines changes; ROADS3's `--road-opacity` stays at its default 1.0.

---

## Added 2026-09-12 06:5x by lane ROADS4 -- the ruling is BUILT, and one new question

**Drop the `--road-detail 1` you were told to add above: it is the default now.**
`release/NifSkope.exe` built 2026-09-12 06:31:05 (21,819,904 bytes) flips
`--road-detail` to 1.0. Typing it explicitly still works and changes nothing.

**A bake made before 06:31 on 2026-09-12 is a `--road-detail 0` bake** whatever
its folder is called, unless the command line said otherwise. If you are
comparing sheets across that line, that is the difference you are looking at.
`--road-detail 0` reproduces the old bakes byte for byte, every file, so the old
look is one switch away and needs no old exe.

**Known red, and it is expected, not a regression to chase.**
`tests/spells/lodgen_roads.sh` now reads 11 checks / 1 failure: the R5
road-presence metric scores 0.3078 against a bar of 0.3223. That bar was
calibrated when detail 0 was the default. Detail 1 is measurably further from
vanilla's road tone and is what bungo asked for; the bar is what needs
re-stating.

**New question, and you almost certainly want to leave it alone:
`--road-ground-paint 0..1`, default 1.0.** Fallout 4's road models contain
shapes materialled as landscape ground -- the verge and the junction fill --
and they win about a third of the road plane (36.1 % on Sanctuary's chunk,
24.9 % on the other test chunk). This switch fades them out. It was built to
test whether that is the fix for the hard edge you can see between the verge and
the asphalt, and **baking it says no**: the seam gets monotonically worse as the
value falls (a boundary gradient of 15.387 at 1.0, 33.352 at 0, against
vanilla's 5.362), because the verge darkens toward our own ground while the
asphalt does not move. Left at 1.0 it is the same bytes as before it existed.

**What the numbers say the real difference is**, for when you look at a sheet
and wonder: our asphalt sits at luminance **110.82** where vanilla's is
**93.54**, while our verge is already within 5.5 levels of vanilla's. That is
`--road-opacity`, and there is no value that suits both test chunks:
`--road-opacity 0.83` keeps the road-presence metric green and halves the seam
excess; `--road-opacity 0.326` matches vanilla's seam on both chunks and guts
the metric. **Nothing about opacity was changed and no value is recommended.**
The picture that prices the choice is
`scratchpad/roads4_20260912/images/road_ground_look.png`.

---

## 2026-09-12, lane LAND1: `--land-guide`, and why there is nothing to type

You asked, over the TILING3 warp: *"since we're reusing vanilla terain normals
and slope maps, might as well use them to guide this a bit"*. That is built, it
is `--land-guide`, and **no value is recommended, so nothing in this file's
command lines changes.**

The short version of what it is worth. The macro shape of the ground steers the
land-texture lookup; the shape is read from the HEIGHTMAP, not from vanilla's
`_msn`, and that was measured rather than assumed (at the scale that matters the
`_msn` disagrees with the heightmap by less than the heightmap disagrees with
itself one octave away, so it carries nothing extra). Five rules were built and
119 real bakes were scored. **The finding is that the HEX LATTICE is what breaks
the repeat and the guide rules are passengers on it**: on its own the best rule
passes the repeat law on 2 of 7 sheets, `--land-hex 256` alone passes 4 of 7, and
the winner on top of it passes 5 of 7.

**If you want to look at it**, the one arm worth a region bake is

```
--land-sample stochastic --land-guide aspecthex:1.0 --land-guide-scale 256
```

on top of whatever you already use, and the honest price is that the band-error
gate goes from 7 of 7 sheets to 5 of 7 — a rotated sampling frame moves energy
between the radial bands. `--land-guide aspecthex:0.5` keeps 6 of 7 of that for
almost all of the repeat gain, if the full-strength one looks smeared to you.

**Where it clearly earns its place is steep ground**, and the two pictures are
`scratchpad/land1_20260912/images/a_land_guide_slope.png` (the steepest sheet
measured) and `a_land_guide_flat.png` (the flattest). On the steep one it takes
the repeat to 0.152 where the 683-unit warp you called too strong only reaches
0.280, and it does it at a grain 30 % above vanilla's instead of the warp's
**82 %** — that over-sharpening is the thing you were looking at. On the flat one
no rule gets near vanilla, because with no slope there is nothing to steer by.

**Leaving it out is a real option and costs you nothing.** `--land-guide` defaults
to `off`, and `off` was proved to be the previous bake's bytes on all fourteen
test sheets, every file of every tile. A bake made without it is exactly the bake
you were making yesterday.

## `--incremental` -- rebaking after an edit without rebaking everything

New the same day, from lane LAND1's second half. Every region bake now drops a
small ledger beside its output (`<out-dir>/<Worldspace>.lodb`): for each chunk,
a fingerprint of everything that chunk was built from, and a fingerprint of
every file it produced. Nothing about your bakes changes -- the ledger is
written whether you ever use it or not, because the alternative is a feature
that only works if yesterday guessed you would want it today.

Then, after you edit a hill or move a building:

```
...the same command you baked with... --incremental <that same out-dir>
```

It compares the fingerprints, rebakes the chunks whose inputs moved plus every
chunk within one cell of one, and leaves the rest of your output tree exactly
where it is. It prints what it decided, every run:

```
incremental: 4 of 9 chunks dirty (1 inputs moved, 0 not in the ledger, 0 output lost, 3 by neighbour)
```

Three things worth knowing before you rely on it:

* **It refuses rather than guessing.** No ledger, a different region, different
  switches, or `--atlas`/`--arrays`/`--impostors` on the command line -- each
  one stops the run with a sentence saying which and what to do instead, having
  written nothing. Those three flags each build ONE thing out of the whole
  region, and there is no honest way to build them from a quarter of it.
* **It is the same bytes, not nearly the same bytes.** A dirty rebake was
  measured against a full bake of the same edited plugin, file by file, byte by
  byte, over two regions and four kinds of edit. If that ever stops being true
  it is a bug, not a tolerance.
* **`9 of 9 dirty` is a real answer.** It means the diff found nothing to skip
  -- usually because a switch changed. The run is correct; it just is not fast,
  and it says so rather than letting the clock imply it.

The contract, the dependency map and the full refusal list are in
`docs/LODGEN_LEDGER_FORMAT.md`.

## Added 2026-09-12 12:2x by lane GROUND1 -- two new switches, both off by default

Neither of these changes anything about a bake you do not ask for. With neither
token in the command line the whole tree comes out byte-identical to a bake made
with the 2026-09-12 09:32 build, ledger included -- that is gated, not assumed.

### `--terrain-object-ao` -- the far terrain takes shade from the objects on it

Today the far terrain shades itself from its own horizon only, so a building or
a rock formation sitting on it casts nothing onto the ground in the LOD. With
this on, the placed objects are gathered into a height field and the same
eight-direction horizon march runs against them; the two occlusions multiply.

```
--terrain-object-ao                      turn it on
--terrain-object-ao-strength 0.5         how hard, 0..4, default 0.5
--no-terrain-object-ao                   the explicit off
--dump-object-ao FILE                    write the term out for inspection
```

**0.5 is the default for a measured reason**: it is the largest sampled strength
at which no texel on the measured region clamps to solid black. At 1.0, 276,234
texels of 1,048,576 clamp. Turn it up if you want it heavier, but look at the
dark side of a hill before you go past 1.

**It reaches the sheets and not the mesh.** Chunk `.BTR` files come out
byte-identical with it on. Its reach is 1,458 world units, which is inside the
widening `--incremental` already does, so it does not make dirty rebakes larger.

**One refusal you will meet if you combine it with `--lodl`:** the two together
exit 2 and say so. The `.lodl` ring-0 AO plane is written by the same code that
serves `--refresh-ao`, whose byte-identity promise this lane was not allowed to
break, so the pyramid mask and ring 0 would disagree about object shade. Until
that is settled, bake the two separately or leave the object AO off for `.lodl`
runs.

### `--erosion` -- fluvial and geological detail the height grid cannot carry

bungo, over a vanilla/ours normal-map comparison: *"We lose all the fluvial,
erosion features and other topographical features"*. Our far terrain is rebuilt
from a 128-unit height grid while a bake texel is 32 units, so everything finer
than four texels -- which is where vanilla's channels and gullies live -- is
simply not in the input. This grows some of it back.

```
--erosion 1                  strength, default 0 = off
--erosion-iterations 4       feedback rounds, 1..8, default 4
--erosion-seed 1             moves the noise, default 1
--land-detail-source erosion optional: use this pass as THE land detail source
```

**What it touches:** the `_msn` normal sheet and the colour sheet's shading, and
nothing else. The mesh, the `.lodl`, the heights and the loaded terrain are
byte-identical with it on -- it is a delta field read by two writers, not an
edit to the heightfield.

**What it costs:** about **+3 seconds per dim-4 chunk**, which made a four-chunk
region take 18.2 s instead of 6.2 s on one thread -- 2.9 times as long. That
was not measured at dim 8, 16 or 32, so assume the whole Commonwealth is
noticeably slower and start with a region.

**Every bake with it on prints a census line** -- cells, moved cells, mean
absolute height change in world units, largest cut, largest fill -- and the same
numbers go into the ledger. If that mean is in the thousands something is wrong;
on a normal run it is in the teens.

**The honest limitation, so the picture does not surprise you.** Measured
against 22 vanilla sheets, the amount of fine relief is right (-5.6 %, inside
vanilla's own sheet-to-sheet spread) and its direction is most of the way there
(anisotropy -20 %), but vanilla's relief gets **stronger on steeper ground**
more than ours does: correlation +0.29 against vanilla's +0.50. So it reads as
eroded ground, not yet as vanilla's eroded ground. The cause and the next
experiment are named in `docs/LODGEN_TERRAIN_VT.md` section 2.5i.

**There is no rock/sediment tinting and that is deliberate** -- lane TILING3
measured that about 98 per cent of vanilla's fine colour is not a function of
its fine normal, so a colour-by-erosion palette would be inventing a law the
corpus does not support. The colour gets shading only.

`--land-detail-source vanilla` and `vanilla-blend` (the TILING3 path that copies
vanilla's own normal detail) are untouched by all of this and remain the option
if you want vanilla's detail rather than a simulation of it.
