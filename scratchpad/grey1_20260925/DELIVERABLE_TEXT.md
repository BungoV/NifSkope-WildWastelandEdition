## HANDOFF text

GREY1 (2026-09-25 21:10-21:45, measure only, branch grey1-20260925, nothing changed or re-baked): why our far-field
buildings read greyer than in game. Ranked:
1. The per-placement material swap never reaches the LOD. 44% of Commonwealth building placements carry one; in
   full detail they are 25% more saturated than their shared LOD atlas (S 0.189 vs 0.151). The swap moves colour
   p10 0.7x .. p90 1.6x over 4,049 (base, swap) variants. This is vanilla's own LOD trait.
2. The game's light is coloured (shaded faces lit blue, S ~0.3, vanilla and Physical Weathers alike). The
   viewer's is not: Legacy is white, and Lookdev gives far-field shapes a flat grey ambient, not the DALC.
3. The viewer's tone map under the headlight takes 5%.
Not causes: missing textures (the on-screen base colour equals the atlas), the palette rule, mips, FO4CS grading,
imagespace.
Proposal: a per-placement RGB multiplier (census2.py's full/LOD mean colour of the (base, swap)); Lookdev DALC for
far-field shapes. Refuter: the grey buildings he points at carry no XMSP/MODS.
Side finding: the Physical Weathers CommonwealthClear IMSP points at DLCCoast REFR/STAT IDs, not imagespaces.
Full record: scratchpad/grey1_20260925/DONE.md.

## WW_CHANGES text

- 2026-09-25 GREY1 (measure only, no code): far-field grey diagnosed. The cause is the material-swap tint that
  a shared LOD atlas cannot carry, plus the weather's blue shade light, which the viewer's lighting lacks. The
  viewer's own tone map takes 5%. Scripts are in scratchpad/grey1_20260925 (census2.py, weather_light.py,
  measure_pics.py). New skill: fo4-surface-colour-census.

## MISTAKES text

- 2026-09-25 21:1x GREY1: compared the LOD atlas with the full model's RAW diffuse. That was wrong for the 41% of
  building area that is grayscale-to-palette (plus material swaps), and it gave the opposite sign ("LOD MORE
  saturated"). Rule: an in-game colour is palette[diffuse.G, row] with the swap's CNAM row and SNAM material.
  Now in skill fo4-surface-colour-census.
- 2026-09-25 21:30 GREY1: typed a log time (21:33) without reading the clock. Corrected to the read time.
