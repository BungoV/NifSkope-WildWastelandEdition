# BAKE2 progress (lane BAKE2, worktree E:\Projects\NifskopeWWE-bake2, branch bake2-20260925 from main d5764fbe)

- 18:53 (clock read) worktree built: main's objects reused (main clean in src, make -n 0 g++), REVISION objects
  deleted and rebuilt, ww_build.sh rc 0. exe 7cfccac1 (rung copy release/NifSkope.before_bake2.exe; run copy
  scratchpad/bake2_20260925/run/release). NIFSKOPE_REVISION is empty in this Makefile, as in main's.
- 18:55 worldspaces in his 47-plugin order (--list-worldspaces): SanctuaryHillsWorld 000A7FF4 (Fallout4.esm),
  NukaWorld 0600290F (DLCNukaWorld.esm), DLC03FarHarbor 03000B0F (DLCCoast.esm).
- LAND extents (--dump-land, land_extent.py): pre-war 118 LAND cells, x -25..2 y -9..25 (two blocks: the
  Sanctuary block -25..-16 x 17..25 = 90 cells, and 28 cells at x -3..2 y -9..-4); Nuka-World 4225 cells, -32..32
  x -32..32 (full square); Far Harbor 2067 cells, x -29..20 y -21..31.
- .lodl header bounds (after the lodl stage, i32 at 0x08): pre-war -28,-13..7,34; Nuka-World -32,-32..32,32;
  Far Harbor -73,-59..69,78. The header is the worldspace's CELL box, wider than the LAND box on pre-war and FH.
- Vanilla terrain LOD loose in DataUnpacked: Commonwealth, DiamondCity, SanctuaryHillsWorld only. Pre-war ships a
  whole-map set (3062 .BTR incl. objects dir, 6121 textures, 22 object meshes, LODSettings -96..) -> fill ON there.
  DLC03FarHarbor and NukaWorld: none loose -> fill OFF for both (nothing extracted).
- 18:56 cards: 61 tree candidates over the three worlds (sanct 18, NW 26 all vanilla 00-, FH 43 incl. 14 DLCCoast);
  30 already in BAKE1's library (no card-bake code change on main since BAKE1's base) -> copied; 31 new baking with
  BAKE1's library settings (N8, ring 0, tile 256, ref 1328.6 = also the max over the three lists).
- 18:57 lodl stage rc 0 for all three (1-3 s each), cross-check 0 mismatched.
- G5 before-listings taken 18:56 (mods top 78, root 8, profile 13 files, FO4CSLOD 3681 entries: Commonwealth only).
