# Mesh LOD texture arrays v5 — `<ws>.LodgenArrays*`

**Contract version: sidecar `# lodgen texture arrays 5`; the `.lodm` beside each
set is `kind: "array"` v1.**
**Status: SHIPPED, gated, never flown in a game.** Writer
`lodgenBuildTextureArrays()` in `src/lodgen.cpp`; container writer
`lodgenWriteDdsArray()` in the same file.

One DX10 texture array per **texture size class** and **family** over every
source texture the object chunks reference — tiling or not, which an atlas
cannot do. The layer index rides in **UV2.y** of every vertex of the shape, and
each chunk's manifest gains an `A` line naming the shape's layer and array.

**The stock engine reads none of it**: the chunk shapes keep their own texture
paths, so this is additive and its zero-effort fallback is "do not read the
arrays".

The material sidecar is `docs/LODGEN_LODM_FORMAT.md`; the manifest lines are
`docs/LODGEN_MANIFEST_FORMAT.md` §6; the DDS container and the mip law are
shared with `docs/LODGEN_CARD_SHEETS.md` §6. The design record is
`docs/LODGEN_IMPOSTOR_SPEC.md`.

---

## 1. Files

```
Data\Textures\Terrain\<ws>\Objects\<ws>.LodgenArrays.<W>x<H>_d.DDS       legacy  BC3
                                   <ws>.LodgenArrays.<W>x<H>_n.DDS               BC3
                                   <ws>.LodgenArrays.<W>x<H>_gsaos.DDS           BC3
                                   <ws>.LodgenArrays.<W>x<H>_g.DDS               BC1
                                   <ws>.LodgenArrays.<W>x<H>.lodm        kind "array"

                                   <ws>.LodgenArraysPBR.<W>x<H>_bc.DDS   pbr     BC3
                                   <ws>.LodgenArraysPBR.<W>x<H>_n.DDS            BC3
                                   <ws>.LodgenArraysPBR.<W>x<H>_rmaos.DDS        BC3
                                   <ws>.LodgenArraysPBR.<W>x<H>_e.DDS            BC1
                                   <ws>.LodgenArraysPBR.<W>x<H>.lodm     kind "array"

                                   <ws>.LodgenArrays.txt                 one sidecar for the run
```

**`<W>x<H>` is the PER-LAYER TEXTURE SIZE**, e.g. `256x256`. That is the
opposite of the card arrays, where `<WxH>` is the whole sheet
(`docs/LODGEN_CARD_SHEETS.md` §1.2). The two file names look alike; the numbers
do not mean the same thing.

The game path the shapes and the `.lodm` carry is
`data\Textures\Terrain\<ws>\Objects\<ws>.LodgenArrays…` — lowercase `data\`,
as the writer emits it. Compare case-insensitively.

The grouping key is **`family | <W>x<H>`**: one array per size class per family,
so a chunk can carry both kinds at once.

---

## 2. Ordering: arrays run BEFORE the atlas, the merge runs after both

1. chunks are written;
2. **texture arrays** — keys on the shapes' own diffuse paths, which the atlas
   is about to repoint;
3. atlas (`--atlas`), optional;
4. merge (`--merge`);
5. card arrays;
6. far-ring simplification.

Getting 2 and 3 the wrong way round makes every array layer key on the atlas
sheet, which is one path for the whole worldspace.

---

## 3. What a layer holds

The channel roles are the `.lodm` family contract
(`docs/LODGEN_LODM_FORMAT.md` §2.1). What is specific to a **mesh** layer:

| sheet | legacy law | pbr law |
|---|---|---|
| `_d` / `_bc` | the source's colour with its own alpha | the source `.lodm`'s base colour |
| `_n` | the source normal's **X and Y**; height **NEUTRAL (128)**; sway **0** | the source `.lodm`'s normal; blue = height only when it says `heightInBlue` |
| `_gsaos` / `_rmaos` | gloss = smoothness × the vanilla `_s` map's G; specular = the map's R (the normal's alpha without a map) × the specular strength; **AO neutral (255)**; subsurface mask **1** for a source an alpha-tested shape uses | the source `.lodm`'s third texture **raw**; mask as legacy |
| `_g` / `_e` | the colour × its own alpha × the **source's emissive colour**, where the shape is not alpha-tested; **black** where it is; **black** where the emissive colour is black — which is every measured vanilla LOD material | the source `.lodm`'s `emissive` **raw**; black when it names none |

**Height and sway are neutral on a mesh layer, and AO is neutral, on purpose.** A
mesh carries its own sway in vertex alpha and its own AO in vertex colour B
(`docs/LODGEN_VERTEX_PACKING.md`). A card carries them in the sheet because a
card has no vertices to put them on. A consumer must not read a mesh array's
normal-blue as height, or its mask-blue as AO, without checking the kind.

**The emissive multiple** is not in the sheet. `array.emissiveScale` is a list
parallel to `array.layers`, one float per layer, read off the chunk shape the
generator wrote the source's emission into. `lodgen --dump-shapes <file.BTO>`
prints a chunk's shader constants back so a gate can check a layer against its
**source**, not against the pass that wrote it.

---

## 4. The layer reaches the mesh through UV2.y

The array pass writes the layer index into **UV2.y** of every vertex of the
shape, as a plain integer in a half float, and appends
`A <shapeBlock> <layer> <lodm>` to the chunk's manifest.

Two cases a reader must handle:

* **A shape with no UV2** (the object profile without the extra channels) still
  gets its `A` line, and the run's report counts it as `shapesWithoutUv2`. The
  manifest is then the only place the layer exists.
* **`layer == −1`** on an `A` line means the merge concatenated shapes from
  different layers and the layer is **per vertex** in UV2.y. A positive layer
  means the whole shape is on that one.

Read the UV2 offset out of the vertex descriptor, never from a remembered
constant — `docs/LODGEN_VERTEX_PACKING.md` §Objects.

---

## 5. The sidecar `<ws>.LodgenArrays.txt`

One header line, then one line per layer, space-separated:

```
# lodgen texture arrays 5: family class layer lodm color normal mask emissive source emissiveScale (docs/LODGEN_IMPOSTOR_SPEC.md)
```

| # | column | meaning |
|---|---|---|
| 0 | `family` | `legacy` or `pbr` |
| 1 | `class` | `<W>x<H>` |
| 2 | `layer` | the layer index in that array |
| 3 | `lodm` | the array `.lodm` game path |
| 4 | `color` | the source colour texture |
| 5 | `normal` | the source normal texture |
| 6 | `mask` | the source `_s` / third texture |
| 7 | `emissive` | the emissive texture's path where a `.lodm` named one; **the COLOUR texture's path** where the glow rule composed it; `-` where the layer emits nothing |
| 8 | `source` | the source material or diffuse the layer was keyed on |
| 9 | `emissiveScale` | the multiple |

**`emissiveScale` is column 9 and it went on the END** at version 5, so a reader
that indexes the first nine columns by position is unaffected. That is the same
rule the manifest's `C` line follows.

---

## 6. The atlas sheets — a different, older path

`--atlas` is the **stock-engine** path and is not part of the array contract; it
is documented here only because the two live in the same folder and are easy to
confuse.

```
Data\Textures\Terrain\<ws>\Objects\<ws>.LodgenObjects.DDS      diffuse
                                   <ws>.LodgenObjects_n.DDS    normal
                                   <ws>.LodgenObjects_s.DDS    BC5, specular pair
```

`_s` is composed with **each shape's own constants folded in** — R = the map's R
× the specular strength, G = the map's G × the smoothness, 255/255 where a cell
has no map — so every atlased shape then carries slot 7 = the sheet at
smoothness 1 / strength 1, which is what vanilla's chunks carry. That is what
makes two shapes differing only in their constants mergeable.

Vanilla's own `Commonwealth.Objects.DDS` is **DXT1** (4096×2048, 13 mips,
5,592,552 bytes) with `_n` and `_s` both BC5U. `--atlas-bc1` (the stock target's
default) matches it; FO4CS keeps BC3 for the eight-bit alpha.

**The atlas breaks per-material tree motion**: it repoints many buckets onto one
sheet, so several tree textures collapse to a single path. Use arrays where
per-material distinction matters.

**The atlas is dropped entirely on the FO4CS-native target** — see
`docs/LODGEN_NATIVE_LODO_LODI.md` §1.

---

## 7. DDS container

Identical to `docs/LODGEN_CARD_SHEETS.md` §6: DX10, `dxgiFormat` **77**
(`BC3_UNORM`) for colour / normal / mask and **71** (`BC1_UNORM`) for the
emissive, `resourceDimension` 3, `arraySize` = the layer count, payload
**layer-major** with each layer's whole mip chain contiguous, rows tightly packed
at `ceil(w/4)·blockBytes`, mip filter a 2×2 box rounded half-up stopping while
`w > 4 && h > 4`.

The BC1 mip filter forces alpha to 0xFF down the chain, which is harmless here
because the emissive is opaque by contract.

---

## 8. Invariants a reader may assume

1. Every layer of one array has the same dimensions and the same mip count.
2. `array.layers.size() == array.emissiveScale.size() == arraySize` in the DDS
   header.
3. A layer index is stable for the life of a bake: nothing is inserted or
   removed after the arrays are written, and the merge does not renumber layers
   (it only sets `−1` where a shape spans several).
4. The far-ring simplifier groups by (identity index, **layer**), so no collapse
   crosses a layer and no vertex's UV2.y is interpolated between two layers.
5. Layer contents are **not** a re-encoding of anything: each layer is encoded
   once from the source texture.

## 9. Gate

`tests/spells/lodgen_texture_arrays.sh` — the game's sources (legacy), then a
loose root with one pbr source `.lodm` (its `_rmaos` layer equals its `_bc`
layer block for block, its `_e` layer decodes to the same, and its
`emissiveScale` of 2.5 reaches both the sidecar column and the `.lodm`). The
emissive is **decoded**, not just headed: a glow-rule layer must equal its
diffuse × that diffuse's alpha × the source's emissive colour and **not** the
plain diffuse; a layer the sidecar says emits nothing must decode black; and
where an opaque source's emissive colour is black, that layer must decode black
**while its own diffuse × alpha does not** — the half of the check that fails if
the colour multiply is dropped.

Measured on Sanctuary (−20,24)…(−19,25): one class, 256×256, eight layers, both
arrays exactly header + 8 × mip chain, every vertex of all 14 shapes carrying its
layer.

## 10. Sample files

**None on disk.** No array output exists in this tree or in bungo's mod folder —
see `scratchpad/handoff_fo4cs/README.md` §5.

---

## Provenance

`src/lodgen.cpp` sha256 `6d7388c53a13343e`,8286 lines;
`src/lodgenmanager.cpp` read at the same time.

| claim | line | anchor |
|---|---|---|
| file stem `.<W>x<H>` / `PBR.<W>x<H>` | `lodgen.cpp:4255` | `QString( "%1.%2" ).arg( cls.pbr ? QStringLiteral( "PBR" ) : QString() ).arg( sizeKey )` |
| grouping key `family\|<W>x<H>`, W/H = layer size | `lodgen.cpp:4144` | `classes[QString( "%1\|%2x%3" )…arg( w ).arg( h )]` |
| the four sheets and their suffixes | `lodgen.cpp:4260-4264` | `const struct { QString suffix; … } sheets[4]` |
| `textures.emissive` always written on an array | `lodgen.cpp:4278` | `tex.insert( QStringLiteral( "emissive" ), gameBase + emSfx );` |
| `array.emissiveScale` parallel to `layers` | `lodgen.cpp:4281-4289` | `arr.insert( QStringLiteral( "emissiveScale" ), scales );` |
| sidecar version 5 header and column order | `lodgen.cpp:4250` | `ss << "# lodgen texture arrays 5: family class layer lodm color normal mask emissive source emissiveScale` |
| layer written into UV2.y; `shapesWithoutUv2` | `lodgen.cpp:4333-4339` | `nif.set<HalfVector2>( row, "UV 2", HalfVector2( Vector2( uv2[0], float( lit.value().second ) ) ) )` |
| `A` line appended to the manifest | `lodgen.cpp:4344` | `lines.append( QString( "A %1 %2 %3" )` |
| the channel laws, legacy and pbr | `lodgen.cpp:3998-4019` | doc comment on `lodgenBuildTextureArrays` |
| arrays run BEFORE the atlas | `lodgenmanager.cpp:2061` | `// before the atlas: the arrays key on the shapes' own diffuse paths` |
| array file and game paths | `lodgenmanager.cpp:2069-2070` | `arrDir + "/" + ws + QStringLiteral( ".LodgenArrays" )` |
| atlas sheet paths | `lodgenmanager.cpp:2088-2089` | `atlasDir + "/" + ws + QStringLiteral( ".LodgenObjects" )` |
| card arrays run after, same folder | `lodgenmanager.cpp:2131-2132` | `arrDir + "/" + ws + QStringLiteral( ".LodgenCards" )` |
| DX10 header, formats 77 / 71, arraySize | `lodgen.cpp:3968-3982` | `const quint32 dx10[5] = { bc3 ? 77U : 71U, 3U, 0U, quint32( layers.size() ), 0U };` |
| payload layer-major | `lodgen.cpp:3965-3966` | `for ( const std::vector<quint32> & layer : layers )` |
| mip law and BC1 alpha forcing | `lodgen.cpp:3878, 3895` | `while ( mw > 4 && mh > 4 …`, `( bc3 ? … : 0xFFU ) << 24` |
