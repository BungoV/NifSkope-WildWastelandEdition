# TINT1 progress
- 2026-09-25 18:54 (clock read) worktree E:\Projects\NifskopeWWE-tint1, branch tint1-20260925 from main d5764fbe. Objects copied from main (make -n = 0 there), 8 REVISION objects deleted, build 1 RC=0, exe b72cef2a (rung copy release/NifSkope.before_tint1.exe).
- 2026-09-25 18:54 SAFETY GATE read: wt-fixfirst (b0719da, 2026-09-09) has NO .lodo reader in src (0 hits for .lodo/.lodi/lodoIdentity). The deployed DLL (mods\FO4CS, 2026-09-24 19:11) does read .lodo; its strings match src/ImprovedLOD/ImprovedLODLodo.h in wt-debris1 (= wt-lod0, commit 61ab985). Verdict: version != 4 is REFUSED with a logged reason (lines 224-225, Load.h 175-176, Runtime.cpp 182-183). Install allowed.
- 2026-09-25 18:57 CENSUS (census.py, installed BAKE1 v4 library, 184,431 placements, 3,125 meshes / 3,247 shapes, models resolved through his MO2 stack: 2,985 vanilla Meshes.ba2, 92 BNS Trees, 43 DLCCoast, 5 NukaWorld, 0 missing). Shapes with channel AND Vertex_Colors: 172 (166 meshes); 3,075 shapes have no colour channel at all; 0 shapes have a channel without the flag. Real hue: 14 shapes -> 6,539 placements (3.5%). Grey shading R=G=B: 32 shapes -> ~3,586 placements (1.9%). White-only (alpha, no visible tint): 126 shapes -> ~20k placements. Buildings with hue: BathHouse wings (4 models, 10 placements), CovWallExLrg01, VltGearDoorRoomExt02, Amphitheater (1 each). Tinted buildings are NOT vertex colour in the LOD NIFs.

## 19:06 (clock read)
- Object re-bake running (started 18:59:35, exe b72cef2a, no VT/cover/tex-dir; at chunk job ~1700 of ~3060 at 19:05).
- BAKE2 is alive too (the brief said TINT1 was the only lane); its bake writes to its own scratchpad.
- Before pictures done with the installed v4 pair, same camera as the after set will use (shot.sh, pics.sh):
  08_boston_oblique 11,416 drawn; amphitheater 1,437; maple 729; hue window (-16,21..-9,28) 3,388. All non-grey (sd 31-41).
