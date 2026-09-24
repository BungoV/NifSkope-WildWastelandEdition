# Impostors: the tear is mended, the bake is 4x, twelve vanilla trees

**Contact sheet: `pics_sheets/contact_sheet.png`.** Twelve vanilla models, each shown as model | card. The camera is at azimuth 30, elevation 15, which falls between two baked angles.

| tree | vanilla path |
|---|---|
| evergreen (the only conifer in the base game) | meshes/Landscape/Plants/InstituteEvergreen01.nif |
| cedar | meshes/Landscape/Trees/Cedar01.nif |
| cedar | meshes/Landscape/Trees/Cedar03.nif |
| maple | meshes/Landscape/Trees/TreeMapleForest1.nif |
| maple, leafy | meshes/Landscape/Trees/TreeMapleInstitute06Green.nif |
| elm | meshes/Landscape/Trees/TreeElmForest01.nif |
| big oak-like | meshes/Landscape/Trees/TreeHero01.nif |
| burnt | meshes/Landscape/Trees/BlastedForestBurntTreeUpright01.nif |
| dead | meshes/Landscape/Trees/BlastedForestDestroyedTreeUpright02.nif |
| blasted | meshes/Landscape/Trees/TreeBlasted02.nif |
| swamp | meshes/Landscape/Trees/TreeSMarsh01.nif |
| shrub | meshes/Landscape/Plants/FoothillsShrubLarge01.nif |

## Other files

- **`pics_sheets/sheet_<tree>.png`**: one sheet per tree.
  - Model | card at azimuths 0, 30, 90 and 205, all at elevation 15. The 30 and 205 views fall between baked angles.
  - One raised view: azimuth 60, elevation 50.
- **`pics_sheets/old_vs_new_cut.png`**: model | card with the old cut | card with the new cut. Same bake, same camera.

## What changed

**The tear.**
- The old drawer averaged three frames taken from far-apart angles. Where only one frame held a branch, the average fell under the cut and a hole opened.
- The new drawer lets each frame keep its own silhouette, in proportion to its weight, as a fine stipple.
- Holes on the gate trees:
  - blasted maple: 45% → 6%
  - forest maple: 68% → 19%
  - rock: 27% → 5%
- The card does not jump more as the camera turns. The worst jump is 0.3 to 1.3 times the old one.

**Honest price.**
- The card is fuller, and closer to the model on 6 of 7 trees scored:
  - Cedar01: match 0.26 → 0.34
  - TreeHero01: match 0.38 → 0.44
  - InstituteEvergreen01: flat, 0.79 → 0.78
- It also paints more outside the model: stray speckles and faint ghost branches.
  - Cedar01: 40% → 53% of the card's pixels
  - BlastedForestBurntTreeUpright01: 4% → 16%
- Look at Cedar01 at azimuth 205 and at the evergreen seen from above (elevation 50). The top view is the weakest; a 4x4 grid has few high angles.
- The stipple is visible up close. At LOD distance it should read as leaf noise, but that is your call in game.

**4x bake.** The same bake time as 2x, within noise. The edge-position test that 2x failed now passes (0.73, bar 1.0).

**Not in the set.** Three models baked to an EMPTY card, on the old exe as well, so this is not new:
- Trees/Sapling01.nif
- Trees/TreeElmUndergrowth01.nif
- Plants/ShrubGroupLarge05.nif

They are left out and flagged.
