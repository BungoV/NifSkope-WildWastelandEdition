
### 4.4 The whole-machine hunt, for completeness

`python hunt.py` walked `E:\`, `X:\`, `C:\Users\bungo`, `C:\Program Files`,
`C:\Program Files (x86)`, `C:\ProgramData` — **50004 directories in 900 s** —
looking for any directory holding `<ws>.<level>.<x>.<y>[_msn].(dds|btr|bto)`
outside `E:\Tools\Fallout 4\DataUnpacked`:

    === C. directories holding GENERATED-LOOKING terrain LOD, outside the vanilla unpack (0) ===

**Honest caveat:** that walk hit its 900 s budget and is therefore *not*
exhaustive. The targeted scan in 4.2, which did run to completion over the
actual FO4 mod stack and both tool trees, is the stronger negative.

---

## 2. LENS 2 — SOURCE. **This is the finding that answers bungo's question.**

The CLI (`--dump-land`) is refused by this sandbox, so I wrote my own read-only
ESM walker instead: `esmland.py`, built from the subrecord layouts in
`src/esmdata.cpp` (BTXT/ATXT/VTXT at :305-333, VHGT at :334, VCLR at :351).
It handles GRUP nesting, the 24-byte record header, zlib-compressed records
(flag `0x00040000`) and the `XXXX` oversize subrecord.

### 2.1 Every cell has height. Almost none has texture.

`python esmland.py` over `Fallout4.esm`, worldspace `0000003C`:

    worldspace 0000003C group found: True
    LAND records seen:   36864
    cells with a LAND:   36864
    cell x range -96..95   y range -96..95
    with VHGT 36864   with VCLR 2362   with BTXT 3517   with ATXT(layers) 3937

36864 = **192 x 192**, cells **-96..95** in both axes — *exactly* the extent of
the shipped LOD texture pyramid measured in section 0, and independently
corroborated by the heightmap filename found in 4.2
(`Commonwealth.HeightMap.-96.-96.95.95.-8316.44862.dds`).

So the source geometry is complete: **every one of the 36864 cells carries VHGT.**
But:

| subrecord | cells carrying it | share |
|---|---|---|
| VHGT (height) | 36864 | **100 %** |
| ATXT/VTXT (texture layers) | 3937 | 10.7 % |
| BTXT (base texture) | 3517 | 9.5 % |
| VCLR (hand-painted vertex colour) | 2362 | **6.4 %** |

`python landmap.py`:

    cells with ANY texture assignment: 3955 of 36864 (10.7%)
    their bounding box: x -36..32   y -41..32

The ASCII maps (in `landmap_out.txt`) show one compact, solid block and nothing
else anywhere. **The textured region is the playable Commonwealth and only the
playable Commonwealth.**

Sanctuary Hills sits near cell (-20, 24) — inside that box. **North is +Y in a
Bethesda worldspace, so the mountains bungo is looking at, north over Concord,
are at y > 32: outside the box, with no BTXT, no ATXT and no VTXT at all.**

### 2.2 Yet Bethesda shipped rich, distinct, *more saturated* LOD out there

`python landmap.py` classified all 2304 level-4 tiles by whether all 16 of their
cells are textured or none are, then decoded 40 random tiles of each class at
mip 3 (64x64):

    tiles fully inside the textured region:  214
    tiles fully outside:                    2023
    mixed:                                    67

    TEXTURED (playable)  n=40  lum 66.4+-15.6  meanSat 0.155+-0.031  lumStd 9.35+-1.97  msn relief 38.0+- 8.4
    UNTEXTURED (outer)   n=40  lum 71.0+-11.5  meanSat 0.191+-0.023  lumStd 7.37+-4.51  msn relief 23.4+-18.5

Read that carefully. In the outer region — where the ESM contains **no landscape
texture data whatsoever** — vanilla's shipped LOD is *brighter* (71.0 vs 66.4)
and **markedly more saturated (0.191 vs 0.155)** than in the playable area. It
still carries real tonal variation (lumStd 7.37) and real normal-map relief
(23.4). And the brief's item 3 already established the tiles are md5-distinct
per cell, so they are not one repeated fill.

**88 % of the worldspace's terrain LOD (2023 of 2304 level-4 tiles) was shipped
with colour that cannot be derived from the ESM, because the ESM has nothing
there to derive it from.**

### 2.3 Why that is the answer

Join 2.1 and 2.2 to xLODGen's own documentation, quoted in 4.3(a):

> "Default diffuse size - size of diffuse texture **in case there are no texture
> layers for the LOD level**, e.g the entire terrain LOD texture is **the default
> landscape texture**. This is to minimize terrain textures of **outer regions
> without landscape textures**."
> "Default normal size - ... **This is to minimize terrain texture of outer
> regions were no terrain textures have been defined.**"

The tool is describing exactly the region measured in 2.1. Out there a
regenerator has nothing to sample, so it emits **one flat default landscape
texture** — optionally shrunk to a few texels — for a region where Bethesda
shipped the most saturated, per-cell-distinct terrain in the game. That is:

* **DESATURATED** — measured directly. The outer region is where vanilla's
  saturation is *highest* (0.191); one default texture replaces it with a single
  hue. This is the symptom the diffuse explains best, and it is the one a
  normal-map theory does **not** explain.
* **FLATTER** — vanilla's outer lumStd 7.37 and msn relief 23.4 both go to
  roughly zero when the tile becomes one flat colour, and the "Default normal
  size" clause shrinks the normal map too — which section 1.4 proved is where
  *all* the shading at distance comes from.
* **DARKER** — only if the default landscape texture is darker than what
  Bethesda baked. **I could not test this**: see UNVERIFIED. The measured
  outer-region vanilla mean is lum 71.0, so the default would have to come in
  below that.

The 54 distinct BTXT base textures counted (`000AB72E` on 879 cells, `00021336`
on 732, `0014BF47` on 536, ...) are all inside the playable box. **There is no
"far mountain base texture" to look up** — that was one of the brief's
hypotheses and 2.1 kills it.

### 2.4 The "not reproducible from landscape textures" claim — MEASURED, and stronger than claimed

The brief asked me to test the widely-repeated claim that Bethesda's Commonwealth
terrain LOD textures are not reproducible by sampling the landscape diffuse
textures alone. For the outer region the claim is true in the strongest possible
form: **there are no landscape textures assigned to sample.** No weighted mean
can be computed because the weights do not exist. Bethesda's outer LOD was
authored by some other route that does not ship in `Fallout4.esm`.
