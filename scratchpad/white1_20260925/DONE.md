DONE -- WHITE1, 2026-09-25 18:4x (clock read 18:38 at the last measurement). Branch white1-20260925 from d40017be.
No src change, no build, no re-bake: the band is DATA (vanilla's own terrain LOD shows it). Nothing in
mods\FO4CSLOD was written (Commonwealth.VT.16.lodt still 15:50:58, first 12 of sha1 ebbdbfd09d72; .lodl 06:01:57),
so replaced/ is empty.

## Verdict: C1 + C2 together, and the source is vanilla's data
The band is the steep outer wall of the western mountain block. Vanilla's own dim-4 terrain LOD diffuse paints steep
faces pale grey, and `--vt-fill-vanilla` (SEAM1 a6e5e8de, installed 16:14) copies that colour into our sheets.
Where it sits (frame of whole_top_after.png: look-at 0,0, upp 250, 3200x3224, from EXTENT1 pics/top/T.cam.log):
- Land (height != the -352 floor) spans cell x -76.50..76.50, y -76.50..77.03. The band is the outer 3-4 cells of it:
  west x -76.5..-73, north y 77..74, south the same, widest where two walls meet at the corners.
- In the picture: 347 px from the left (10.8%) and 367 px from the top (11.4%), which is what bungo saw.

## Numbers per candidate (localise.txt, c1_land.txt, pictures.out lines "C5")
Ring = the outer 4 cells of the western block (land, x < -43), 875,944 samples. Core = more than 8 cells in,
3,585,759 samples.

| candidate | discriminator | measured | verdict |
|---|---|---|---|
| C1 geometry | height step and slope across the band | -352 to 31,000 units in 3.5 cells (west, y -11..1) and to 35,000 in 3 cells (north). Slope 50-83 deg on the wall; ring mean 50.1 deg against core 23.8; ring share >30 deg 0.765 against core 0.293. The .lodl = LAND: 14,289 of 14,336 samples agree (47 differ, all on cell seams, see C6) over 14 wall cells, both in his MO2 order and in Fallout4.esm. | the WHERE, not the colour: lit fill-OFF has the wall and no pale band |
| C2 vanilla fill | vanilla texels there; fill OFF against fill ON | vanilla lum ring 101.6 / core 84.9 (step +16.7). ON +10.7 (96.2 / 85.5). OFF 0.0 (66.7 / 66.7, flat). corr(ON lum, vanilla lum) over the block 0.977; corr(OFF, vanilla) -0.007 | **the carrier**. The colour is vanilla's. Ours is paler than the fill-off bake but LESS pale than vanilla |
| C3 painted content | LAND BTXT/ATXT winner per cell | ring 1,078 cells, 0 painted; block 5,269 cells, 0 painted; the winner is Fallout4.esm in all 1,078 | refuted: nothing is painted there, so it is default ground |
| C4 VT sampling | .lodt texels read straight from the file; tile grid | the band is in the file's texels (ON +10.7). It is ~112 texels wide and sits at x -76.5, not on a VT.16 tile edge (-80, -64). Border 8 of 528 texels | refuted |
| C5 renderer | albedo-only (WW_LOD_CHANNEL=12) against lit | unlit render ring step +10.5 (= texels +10.7). Lit ON +7.0. Lit OFF -3.8: the headlight DARKENS a steep face | refuted: the renderer weakens the band, it does not make it |
| C6 | -- | side finding, not the band: at 47 samples on cell seams where LAND's two cells disagree (e.g. (-50,76) row 32 = 6408, (-50,77) row 0 = -352), the .lodl carries 6528. That is one sample row at the wall's lip, the same in vanilla LAND. | not the band |

**Does vanilla's terrain LOD look like this?** Yes. Its diffuse texels have the same band, stronger (+16.7 against
our +10.7), in the same place (pics/band_nw_zoom4x.png, band_sw_zoom4x.png, panel "vanilla dim-4 LOD diffuse texels").

## Addendum (bungo: "Are those mountain peaks? all of them")
Pale texel = lum >= the population's p85 AND chroma <= its median. Defined before the split; the same rule for ON and vanilla.
- **Western block (x < -43)**, 5,266,015 samples, pale 12.9%:
  - above the block's 80th height percentile (32,376): pale 0.178, non-pale 0.203 (population 0.200);
  - slope > 30 deg: pale 0.976, non-pale 0.270;
  - corr(lum, height) 0.060, corr(lum, slope) 0.641;
  - veins only (the ring left out): >30 deg pale 0.967 / non-pale 0.231; above p80 0.289 / 0.206;
  - vanilla texels give the same numbers (0.976 / 0.271; 0.183 / 0.202).
- **All land outside the playable map**, 11.3M samples:
  - above p80: pale 0.391, non-pale 0.178;
  - >30 deg: pale 0.949, non-pale 0.144.
  - Height's share there is mostly slope: at a fixed slope, pale share below / above p80 height is 0.006/0.011 (0-30 deg),
    0.17/0.28 (30-40), 0.50/0.63 (40-50), 0.78/0.82 (50-90).
- **Answer: NO, not peaks. Pale = steep faces.** Flat high ground is not pale (1.1%). A steep face is pale at any
  height, and a little more often when it is high up.

## Pictures (untracked, pics/)
- whole_compare.png: the whole map in his frame, 1/3 scale. Panels: his render, a lit fill-OFF render, an unlit ON
  render, vanilla texels, ON texels, OFF texels, slope, pale/steep overlay.
- band_nw_zoom4x.png, band_sw_zoom4x.png: the band zoomed 4x, the same panels. Vanilla's own LOD sits beside ours.
- T_on.png: rows 12..3235 = EXTENT1 pics/top/T.png, 0 px differ (the control). T_off.png: fill-OFF sheets.
  T_on_c12.png: unlit base colour. All 3200x3248, the same look-at and upp as whole_top_after.png.
- tex_{on,off,van}_frame.png: texels sampled into his exact frame (3200x3224).
- There is no before/after pair: there is no fix.
- Renders came from EXTENT1's exe 53f8f18c, copied into release/, the exe that drew his picture. d40017be's later src
  changes (lodgen/lodo/lodinative) were not built.

## What would refute this
- Vanilla dim-4 diffuse texels at the ring NOT paler than the core. Measured +16.7.
- The fill-OFF bake showing the band. Measured 0.0.
- Pale texels not concentrated on steep slopes. Measured 0.976 against 0.270.
- A game view of vanilla terrain LOD there without the pale walls. That is bungo's to check; I have not flown it.

## If he wants it gone anyway (a ruling, not a defect fix; nothing was coded)
The fill reproduces what vanilla draws. Two options would each be a new look policy and need his word:
- fade the fill's weight on steep faces;
- desaturate or darken vanilla's steep-face colour.
Either one departs from vanilla on purpose. I wrote no code for either.

## Commits
Text only, explicit paths: the scripts and .txt outputs under scratchpad/white1_20260925/. PNG and NPY files are
untracked. No push, no merge.

## Skill review
- Loaded:
  - nifskope-ww-worktree-build (worktree created; no build was needed, so the object copy was skipped);
  - ww-artefact-localise (substitute-and-photograph: fill OFF/ON, lit/unlit, vanilla texels);
  - ww-texel-picture (whole panels, nearest x4, one crop for every arm);
  - ww-lodl-offline-census (lodl_bulk scatter, 0 of 300 mismatches against plane_word);
  - ww-whole-map-picture (frame, shot.sh);
  - nifskope-ww-render-shot (channel 12 unlit, frame height read back).
- Not loaded: nifskope-ww-lodgen and nifskope-ww-vanilla-compare. Nothing was baked, and the vanilla side was texels
  read with SEAM1's vanilla_tiles.py, not a staged render.
- Wished for: a "whole-map texel census in his frame" recipe:
  - cache every field at 32 per cell;
  - ring against core, fill OFF/ON/vanilla;
  - map texels into the picture frame;
  - the 12-row offset when the render floor gives 3248 rows instead of 3224.
- Written: that recipe, as section 7 of ww-whole-map-picture (amended; no new skill). It is not mirrored into
  E:\Tools\AISkills: that is outside this lane's write scope, the same call EXTENT1 made.
