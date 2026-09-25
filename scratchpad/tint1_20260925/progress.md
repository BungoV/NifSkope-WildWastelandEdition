# TINT1 progress
- 2026-09-25 18:54 (clock read) worktree E:\Projects\NifskopeWWE-tint1, branch tint1-20260925 from main d5764fbe. Objects copied from main (make -n = 0 there), 8 REVISION objects deleted, build 1 RC=0, exe b72cef2a (rung copy release/NifSkope.before_tint1.exe).
- 2026-09-25 18:54 SAFETY GATE read: wt-fixfirst (b0719da, 2026-09-09) has NO .lodo reader in src (0 hits for .lodo/.lodi/lodoIdentity). The deployed DLL (mods\FO4CS, 2026-09-24 19:11) does read .lodo; its strings match src/ImprovedLOD/ImprovedLODLodo.h in wt-debris1 (= wt-lod0, commit 61ab985). Verdict: version != 4 is REFUSED with a logged reason (lines 224-225, Load.h 175-176, Runtime.cpp 182-183). Install allowed.
- 2026-09-25 18:57 CENSUS (census.py, installed BAKE1 v4 library, 184,431 placements, 3,125 meshes / 3,247 shapes, models resolved through his MO2 stack: 2,985 vanilla Meshes.ba2, 92 BNS Trees, 43 DLCCoast, 5 NukaWorld, 0 missing). Shapes with channel AND Vertex_Colors: 172 (166 meshes); 3,075 shapes have no colour channel at all; 0 shapes have a channel without the flag. Real hue: 14 shapes -> 6,539 placements (3.5%). Grey shading R=G=B: 32 shapes -> ~3,586 placements (1.9%). White-only (alpha, no visible tint): 126 shapes -> ~20k placements. Buildings with hue: BathHouse wings (4 models, 10 placements), CovWallExLrg01, VltGearDoorRoomExt02, Amphitheater (1 each). Tinted buildings are NOT vertex colour in the LOD NIFs.

## 19:06 (clock read)
- Object re-bake running (started 18:59:35, exe b72cef2a, no VT/cover/tex-dir; at chunk job ~1700 of ~3060 at 19:05).
- BAKE2 is alive too (the brief said TINT1 was the only lane); its bake writes to its own scratchpad.
- Before pictures done with the installed v4 pair, same camera as the after set will use (shot.sh, pics.sh):
  08_boston_oblique 11,416 drawn; amphitheater 1,437; maple 729; hue window (-16,21..-9,28) 3,388. All non-grey (sd 31-41).

## 19:24 (clock read)
- Spells on exe b72cef2a: lodgen_native 32 checks 0 failures PASS; lodgen_loadorder 24/0 PASS (its red rung leg fails 2 as it must);
  lodgen_cardlink PASS 0 failures (RUNG + CARDS pointed at the cardlink1 worktree). lod_generation (GUI) waits for the bake.
- Side probes: the building tint is NOT a palette material (0 of 2,848 placed-LOD .bgsm shapes; control: 282 of 6,616 vanilla
  .bgsm have it) and NOT in the shipped stock .bto either (Boston dim-4 chunks, vanilla and BNS: no colour channel at all).

## 19:41 (clock read)
- New .lodo written 19:33 (v5, 6,933,236 B, 52,925 colour rows); instances stage (the ~29 min serial join) running.
- G1 on the .lodo: strip(v5) is the v4 length; 27 bytes differ. Attributed:
  - 0x04 version word (expected);
  - 0xB8 loadOrderHash: CORE.esp changed size since BAKE1 (14,270 -> 14,766 B, file time 14:32) -- his plugin, read only;
  - 0x28 cardCorpusHash: the 15 legacy _n card arrays differ in a few hundred bytes each (e.g. 201 of 1.97 MB);
  - 2 vertex self-AO bytes (228 -> 201), WaterTowerConcord01_LOD / WaterTowerGeneric01_LOD;
  - headerCrc32 + indexCrc32 follow from those.
  Code or run noise? Small bake (cells -20,17..-17,20, holds the Concord tower) run twice on my exe and once on BAKE1's.
- .lodb: an objects-only bake's record drops the VT line (levels, 12,276 tiles, cover) -- do NOT install the new .lodb.

## 20:19 (clock read)
- Bake done 20:03:54 rc 0 (3,859 s). Gate (d): 3,670 object files, same names as installed; 3,652 byte-identical
  (all 3,060 .lodj, all 486 manifests, 106 of 121 arrays); 18 differ: .lodo, .lodi, .lodb, 15 legacy _n arrays.
  Placements 184,431 = installed.
- G1 lodi: only lodoIdentity/headerCrc32 (allowed) + loadOrderHash 0x90 (CORE.esp). G2: 166 flagged meshes = source
  both ways; 1 row disagreement = 2 black vertices no triangle uses in TreeFirForest04Gr_LOD_0 (dropped by design).
- Determinism: my exe twice on cells -20,17..-17,20 = byte-identical (A = B). BAKE1's exe there (C) reproduces the
  whole-map differences (2 water-tower self-AO bytes, the legacy _n arrays): code change, not run noise.
  A no-v5 build of this tree (08bf7589) to split SEAM1 from the other 4 commits is exec-blocked on this machine
  (new exes: "Permission denied" / hang for minutes; the AV scanning new binaries?) -- retrying.
- INSTALLED 20:13:47..20:13:50: .lodo, .lodi, 15 legacy _n arrays; backups in replaced/, sha1s in install_record.tsv;
  .lodb kept (BAKE1's, it records the VT); VT + .lodl sha1 unchanged; 3,677 files. native-verify rc 0, v5, 52,925 rows.
  BAKE2 took a before-listing of FO4CSLOD at 18:56: these 17 mtimes are TINT1's.
- After pictures: 0 px differ from before on all four. Viewer defect found: vertex row had no colour field for a
  library-only colour (src/lodinative.cpp). Fixed in 61d920ab, built 880f5056, waiting for it to be allowed to run.

## 21:03 (clock read)
- Viewer: the fix (880f5056) DOES carry the colour -- a doctored pair with every colour row pure red (doctor.py,
  red/, CRCs + lodoIdentity recomputed) gives 185,943 red px flat, 0 before the fix. The lit renders stayed
  identical because the lit path multiplies vertex colour only with Scene::DoVertexColors, which a headless run
  inherits from the persisted UI option (nifskope_ui.cpp ~22768: only WW_RENDER_FLAT / WW_LODL_AO / WW_LODL_CHANNEL
  force it). Proof: with WW_LODL_AO=1 the red pair differs from the installed pair on 137,332 px (was 0).
- Pictures redone with WW_LODL_AO=1 on 880f5056, v4 (replaced/) vs v5 (installed), pics/vc_on/:
  Boston 1,040 px differ; Amphitheater 5,389; blasted maple 4,814; hue window 13,404 (of 2,598,400). *_diff.png beside.
- Determinism D (no-v5 build 08bf7589) vs A (v5) and C (BAKE1 exe): the 15 _n arrays = A (so the 4 other commits,
  not SEAM1); the water-tower self-AO byte 228 = C (so SEAM1's build). strip(A.lodo) vs D = 0x04, the two CRCs and
  that one vertex byte (WaterTowerConcord01_LOD, a mesh with NO colour flag). Mechanism not proven; best reading:
  the AO caster is inline (lodgenao.h:160) and compiled into lodofile.cpp with -O3 -march=haswell (FMA), so SEAM1's
  edit to that file moved float codegen and one grazing ray flipped (27/255 = one ray). Refuter: a build of
  SEAM1's lodofile.cpp with -ffp-contract=off giving 201 again.
- Spells on 880f5056: lod_generation 128/0 PASS; lodl_channels 54/0 PASS (main-tree fixtures by absolute path).
