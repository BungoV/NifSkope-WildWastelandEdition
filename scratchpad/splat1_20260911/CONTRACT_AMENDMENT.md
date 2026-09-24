# Amendment text for docs/LODGEN_TERRAIN_VT.md (delivered, NOT applied)

Lane SPLAT1 touched no document. This is the text, with its provenance
(ww-contract-provenance), for the director or for phase B to splice.

Anchors re-read 2026-09-11 16:2x: `docs/LODGEN_TERRAIN_VT.md` is 1211 lines and
the paragraph to replace begins at line 734 with the words
"**Each source diffuse is sampled the same way**". `src/lodgen.cpp` at the same
moment is 450,162 bytes / 10,275 lines, sha1
`d81bfcc94016956a185867ca5f594fee46f7313c`, with `constexpr float TILE` at 6335
and 7623 -- BOTH FILES ARE LIVE UNDER OTHER LANES (CARDS-AGG wrote lodgen.cpp at
15:44 and moved every number by about 137), so re-run
`scratchpad/splat1_20260911/anchors.txt`'s pass before splicing anything here.

## 1. Replace the sampling paragraph in 2.5

OLD (line 734 onward), verbatim:

> **Each source diffuse is sampled the same way**: `u = frac(wx/2048)`,
> `v = frac(wy/2048)` (the bake's world-space tiling), at the mip
> `clamp( log2( max(1, unitsPerTexel / (2048/textureWidth)) ), 0, maxMip )`,
> trilinear. Get the mip wrong and the two bands differ by the texture's own
> high-frequency detail, which is the visible half of a seam.

NEW:

> **Each source diffuse is sampled the same way**: `u = frac(wx/T)`,
> `v = frac(wy/T)` at the mip
> `clamp( log2( max(1, unitsPerTexel / (T/textureWidth)) ), 0, maxMip )`,
> trilinear, where **T = 341.3333 world units, the engine's own landscape
> texture repeat**: twelve repeats a cell, six a quadrant. The runtime and the
> pyramid must use the SAME T or ring 0 seams by the texture's own detail,
> which is the visible half of a seam. `--land-tiling <units>` overrides it for
> a user who has changed `fLandTextureTilingMult`; `--land-tiling 2048`
> reproduces every sheet written before 2026-09-11 byte for byte.

## 2. Add, as its own paragraph after it: where T comes from

> **T IS THE ENGINE'S, AND IT IS NOT IN THE DATA.** No LAND, LTEX or TXST field
> carries a tiling scale -- LTEX is EDID + TNAM + HNAM + SNAM + GNAM, TXST is
> texture paths and flags, checked over every landscape texture in the
> Sanctuary region. The number is read out of `Fallout4.exe` **1.10.155.0**
> (65,319,936 bytes), re-derived from the binary by
> `scratchpad/splat1_20260911/s2c_engine_tiling.py`:
>
> * `fLandTextureTilingMult:Landscape`, one copy, file 0x2C84DD8 / VA
>   0x142C861D8. Its `Setting` record `{vtable, data, name}` at file 0x36E83A8
>   carries **data 0x3FC00000 = 1.5f**; the neighbouring records decode to
>   `bCurrentCellOnly` 0, `iMaxGrassTypesPerTexure` 2, `fTexturePctThreshold`
>   0.005, which is what says the stride and the field order are right. The
>   setting is absent from `Fallout4_Default.ini`, so 1.5 is what runs.
> * The data slot (VA 0x1436E97B0) has exactly ONE code reference, at VA
>   0x1403A74C6: `xmm6 = 4.0 / mult` (0x1403A74E5, 0x1403A74ED; falls back to
>   16.0 at 0x142C4B1BC when the setting is 0), then `xmm2 = 1.0 / xmm6 =
>   mult/4 = 0.375` (0x1403A75FD, 0x1403A760F).
> * The loop it feeds (0x1403A7620 outer / 0x1403A7650 inner, both `cmp .., 0x11
>   ; jl`) is the **17x17 quadrant vertex grid**, and it stores an 8-byte (u,v)
>   pair per vertex at 0x1403A76C7 with `u = col * 0.375`.
> * 17 vertices = 16 quads = one quadrant = 2,048 world units, so the vertex
>   spacing is 128 units and **T = 128 / 0.375 = 341.3333**.
>
> Addresses are for the 1.10.155 build and are re-derived, never typed, by the
> script above.

## 3. Correct the VCLR sentence in 2.5 (section 5 red 1)

OLD:

> Measured on the Commonwealth: **2,362 of 36,864 cells carry a VCLR at all**,
> and over the Sanctuary region (cells -20..-17 x 24..27) every byte of every
> VCLR present is in **249..255** -- white to within 6/255.

NEW:

> Measured on the Commonwealth: **2,362 of 36,864 cells carry a VCLR at all.**
> Over the Sanctuary region (cells -20..-17 x 24..27) **11 of 16 cells carry
> one and the bytes run 203..255**; over cells -20..-17 x 20..23, 16 of 16 carry
> one, over 170..255 (lane SPLAT1, `s3_candidates.py`). VCLR is therefore not
> white, but it is still not the grading: removing the multiply entirely moves
> the sheet's local variance by **0.02 of a 52-unit excess**, and the cells that
> carry NO VCLR show the LARGER excess (63.98 against 52.97). The gate's floor
> stays a blend that drops the weights.

## 4. What this amendment does NOT claim

The tiling change removes the speckle; it does not close the whole-tile colour
difference against vanilla (15.0 and 20.2 of 255 after, against 16.9 and 21.7
before, this lane's own metric). The grading -- vanilla's uniform x0.82-0.83 --
remains the open item "splat calibration vs vanilla grading".
