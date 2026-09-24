Lane VT1, 2026-09-16: the pyramid-assembled terrain sheet and the direct chunk bake now write the same bytes on the ruled land default, and the parked check is back on the new look.

The two paths disagreed by 4 bytes on one chunk and 27 on another because they disagreed about the ground underneath,
not about the colour. Where two of Bethesda's cells both carry the same row of heights, whichever is written last wins
— except inside a protected box, and each baker was protecting its own box: the chunk baker a whole chunk, the tile
baker its own quarter of one. So the same spot came out up to 64 units higher on one path, the warp reads the slope of
that ground to decide how hard to push the land pattern around, and 33 texels took a different colour.

The tile baker now protects the box of the chunk it will be assembled into, so a texel's value depends on where it is
in the world and on nothing else. The direct bake writes exactly the bytes it wrote before — all 12 files of the test
region identical to the old exe — so no ruled default moved.

Exe release/NifSkope.exe, 22,293,504 bytes, 2026-09-16 12:49:19; rung kept at NifSkope.before_vt1.exe (11:54). Nothing
committed. Gates: lodgen_terrain_vt.sh 44 checks 1 failure (V9c only); lodgen_terrain.sh 26/0;
lodgen_native_baseline --check 25 files 0 differ. Two skips, both named: lodgen_defaults.sh needs a pre-12-September
exe that is not in the tree, and lodt_write.sh does not exist at all.

V9c is still red and is not this lane's: it reads a DXT5 file with a DXT1 reader, and its bars were set on our own
normals while these chunks now use vanilla's normal file copied whole. Read properly, the seam is 0.99 times an
ordinary step against a bar of 3.20 — there is no seam. Fixing it is bungo's call. Report: scratchpad/vt1_20260912/.
