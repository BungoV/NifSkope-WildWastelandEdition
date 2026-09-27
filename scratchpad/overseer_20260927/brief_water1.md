# Lane WATER1 -- water baked as water, and drawn as flat water

Worktree: E:\Projects\NifskopeWWE-water1, branch water1-20260927 from night-20260927 @ 8f58e7db.
FIRST read the shared rules: C:\Users\bungo\AppData\Local\Temp\claude\E--Projects-Claude\b560e4ec-6e66-4c21-9572-1ad4acca0043\scratchpad\night_rules.md
Report: scratchpad\water1_20260927\DONE.md in the worktree.

## His words
"water doesn't actually show water, ground cover is empty, water meshes are just the terrain meshes? the water should
be flat, you're not actually showing me the water with baked data" / "and the water maps will finally bake on the water? hopefully"

## What is wrong (audit rank 1; rows E11-E13 and the water pictures 34, 38, 42-44, 67-79)
- The default terrain bake writes the .lodl v2 cell table only: water height per 4096-unit cell, and the has-water
  flag set on all 36,864 cells, so it cannot say where water is. No body / flow / shore planes.
- The v3 water bodies (--water-bodies; the water5_20260910 lane; 346 bodies, flow, shore distance) are right but opt-in.
- The viewer draws NO water surface: every water picture is numbers painted on the ground mesh.
- The audit drew the v3 bodies flat offline: ...\scratchpad\audit1\pics\A3_water_as_flat_surfaces.png (the target look).

## The work
1. BAKE: the v3 water bodies become the default of the terrain bake (command line and the NifSkope panel), with
   --no-water-bodies to go back. Measure the bake-time cost on the Boston box and the whole-map estimate.
   Fix the v2 has-water cell flag so it is set only where a body's water is above the ground in that cell (or say
   with numbers why the flag must stay as it is for vanilla parity).
2. VIEW: the far-LOD viewer draws every water body as a FLAT surface at its body height, over the terrain, clipped to
   where the body is (the body-id plane), semi-transparent enough to see it is water. The water plane views
   (WW_LODL_PLANE= waterheight / watertype / bodyid / flow / shore / cellflags) paint their data ON THE WATER SURFACE,
   and the ground under it is drawn in its normal view (not painted). The flow view shows direction, readable
   (colour wheel or arrows) with a legend. A default view (no plane) also draws the water surface in a plain water colour.
3. On a v2-only file (no bodies), draw the per-cell water height as flat per-cell sheets only where water is above
   the ground, and say on the view's note line that the file has no bodies.
4. Gates:
   - byte-identical: with --no-water-bodies the bake output equals night-20260927's bake of the Boston box (hashes);
   - the viewer with water drawing forced off (env, gate only) equals today's pictures byte for byte;
   - flat: every water pixel of one body has the same world height (read back the depth or recompute; spread 0);
   - the Charles is one body at its measured height with the bridges drawn over it and no ground poking through
     inside the body mask (count ground pixels above water inside the mask: 0, or explained);
   - legend matches picture (ww-legend-matches-picture) for every water view.
5. Pictures, each full size with its title bar, maps1 Boston camera: default view with water; water height on water;
   water type; body id; flow; shore distance; and one whole-Commonwealth oblique of the default view with water
   (LIT1's whole camera: E:\Projects\NifskopeWWE-lit1\scratchpad\lit1_20260927\pics\whole\*.cam.log).
