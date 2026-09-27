# Lane TIDY1 -- empty channels, duplicate texture layers, and the views that mislead

Worktree: E:\Projects\NifskopeWWE-tidy1, branch tidy1-20260927 from the night-20260927 head the overseer names at launch.
FIRST read the shared rules: C:\Users\bungo\AppData\Local\Temp\claude\E--Projects-Claude\b560e4ec-6e66-4c21-9572-1ad4acca0043\scratchpad\night_rules.md
Report: scratchpad\tidy1_20260927\DONE.md in the worktree.

## His words
"review everything else for what it ultimately needs to be, and what it actually is right now" / "we fix all the maps
that are broken, overnight"

## What is wrong (audit ranks 7, 8, 9)
7. Empty channels shipped: emissive _g on every texture array (0 everywhere -- FIRST find out whether any LOD material
   has a glow source (BGSM glow map / emissive flag) the bake failed to read; if a source exists, bake it; if none
   exists, stop shipping the empty file); mesh _n B/A constant 132/0; mesh _gsaos B constant 255; the .lodl ground
   cover plane (GCVR 0) -- find out why it is empty (the real cover is in VT mask A): if the .lodl plane is meant to
   carry it, fill it, otherwise stop writing it; the .lodl AO plane (a coarser copy of mask B); the per-piece sky byte
   (a coarser copy of the per-vertex stream).
   For each: ship-or-drop decision with the reason, and the FO4CS reader impact listed. A dropped field that a reader
   expects is a format change: follow the rules' format-change clause, and do NOT collide with lane GROUND1's .lodi
   version bump (it is merged into night-20260927 before you start, so build on its version).
8. 10 of 114 texture-array layers are duplicates: the same texture under two spellings of its material path.
   Normalise the key (case, slashes, Data\ prefix, extension) so each texture has one layer; count before/after.
9. Views that mislead (viewer only): 37 is a tint -- caption and legend say so; 46 is captioned "Building" but shows
   trees -- find why (array set naming) and fix the label source; the duplicate views 22, 28, 66, 68, 70, 72, 73 --
   drop the duplicate channel names or alias them; 36 cellrange: caption "per-cell min/max height (culling table, one
   value per 4096-unit cell)". Do not touch the water views or identity views (lanes WATER1 and IDENT1 own them).

## Gates
- Every drop/fix: off (gate only) byte-identical to night-20260927's Boston bake; on: the named file/plane/byte gone or
  filled, bytes saved per Boston box and whole-map estimate.
- Duplicates: 114 -> 104 layers (or the measured number), every placement still resolves to a layer with identical
  texels (hash per layer before/after).
- Glow: a count of LOD materials with a glow source, and if > 0, a picture of the glow channel over Boston.

## Pictures
Each full size with its title bar, maps1 Boston camera: only where something visible changed (glow if baked, 37/46
relabelled views).
