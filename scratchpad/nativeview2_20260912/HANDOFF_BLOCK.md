LANDED 2026-09-16 12:24 — the native LOD terrain is now lit from its normal sheet, not from a frame it never had. release/NifSkope.exe 22,288,896 B, 11:54:47. NOT COMMITTED.

The dark blotches were a viewer bug, and the proof was already on the table: swapping every normal tile for a flat one changed nothing. A terrain sheet is a MODEL-space normal map — it carries all three directions in the world's own axes — but the FO4 shader had no way to read one, so it treated it as the other kind and pointed the sheet's "up" sideways. One new branch in res/shaders/fo4_default.frag, taken only when the shape sets Shader Flags 1 bit 12, reads it the right way.

The channel order was measured on six of Bethesda's own shipped sheets against the real heights of those cells, not assumed: red is east, blue is north, green is up. Flipping the row order kills the correlation, which is the check that it is really that and not luck.

The dark fraction of the native terrain at the oblique goes from 20.01 per cent to 0.58. Everything without that bit renders byte-for-byte identical — the objects and the old .BTR both match exactly.

Two things for you to rule on. The old .BTR does NOT brighten, and the reason is measured: it is Shader Type 18 and the program scan sends it to a different shader entirely (sk_msn.prog, the Skyrim model-space one). It was never broken in the same way. Routing it to the FO4 shader instead is a real decision with reach beyond terrain, so I left it alone. And the brief asked me to cite FO4CS's shader for the channel order — that shader does not exist in their tree; there is one passing mention of _msn and no decode. Said so plainly rather than dressing one source up as two.

New gate tests/spells/native_lighting.sh, 14 checks 0 failures. render_shot 82/0 and lodl_open 23/0 keep their counts. native_open keeps its count too but still fails one check — an object coverage test that fails identically on the build before this change, so it is somebody's red, not this one's.

Your open NifSkope is running the old exe and needs a restart to show any of this. Pictures are in scratchpad/nativeview2_20260912/images/ for the director to send.
