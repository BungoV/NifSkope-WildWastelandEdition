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
- 19:01 (clock read) cards DONE: 31 new sets baked 18:56:59 -> 19:01:27, 0 FAILED/MISSING; library 61 sets, 0 empty,
  lowest coverage 7.3 % (000531ae, a reused BAKE1 set); new and reused sidecars both `projection ortho`, `legacy 256`.
- 19:01:36 pre-war chunks stage launched (region -25 -9 2 25, --dim all, fill ON). TINT1's -no-gui Commonwealth bake
  is running at the same time (allowed; the starve rule is a bake past twice its expected time).
- Instruments ready: checks.py (no-LAND placement cells, named; flat-grey dim-4 chunks from VT.16). Grey refuter:
  SEAM1's pre-fill Commonwealth VT.16 reads 2021 of 2304 chunks flat grey; the installed fill-ON one 0 of 2304;
  playable Sanctuary / Boston blocks 0 of 16 on both (the grey was the unpainted ring). Threshold chroma < 6 and
  luminance SD < 2 per chunk.
- Landmark cells (cell_names.py over the plugins' CELL EDIDs): Nuka-World Galactic Zone -6..-3 x -2..1, Nuka-Town
  -1..0 x -6..-1, Kiddie Kingdom 0..3 x 0..3 -> oblique frame -8 -7 3 4; Far Harbor town FarHarborExt 13..14 x
  6..8 -> frame 9 3 16 10; pre-war PrewarSanctuaryExt01 -20,21, PrewarPlayerHouse01 -20,22 -> frame -24 18 -17 25.
