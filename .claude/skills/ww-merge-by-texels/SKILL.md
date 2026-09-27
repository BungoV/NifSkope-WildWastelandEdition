---
name: ww-merge-by-texels
description: Merge or drop texture-array layers/sheets in the WW lodgen without changing a pixel -- decide by the composed texels, never by path spelling, and gate by a source -> layer texel hash taken from the shipped sheets. Use when a lane dedupes array layers, drops an "empty" sheet (emissive _g/_e, a constant channel), or is told N layers are duplicates.
---

# Merge by texels, gate by source -> layer hash (lane TIDY1, 2026-09-27)

## Why
The audit said "10 of 114 array layers are duplicates" by reading the sidecar: the same material under two
spellings (`c:\projects\fallout4\build\pc\data\materials\lod\X.bgsm` and `materials\lod\X.bgsm`). Measured on the
shipped sheets, only 7 of those pairs were identical. 5 differed in the mask sheet's A byte, which is the
alpha-test flag of the SHAPES that used the spelling (`_gsaos` A = `src.alphaTested ? 255 : 0`), and Wrhs01
differed in gloss (NIF shader smoothness). A path-normalising merge would have silently switched alpha test on
or off for every placement of those shapes. A different-path pair (ElmTrunks under two texture folders) WAS
identical and merges. So the path is not the evidence. The texels are.

Likewise, "vanilla LOD has no glow" (0 of 121 LOD materials) did NOT make every `_g` black: card sets are shot
from the FULL models, and TreeAspen01-03 carry emissive 0.05, so 2 of 16 Boston card arrays have 6 and 46 lit
BC1 blocks (max 8/255). An "every sheet is empty" rule would have thrown that light away.

## Do
1. **Measure first, offline, from the shipped sheets.** Slice each DX10 array per layer: a 148-byte header, then
   layer-major data, each layer carrying a full mip chain (BC1 = 8 B blocks, BC3/BC7 = 16). Hash the colour,
   normal and mask slices of each sidecar row (column 3 = layer, 4 = .lodm, 9 = source). Group by (class,
   hash). That is the true merge count. `scratchpad/tidy1_20260927/gates.py predict <Objects dir>` does this
   read-only, on any bake or on the installed mod.
2. **Test "black" the way the reader sees it.** A BC1 block is black when both 565 endpoints are 0. Count the
   files and layers that are not, and find their source (the card `.txt` has an `emissive <mult> shapes <n>`
   line) before you write "all empty".
3. **In the writer, compare the composed 32-bit layers (all sheets and the emissive multiple), not the key.**
   Use a hash bucket and then an exact compare. The duplicate key is aliased to the kept layer, so a shape's
   UV2.y and its `A` line resolve to it. Log each merge (`KEY = layer N (REPKEY), identical texels`) so a gate
   can resolve old keys to the kept layer.
4. **Gate** (a snapshot of the base bake, taken BEFORE the base can be deleted):
   - OFF (the env switches): every file under `mod/` is sha1-identical to the base.
   - ON:
     - Every base source resolves (directly or through a logged alias) to a layer with the SAME texel hash.
     - Layers fall by exactly the number of aliases.
     - Every manifest `A` line resolves to a listed layer.
     - Every black emissive file is gone, and every lit one is kept byte-identical.
   - Prove the ON gate fails on unchanged output: run compare base-vs-base in `on` mode and require FAILs.

## Don't
- Don't normalise a material path to merge layers: the shapes behind two spellings can differ.
- Don't drop a sheet on a census of the materials alone. Check the sheet's texels: cards come from full models.
- Don't count bytes from a sidecar. Count the per-layer slice size from the DDS itself.
