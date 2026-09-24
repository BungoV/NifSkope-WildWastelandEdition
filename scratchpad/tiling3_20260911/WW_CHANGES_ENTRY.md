# WW_CHANGES entry -- lane TILING3, 2026-09-11

Splice under the LOD generation section. Text only; the overseer owns the file.

---

## 2026-09-11 — Far-terrain sheets reuse Bethesda's own where they exist, and where the grain really comes from

**Far-terrain colour and normal sheets now reuse Bethesda's own, where Bethesda
shipped them.** The far-terrain LOD sheets carried the land textures' tiling
repeat instead of their grain -- the ground read as a stamped pattern, and the
one earlier attempt to remove it (`--land-sample average`) removed the grain with
it. This lane measured where vanilla's grain actually comes from and changed what
the bake writes.

* **`--land-detail-source vanilla` is the new default.** A terrain chunk that has
  a shipped vanilla `_msn` now writes **vanilla's `_msn` byte for byte**; our
  normal bake is skipped for that chunk. A chunk whose cells carry **no land
  paint at all** -- the out-of-bounds ground, whose colour Bethesda baked outside
  the Creation Kit and which cannot be recovered from the ESM -- writes
  **vanilla's colour sheet byte for byte** too. Every other chunk keeps our own
  composite and gains a fitted **crevice term** from vanilla's fine relief.
  `--land-detail-source none` restores the previous bytes exactly.
* **`--land-detail-source vanilla-blend`** is a second value for reshaped terrain:
  vanilla's fine normal detail over our coarse normal, up recomputed so the stored
  normal stays unit length. Not the default.
* **`--vanilla-lod-root PATH`** says where vanilla's sheets are read. They are read
  as **loose files only**, never through the resource stack, so a bake cannot
  "reuse vanilla" by copying its own previously installed output out of the game's
  `Data`. Default `E:/Tools/Fallout 4/DataUnpacked/Data`.
* **`--land-shade K`** (default **-3.242**) is the crevice coefficient, in 8-bit
  luminance levels per unit of detail-normal divergence. It was fitted on seven
  shipped vanilla sheets, with the same sign on 7 of 7 and over its own phase-twin
  floor on 7 of 7. `0` keeps the sheet reuse and turns the shading off.
* **The bake's `report` line now carries a census**, unconditionally:
  `landDetail`, `vanillaRoot`, `msnCopied`, `msnOurs`, `colCopied`, `colOurs`,
  `chunksLayered`, `chunksLayerless`, `chunksLayerlessNoVanilla`, `chunksShaded`,
  `landShade`. A bake that silently failed to find vanilla's sheets says so in its
  own output.
* **An experimental repeat fix is present and OFF.** `--land-sample stochastic`
  applies a smooth deterministic domain warp of world position before the texture
  lookup (`--land-warp`, `--land-warp-lattice`, `--land-warp-octaves`) plus a mip
  bias (`--land-mip-bias`). It removes the repeat and keeps the grain on 6 of the
  7 sheets it was selected on -- not 7 -- so it is not the default. Being a pure
  function of world position it is seamless across chunk and cell boundaries and
  byte-identical at 1 vs 16 chunk threads.

Nothing else moved: `_data`, the BTR and BTO files, the BTO manifest and every
`.lodt` / `.lodm` are byte-identical to the previous build's on both test tiles.
The landscape tiling constant, the road pass and the edge blend were not touched.

**Your open NifSkope window needs a restart to pick this up.**
