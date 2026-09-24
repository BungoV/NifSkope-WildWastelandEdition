# Lane CHANVIEW1 -- every baked channel of chunk 4.4.-12 shown, one picture each, same framing as the AO

Tree `E:/Projects/NifskopeWildWastelandEdition`, branch `main`, only lane in the tree. Opened 2026-09-18 08:11 CEDT
(`date`-read; `scratchpad/chanview1_20260918/LAUNCH_STAMP` = `2026-09-18 08:11:29`, the G4 reference time).
Report written incrementally, a section per finished step.

## 0. The exe at launch, and the rung

| what | value |
|---|---|
| `release/NifSkope.exe` mtime | 2026-09-18 **07:31:05.258124600 +0200** (`ls --time-style=full-iso`) |
| size | **22,686,720 B** |
| sha1 | **`a63e26b9e7e7bbf7cb4ebd2676ac552bb0fd812c`** |
| rung cut 2026-09-18 08:19 CEDT, before any build | `release/NifSkope.before_chanview1.exe`, same 22,686,720 B, same sha1 `a63e26b9e7e7...` |
| processes at launch | `tasklist \| grep -i -E "Fallout4\|NifSkope"` -> no match, `rc=1`. No game, no NifSkope. |

That is SLAB1's final exe (HANDOFF LANDED block: 07:31:05, 22,686,720 B, sha1 a63e26b9e7e7) -- read here with `ls`
and `sha1sum`, not copied from the brief.

G4's baseline is already recorded: `find scratchpad/slab1_20260918/after -newer <LAUNCH_STAMP>` -> **0 files** at
08:19. The lane is viewer-only and that number must still be 0 at the end.

Read order actually followed: `scratchpad/brief_chanview1.md` in full -> `CONSTITUTION.md` in full -> `HANDOFF.md`
top block (the LIVE CHANVIEW1 line 368, the SLAB1 LANDED block line 369, the hotfix 7/7b/7c/7d entries 392-437) ->
root `MISTAKES.md` top entries (05:1x a file byte is not a pixel; 05:0x a proxy plane shown as the channel) ->
`src/lodinative.cpp` in full (917 lines; the `WW_LODL_AO` seam at 487 and the note lines at 865-880) ->
`src/btdterrain.cpp` 1570-1675 (hotfix 7b's bilinear mask-B sampler) -> `src/lodtsheets.cpp`/`.h`
(`LodtSheets::maskAo`, `bc1BlueBlock`) -> `docs/LODGEN_NATIVE_LODO_LODI.md` s2/s4/s4.1c/s4.3 ->
`docs/LODGEN_TERRAIN_VT.md` v2 header -> `res/shaders/fo4_default.frag` 275-360 (the stock channel block) ->
SLAB1's `bake.sh`, `render.sh`, report s1.1 and s5.6.

## 1. The channel table, read from the shipped files (step 1)

### 1.1 Which files the table -- and every picture -- is read from, and why

The bake is SLAB1's after-bake. Nothing was re-baked: `scratchpad/slab1_20260918/after/` already holds everything
this lane needs, and G4 above proves it is untouched.

| role | file | provenance |
|---|---|---|
| terrain sheets | `E:/Projects/NifskopeWildWastelandEdition/scratchpad/slab1_20260918/after/vt/FO4CSLOD/Commonwealth/Commonwealth.VT.{1,2,4}.lodt` + `.lodm` | SLAB1's after-bake, 2026-09-18 07:32 |
| terrain mesh | `E:/Projects/NifskopeWildWastelandEdition/scratchpad/viewfix_20260917/chunkB/lodl/FO4CSLOD/Commonwealth/Commonwealth.lodl` | unchanged since hotfix 7c; SLAB1 rendered the same file |
| objects | `E:/Projects/NifskopeWildWastelandEdition/scratchpad/viewfix_20260917/urban_ao/nat/FO4CSLOD/Commonwealth/Commonwealth.lodi` (**v6**) + `.lodo` (v4), 2026-09-18 05:18/05:19 | see below |

**The `.lodi` had to be settled before the table could be written, and it is not the one SLAB1 rendered.**
There are two Commonwealth `.lodi`/`.lodo` pairs in the tree, both 33,123 instances, both covering this chunk:

| pair | version | v6 scene-AO stream | `.lodo` per-vertex `selfAO` over this chunk |
|---|---|---|---|
| `scratchpad/viewfix_20260917/urban_authored/FO4CSLOD/Commonwealth/` | **v5** | absent (`vertexAoBytes == 0`) | **constant 255** -- not baked |
| `scratchpad/viewfix_20260917/urban_ao/nat/FO4CSLOD/Commonwealth/` | **v6** | present, 53,349 bytes on this chunk | 38..255, 9 levels |

SLAB1's eight picture logs all read the **v5** pair (`grep -o "lodi Commonwealth.lodi v[0-9]*"` -> `8 ... v5`), so
its AO pictures went down the `WW_LODL_AO` fallback: *"self-AO x placement AO as vertex colour; 2446 placements
measured (mean 181.0)"*. The hotfix 7c/7d AO pictures bungo actually saw went down the **v6** path -- their log
`scratchpad/viewfix_20260917/images/oao_full_L0.log` says `lodi Commonwealth.lodi v6` and
*"WW_LODL_AO: .lodi v6 scene vertex AO used on 2446 placements (53349 bytes, mean 175.8)"*.

This lane renders the **v6** pair, for four reasons stated before any picture was taken:

1. The brief's step-1 table requires **"v6 scene AO (per instance vertex)"**. The v5 pair cannot supply that row --
   the stream is not in the file.
2. The brief's `selfao` channel is *"the `.lodo` byte alone"*. On the v5 pair that byte is **constant 255** over the
   whole chunk, so `selfao` would be a flat grey and gate (b) (render != default) could not be satisfied honestly.
   On the v6 pair it is a real channel.
3. `ao` must be *"= WW_LODL_AO today, unchanged bytes"*. Today's `WW_LODL_AO` prefers the v6 stream when it is
   there; rendering the v5 pair would silently make `ao` the fallback product instead.
4. It is the file the AO picture in his hands came from, so `ao` here and the AO he has already seen are the same
   channel of the same file.

The **sheets** are SLAB1's after-bake either way, which is what "SHEETS and LODI at SLAB1's after-bake" pins down
(SLAB1's own `render.sh` header says the `.lodi`/`.lodl`/`.lodo` are hotfix 7c's, *"only the SHEETS change between
the before and the after picture"*). Both readings are tabled in s1.3 so nothing is hidden by the choice.

### 1.2 The population, and the control that proves the reader

`scratchpad/chanview1_20260918/channel_table.py` reads the files with the existing readers -- one per format:
`tests/spells/lodgen_native_decode.py` (`read_lodi`, `read_lodo`) and `tests/spells/lodgen_vt_check.py` (`Lodv`,
the `.lodt` container). The only thing it implements itself is the **BC3 alpha block**, because no reader in the
tree decodes it (`LodtSheets::bc1BlueBlock` only ever wanted the mask sheet's B) -- and on this chunk that code
never fires, see `mask-a` below.

It replicates `src/lodinative.cpp`'s selection exactly: the region filter, `WW_LODI_SLOT=0`, the
`clusterLods[c].level == level` keep plus the root-below rule, and the v6 `meshRange` span check.

**The control, chosen before the run:** the reader's population and its placement-AO mean must reproduce numbers
the shipped viewer already prints in its own note lines.

| number | my reader | the viewer's note line | source log |
|---|---|---|---|
| placements drawn | **2,446** | 2,446 | `oao_full_L0.log`, `flat_close_after.log` |
| vertices drawn (slot 0, level 0) | **53,349** | 53,349 | `oao_full_L0.log` |
| v6 scene-AO bytes | **53,349** | 53,349 | `oao_full_L0.log` |
| v6 scene-AO mean | **175.849557** | mean 175.8 | `oao_full_L0.log` |
| placement-AO mean (v5 fallback) | **181.024939** | mean 181.0 | `flat_close_after.log` |
| terrain mask-B range | **33..255** | values 33..255 | `flat_close_after.log` |

It did not reproduce them on the first run, and that is in `MISTAKES.md` (2026-09-18 08:0x): the first reader walked
the `.lodi` chunk table south-up and reported 10,691 placements. The shipped `lodiChunkAt`
(`src/lodifile.cpp:241`) is north-up. Two rows of this table are the only reason that was caught.

Region `4,-12,7,-9`, `WW_LODI_LEVEL=0`, `WW_LODI_SLOT=0`: **2,446 placements, 53,349 vertices**, of which
**147 placements are trees** (`seed != 0`).

### 1.3 The table (min / max / mean / distinct over chunk 4.4.-12)

Per placement (2,446 values each), from the `.lodi`:

| channel | source | min | max | mean | distinct | note |
|---|---|---|---|---|---|---|
| `identity` / `identityraw` | `.lodi` cold record `identity` | 0 | 2448 | **1224.893704** | **2446** | one value per placement, no collision inside the chunk (s4.1c's identity law holds here) |
| `sky` | `.lodi` instance 0x11 | 0 | 255 | **129.304988** | 234 | |
| `ground` | `.lodi` instance 0x12 | 0 | 255 | **61.299264** | 245 | ground-contact blend over the 256-unit ramp |
| `seed` | `.lodi` instance 0x13 | 0 | 255 | **7.419052** | 112 | 0 on the 2,299 non-trees; 147 trees carry 111 distinct non-zero seeds |
| placement AO | `.lodi` placement-AO array | 38 | 254 | **181.024939** | **9** | 0 unmeasured |

Per vertex (53,349 values each), from the `.lodo` and the `.lodi` v6 stream:

| channel | source | min | max | mean | distinct | note |
|---|---|---|---|---|---|---|
| `sway` | `.lodo` vertex 0x0E | 0 | 255 | **8.889033** | 158 | identical on both `.lodo` pairs |
| `selfao` | `.lodo` vertex 0x0F (**v6 pair**) | 38 | 255 | **234.763969** | **9** | on the v5 pair: **constant 255** |
| `ao` (v6 scene AO) | `.lodi` v6 per-instance vertex stream | 38 | 255 | **175.849557** | **9** | on the v5 pair: **absent** |

All three AO channels are quantised to the **same nine levels** (38, 65, 92, 119/120, 146/147, 173/174, 200/201,
227/228, 254/255 -- a step of 27). That is a property of the bake, not of the reader; it is why `distinct` is 9 on
three rows that cover 53,349 vertices, and the pictures will show banding that is really there. Their histograms
differ (e.g. 255 covers 42,271 vertices of `selfao` but only 17,847 of `ao`), so they are three different channels,
not one byte read three times.

Terrain sheets (`Commonwealth.VT.1.lodt`, the finest container: **west 4, north -9, 4x4 tiles, all 16 present** --
exactly chunk 4.4.-12; content 512, border 8, stored 528; mip 0; borders excluded => **4,194,304 texels**):

| channel | sheet | format | min | max | mean | distinct | note |
|---|---|---|---|---|---|---|---|
| `mask-r` roughness | role 5 MASK | BC1 (dxgi 71) | 66 | 239 | **180.413451** | 85 | |
| `mask-g` metallic | role 5 MASK | BC1 (dxgi 71) | 0 | 0 | **0.000000** | **1** | **constant 0 (legacy materials)** |
| `mask-b` AO | role 5 MASK | BC1 (dxgi 71) | 33 | 255 | **159.214451** | 118 | the viewer resamples this bilinearly per grid vertex and prints mean 158.8 -- same range, different sampling |
| `mask-a` ground cover | role 5 MASK | -- | -- | -- | -- | -- | **absent**: the mask sheet is BC1 on every one of the 16 tiles, so it carries no alpha |
| `normal` R | role 2 MSN | BC1 (dxgi 71) | 24 | 231 | **130.596864** | 100 | |
| `normal` G | role 2 MSN | BC1 (dxgi 71) | 195 | 255 | **252.676241** | 46 | model-space Z-up: G pinned near 255 is the flat ground |
| `normal` B | role 2 MSN | BC1 (dxgi 71) | 16 | 239 | **129.607025** | 111 | |
| `emissive` | role 6 | -- | -- | -- | -- | -- | **absent**: the container carries no sheet with role 6 |

### 1.4 The rule that served each mask layer (bungo 06:1x)

The bake's own per-layer census, read verbatim out of `scratchpad/slab1_20260918/after/bake.log` (the `vt:` line),
not inferred from the texels:

```
sheets 3 emissive none  maskPbrm 0  maskLegacyInverted 17  maskNoneDefault 0
maskRoughMaps 17  maskMetalMaps 0  maskEmissiveMaps 0  maskDistinctLtex 17  maskLayerRefs 322
coverTiles 0  cover 0  coverIn <empty>
```

So, for this chunk, with numbers:

- **17 distinct landscape textures**, referenced 322 times across the chunk's layers.
- **All 17 were served by the legacy rule** (`maskLegacyInverted 17`): the layer's legacy gloss map inverted into
  roughness. **0 layers came from a `.pbrm`** (`maskPbrm 0`), and 0 fell back to the no-source default
  (`maskNoneDefault 0`). The bake log's `File ' "materials/.../*.pbrm" ' not found in archives` lines are those
  layers looking for a `.pbrm` first and not finding one.
- **17 of 17 had a gloss map to invert** (`maskRoughMaps 17`) -- so `mask-r` is a real measured channel, not a
  constant, which its 85 distinct values confirm.
- **0 of 17 had a metal map** (`maskMetalMaps 0`) -- so `mask-g` is **constant 0**, and it is constant 0 *because
  the materials are legacy*, exactly the case his 06:1x message named. It is still pictured, and its caption and
  note line say `constant 0 (legacy materials)`.
- **0 emissive maps** (`maskEmissiveMaps 0`, `emissive none`) -> no role-6 sheet exists -> `emissive` refuses by
  name with "emissive sheet absent".
- **0 cover tiles** (`coverTiles 0`, and the ground-cover input `coverIn` is empty) -> no tile was written in the
  BC3 cover format, so the mask sheet has no alpha plane anywhere in this chunk -> `mask-a` refuses by name. The
  format supports it (the mask sheet's `dxgiCover` is **77 = BC3**); this bake had nothing to put in it.

That is four of the thirteen names that are honest refusals or constants on this bake, declared here **before** any
picture, so that no picture can be captioned as a channel it is not (MISTAKES 05:0x).

## 2. The switch: `WW_LODL_CHANNEL=<name>` (step 2)

### 2.1 The seam

One switch, on the native path, read at exactly the seam `WW_LODL_AO` is read at -- and
`WW_LODL_AO=1` is left alone: it is still the `ao` channel, byte for byte (gate G2, section 3.3).

The name is parsed once, in `src/lodinative.cpp`, by `lodlChannelFromEnv()`
(declared in `src/lodinative.h` beside `lodiSpecFromEnv`), which returns a
`LodlChannel` enum and, through its out-parameter, the string the user actually gave so
the refusal can name it. Both consumers call that one function, so there is one spelling
of the name list in the tree:

| file | what it does with the channel | why it is this file |
|---|---|---|
| `src/lodinative.h` | the enum + `lodlChannelFromEnv` / `lodlChannelName` / `lodlChannelNames` | the header the seam already lives in |
| `src/lodinative.cpp` | the OBJECT channels: identity, identityraw, sky, ground, seed, sway, selfao, ao | this is where `WW_LODL_AO` already painted vertex colour |
| `src/btdterrain.cpp` | the TERRAIN channels: mask-r/g/b/a, emissive, normal, and `ground`'s terrain half | the existing sheet sampler; no second decoder was added |
| `src/lodtsheets.h` / `.cpp` | `sheetChannel(role, tx, ty, channel, out, why)` + `hasRole(role)` | the existing BC decoder, widened from blue-only to R/G/B/A |
| `src/nifskope_ui.cpp` | one line: a non-empty `WW_LODL_CHANNEL` also sets `Scene::DoVertexColors` | the seam forces it -- otherwise a flat-byte channel is painted and never shown |

`src/lodtsheets.cpp`'s old `bc1BlueBlock()` became `bc1ColourBlock(block, channel, out)`
(R 5-bit, G 6-bit, B 5-bit, all expanded the same way the blue path already did), and a new
`bc3AlphaBlock()` reads the BC3 alpha block for `mask-a`. The old `maskAo()` is now one line
-- `sheetChannel(LODV_ROLE_MASK, tx, ty, 2, out, why)` -- so the AO path and the channel path
are literally the same code reading the same texels, which is what makes G2 hold.

In `lodinative.cpp` the per-vertex carrier was generalised rather than duplicated:
`OutVert::ao` (one float) became `OutVert::chan[3]` (a colour), and the two bytes a channel
may want per vertex are now carried beside it (`selfAo`, `sway`). `Bucket::withAo` became
`Bucket::withColour`. Nothing else about emission changed: the same buckets, the same
draw calls, the same meshes.

Writer, format and bake defaults are untouched. `git status` for section 7 shows six source
files and no file under `src/lodgen*`, `src/nativeemit*` or `src/io/`.

### 2.2 The names, and where each one's bytes come from

| name | what it paints | the bytes | which file they were read from |
|---|---|---|---|
| `identity` | every placement its own colour, the stock channel-1 hash | `.lodi` placement identity (16-bit) | `Commonwealth.lodi` |
| `identityraw` | the identity's low byte as grey | `.lodi` placement identity & 0xFF | `Commonwealth.lodi` |
| `sky` | per-placement sky visibility, grey ramp | `.lodi` 0x11 | `Commonwealth.lodi` |
| `ground` | ground-contact blend, grey ramp -- placements AND terrain | `.lodi` 0x12; terrain drawn at the ramp's value at the surface | `Commonwealth.lodi` |
| `seed` | per-placement tree seed hashed to colour, **0 = black = not a tree** | `.lodi` 0x13 | `Commonwealth.lodi` |
| `sway` | per-vertex wind-sway weight, grey ramp | `.lodo` vertex 0x0E | `Commonwealth.lodo` |
| `selfao` | per-vertex self-AO, grey ramp | `.lodo` vertex 0x0F | `Commonwealth.lodo` |
| `ao` | exactly what `WW_LODL_AO=1` has always drawn | v6 per-instance vertex AO x placement AO; terrain from mask B | `.lodi` v6 stream + `Commonwealth.VT.1.lodt` |
| `mask-r` | terrain roughness | mask sheet R | `Commonwealth.VT.1.lodt` role 5 |
| `mask-g` | terrain metallic | mask sheet G | `Commonwealth.VT.1.lodt` role 5 |
| `mask-b` | terrain sky AO | mask sheet B | `Commonwealth.VT.1.lodt` role 5 |
| `mask-a` | terrain ground cover | mask sheet A | **absent on this bake** -- the sheet is BC1 |
| `emissive` | the emissive sheet bound as base colour, texturing ON | role-6 sheet | **absent on this bake** -- no role-6 sheet |
| `normal` | the MSN sheet bound as base colour, texturing ON | role-2 sheet | `Commonwealth.VT.1.lodt` role 2 |

`sky`, `ground`, `sway`, `selfao`, `mask-r/g/b/a` are a single byte painted into all three
colour components, so the picture is a grey ramp and 128 grey is byte 128. `identity` and
`seed` are the byte hashed to a colour, because a flat ramp cannot separate 2,446 neighbours.
`normal` and `emissive` are a texture, so they are the only two drawn with texturing ON.

### 2.3 The note line

Every channel writes its own note line to stdout the way `WW_LODL_AO` does, and every number
in it is **read back from what was uploaded** -- the accumulators sit on the write into
`OutVert::chan[]` and on the decoded sheet texels, not on the intent. Verbatim, from
`refuter_close/*.log`:

```
WW_LODL_CHANNEL=identity: the placement identity, hashed to colour (the stock channel 1 palette) from Commonwealth.lodi, 2446 placements read; min 0, max 2448, mean 1224.894
WW_LODL_CHANNEL=identityraw: the placement identity's low byte as grey from Commonwealth.lodi, 2446 placements read; min 0, max 255, mean 124.282
WW_LODL_CHANNEL=sky: the per-placement sky visibility (.lodi 0x11) from Commonwealth.lodi, 2446 placements read; min 0, max 255, mean 129.305
WW_LODL_CHANNEL=ground: the per-placement ground-contact blend (.lodi 0x12) from Commonwealth.lodi, 2446 placements read; min 0, max 255, mean 61.299
WW_LODL_CHANNEL=ground: the terrain drawn at the contact ramp's value AT THE SURFACE (constant 255) in the same grey ramp as the placements, 16641 vertices; the per-placement bytes are in the objects' note line
WW_LODL_CHANNEL=seed: the per-placement tree seed, hashed to colour; 0 = not a tree = black (.lodi 0x13) from Commonwealth.lodi, 2446 placements read; min 0, max 255, mean 7.419
WW_LODL_CHANNEL=sway: the per-vertex wind-sway weight (.lodo 0x0E) from Commonwealth.lodo, 53349 vertices read; min 0, max 255, mean 8.889
WW_LODL_CHANNEL=selfao: the per-vertex self-AO (.lodo 0x0F) from Commonwealth.lodo, 53349 vertices read; min 38, max 255, mean 234.764
WW_LODL_CHANNEL=mask-r: terrain mask-r from the MASK SHEET'S R (the texture, Commonwealth.VT.1.lodt), 512 texels a cell, bilinear a vertex; 16 tiles read, 257 vertices without a tile (drawn open); values 99..231, mean 181.9
WW_LODL_CHANNEL=mask-r: over the sheet's own CONTENT texels, 4194304 texels of 16 tiles, values 66..239, mean 180.413
WW_LODL_CHANNEL=mask-g: constant 0 over this chunk
WW_LODL_CHANNEL=mask-g: over the sheet's own CONTENT texels, 4194304 texels of 16 tiles, values 0..0, mean 0.000
WW_LODL_CHANNEL=mask-b: over the sheet's own CONTENT texels, 4194304 texels of 16 tiles, values 33..255, mean 159.214
WW_LODL_CHANNEL=mask-a: ABSENT on this bake -- tile 0,3 is BC1 (dxgi 71): it carries no alpha; the lit view is unchanged
WW_LODL_CHANNEL=normal: the role-2 sheet of Commonwealth.VT.1.lodt bound as the terrain's base colour, 16 tiles, 4194304 content texels a channel; R 24..231 mean 130.597, G 195..255 mean 252.676, B 16..239 mean 129.607
WW_LODL_CHANNEL=emissive: emissive sheet ABSENT -- Commonwealth.VT.1.lodt carries no sheet with role 6; nothing drawn differently
```

`ao` keeps the four `WW_LODL_AO:` lines it always printed, unchanged, because changing them
would have broken G2:

```
WW_LODL_AO: .lodi v6 scene vertex AO used on 2446 placements (53349 bytes, mean 175.8), 0 slices did not match the drawn mesh
WW_LODL_AO: self-AO x placement AO as vertex colour; 2446 placements measured (mean 181.0), 0 unmeasured (drawn open)
WW_LODL_AO: terrain AO from the MASK SHEET'S B (the texture, Commonwealth.VT.1.lodt), 512 texels a cell, bilinear a vertex; 16 tiles read, 257 vertices without a tile (drawn open); values 33..255, mean 158.8
WW_LODL_AO: over the sheet's own CONTENT texels, 4194304 texels of 16 tiles, values 33..255, mean 159.214
```

**The two terrain populations, said out loud.** A mask channel gets TWO note lines, and they
are deliberately different numbers over different populations:

* the first is the **bilinear resample at the 129x129 grid vertices** the viewer actually
  uploads -- that is what the picture is made of (mask-r 99..231, mean 181.9);
* the second is the **content-texel census** over the 4,194,304 texels of the 16 decoded
  tiles, borders excluded -- that is the file's own number, and it is the one an
  independent reader of the container counts (mask-r 66..239, mean 180.413).

The report's table (section 1.3) and the gate both use the census line. This split is here
because the first attempt reported only the resample, and its mean missed the reader's by
1.49 -- close enough to look right and wrong for a stated reason (section 11).

### 2.4 An unknown name

An unknown name refuses **by the name given**, lists the known names, and draws nothing
different (its render is byte-identical to the default -- section 3.2):

```
WW_LODL_CHANNEL: REFUSED "nosuchchannel" -- no such channel; the scene is the default one. Known names: identity, identityraw, sky, ground, seed, sway, selfao, ao, mask-r, mask-g, mask-b, mask-a, emissive, normal
WW_LODL_CHANNEL: REFUSED "nosuchchannel" -- no such channel; the terrain is the default one. Known names: identity, identityraw, sky, ground, seed, sway, selfao, ao, mask-r, mask-g, mask-b, mask-a, emissive, normal
```

---

## 3. The refuter, per channel, BEFORE any picture (step 3)

The rule (root `MISTAKES.md` 05:1x): a channel whose render is byte-identical to the default
render is **not wired**, and no caption may rescue it. So every channel was rendered twice --
once with `WW_LODL_CHANNEL=<name>`, once without, same exe, same bake, same framing, same
size -- and the two PNGs counted pixel by pixel; and the note line's mean was put beside the
independent Python reader's mean from section 1.3.

Harness: `scratchpad/chanview1_20260918/refute.sh <framing> <outdir>` takes the renders (one
NifSkope at a time, `--port 12078`, `WW_WINDOW_AT=1960,40`, second monitor, never a desktop
capture); `refute.py <outdir> table_after.json` does the counting. The default for the twelve
flat-byte channels is `default.png` (`WW_RENDER_FLAT=1`, no channel); the default for
`normal` and `emissive` is `default_tex.png` (texturing ON, `WW_LOD_CHANNEL=12` raw base
colour, no channel), because those two are textures and must be compared against the textured
scene they replace.

### 3.1 Both framings, measured

`refuter_close.txt` (close framing, 1400x1091, centre 24900,-41300,450 ortho 2600):

| channel | pixels differing from default | note mean | reader mean | verdict |
|---|---:|---:|---:|---|
| identity | 1,109,710 | 1224.894 | 1224.894 | ok |
| identityraw | 1,109,585 | 124.282 | 124.282 | ok |
| sky | 1,081,487 | 129.305 | 129.305 | ok |
| ground | 1,109,361 | 61.299 | 61.299 | ok |
| seed | 1,109,710 | 7.419 | 7.419 | ok |
| sway | 1,109,710 | 8.889 | 8.889 | ok |
| selfao | 251,518 | 234.764 | 234.764 | ok |
| ao | 1,015,816 | 175.800 | 175.850 | ok |
| mask-r | 380,174 | 180.413 | 180.413 | ok |
| mask-g | 380,174 | 0.000 | 0.000 | ok (constant 0) |
| mask-b | 380,084 | 159.214 | 159.214 | ok |
| mask-a | 0 | -- | -- | **ABSENT, and said so by name** |
| emissive | 0 | -- | -- | **ABSENT, and said so by name** |
| normal | 397,352 | 130.597 (R; G 252.676, B 129.607 also match) | 130.597 | ok, 3 texel channels |

`refuter_full.txt` (full framing, centre 24576,-40960,0 ortho 8192):

| channel | pixels differing | note mean | reader mean | verdict |
|---|---:|---:|---:|---|
| identity | 487,491 | 1224.894 | 1224.894 | ok |
| identityraw | 483,423 | 124.282 | 124.282 | ok |
| sky | 454,279 | 129.305 | 129.305 | ok |
| ground | 487,212 | 61.299 | 61.299 | ok |
| seed | 487,491 | 7.419 | 7.419 | ok |
| sway | 487,490 | 8.889 | 8.889 | ok |
| selfao | 111,674 | 234.764 | 234.764 | ok |
| ao | 1,255,009 | 175.800 | 175.850 | ok |
| mask-r | 1,014,131 | 180.413 | 180.413 | ok |
| mask-g | 1,014,134 | 0.000 | 0.000 | ok (constant 0) |
| mask-b | 1,013,558 | 159.214 | 159.214 | ok |
| mask-a | 0 | -- | -- | **ABSENT, and said so by name** |
| emissive | 0 | -- | -- | **ABSENT, and said so by name** |
| normal | 1,027,700 | 130.597 | 130.597 | ok, 3 texel channels |

Every difference is greater than zero on twelve channels at both framings, and every note
mean equals the reader's mean to three decimals except `ao`, whose note line rounds to one
decimal in the string the viewer has always printed (175.8 against 175.850 -- 0.05, inside
the gate's tolerance of 1).

`selfao`'s count is the smallest because self-AO only differs from white where a mesh
occludes itself: 234.76 of 255 means most of the chunk's vertices are at or near white, so
only the placements themselves repaint. `sky`'s is next-smallest for the same reason at the
other end. The full framing inverts the ratio for the terrain channels (the terrain fills
more of the frame there and the placements fill less), which is itself a check that the two
framings are not the same picture.

### 3.2 Two channels that are honestly absent

`mask-a` and `emissive` are the two names whose bytes this bake does not carry, and the
refuter's floor for them is inverted: their render MUST be identical to the default, and the
note line MUST say why by name. Both hold. This is declared here, before any picture, so that
no picture can be captioned as a channel it is not (root `MISTAKES.md` 05:0x):

* `mask-a`: the mask sheet's tiles are BC1 (dxgi 71) -- a BC1 block carries no alpha at all,
  so there is no ground-cover channel to sample. The bake's own census agrees:
  `coverTiles 0 cover 0` (section 1.4).
* `emissive`: `Commonwealth.VT.1.lodt` carries no sheet with role 6. The bake's census:
  `maskEmissiveMaps 0 ... emissive none sheets 3`.

They are still pictured (the brief: a constant channel is reported as constant and STILL
pictured), and their captions say ABSENT, not a value.

### 3.3 The way back (gate G2) and the refusal control

| control | what it proves | result |
|---|---|---|
| `WW_LODL_CHANNEL=nosuchchannel` vs no channel | an unknown name changes nothing | **bytes identical**, both framings |
| `WW_LODL_AO=1`, `WW_LODL_CHANNEL` unset, NEW exe vs `WW_LODL_CHANNEL=ao` | the old switch still means the same picture | **bytes identical**, both framings |
| `WW_LODL_AO=1` on the RUNG exe (`NifSkope.before_chanview1.exe`) vs both of the above | the lane did not move the AO view at all | **bytes identical**, both framings |

sha1 of all three, close framing: `d339bc06f9d274d8ea0bef78b195bbee89031a9f`.
sha1 of all three, full framing: `67214b64bdc22e706f8828a299740cf64f5bc8bd`.

G1 and G2 are green at both framings. The pictures in section 4 follow.

---

## 4. The pictures (step 4)

All of them are in `scratchpad/chanview1_20260918/images/`, 31 files. Every one is a
headless render or a texel read of **SLAB1's after-bake**
(`scratchpad/slab1_20260918/after/vt/FO4CSLOD/Commonwealth`) with the v6 `.lodi`/`.lodo`
pair, taken one NifSkope at a time on the second monitor (`--port 12078`,
`WW_WINDOW_AT=1960,40`). Not one is a desktop capture, and no picture is a proxy: each
channel picture is the channel the shipped file carries, read back by the note line that
sits under it in the contact sheet.

### 4.1 Per channel, both framings (28 files)

`chunk_<channel>_full.png` and `chunk_<channel>_close.png` for all fourteen names. These
are the very renders section 3 refuted -- the same PNGs the pixel-difference and the
mean-comparison were computed on, copied into `images/` rather than re-rendered, so the
picture the report shows and the picture the gate measured are the same bytes.

| framing | camera | what it is for |
|---|---|---|
| `full` | centre 24576,-40960,0 ortho 8192, view 1 | the whole chunk, terrain-dominant |
| `close` | centre 24900,-41300,450 ortho 2600, view 8 | the deck and its placements, object-dominant |

`WW_RENDER_FLAT=1` (vertex colour, no lighting, no texturing, no tone map) for the twelve
flat-byte channels, so a 128 byte is 128 grey on the screen. Texturing ON, unlit
(`WW_LOD_CHANNEL=12`, the stock raw-base-colour view) for `normal` and `emissive`, because
those two ARE textures.

Caption for each, the same sentence in the contact sheet's cell:

| picture | caption |
|---|---|
| `chunk_identity_*` | identity, 2,446 placements, `.lodi` identity hashed to colour -- min 0, max 2448, mean 1224.894 |
| `chunk_identityraw_*` | identityraw, 2,446 placements, `.lodi` identity low byte as grey -- min 0, max 255, mean 124.282 |
| `chunk_sky_*` | sky visibility, 2,446 placements, `.lodi` 0x11 -- min 0, max 255, mean 129.305 |
| `chunk_ground_*` | ground-contact blend, 2,446 placements, `.lodi` 0x12; terrain drawn at the ramp's value at the surface (constant 255) -- min 0, max 255, mean 61.299 |
| `chunk_seed_*` | tree seed hashed to colour, 0 = not a tree = black, 2,446 placements, `.lodi` 0x13 -- min 0, max 255, mean 7.419 (147 of the 2,446 are trees) |
| `chunk_sway_*` | leaf-sway weight, 53,349 vertices, `.lodo` 0x0E -- min 0, max 255, mean 8.889 |
| `chunk_selfao_*` | per-vertex self-AO, 53,349 vertices, `.lodo` 0x0F -- min 38, max 255, mean 234.764 |
| `chunk_ao_*` | the AO view unchanged: v6 scene vertex AO x placement AO, 53,349 vertices -- min 38, max 255, mean 175.850 |
| `chunk_mask-r_*` | terrain **roughness**, mask sheet R, 4,194,304 content texels -- min 66, max 239, mean 180.413; served by the legacy-inverted rule on all 17 layers |
| `chunk_mask-g_*` | terrain **metallic**, mask sheet G -- **constant 0 (legacy materials)**, 4,194,304 content texels, min 0 max 0 mean 0.000 |
| `chunk_mask-b_*` | terrain sky AO, mask sheet B, 4,194,304 content texels -- min 33, max 255, mean 159.214 |
| `chunk_mask-a_*` | terrain **ground cover**, mask sheet A -- **ABSENT on this bake**: the sheet is BC1 (dxgi 71) and a BC1 block carries no alpha; the picture is the UNCHANGED default render and is captioned as such |
| `chunk_emissive_*` | **emissive sheet ABSENT**: `Commonwealth.VT.1.lodt` carries no sheet with role 6; the picture is the UNCHANGED default textured render |
| `chunk_normal_*` | the role-2 MSN sheet bound as the terrain's base colour, 4,194,304 content texels a channel -- R 24..231 mean 130.597, G 195..255 mean 252.676, B 16..239 mean 129.607 |

The two absent names are pictured because the brief asks for every channel to be pictured,
and their caption says ABSENT rather than a value (section 3.2). A reader who opens
`chunk_mask-a_close.png` sees the default scene; the caption, the note line and this table
all say why, in the same words.

### 4.2 `channels_contact.png` -- all fourteen at the close framing

1504x1666, four across, built by `make_contact.py`. Each cell: the render, the channel
name, `min / max / mean`, and the population sentence naming the file the bytes came from.

**The caption arithmetic** (`ww-texel-picture` rule 4): the script does not compute any of
those numbers. It reads `table_after.json` -- the output of the independent Python reader
in section 1.3 -- and prints it. So the number in the caption cannot drift from the number
in the report; there is one number and one source for it. Section 3 separately shows that
the viewer's own note line agrees with that same number to three decimals for all fourteen
names, which is what makes the caption a claim about the shipped file and not about the
script.

Colours: green = the channel carries values AND its render differs from the default (the
section 3 floor); red = a constant or an absent channel. `mask-g` is red and reads
`constant 0 (min 0 max 0 mean 0.000)`; `mask-a` and `emissive` are red and read
`ABSENT -- panel is the UNCHANGED default render`, with the reason on the line below.

Read back and looked at (rule 5): the first build clipped `mask-a`'s reason line at the
right edge of the page. Wrapped to the cell width and rebuilt; nothing clips now.

### 4.3 `mask_sheet_texels.png` -- the four mask channels as TEXELS

2192x819, built by `make_masktexels.py`. The mask sheet reaches the screen only as a
bilinear tap at a terrain vertex, so a render is a resample of the texels and not the
texels; this reads the container itself through the lane's one sheet reader
(`channel_table.sheet_plane` on `lodgen_vt_check.Lodv`), mip 0, content texels only.

The window: **texels 384,128..640,384** of the 2048x2048 content mosaic (16 tiles of
512x512), at nearest-neighbour x2, one texel = 2 device pixels, no resample, the SAME
window and the same indices in all four panels. It was chosen **by the metric, not by eye**
(rule 1): it is the 256x256 window with the LOWEST sky-AO mean on the chunk (B mean
66.995), i.e. the most sky-occluded ground -- the ground under the deck.

Each panel carries two readings (rule 7), because a window picked for being extreme has an
extreme number:

| panel | this crop | WHOLE SHEET (the report's number, the one the gate quotes) |
|---|---|---|
| mask-r roughness | min 66, max 231, mean 160.309 | min 66, max 239, mean 180.413 |
| mask-g metallic | min 0, max 0, mean 0.000 | min 0, max 0, mean 0.000 -- constant |
| mask-b sky AO | min 33, max 148, mean 66.995 | min 33, max 255, mean 159.214 |
| mask-a ground cover | -- | ABSENT: BC1 (dxgi 71) carries no alpha |

The header says which of the two is the verdict and why they differ (65,536 texels of
4,194,304, chosen for being extreme). The `mask-a` panel is not a black square pretending
to be data: it is a red frame that says "no texels exist to photograph" with the reason.

### 4.4 `gloss_vs_roughness.png` -- the inversion, read on one layer (bungo 06:1x)

1644x833, built by `make_gloss.py`. bungo asked for roughness and metallic "since these are
baked from legacy textures and materials, not .pbrm", and for the inversion to be readable.

The law is one place in the tree:

```
layerRough()  ->  1.0f - lodgenLegacyGloss( m.glossScale, specGreen )     src/lodgen.cpp:11151
lodgenLegacyGloss( s, g ) = clamp( clamp(s,0,1) * g, 0, 1 )               src/lodgen.cpp:1800
out->roughnessChannel = 1;   // the `_s` map's GREEN channel               src/lodgen.cpp:1907
```

and for a TERRAIN layer the smoothness argument is the literal `1.0f`
(`src/lodgen.cpp:10825`, the LTEX texture-set resolve), so **on this bake the law is exactly
`roughness = 255 - gloss`** and nothing else. That is worth saying plainly because it is the
whole of bungo's question: the chunk has no PBRM roughness anywhere, so every roughness
texel on it is one minus a vanilla specular map's green channel.

| panel | what it is | min | max | mean |
|---|---|---|---|---|
| SOURCE gloss | the GREEN channel of `textures/landscape/ground/dirtgravel01_s.dds` (BC5U, mip 0, 1024x1024), from the vanilla corpus `E:/Tools/Fallout 4/DataUnpacked/Data` | 14 | 251 | 69.044 |
| BAKED roughness | the same texels through the law | 4 | 241 | 185.956 |
| SHIPPED sheet R | `Commonwealth.VT.1.lodt` role 5 R at the same 256-texel window as 4.3 | 66 | 231 | 160.309 |

69.044 + 185.956 = 255.000 exactly, which is the inversion stated as a number rather than as
a look. The crop (256x256 at x2) was chosen by the metric: the window of the source with the
widest gloss range. The third panel is there so the shipped bytes are on the page, and its
caption says it is the blend of up to 17 layers over each texel and NOT this layer alone --
which is why its range (66..239 over the whole sheet) is narrower than the source's
(14..251): blending seventeen layers pulls every texel toward the mean.

**metallic**: constant 0 across the chunk, and the picture and the note line both say
`constant 0 (legacy materials)`. The bake's own per-layer census agrees and is the
provenance: `maskPbrm 0, maskLegacyInverted 17, maskRoughMaps 17, maskMetalMaps 0,
maskDistinctLtex 17, maskLayerRefs 322` (section 1.4). A legacy material carries no metallic
to read, and bungo's standing ruling is that metallic is derived from a PBRM or not at all,
so 0 here is the honest answer and not a missing bake.

### 4.5 The file list (gate G3)

| file | what |
|---|---|
| `images/chunk_{identity,identityraw,sky,ground,seed,sway,selfao,ao,mask-r,mask-g,mask-b,mask-a,emissive,normal}_full.png` | 14 |
| `images/chunk_{...same 14...}_close.png` | 14 |
| `images/channels_contact.png` | 1 |
| `images/mask_sheet_texels.png` | 1 |
| `images/gloss_vs_roughness.png` | 1 |
| **total** | **31** |

---

## 5. The gate, its counts, and every floor shown RED (step 5)

`tests/spells/lodl_channels.sh` -- 48 checks, and it is a render spell rather than a
`WW_*_TEST` translation unit, which is a deviation from `ww-test-harness-add` and is stated
here with its reason: **everything this gate asserts already leaves the application in
writing.** The switch prints its note lines to stdout and the render hook writes the PNG, so
an in-window harness would add a second way for the same facts to be wrong. The spell's
shape is `lodl_open.sh`'s (the `_harness.sh` include, `winpath()` for every absolute path, a
named SKIP with the missing fixture's path when a fixture is gone, `N checks, M failures`
then `PASS`/`FAIL`), and its head comment carries bungo's 06:0x and 06:1x words verbatim and
the reason each floor exists.

Three files:

| file | what it is |
|---|---|
| `tests/spells/lodl_channels.sh` | takes the seventeen renders, one NifSkope at a time, `--port 42941`, `WW_WINDOW_AT` from `_harness.sh` |
| `tests/spells/lodl_channels_check.py` | reads them back and decides; renders nothing, decodes nothing |
| `tests/spells/lodl_channels_table.py` | the independent reader, moved here from the lane's scratchpad so the gate carries it -- ONE copy in the tree, over `lodgen_native_decode.py` and `lodgen_vt_check.py` |

### 5.1 The run, on the final exe

```
exe   release/NifSkope.exe  (2026-09-18 08:40:07.879937600 +0200  22718976 B)
lodi  scratchpad/viewfix_20260917/urban_ao/nat/FO4CSLOD/Commonwealth/Commonwealth.lodi
sheet scratchpad/slab1_20260918/after/vt/FO4CSLOD/Commonwealth/Commonwealth.VT.1.lodt
48 checks, 0 failures
PASS
```

1 minute 48 seconds, 2026-09-18 09:01:31 to 09:03:20. The sub-gates, against the brief's
letters:

| sub-gate | checks | what each one says |
|---|---|---|
| floor | 1 | the flat default and the textured default are DIFFERENT pictures, so a later "0 px differ" means identical and not "broken comparison" |
| (a) | 14 | note line present; `2446 read` / `53349 read` / `4194304 read` / `constant` / `ABSENT` **by name** |
| (b) | 14 | 12 channels differ from the default by 251,518 to 1,109,710 px; `mask-a` and `emissive` are identical (0 px) and say ABSENT |
| (c) | 14 | note mean == reader mean within 1 on every per-placement, per-vertex and per-texel channel; the two absent names assert that the READER also calls them absent |
| (c) floor | 1 | the same tolerance REFUSES identityraw's mean (124.282) against sky's (129.305) |
| (d) | 1 | `WW_LODL_AO=1` with `WW_LODL_CHANNEL` unset is byte-identical to `WW_LODL_CHANNEL=ao` (351,578 B vs 351,578 B) |
| (e) | 3 | refused BY THE NAME GIVEN; the refusal lists the known names; the refused render is byte-identical to the default (6,844 B vs 6,844 B) |

### 5.2 Every floor shown RED

A floor that has never fired is a pre-registration, not a result
(`ww-test-harness-add` s9). Two sabotaged copies of the green render directory, the checker
re-run on each, nothing else changed:

**Run 1** -- `sky.png` replaced by `default.png`; `mask-a.png` and `unknown.png` replaced by
`identity.png`; `ao_way_back.png` replaced by `default.png`; `default_tex.png` replaced by
`default.png`; `ground.log`'s `mean 61.299` rewritten to `mean 99.999`:

```
FAIL (floor) the flat default and the textured default are DIFFERENT pictures, ...
FAIL (b) sky: render differs from the default, 0 px
FAIL (c) ground: note mean 99.999 vs reader 61.299 (tolerance 1)
FAIL (b) mask-a: absent, so its render is IDENTICAL to the default (1109710 px differ, must be 0)
FAIL (b) emissive: absent, so its render is IDENTICAL to the default (1478400 px differ, must be 0)
FAIL (d) WW_LODL_AO=1 ... is byte-identical to WW_LODL_CHANNEL=ao (351578 B vs 6844 B)
FAIL (e) the refused render is byte-identical to the default (95460 B vs 6844 B)
48 checks, 7 failures
```

**Run 2** -- `selfao`'s note line deleted from its log; `nosuchchannel` rewritten to
`somechannel` in the refusal; `ABSENT` rewritten to `quiet` in `mask-a`'s note:

```
FAIL (a) selfao: note line present, 0 read
FAIL (c) selfao: note mean none vs reader 234.764 (tolerance 1)
FAIL (a) mask-a: the note line says ABSENT by name -- ... quiet on this bake -- tile 0,3 is BC1 ...
FAIL (e) an unknown name is refused BY NAME -- ... REFUSED "somechannel" ...
48 checks, 4 failures
```

So: (a) fires on a missing note line AND on an absent channel that stops saying ABSENT;
(b) fires in both directions -- a channel that stops differing and an absent one that starts;
(c) fires on a wrong mean and on a missing one; (d) and (e) fire on a byte difference; and
the (c) tolerance is not vacuous because the same 1-unit bar refuses a wrong pairing while
accepting all fourteen right ones. The sabotaged copies were deleted afterwards.

### 5.3 One real defect the gate found, in the gate

The first run came back **48 checks, 1 failure**: `(a) normal: note line present, 0 read`.
The viewer was right; the CHECKER's population parser knew `N placements read`,
`N vertices read` and `N tiles read` but not the `normal` line's wording
(`16 tiles, 4194304 content texels a channel`), so it read the count as 0. Fixed in
`count_of()` by adding the two missing phrases, re-run on the same renders: 48/0. Recorded
here rather than quietly repaired because a gate whose first run is green has usually not
been run.

---

## 6. The neighbours (gate G5)

All on the FINAL exe (2026-09-18 08:40:07, sha1 `62efc25c3871`), one NifSkope at a time,
game and NifSkope verified down before each. "Owner's count" is the number the brief carries
for each of them.

| spell | owner's count | on the final exe | verdict |
|---|---|---|---|
| `render_shot.sh` | 82 / 0 | **82 checks, 0 failures**, PASS (09:04:18-09:06:42) | unchanged |
| `lodl_open.sh` | 23 / 0 | **23 checks, 0 failures**, PASS (09:06:48-09:08:27) | unchanged |
| `native_open.sh` | 17 / 0 / 2 | **17 checks, 0 failures, 2 skipped**, PASS (09:08:33-09:12:39) | unchanged |
| `native_lighting.sh` | 14 / 0 | **14 checks, 3 failures**, FAIL (09:12:45-09:13:48) | see below -- NOT this lane |
| `lodgen_native.sh` | (all green) | **87/0, 56/0/1, 17/0/0, 15/0/1, 29/0**, rc 0 (09:15:25-09:21:21) | unchanged |

### 6.1 `native_lighting.sh` is red, and it is red on the RUNG too

The honest before/after, because the brief asks for one: the SAME spell was run on the rung
exe `release/NifSkope.before_chanview1.exe` and produced **the same three failures, with the
same numbers**:

```
FAIL gate (b): darkest-fifth IoU own vs flat is 1.000, bar 0.800 (measured 0.705 here, 0.858 on the rung)
FAIL gate (b): own-minus-flat blockSD is 0.00 over 266 blocks, floor 3.50
FAIL gate (d): at the oblique, luma orders west > flat > east on 0.00% of 311795 covered pixels, bar 99.00%
```

Before == after, so G5 holds: this lane did not move it. The cause is a FIXTURE collision,
not code, and it was measured rather than guessed -- the two texture caches the gate compares
are now byte-identical:

```
scratchpad/nativeview1_20260912/work/sheetcache/Textures/LODLSheets/Commonwealth.VT.2.0.4.c.DDS  fd2436e7670e1fc1b3751d3db2a19e5ebafb681e
scratchpad/nativeview2_20260912/work/flatcache/Textures/LODLSheets/Commonwealth.VT.2.0.4.c.DDS   fd2436e7670e1fc1b3751d3db2a19e5ebafb681e
```

The `flatcache` directory is dated 2026-09-16, four days after the `sheetcache`, so the
"flat" arm appears to have been regenerated or copied with the "own" content. The gate is
therefore comparing a picture with itself, which is exactly the shape its own numbers
describe: IoU 1.000, blockSD 0.00, 0.00% ordered. **A gate comparing a file with itself must
not read as a code defect**; it should regenerate the flat arm or SKIP with the reason named.
Left for its owner, with the measurement above; a background task chip was raised for it.

---

## 7. Build

| | |
|---|---|
| exe at launch (the rung) | `release/NifSkope.exe` 2026-09-18 07:31:05, 22,686,720 B, sha1 `a63e26b9e7e7...`, preserved as `release/NifSkope.before_chanview1.exe` |
| build 1 | rc **0**, exe 2026-09-18 08:34:43, 22,715,904 B, sha1 `16c5397d3c9fb13f387cfe1cf9ed487adf42a75a` |
| build 2 (the terrain content-texel census added) | rc **0**, exe **2026-09-18 08:40:07.879937600 +0200, 22,718,976 B, sha1 `62efc25c3871610519f48523aae501b728ddd7e4`** |
| final exe | build 2, unchanged since; `find src res -newer release/NifSkope.exe` is EMPTY at 09:24, so the exe is current with every source this lane touched |

MSYS2 UCRT64, `export PATH=/ucrt64/bin:/e/Tools/GIT/mingw64/bin:$PATH`,
`mingw32-make -f Makefile.Release -j8`; make's exit code is the gate, and it was 0 both
times. Only pre-existing warnings; nothing new. The game and NifSkope were verified down
before every build and before every one of the ~60 exe runs
(`tasklist | grep -i -E "Fallout4|NifSkope"` as its own command). `NifSkope_inuse_2000.exe`
was never touched, and neither was any `NifSkope.before_*.exe`,
`NifSkope.archlock1_rung.exe` or `NifSkope.at_0117.exe`.

### 7.1 What the lane touched, and what it did not

`find src res tests docs -newer <launch stamp>`:

```
docs/LODGEN_NATIVE_LODO_LODI.md
src/btdterrain.cpp
src/lodinative.cpp
src/lodinative.h
src/lodtsheets.cpp
src/lodtsheets.h
src/nifskope_ui.cpp
tests/spells/lodl_channels.sh
tests/spells/lodl_channels_check.py
tests/spells/lodl_channels_table.py
```

plus `.claude/skills/nifskope-ww-render-shot/SKILL.md` and `MISTAKES.md`. **Nothing under
`src/lodgen*`, `src/nativeemit*` or `src/io/`**: no writer, no format, no bake default was
touched, which is the brief's constraint and is checkable from that list. `src/glview.cpp`
was NOT needed. `src/nifskope_ui.cpp` is one added line and its reason is stated in section
2.1: without `Scene::DoVertexColors` a flat-byte channel is painted into the vertex colour
and never shown, so the seam forces it, exactly as it already forces it for `WW_LODL_AO`.

All six sources verified 100% LF (CRLF count 0) by Python byte count, as are the three new
`tests/spells` files and the two documents.

**G4 (no bake output changed)**: `find scratchpad/slab1_20260918/after -newer
scratchpad/chanview1_20260918/LAUNCH_STAMP` is EMPTY, re-checked at 08:55 and again at the
end. Nothing was re-baked; every file this lane read was already on disk.

---

## 8. Docs, and the skill text for the director

### 8.1 `docs/LODGEN_NATIVE_LODO_LODI.md`

A new final section, **"Viewer -- every baked channel, one switch (`WW_LODL_CHANNEL`)"**,
after the "Lighting -- the terrain sheets are MODEL-space normal maps" section. It carries
bungo's 06:0x and 06:1x words, the seam (which file parses the name, which two consume it,
why `nifskope_ui.cpp` needed one line), the **fourteen-name table with provenance on every
line** (which file and which byte offset or sheet role each channel's bytes live at, keyed to
the sections of that same document: s3.1 for the `.lodo` vertex bytes, s4.1c / s4.3 / s4.7 /
s4.8 for the `.lodi` ones, the `.lodt` roles for the sheets), the note-line contract and the
two-population rule, the legacy mask law written out
(`roughness = 1 - lodgenLegacyGloss(1.0, glossGreen) = 1 - glossGreen` for a terrain layer,
metallic 0 by bungo's ruling, with the bake's per-layer census named as the provenance), and
the gate.

### 8.2 The `nifskope-ww-render-shot` skill -- the text to apply to BOTH trees

Two edits to `.claude/skills/nifskope-ww-render-shot/SKILL.md`. Applied to the repo copy in
this tree; the director applies the same to the other.

**Edit 1, the frontmatter description.** Replace

```
WW_LOD_CHANNEL for the generated vertex channels)
```

with

```
WW_LOD_CHANNEL for the generated vertex channels, WW_LODL_CHANNEL=<name> for the native .lodo/.lodi/.lodt far field's own baked channels)
```

**Edit 2, a new subsection** inserted immediately after the existing "Channels: 1 identity
hashed colour per object, ... `tests/spells/lod_channel_preview.sh` is the gate." paragraph
and before "## Object chunks:", verbatim:

````markdown
### The NATIVE far field has its OWN channel switch: `WW_LODL_CHANNEL=<name>` (2026-09-18, lane CHANVIEW1)

`WW_LOD_CHANNEL` above is the STOCK path: a numbered channel, on a `.bto`/`.btr`, through
`res/shaders/fo4_default.frag`. It does nothing on the native pair. The `.lodo`/`.lodi`/`.lodt`
far field carries different bytes, and they are viewed by NAME:

| name | what it paints | where the byte lives |
|---|---|---|
| `identity` | per-placement colour, the same hash as stock channel 1 | `.lodi` instance identity |
| `identityraw` | that identity's low byte as grey | `.lodi` instance identity & 0xFF |
| `sky` | per-placement sky visibility | `.lodi` byte 0x11 |
| `ground` | ground-contact blend, PLACEMENTS and TERRAIN in one ramp | `.lodi` byte 0x12 (terrain = the ramp at the surface, constant 255) |
| `seed` | tree seed hashed to colour, **0 = not a tree = black** | `.lodi` byte 0x13 |
| `sway` | per-vertex leaf sway | `.lodo` vertex byte 0x0E |
| `selfao` | per-vertex self-AO | `.lodo` vertex byte 0x0F |
| `ao` | **exactly what `WW_LODL_AO=1` has always drawn** | `.lodi` v6 vertex AO x placement AO; terrain = mask sheet B |
| `mask-r` | terrain roughness | `.lodt` role-5 sheet, R |
| `mask-g` | terrain metallic | role-5 sheet, G |
| `mask-b` | terrain sky AO | role-5 sheet, B |
| `mask-a` | terrain ground cover | role-5 sheet, A -- **a BC1 sheet has none**, and the switch says so |
| `emissive` | role-6 emissive sheet as base colour, **texturing ON** | `.lodt` role 6, absent on most bakes |
| `normal` | role-2 MSN sheet as base colour, **texturing ON** | `.lodt` role 2 |

Four things that decide whether the picture is worth taking:

* **`WW_RENDER_FLAT=1` for the twelve flat-byte names, and NOT for `normal`/`emissive`.**
  Those two are textures; render them with texturing on and `WW_LOD_CHANNEL=12` (raw base
  colour, unlit, no tone map) or you photograph the lighting instead of the sheet.
* **The switch needs the pair AND the sheets pinned**: `WW_LODL_OBJECTS=<file.lodi>`,
  `WW_LODL_SHEETS=<dir with the .lodt>`, `WW_LODL_REGION=x0,y0,x1,y1,level`,
  `WW_LODI_LEVEL`, `WW_LODI_SLOT`, opened on the matching `.lodl`. A `.lodi` at v5 has no
  vertex-AO stream and no per-vertex self-AO, so `ao` and `selfao` come out empty on it --
  check the version before blaming the switch.
* **Read the note line, do not trust the render.** Every name prints
  `WW_LODL_CHANNEL=<name>: <what> from <file>, N placements|vertices|texels read; min, max,
  mean`, read back from what was UPLOADED. A terrain channel prints two lines over two
  populations -- the bilinear resample at the grid vertices (what the picture is made of)
  and the content-texel census over the decoded tiles (the file's own number). Quote the
  census when the report quotes the file.
* **A channel whose render is byte-identical to the default render is NOT WIRED.** Take the
  default of the same framing every time and count the differing pixels before captioning
  anything (root `MISTAKES.md` 05:1x). The two names a bake genuinely lacks invert that
  floor: they must be identical AND must say `ABSENT` by name.

An unknown name is refused by the name given and lists the known names; it draws the default
scene, so a typo photographs as a perfectly good picture of nothing. Gate:
`tests/spells/lodl_channels.sh`.
````

---

## 9. For `WW_CHANGES.md` and `HANDOFF.md` (NOT edited by this lane)

### 9.1 The `WW_CHANGES.md` paragraph

> **Native LOD: every baked channel is viewable, one switch (2026-09-18).**
> `WW_LODL_CHANNEL=<name>` paints one baked channel of the native far field on the same seam
> `WW_LODL_AO` is read at: `identity`, `identityraw`, `sky`, `ground`, `seed`, `sway`,
> `selfao`, `ao` on the placements and their meshes, and `mask-r` (roughness), `mask-g`
> (metallic), `mask-b` (sky AO), `mask-a` (ground cover), `emissive` and `normal` on the
> terrain through the existing sheet sampler. Every name prints a note line -- source file,
> N placements or vertices or texels read, min, max, mean -- read back from what was
> uploaded, not from what was intended; a channel the bake does not carry says `ABSENT` by
> name with the reason and draws the default view; an unknown name is refused by the name
> given and draws nothing different. `WW_LODL_AO=1` is unchanged and is byte-identical to
> `WW_LODL_CHANNEL=ao` at every framing. Viewer only: no writer, no format, no bake default
> moved. Gate `tests/spells/lodl_channels.sh` (48 checks); documented in
> `docs/LODGEN_NATIVE_LODO_LODI.md` under "Viewer -- every baked channel, one switch".

### 9.2 The `HANDOFF.md` LANDED block

> ## LANDED -- CHANVIEW1, 2026-09-18 09:2x -- every baked channel of the native path is viewable
>
> **The exe.** `release/NifSkope.exe` 2026-09-18 08:40:07, 22,718,976 B, sha1
> `62efc25c3871610519f48523aae501b728ddd7e4`. The rung is
> `release/NifSkope.before_chanview1.exe` (07:31:05, sha1 `a63e26b9e7e7...`).
> **bungo's open NifSkope window needs a restart to get this.**
>
> **What landed.** One viewer switch, `WW_LODL_CHANNEL=<name>`, fourteen names, on the same
> seam `WW_LODL_AO` uses. Six sources: `src/lodinative.{h,cpp}` (the name parse and the eight
> object channels), `src/btdterrain.cpp` (the six terrain channels, through the existing sheet
> sampler -- no second decoder), `src/lodtsheets.{h,cpp}` (the blue-only mask reader widened to
> R/G/B/A; `maskAo()` is now one call into it, so the AO path and the channel path read the
> same texels by the same code), `src/nifskope_ui.cpp` (one line: a non-empty
> `WW_LODL_CHANNEL` also sets `Scene::DoVertexColors`). No writer, no format, no bake default.
>
> **Proved, not asserted.** Every channel's render differs from the default render of the same
> framing at BOTH framings (251,518 to 1,255,009 pixels); every note line's mean equals an
> independent Python reader's mean to three decimals; `WW_LODL_AO=1` on the new exe, on the
> rung exe, and `WW_LODL_CHANNEL=ao` all produce byte-identical PNGs (close sha1
> `d339bc06f9d274d8ea0bef78b195bbee89031a9f`, full `67214b64bdc22e706f8828a299740cf64f5bc8bd`).
>
> **Honest absences, declared before the pictures.** On this bake `mask-a` (ground cover) and
> `emissive` carry no bytes -- the mask sheet is BC1, which has no alpha at all, and the
> container has no role-6 sheet. Both render identically to the default AND say so by name.
> `mask-g` (metallic) is `constant 0 (legacy materials)`: the chunk's 17 layers are all legacy,
> and metallic is derived from a PBRM or not at all, by bungo's ruling.
>
> **Gate.** `tests/spells/lodl_channels.sh`, 48 checks, 0 failures, 1m48s. Every floor
> demonstrated red on a sabotaged copy (report section 5.2).
>
> **Neighbours on the final exe.** `render_shot.sh` 82/0, `lodl_open.sh` 23/0, `native_open.sh`
> 17/0/2, `lodgen_native.sh` all green. `native_lighting.sh` reads 14/3 -- **and reads exactly
> the same 14/3 on the rung**, so this lane did not move it; its two sheet-cache fixtures have
> become byte-identical (`scratchpad/nativeview1_20260912/work/sheetcache` vs
> `scratchpad/nativeview2_20260912/work/flatcache`, same sha1), so the gate compares a picture
> with itself. Owed to its owner.
>
> **Pictures.** 31 in `scratchpad/chanview1_20260918/images/`: every channel at both framings,
> one labelled contact sheet, the mask sheet's four channels as texel crops of one window, and
> the source gloss beside the baked roughness for one layer.
>
> **Docs.** `docs/LODGEN_NATIVE_LODO_LODI.md` gains a viewer section with the channel table and
> provenance per line. `.claude/skills/nifskope-ww-render-shot/SKILL.md` gains the native
> channel list beside the stock one (the text is in the lane report section 8.2; the director
> applies it to the other tree).

---

## 10. The rows for bungo

His 06:0x question was "Anything else I'm missing?", so this is the list with the answer
beside each one, on chunk 4.4.-12:

| what he asked for | picture | what the chunk actually carries |
|---|---|---|
| leaf sway bake | `chunk_sway_{full,close}.png` | 0..255, mean 8.9 -- almost all of it is zero, because only 147 of the 2,446 placements are trees |
| identity bake | `chunk_identity_*`, `chunk_identityraw_*` | 2,446 distinct identities, every placement its own colour |
| sky visibility bake | `chunk_sky_*` | 0..255, mean 129.3, 234 distinct values |
| ground-contact blend | `chunk_ground_*` | 0..255, mean 61.3 -- most placements sit well clear of the surface |
| ground cover | `chunk_mask-a_*` | **not on this bake**: the mask sheet is BC1 and carries no alpha at all (`coverTiles 0`) |
| roughness | `chunk_mask-r_*`, `gloss_vs_roughness.png` | 66..239, mean 180.4, every one of the 17 layers served by the legacy rule |
| metallic | `chunk_mask-g_*` | **constant 0 (legacy materials)** -- there is no metallic in a legacy material to read |
| specular / gloss, and the inversion | `gloss_vs_roughness.png` | source gloss mean 69.0, baked roughness mean 186.0, and they sum to exactly 255 |
| the AO he already reads | `chunk_ao_*` | unchanged, byte for byte |
| (not asked, but in the files) | `chunk_seed_*`, `chunk_selfao_*`, `chunk_mask-b_*`, `chunk_normal_*`, `chunk_emissive_*` | the seed, the per-vertex self-AO, the sky-AO sheet, the MSN sheet, and an emissive sheet the bake does not write |

Wetness and shore are not baked on the native path, by the director's ruling, and are not in
the list.

---

## 11. `MISTAKES.md` entries from this lane

Two, both spliced at the top of the root ledger.

1. **2026-09-18 08:0x -- a second reader believed before it agreed with the shipped reader.**
   The lane's Python reader first reported 10,691 placements for a chunk the viewer draws
   2,446 of, because it walked the `.lodi` chunk table SOUTH-up while `lodiChunkAt`
   (`src/lodifile.cpp:241`) walks it NORTH-up; and it read the normal sheet as role 4
   (HEIGHT, R16) instead of role 2 (MSN). The rule: **a second reader is not believed until
   one of its numbers reproduces a number the shipped code already prints.**
2. **2026-09-18 09:0x -- a decoder nobody could catch, because the data that would have
   caught it is absent.** The same reader's `bc3_alpha()` used palette weights summing to 8
   and 6 where they must sum to 7 and 5, so `a0 = a1 = 255` decoded to 291 -- not a byte. It
   survived the table, the switch, the refuter, two builds and a fourteen-channel
   cross-check, because **the only channel that reaches it is `mask-a`, and this bake's mask
   sheet is BC1, which has no alpha**: the function was never called. It surfaced when the
   gloss picture asked it to decode a BC5U `_s` map. The C++ beside it
   (`bc3AlphaBlock`, `src/lodtsheets.cpp:258`) was correct all along. Nothing reported before
   the fix is affected, and that was CHECKED, not assumed -- the reader was re-run and its
   JSON is byte-identical to the table in section 1.3. The rule: **an absent channel does not
   exercise the code that reads it, so "the suite is green" says nothing about that code**;
   feed the branch a synthetic block whose answer is known, or write UNEXERCISED beside the
   green count.

Two smaller ones that were fixed in flight and are recorded here rather than in the ledger,
because each was caught by this lane's own floor before it reached a number anyone would
quote: the `mask-r` note line first reported the 129x129 bilinear resample (mean 181.9) where
the report wanted the 4,194,304-texel census (mean 180.413), a 1.49 miss that looked right --
fixed by printing BOTH lines and saying which is which (section 2.3); and the gate's first
run failed `(a) normal` because the checker's population parser did not know that note line's
wording (section 5.3).

---

## 12. Finished-work skill review

Per the brief, a procedure invented twice becomes a skill. One qualifies.

**`scratchpad/chanview1_20260918/skills_proposed/ww-channel-view-refuter/SKILL.md`** --
*Prove a per-channel debug view is WIRED before any picture of it is captioned.* It was built
three times in this lane alone: once as `refute.sh`/`refute.py` at the close framing, again
at the full framing, and a third time as `tests/spells/lodl_channels.sh`. Its six sections
are the two-render pixel floor (and the trap of comparing a textured channel against a flat
default), the note line that reads back the upload rather than the intent, the independent
reader with the two-population rule and a tolerance that has its own floor, the INVERTED
floor for a channel the artefact genuinely does not carry, the byte-for-byte way back when a
new switch subsumes one bungo already reads, and a closing paragraph saying what it does not
prove (that the bake is correct -- that is a different lane).

Existing skills used and found sufficient, so not amended: `ww-texel-picture` (its rule 1
crop-by-the-metric, rule 3 fixed cell, rule 4 caption arithmetic, rule 5 open the picture and
rule 7 crop-floor-vs-sheet-floor all earned their keep on `channels_contact.png`,
`mask_sheet_texels.png` and `gloss_vs_roughness.png` -- rule 5 caught a clipped caption that
the script's own output could not show), `ww-test-harness-add` (followed, with the one stated
deviation in section 5: a render spell rather than a `WW_*_TEST` translation unit, because
the switch already prints everything the gate reads), `ww-one-reader-per-format` (one reader,
moved into `tests/spells` so the gate carries it rather than copying it), and
`nifskope-ww-render-shot` (which this lane extends rather than re-derives, section 8.2).
