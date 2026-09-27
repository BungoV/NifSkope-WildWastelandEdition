# Lane TERR1 -- ground shading that sees the buildings, and a ground normal map with the objects in it

Worktree: E:\Projects\NifskopeWWE-terr1, branch terr1-20260927 from night-20260927 @ 8f58e7db.
FIRST read the shared rules: C:\Users\bungo\AppData\Local\Temp\claude\E--Projects-Claude\b560e4ec-6e66-4c21-9572-1ad4acca0043\scratchpad\night_rules.md
Report: scratchpad\terr1_20260927\DONE.md in the worktree.

## His words
"normal map on the ground has no details baked from the objects (roads, railway tracks, concrete pieces, decals, etc)"

## What is wrong (audit ranks 4 and 5; pictures 07, 19, 32, 65, 66)
- Ground sky shading (VT mask B, a horizon march over terrain only, docs/LODGEN_TERRAIN_VT.md ~966) and the .lodl AO
  plane ignore buildings: street canyons are no darker than open ground; ground beside buildings is even brighter
  (227 vs 217).
- The ground normal sheet (_msn, role 2) is built from the heightmap only; roads, railway tracks, concrete slabs and
  decals are stamped into the COLOUR sheet but never into the normal sheet (LODGEN_TERRAIN_VT.md:792: left out for
  vanilla parity). bungo overrules that parity: he wants them in.

## The work
1. Sky shading with objects: the terrain's sky/horizon march also hits the placed objects (their LOD meshes or their
   occupancy -- choose by measurement against a ray cast on a sample), so ground in a street canyon or under a
   bridge reads darker. Reuse the object AO machinery (src/lodgenao.h) if it fits. Keep the terrain-only term as
   an internal intermediate if FO4CS needs it separately (say so).
2. Normal stamping: wherever the colour sheet is stamped from a flat object (roads, rails, slabs, pavement, decals),
   stamp that object's normal map too, rotated into world space and blended over the height normal with the same mask
   and the same alpha the colour uses. The railway tracks' rails and sleepers, kerbs and slab joints must show in the
   lit view. Keep the sheet's channel order (R east, G up, B north) and its BC1 (or say with numbers why it needs more).
3. Gates:
   - off (command-line, gate only) = night-20260927's Boston terrain sheets byte for byte;
   - sky: street-canyon texels vs open-ground texels -- canyon now darker (numbers, named streets); open ground away
     from buildings unchanged within 1 level; corr with the relief term stays > 0.5 where no object is near;
   - normals: on stamped road texels the normal differs from the height-only normal (share and mean angle); off the
     stamped mask it is byte-identical; a rail line's normal crosses sign across each rail (profile across one track);
     east/north orientation refuter as maps1 did (east-facing R > west-facing R);
   - whole-map terrain bake time estimate before/after.
4. Pictures, each full size with its title bar, maps1 Boston camera: ground sky shading before and after; ground
   normal map before and after; a 4x close crop of a railway line and a road junction, normal before and after;
   and (if the lit view is in your tree -- it is NOT on night-20260927; skip if absent) nothing else.
