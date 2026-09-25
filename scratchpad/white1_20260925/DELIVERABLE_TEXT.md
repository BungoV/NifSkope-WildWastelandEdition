## HANDOFF text
WHITE1 (2026-09-25, branch white1-20260925 from d40017be): bungo asked "What's up with those white corners though?" about
whole_top_after.png. It is DATA, and there is no code change, build or re-bake.
- **What the band is.** The pale band is the steep outer wall of the western mountain block: land x -76.5..76.5,
  y -76.5..77. Heights run from the -352 floor to 31,000-35,000 units within 3-4 cells, at 50-83 deg.
- **Where the colour comes from.** Bethesda's own dim-4 terrain LOD diffuse paints steep faces pale grey: ring
  +16.7 lum over the core. `--vt-fill-vanilla` copies that colour: ours +10.7, corr 0.977 with vanilla. The
  fill-OFF bake is flat there (0.0).
- **Candidates refuted.** 0 painted LAND in the ring (C3). The file's own texels carry the band (C4). The unlit
  render equals the texels and the lit render is weaker (C5).
- **bungo's "are those mountain peaks?"** No, they are steep faces. 97.6% of pale texels in the western block sit on
  slopes > 30 deg (non-pale 27.0%). 17.8% sit above the block's p80 height (non-pale 20.3%).
- **Owed.** A ruling only if he wants it gone: fade the fill's weight on steep faces, or darken vanilla's cliff
  colour. Either one departs from vanilla on purpose.
- **Side finding.** On 47 seam samples at the wall's lip, LAND's two cells disagree with each other, and the .lodl
  carries a neighbour's value there. It is one sample row and not the band.
Report: E:\Projects\NifskopeWWE-white1\scratchpad\white1_20260925\DONE.md; pictures in its pics/.

## WW_CHANGES text
2026-09-25 WHITE1 (no code change): the pale band round the western mountain block in the whole-map top-down picture
is vanilla's terrain LOD colour.
- **The band.** Bethesda paints steep faces pale grey, ring +16.7 lum. The vanilla fill carries it into our VT sheets
  at +10.7. It sits on the block's 3-4-cell outer wall (50-83 deg).
- **The pale veins inside the block** are the same thing: 97.6% of pale texels are on slopes > 30 deg, and they are
  not peaks.
- **Evidence.** Fill-OFF sheets are flat there; the unlit render equals the file's texels; 0 painted LAND in the band.
- **Tools.** scratchpad/white1_20260925 (cache.py, localise.py, addendum2.py, c1_land.py, pictures.py, renders.sh).

## MISTAKES text
None from this lane. One note for the ledger:
- **The frame height.** shot.sh asks for `WxH+59`. On exe 53f8f18c the window chrome is 35 rows, not 59, so the
  PNG comes back H+24 tall: H=3224 gave 3248. EXTENT1's 3224-row picture therefore implies H=3200 was passed; that is
  inferred, because its call was not recorded. The look-at and upp are the same, so the frame is 12 rows taller at top
  and bottom.
- **Why it matters.** A pixel-compare against EXTENT1's whole_top_after.png must crop rows 12..3235 first. Without the
  crop, every pixel differs; with it, 0 px differ.
- **Where it is recorded.** Section 7 of the ww-whole-map-picture skill.
