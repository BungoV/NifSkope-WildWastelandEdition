# LANE IMAGES -- the two mountain pictures owed to bungo

Header: tree E:\Projects\NifskopeWildWastelandEdition, branch main, uncommitted
tree ("Not yet": NO commits). Read CONSTITUTION.md first, then HANDOFF.md top
block ("The mountains, and the mod bungo wants"), then the
`nifskope-ww-render-shot` skill in full. Model: Opus 5. Files you may write:
scratchpad/mountains_20260907/images/ and your report. Never touch src/. Never
build. Before executing release/NifSkope.exe check Fallout4.exe is not running
(tasklist) and that no other NifSkope.exe is running (one instance ever); every
window on the second monitor. If either check fails, stop and say so.

bungo's words, verbatim: "a comparison image, from a distance, and a detailed
close up shot of one of those mountain peaks outside the playable area".
The lane MSN is running at the same time and writes only scripts under
scratchpad/mountains_20260907/; do not edit any file it may write. Create your
own subfolder images/.

## The work
1. Pick one far mountain region outside the painted box (x -36..32, y -41..32
   is painted; go beyond it, e.g. the north-west or south-east corner) where
   vanilla's shipped far LOD (.btr terrain + its diffuse/normal sheets, from
   E:\Tools\Fallout 4\DataUnpacked\Data, never a mod folder) has visible colour
   and relief. Name the cell and the LOD tile.
2. Image 1, the distance comparison: vanilla's far terrain vs OUR regenerated
   far terrain for the same tile, same camera, same lighting, side by side,
   labelled "vanilla" / "ours" burned into the image, through the render hook
   (WW_RENDER_SHOT and the WW_RENDER_* switches; WW_LOD_CHANNEL if a channel
   view helps). Our sheets: regenerate with the scripts in
   scratchpad/mountains_20260907/ or with lodgen (skill nifskope-ww-lodgen)
   into a folder under images/; state exactly which command produced them.
3. Image 2, the close-up: one peak in that region, vanilla vs ours, same
   camera, close enough that the `_msn` detail difference is visible. Same
   labelling.
4. If a true render of far terrain is not possible through the hook, fall back
   to the same two comparisons as texture-sheet crops (diffuse and `_msn`,
   msn_compare.py builds the normal pair) and say so plainly at the top of the
   report; do not present a crop as a render.

## Gates
- Two PNGs exist: images/mountain_distance_compare.png and
  images/mountain_peak_closeup.png, each with both halves labelled, each
  under 8 MB.
- The report names the cell, tile, camera parameters and the commands, so the
  images regenerate.
- The exe used is release/NifSkope.exe and is newer than every source file
  (state its mtime and the newest source mtime).

## Rules
- No src/ edits, no builds, no commits, no writes outside images/ and the
  report. One NifSkope instance; second monitor; never SetForegroundWindow.
- Skills to invoke: `nifskope-ww-render-shot` (mandatory), `nifskope-ww-lodgen`
  for regenerating our sheets, `nif` if a .btr/.bto needs inspecting.

## Report
Write scratchpad/lane_images_report.md incrementally, section by section:
1. Region chosen and why. 2. How ours was generated (commands). 3. Image 1
(command, camera). 4. Image 2. 5. What the pictures show, in two sentences,
without a verdict on the cause. 6. Mistakes (or "none"). 7. Finished-work
skill review: skills used, procedures re-derived that should be a skill, ones
written, or the reason for declining.
Final message to the director: under 25 lines, with the two image paths first.
