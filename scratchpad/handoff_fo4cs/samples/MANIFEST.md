# The FO4CS sample set -- every file, its size, its command, its contract

Written by lane IMAGES5, 2026-09-09, on `release/NifSkope.exe` **19:35:14**;
**the four chunk levels were REGENERATED at 21:2x by lane CARDFIT3** on
`release/NifSkope.exe` **21:01:14**, over a card library re-baked under the new
impostor frame law (WW_CHANGES 2026-09-09, "the impostor frame law"). The VT
pyramid was not touched -- it has nothing to do with cards -- so its rows are
IMAGES5's, and its exe is 19:35:14. `Fallout4.exe` and any other `NifSkope.exe`
were checked absent before every run; the gate is inside `make_samples.sh`.

**What changed for a reader:** every card set now carries `card.pad` (the
padding in texels per axis on each side of a frame), `card.mips` is
`1 + log2(min(pad))` so **no shipped mip bleeds across a frame border**, and the
frames are tighter -- seven frame classes here against the four the old ladder
produced, which is why there are seven `LodgenCards.legacy.<WxH>` array sets
instead of four. `card.center` is unchanged in meaning and is now DOCUMENTED as
what it always was: the offset from the object's pivot to the card's centre
(`docs/LODGEN_LODM_FORMAT.md` 3.1). The crossed-quad `_fs.DDS` sheets are
**DXT5 with a real alpha** now, not DXT1 with none.

**`--card-half-aux` was NOT used**, here or before: all four sheets of every card
set are full size, which is the 2026-09-06 ruling's default (the base colour's
alpha is the coverage, so it is the silhouette and never divides). The toggle is
`HALF_AUX=1` on `make_samples.sh` if a set at half resolution is ever wanted.

**The region is the one containing cell (0,0)** -- cells 0..3 x 0..3. At each
far level the sweep bakes every chunk TOUCHING that rectangle, which here is
exactly the one chunk that CONTAINS it, so the four levels are four views of
the same ground, nested:

| level | chunk | cells |
|---|---|---|
| 4 | `Commonwealth.4.0.0` | 0..3 x 0..3 |
| 8 | `Commonwealth.8.0.0` | 0..7 x 0..7 |
| 16 | `Commonwealth.16.0.0` | 0..15 x 0..15 |
| 32 | `Commonwealth.32.0.0` | 0..31 x 0..31 |

---

## The commands

Both are in `make_samples.sh`, which is in this directory and carries the
game-down and one-instance gates. `ESM` =
`X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm`, `DATA` =
`E:/Tools/Fallout 4/DataUnpacked/Data`, `CARDS` =
`<repo>/scratchpad/cardfit_20260909/cards_after` (the same 19 Sanctuary trees,
re-baked 2026-09-09 21:0x under the new frame law; the older library under the
previous law is kept at `<repo>/scratchpad/images_20260909/gen/cards_trees19`),
`S` = this directory. Every path absolute.

**`C<dim>` -- the chunk bakes, one per level:**

```
release/NifSkope.exe -no-gui lodgen "$ESM" --worldspace 3C \
  --terrain-region 0 0 3 3 --dim <4|8|16|32> \
  --out-dir $S/L<dim> --tex-dir $S/L<dim>/textures/terrain/Commonwealth \
  --data-root "$DATA" --arrays --impostors "$CARDS" --cover [--slot-fallback]
```
`--slot-fallback` at dim 16 and 32 only. Identity is ON (the default), which
is what writes the `.BTO` channel contract and the `.manifest.txt`.
**No `--atlas`**: the atlas sheets are the stock engine's draw-call
optimisation and the native target drops them (README section 1); `--arrays`
is the FO4CS path and must run before an atlas would.

**`V` -- the terrain virtual texture, one run for the whole pyramid:**

```
release/NifSkope.exe -no-gui lodgen "$ESM" --worldspace 3C \
  --terrain-region 0 0 3 3 --vt $S/vt --vt-height --cover \
  --out-dir $S/vt/tex --tex-dir $S/vt/tex/textures/terrain/Commonwealth \
  --data-root "$DATA"
```
`--vt-height` is OFF by default and is passed deliberately: it adds the fourth
R16 height sheet per tile (+133% on a tile), and a sample set that omits a
sheet cannot be used to write a reader for it. `vt/tex/` is that run's own
chunk output, kept because `--vt-btr` (on by default) ASSEMBLES the `.btr`
sheets from the pyramid rather than baking them again, so it is the one
artefact that proves the assembly path ran.

---

## Every file

Sizes read from disk by `make_manifest.py`, not typed. `cmd` names which
command above wrote it.

| file | bytes | cmd | what it is | contract |
|---|---:|---|---|---|
| `L16/Commonwealth.16.0.0.BTO` | 11,294,220 | C16 | object chunk (stock bake) | docs/LODGEN_VERTEX_PACKING.md |
| `L16/Commonwealth.16.0.0.BTO.manifest.txt` | 1,928,297 | C16 | per-object manifest, version 2 | docs/LODGEN_MANIFEST_FORMAT.md |
| `L16/Commonwealth.16.0.0.BTR` | 44,336 | C16 | terrain chunk (stock bake) | docs/LODGEN_VERTEX_PACKING.md |
| `L16/textures/terrain/Commonwealth/Commonwealth.16.0.0.DDS` | 174,888 | C16 | terrain albedo sheet (grass tint folded in) | docs/LODGEN_TERRAIN_VT.md section 1 |
| `L16/textures/terrain/Commonwealth/Commonwealth.16.0.0_data.DDS` | 349,648 | C16 | terrain data sheet: R sky-free AO, G flow wetness, B shore proximity, A ground cover | docs/LODGEN_TERRAIN_VT.md section 1 |
| `L16/textures/terrain/Commonwealth/Commonwealth.16.0.0_msn.DDS` | 174,888 | C16 | terrain model-space normal sheet | docs/LODGEN_TERRAIN_VT.md section 2.2 |
| `L16/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenArrays.128x128.lodm` | 710 | C16 | mesh LOD texture array, one per size class and family, plus a kind:"array" .lodm | docs/LODGEN_TEXTURE_ARRAYS.md |
| `L16/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenArrays.128x128_d.DDS` | 109,348 | C16 | mesh LOD texture array, one per size class and family, plus a kind:"array" .lodm | docs/LODGEN_TEXTURE_ARRAYS.md |
| `L16/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenArrays.128x128_g.DDS` | 54,748 | C16 | mesh LOD texture array, one per size class and family, plus a kind:"array" .lodm | docs/LODGEN_TEXTURE_ARRAYS.md |
| `L16/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenArrays.128x128_gsaos.DDS` | 109,348 | C16 | mesh LOD texture array, one per size class and family, plus a kind:"array" .lodm | docs/LODGEN_TEXTURE_ARRAYS.md |
| `L16/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenArrays.128x128_n.DDS` | 109,348 | C16 | mesh LOD texture array, one per size class and family, plus a kind:"array" .lodm | docs/LODGEN_TEXTURE_ARRAYS.md |
| `L16/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenArrays.256x256.lodm` | 2,076 | C16 | mesh LOD texture array, one per size class and family, plus a kind:"array" .lodm | docs/LODGEN_TEXTURE_ARRAYS.md |
| `L16/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenArrays.256x256_d.DDS` | 3,757,316 | C16 | mesh LOD texture array, one per size class and family, plus a kind:"array" .lodm | docs/LODGEN_TEXTURE_ARRAYS.md |
| `L16/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenArrays.256x256_g.DDS` | 1,878,732 | C16 | mesh LOD texture array, one per size class and family, plus a kind:"array" .lodm | docs/LODGEN_TEXTURE_ARRAYS.md |
| `L16/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenArrays.256x256_gsaos.DDS` | 3,757,316 | C16 | mesh LOD texture array, one per size class and family, plus a kind:"array" .lodm | docs/LODGEN_TEXTURE_ARRAYS.md |
| `L16/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenArrays.256x256_n.DDS` | 3,757,316 | C16 | mesh LOD texture array, one per size class and family, plus a kind:"array" .lodm | docs/LODGEN_TEXTURE_ARRAYS.md |
| `L16/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenArrays.txt` | 11,462 | C16 | array sidecar index: family, class, layer, .lodm, colour, normal, mask, source | docs/LODGEN_TEXTURE_ARRAYS.md |
| `L16/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.1024x1024.lodm` | 795 | C16 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L16/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.1024x1024_d.DDS` | 1,392,788 | C16 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L16/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.1024x1024_g.DDS` | 696,468 | C16 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L16/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.1024x1024_gsaos.DDS` | 1,392,788 | C16 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L16/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.1024x1024_n.DDS` | 1,392,788 | C16 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L16/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.128x256.lodm` | 976 | C16 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L16/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.128x256_d.DDS` | 82,068 | C16 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L16/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.128x256_g.DDS` | 41,108 | C16 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L16/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.128x256_gsaos.DDS` | 82,068 | C16 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L16/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.128x256_n.DDS` | 82,068 | C16 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L16/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.128x512.lodm` | 1,319 | C16 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L16/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.128x512_d.DDS` | 327,828 | C16 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L16/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.128x512_g.DDS` | 163,988 | C16 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L16/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.128x512_gsaos.DDS` | 327,828 | C16 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L16/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.128x512_n.DDS` | 327,828 | C16 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L16/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.256x256.lodm` | 783 | C16 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L16/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.256x256_d.DDS` | 82,068 | C16 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L16/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.256x256_g.DDS` | 41,108 | C16 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L16/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.256x256_gsaos.DDS` | 82,068 | C16 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L16/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.256x256_n.DDS` | 82,068 | C16 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L16/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.256x512.lodm` | 1,510 | C16 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L16/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.256x512_d.DDS` | 819,348 | C16 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L16/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.256x512_g.DDS` | 409,748 | C16 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L16/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.256x512_gsaos.DDS` | 819,348 | C16 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L16/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.256x512_n.DDS` | 819,348 | C16 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L16/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.384x512.lodm` | 973 | C16 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L16/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.384x512_d.DDS` | 516,244 | C16 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L16/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.384x512_g.DDS` | 258,196 | C16 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L16/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.384x512_gsaos.DDS` | 516,244 | C16 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L16/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.384x512_n.DDS` | 516,244 | C16 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L16/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.768x1024.lodm` | 1,150 | C16 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L16/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.768x1024_d.DDS` | 3,096,724 | C16 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L16/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.768x1024_g.DDS` | 1,548,436 | C16 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L16/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.768x1024_gsaos.DDS` | 3,096,724 | C16 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L16/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.768x1024_n.DDS` | 3,096,724 | C16 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L32/Commonwealth.32.0.0.BTO` | 24,704,237 | C32 | object chunk (stock bake) | docs/LODGEN_VERTEX_PACKING.md |
| `L32/Commonwealth.32.0.0.BTO.manifest.txt` | 4,081,782 | C32 | per-object manifest, version 2 | docs/LODGEN_MANIFEST_FORMAT.md |
| `L32/Commonwealth.32.0.0.BTR` | 64,464 | C32 | terrain chunk (stock bake) | docs/LODGEN_VERTEX_PACKING.md |
| `L32/textures/terrain/Commonwealth/Commonwealth.32.0.0.DDS` | 174,888 | C32 | terrain albedo sheet (grass tint folded in) | docs/LODGEN_TERRAIN_VT.md section 1 |
| `L32/textures/terrain/Commonwealth/Commonwealth.32.0.0_data.DDS` | 349,648 | C32 | terrain data sheet: R sky-free AO, G flow wetness, B shore proximity, A ground cover | docs/LODGEN_TERRAIN_VT.md section 1 |
| `L32/textures/terrain/Commonwealth/Commonwealth.32.0.0_msn.DDS` | 174,888 | C32 | terrain model-space normal sheet | docs/LODGEN_TERRAIN_VT.md section 2.2 |
| `L32/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenArrays.128x128.lodm` | 831 | C32 | mesh LOD texture array, one per size class and family, plus a kind:"array" .lodm | docs/LODGEN_TEXTURE_ARRAYS.md |
| `L32/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenArrays.128x128_d.DDS` | 153,028 | C32 | mesh LOD texture array, one per size class and family, plus a kind:"array" .lodm | docs/LODGEN_TEXTURE_ARRAYS.md |
| `L32/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenArrays.128x128_g.DDS` | 76,588 | C32 | mesh LOD texture array, one per size class and family, plus a kind:"array" .lodm | docs/LODGEN_TEXTURE_ARRAYS.md |
| `L32/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenArrays.128x128_gsaos.DDS` | 153,028 | C32 | mesh LOD texture array, one per size class and family, plus a kind:"array" .lodm | docs/LODGEN_TEXTURE_ARRAYS.md |
| `L32/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenArrays.128x128_n.DDS` | 153,028 | C32 | mesh LOD texture array, one per size class and family, plus a kind:"array" .lodm | docs/LODGEN_TEXTURE_ARRAYS.md |
| `L32/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenArrays.256x256.lodm` | 2,289 | C32 | mesh LOD texture array, one per size class and family, plus a kind:"array" .lodm | docs/LODGEN_TEXTURE_ARRAYS.md |
| `L32/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenArrays.256x256_d.DDS` | 4,281,572 | C32 | mesh LOD texture array, one per size class and family, plus a kind:"array" .lodm | docs/LODGEN_TEXTURE_ARRAYS.md |
| `L32/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenArrays.256x256_g.DDS` | 2,140,860 | C32 | mesh LOD texture array, one per size class and family, plus a kind:"array" .lodm | docs/LODGEN_TEXTURE_ARRAYS.md |
| `L32/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenArrays.256x256_gsaos.DDS` | 4,281,572 | C32 | mesh LOD texture array, one per size class and family, plus a kind:"array" .lodm | docs/LODGEN_TEXTURE_ARRAYS.md |
| `L32/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenArrays.256x256_n.DDS` | 4,281,572 | C32 | mesh LOD texture array, one per size class and family, plus a kind:"array" .lodm | docs/LODGEN_TEXTURE_ARRAYS.md |
| `L32/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenArrays.txt` | 13,559 | C32 | array sidecar index: family, class, layer, .lodm, colour, normal, mask, source | docs/LODGEN_TEXTURE_ARRAYS.md |
| `L32/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.1024x1024.lodm` | 795 | C32 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L32/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.1024x1024_d.DDS` | 1,392,788 | C32 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L32/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.1024x1024_g.DDS` | 696,468 | C32 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L32/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.1024x1024_gsaos.DDS` | 1,392,788 | C32 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L32/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.1024x1024_n.DDS` | 1,392,788 | C32 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L32/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.128x256.lodm` | 976 | C32 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L32/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.128x256_d.DDS` | 82,068 | C32 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L32/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.128x256_g.DDS` | 41,108 | C32 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L32/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.128x256_gsaos.DDS` | 82,068 | C32 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L32/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.128x256_n.DDS` | 82,068 | C32 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L32/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.128x512.lodm` | 1,319 | C32 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L32/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.128x512_d.DDS` | 327,828 | C32 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L32/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.128x512_g.DDS` | 163,988 | C32 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L32/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.128x512_gsaos.DDS` | 327,828 | C32 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L32/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.128x512_n.DDS` | 327,828 | C32 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L32/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.256x256.lodm` | 783 | C32 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L32/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.256x256_d.DDS` | 82,068 | C32 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L32/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.256x256_g.DDS` | 41,108 | C32 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L32/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.256x256_gsaos.DDS` | 82,068 | C32 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L32/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.256x256_n.DDS` | 82,068 | C32 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L32/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.256x512.lodm` | 1,510 | C32 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L32/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.256x512_d.DDS` | 819,348 | C32 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L32/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.256x512_g.DDS` | 409,748 | C32 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L32/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.256x512_gsaos.DDS` | 819,348 | C32 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L32/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.256x512_n.DDS` | 819,348 | C32 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L32/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.384x512.lodm` | 973 | C32 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L32/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.384x512_d.DDS` | 516,244 | C32 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L32/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.384x512_g.DDS` | 258,196 | C32 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L32/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.384x512_gsaos.DDS` | 516,244 | C32 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L32/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.384x512_n.DDS` | 516,244 | C32 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L32/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.768x1024.lodm` | 1,150 | C32 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L32/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.768x1024_d.DDS` | 3,096,724 | C32 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L32/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.768x1024_g.DDS` | 1,548,436 | C32 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L32/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.768x1024_gsaos.DDS` | 3,096,724 | C32 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L32/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.768x1024_n.DDS` | 3,096,724 | C32 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L4/Commonwealth.4.0.0.BTO` | 2,058,014 | C4 | object chunk (stock bake) | docs/LODGEN_VERTEX_PACKING.md |
| `L4/Commonwealth.4.0.0.BTO.manifest.txt` | 307,970 | C4 | per-object manifest, version 2 | docs/LODGEN_MANIFEST_FORMAT.md |
| `L4/Commonwealth.4.0.0.BTR` | 50,584 | C4 | terrain chunk (stock bake) | docs/LODGEN_VERTEX_PACKING.md |
| `L4/textures/terrain/Commonwealth/Commonwealth.4.0.0.DDS` | 174,888 | C4 | terrain albedo sheet (grass tint folded in) | docs/LODGEN_TERRAIN_VT.md section 1 |
| `L4/textures/terrain/Commonwealth/Commonwealth.4.0.0_data.DDS` | 349,648 | C4 | terrain data sheet: R sky-free AO, G flow wetness, B shore proximity, A ground cover | docs/LODGEN_TERRAIN_VT.md section 1 |
| `L4/textures/terrain/Commonwealth/Commonwealth.4.0.0_msn.DDS` | 174,888 | C4 | terrain model-space normal sheet | docs/LODGEN_TERRAIN_VT.md section 2.2 |
| `L4/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenArrays.128x128.lodm` | 585 | C4 | mesh LOD texture array, one per size class and family, plus a kind:"array" .lodm | docs/LODGEN_TEXTURE_ARRAYS.md |
| `L4/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenArrays.128x128_d.DDS` | 43,828 | C4 | mesh LOD texture array, one per size class and family, plus a kind:"array" .lodm | docs/LODGEN_TEXTURE_ARRAYS.md |
| `L4/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenArrays.128x128_g.DDS` | 21,988 | C4 | mesh LOD texture array, one per size class and family, plus a kind:"array" .lodm | docs/LODGEN_TEXTURE_ARRAYS.md |
| `L4/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenArrays.128x128_gsaos.DDS` | 43,828 | C4 | mesh LOD texture array, one per size class and family, plus a kind:"array" .lodm | docs/LODGEN_TEXTURE_ARRAYS.md |
| `L4/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenArrays.128x128_n.DDS` | 43,828 | C4 | mesh LOD texture array, one per size class and family, plus a kind:"array" .lodm | docs/LODGEN_TEXTURE_ARRAYS.md |
| `L4/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenArrays.256x256.lodm` | 1,613 | C4 | mesh LOD texture array, one per size class and family, plus a kind:"array" .lodm | docs/LODGEN_TEXTURE_ARRAYS.md |
| `L4/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenArrays.256x256_d.DDS` | 2,621,428 | C4 | mesh LOD texture array, one per size class and family, plus a kind:"array" .lodm | docs/LODGEN_TEXTURE_ARRAYS.md |
| `L4/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenArrays.256x256_g.DDS` | 1,310,788 | C4 | mesh LOD texture array, one per size class and family, plus a kind:"array" .lodm | docs/LODGEN_TEXTURE_ARRAYS.md |
| `L4/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenArrays.256x256_gsaos.DDS` | 2,621,428 | C4 | mesh LOD texture array, one per size class and family, plus a kind:"array" .lodm | docs/LODGEN_TEXTURE_ARRAYS.md |
| `L4/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenArrays.256x256_n.DDS` | 2,621,428 | C4 | mesh LOD texture array, one per size class and family, plus a kind:"array" .lodm | docs/LODGEN_TEXTURE_ARRAYS.md |
| `L4/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenArrays.txt` | 7,634 | C4 | array sidecar index: family, class, layer, .lodm, colour, normal, mask, source | docs/LODGEN_TEXTURE_ARRAYS.md |
| `L8/Commonwealth.8.0.0.BTO` | 4,807,296 | C8 | object chunk (stock bake) | docs/LODGEN_VERTEX_PACKING.md |
| `L8/Commonwealth.8.0.0.BTO.manifest.txt` | 709,561 | C8 | per-object manifest, version 2 | docs/LODGEN_MANIFEST_FORMAT.md |
| `L8/Commonwealth.8.0.0.BTR` | 40,314 | C8 | terrain chunk (stock bake) | docs/LODGEN_VERTEX_PACKING.md |
| `L8/textures/terrain/Commonwealth/Commonwealth.8.0.0.DDS` | 174,888 | C8 | terrain albedo sheet (grass tint folded in) | docs/LODGEN_TERRAIN_VT.md section 1 |
| `L8/textures/terrain/Commonwealth/Commonwealth.8.0.0_data.DDS` | 349,648 | C8 | terrain data sheet: R sky-free AO, G flow wetness, B shore proximity, A ground cover | docs/LODGEN_TERRAIN_VT.md section 1 |
| `L8/textures/terrain/Commonwealth/Commonwealth.8.0.0_msn.DDS` | 174,888 | C8 | terrain model-space normal sheet | docs/LODGEN_TERRAIN_VT.md section 2.2 |
| `L8/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenArrays.128x128.lodm` | 653 | C8 | mesh LOD texture array, one per size class and family, plus a kind:"array" .lodm | docs/LODGEN_TEXTURE_ARRAYS.md |
| `L8/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenArrays.128x128_d.DDS` | 87,508 | C8 | mesh LOD texture array, one per size class and family, plus a kind:"array" .lodm | docs/LODGEN_TEXTURE_ARRAYS.md |
| `L8/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenArrays.128x128_g.DDS` | 43,828 | C8 | mesh LOD texture array, one per size class and family, plus a kind:"array" .lodm | docs/LODGEN_TEXTURE_ARRAYS.md |
| `L8/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenArrays.128x128_gsaos.DDS` | 87,508 | C8 | mesh LOD texture array, one per size class and family, plus a kind:"array" .lodm | docs/LODGEN_TEXTURE_ARRAYS.md |
| `L8/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenArrays.128x128_n.DDS` | 87,508 | C8 | mesh LOD texture array, one per size class and family, plus a kind:"array" .lodm | docs/LODGEN_TEXTURE_ARRAYS.md |
| `L8/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenArrays.256x256.lodm` | 1,653 | C8 | mesh LOD texture array, one per size class and family, plus a kind:"array" .lodm | docs/LODGEN_TEXTURE_ARRAYS.md |
| `L8/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenArrays.256x256_d.DDS` | 2,796,180 | C8 | mesh LOD texture array, one per size class and family, plus a kind:"array" .lodm | docs/LODGEN_TEXTURE_ARRAYS.md |
| `L8/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenArrays.256x256_g.DDS` | 1,398,164 | C8 | mesh LOD texture array, one per size class and family, plus a kind:"array" .lodm | docs/LODGEN_TEXTURE_ARRAYS.md |
| `L8/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenArrays.256x256_gsaos.DDS` | 2,796,180 | C8 | mesh LOD texture array, one per size class and family, plus a kind:"array" .lodm | docs/LODGEN_TEXTURE_ARRAYS.md |
| `L8/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenArrays.256x256_n.DDS` | 2,796,180 | C8 | mesh LOD texture array, one per size class and family, plus a kind:"array" .lodm | docs/LODGEN_TEXTURE_ARRAYS.md |
| `L8/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenArrays.txt` | 8,466 | C8 | array sidecar index: family, class, layer, .lodm, colour, normal, mask, source | docs/LODGEN_TEXTURE_ARRAYS.md |
| `L8/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.256x512.lodm` | 966 | C8 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L8/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.256x512_d.DDS` | 327,828 | C8 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L8/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.256x512_g.DDS` | 163,988 | C8 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L8/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.256x512_gsaos.DDS` | 327,828 | C8 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `L8/textures/terrain/Commonwealth/Objects/Commonwealth.LodgenCards.legacy.256x512_n.DDS` | 327,828 | C8 | card array: the octahedral sheets of every card set that shares a grid and a frame size, plus a kind:"cardArray" .lodm | docs/LODGEN_CARD_SHEETS.md + docs/LODGEN_LODM_FORMAT.md |
| `MANIFEST.md` | 34,801 | - | this file | - |
| `make_manifest.py` | 11,768 | - | the script that made this file | - |
| `make_samples.log` | 6,798 | - | its whole stdout, every run | - |
| `make_samples.sh` | 3,551 | - | the script that made everything here | - |
| `vt/Terrain/Commonwealth.VT.16.lodt` | 746,752 | V | ONE LEVEL of the terrain virtual texture: bordered 256-texel tiles, four sheets each (colour, model-space normal, data, R16 height) | docs/LODGEN_TERRAIN_VT.md |
| `vt/Terrain/Commonwealth.VT.2.lodt` | 11,838,720 | V | ONE LEVEL of the terrain virtual texture: bordered 256-texel tiles, four sheets each (colour, model-space normal, data, R16 height) | docs/LODGEN_TERRAIN_VT.md |
| `vt/Terrain/Commonwealth.VT.32.lodt` | 374,016 | V | ONE LEVEL of the terrain virtual texture: bordered 256-texel tiles, four sheets each (colour, model-space normal, data, R16 height) | docs/LODGEN_TERRAIN_VT.md |
| `vt/Terrain/Commonwealth.VT.4.lodt` | 2,983,168 | V | ONE LEVEL of the terrain virtual texture: bordered 256-texel tiles, four sheets each (colour, model-space normal, data, R16 height) | docs/LODGEN_TERRAIN_VT.md |
| `vt/Terrain/Commonwealth.VT.8.lodt` | 1,492,224 | V | ONE LEVEL of the terrain virtual texture: bordered 256-texel tiles, four sheets each (colour, model-space normal, data, R16 height) | docs/LODGEN_TERRAIN_VT.md |
| `vt/Terrain/Commonwealth.VT.lodm` | 1,964 | V | the pyramid index, kind:"terrainVT" | docs/LODGEN_TERRAIN_VT.md section 4 + docs/LODGEN_LODM_FORMAT.md section 5 |
| `vt/tex/Commonwealth.4.0.0.BTO` | 2,058,014 | V | object chunk (stock bake) | docs/LODGEN_VERTEX_PACKING.md |
| `vt/tex/Commonwealth.4.0.0.BTO.manifest.txt` | 305,056 | V | per-object manifest, version 2 | docs/LODGEN_MANIFEST_FORMAT.md |
| `vt/tex/Commonwealth.4.0.0.BTR` | 50,584 | V | terrain chunk (stock bake) | docs/LODGEN_VERTEX_PACKING.md |
| `vt/tex/textures/terrain/Commonwealth/Commonwealth.4.0.0.DDS` | 174,888 | V | terrain albedo sheet (grass tint folded in) | docs/LODGEN_TERRAIN_VT.md section 1 |
| `vt/tex/textures/terrain/Commonwealth/Commonwealth.4.0.0_data.DDS` | 349,648 | V | terrain data sheet: R sky-free AO, G flow wetness, B shore proximity, A ground cover | docs/LODGEN_TERRAIN_VT.md section 1 |
| `vt/tex/textures/terrain/Commonwealth/Commonwealth.4.0.0_msn.DDS` | 174,888 | V | terrain model-space normal sheet | docs/LODGEN_TERRAIN_VT.md section 2.2 |

**159 files, 167,407,424 bytes (159.7 MB).**

---

## Everything was read back through this tree's own validators

Not "it wrote a file". Each of these exited 0 and printed what it parsed;
the `.lodt` check walks every rule of `docs/LODGEN_TERRAIN_VT.md` section 3.4
and verifies **every tile's CRC**.

```
release/NifSkope.exe -no-gui lodgen --lodt-check $S/vt/Terrain/Commonwealth.VT.<dim>.lodt
release/NifSkope.exe -no-gui lodgen --lodm-check <any .lodm above>
```

| level | present tiles | cover tiles | stored bytes | rc |
|---|---:|---:|---:|---|
| 2 | 32 | 30 | 11,744,960 | 0 |
| 4 | 8 | 8 | 2,959,360 | 0 |
| 8 | 4 | 4 | 1,479,680 | 0 |
| 16 | 2 | 2 | 739,840 | 0 |
| 32 | 1 | 1 | 369,920 | 0 |

Every level reports the same three non-colour sheets -- `role 2` msn and
`role 3` data at dxgi 71 (BC1), `role 3` turning 77 (BC3) where a tile has
ground cover, and `role 4` height at dxgi 56 (R16_UNORM).

`--lodm-check` on one of each kind: `Commonwealth.VT.lodm` ->
`kind terrainVT`, 1,964 bytes; `Commonwealth.LodgenArrays.128x128.lodm` ->
`kind array`; `Commonwealth.LodgenCards.legacy.1024x1024.lodm` ->
`kind cardArray`. All three `lodm ok 1`, `version 1`, `family legacy`.

**The pyramid is PARTIAL and says so**: the index carries
`"partial": true` and `extent {south 0, west 0, north 3, east 3}`, because
`--terrain-region` was given. A consumer must read that rather than assume a
whole worldspace.

---

## What is here that the README said did not exist

| README section 5 said missing | now |
|---|---|
| a version-2 `.lodl` | the five INSTALLED files are version 2 (below); no copy here |
| a `.lodt` at any level | five, levels 2/4/8/16/32, all validated |
| a `<WS>.VT.lodm` index | `vt/Terrain/Commonwealth.VT.lodm` |
| a version-2 manifest | four, one per level (`# lodgen manifest 2 ...`) |
| a `kind:"array"` `.lodm` + `LodgenArrays*` | two size classes per level, all four levels |
| a `kind:"card"` `.lodm` + `<id>_oct_*.DDS` | the 19-tree library, `scratchpad/cardfit_20260909/cards_after` (with `pad`) |
| a `kind:"cardArray"` `.lodm` + `LodgenCards.*` | **seven** frame classes at L16 and L32, one at L8 |
| `LodgenObjects*` atlas sheets | **still absent, deliberately** -- `--atlas` is the stock path and the native target drops it |
| `.lodo` / `.lodi` | **still absent** -- there is no writer |

**The card arrays hold 18 of the 19 trees.** `000a7206 TreeBlasted01Lichen`
baked its sheets and its sidecar like the other 18, but `--impostors` wrote no
`_oct.lodm` and no DDS for it -- 18 `.lodm` and 90 `.DDS` against 19 sidecars.
Candidate, unverified: no placement in the region stood on that base, and the
conversion is per placed card.

---

## The whole-worldspace land file is NOT here

It exists already and is 36 MB; copying it would only make a second one to go
stale. Point at:

```
E:\Projects\Fallout 4 Mods\mods\FO4CS\Terrain\Commonwealth.lodl
```
**35,953,294 bytes, header version 2, written 2026-09-09 17:27** -- read from the file by this
script, not copied from an older page. Contract:
`docs/LODGEN_BTD_FORMAT.md`.

**That version is the live incompatibility.** FO4CS's parser pins
`kVersion = 1u`; all five installed `.lodl` files are version 2 as of
2026-09-09 17:27 and would be REFUSED, not misread. The version-1 twins are
the `*.lodt.bak-20260909` copies beside them (2026-09-05 03:14). The
zero-effort fallback on our side needs no rebuild: `WW_LODL_VERSION=1`.
