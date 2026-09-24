# LOADORDER1 (LOD-A) progress

## 21:09 start
- Read brief_lodnight_common.md, brief_loadorder1.md, CONSTITUTION 1/1a, PLAN s3 LOD-A + s4, helper_reports D.
- Skills loaded: nifskope-ww-lodgen, nifskope-ww-build-verify, search-lean, mo2-mod-content-census,
  nifskope-ww-worktree-build (appeared mid-lane, written by VTFIX1).
- Prechecks: E: 160 GB free; Fallout4.exe not running.

## 21:10 first build of the worktree (rung)
- qmake failed first (`QMAKE_CXX.COMPILER_MACROS is not defined`): copied `.qmake.stash` from the main tree
  (the one untracked build input copied). qmake RC 0, Makefile.Release names the worktree 125x.
- Full `make -j2` running in the background (the main tree's objects would have been reusable -- main 71f96c1
  differs from 47b2cad only in tests/ -- but the full build was already half done when the skill appeared).
- Build script: scratchpad/loadorder1_20260924/build.sh.

## 21:17 code written (not yet in the build: patching nifcli/pro waits for the rung build to finish)
- NEW src/lodgenloadorder.{h,cpp}: lodgenLoadOrderMasters, lodgenMo2GameData (ModOrganizer.ini),
  lodgenLoadOrderFromMo2, lodgenLoadOrderFromPluginsTxt, lodgenApplyMo2Profile. -fsyntax-only RC 0.
- patch_nifcli.py (9 anchors, +27 lines, --check ok), patch_pro.py (--check ok).
- Gate paths picked by census.py (read only):
  G2 meshes/bns/lod/landscape/trees/aspen/treeaspen01_lod_0.nif (BNS, line 66);
     meshes/landscape/grass/tg_bushycane.nif (True Grass, line 64).
  G3 meshes/dlc03/setdressing/dlc03lightoillampon_hanging.nif, loose in BOTH +Ultra Exterior Lighting (line 44)
     and +Ultra Interior Lighting (line 45).
  G4 materials/actors/msmechanic/msmechanicarmor.bgsm only in -X03 (line 12, disabled);
     meshes/actors/powerarmor/x01/x01_torso.nif in -PBR (line 10, disabled) and +X01Tesla (line 14).
- 21:24 census2.py: no loose path two enabled mods ship with different bytes (0) -- G3 rests on the entry attribution

## 21:22 lane build + G1-G4
- Build 21:21:20 BUILD-RC=0 (nifcli.o + lodgenloadorder.o rebuilt), exe sha1 4b9a5977b5877babbeb2ab1670d6f69db367258b.
  Rung (first worktree build) release/NifSkope.before_loadorder1.exe sha1 dadd253ade075582ed1610e5677c476ec0c07a4c.
- Code commit 27e5dfc. G1-G4: 18 checks, 0 failures (gate_1234.txt), each leg's red shown on the rung.

## 21:30 G5
- His LIVE profile does not bake: `TestWorldspace.esp: invalid form ID` (AnotherOne's Test World). Measured
  (formid_probe.py): 483 of its 484 records carry top byte FF over 1 master; libfo76utils (esmfile.cpp:309)
  refuses a raw form ID above 0x0FFFFFFF outside FD/FE before any remap. The error names the plugin.
  Not fixed here: esmfile.cpp is outside the brief's file list. The game presumably treats an index beyond
  the master list as the plugin itself (refuter: an xEdit check of that plugin).
- G5 therefore bakes a COPY of his profile with exactly the refused plugins unticked (checker `untick`):
  bake rc 0 in 6 s, Commonwealth.lodb lists 46 plugins, BNS Trees.esp and TrueGrass.esp by full mod path,
  plugin 0 Data/Fallout4.esm. Rung red: `error opening input file "HUDFramework.esm"`. 6 checks, 0 failures.
- 21:30 kept-green: resources 4/0 PASS; bakerec FAIL only (f) 2 of 12 stock files vs the OLD rung before_bakerec1 -- (f) vs this lane's rung PASS 12 identical (pre-existing drift, not mine); defaults 31/1, the (d) C-line floor, same FAIL on this lane's rung (pre-existing).
- 21:30 SCOPE CHANGE from the director: panel MO2-profile input REQUIRED + output/R2 questions + panel gates.
- 21:36 panel leg built 21:36:07 BUILD-RC=0 (nifskope_ui.o + lodgenmanager.o), exe sha1 c7a96c2fcbb7b5ff86d616c45e424da45fcf11b3. Source row: 3rd choice 'Mod Organizer 2 profile' + Profile / Mods folder rows + read-only 'Mod order' list; self-test leg WW_LODGEN_MO2DISK; spell lodgen_panel_mo2.sh.
- 21:38 first panel run: whole suite 1 failure (the structural check still expected TWO source items) and the CLI comparison printed 0 plugins (a copied profile has no ModOrganizer.ini, so the CLI call needs --data-root). Fixed: patch_selftest2.py (3 items, item 2 = profile), --data-root in the spell. Rebuilt 21:38:51 BUILD-RC=0 (nifskope_ui.o only).

## 21:45 panel gates (exe 21:38:51, sha1 5ff25b1bcfdcc88c4ecd1401df2ea0bfa204ecb4)
- lodgen_panel_mo2.sh: 4 checks, 0 failures (gate_panel.txt). MO2DISK leg 14/0; whole WW_LODGEN_TEST 142/0; GUI plugin list == CLI --print-source path for path (46). RED: rung 0 of floor 14 MO2DISK (rung suite 128/0).
- GUI bake (one chunk -20,24 dim 4, FO4CS target, output = stand-in gate/panel/FO4CSLOD): 3.8 s, 19 files, 0 .BTO/.BTR, no lodgen_bto_scratch left. FO4CS root = <mod>/FO4CSLOD/Commonwealth, so with the mod folder named FO4CSLOD the disk path is mods/FO4CSLOD/FO4CSLOD/Commonwealth = Data/FO4CSLOD/Commonwealth in game (correct, not a bug).
- R2: under the FO4CS target the panel already writes the FO4CS set only (.BTR section hidden, .BTO built in scratch and dropped, keepBto OFF). No new control needed.
- Gap (pre-existing): the panel writes no .lodb bake record; that is CLI only.
- G1-G5 rerun on the final exe: 24 checks, 0 failures (gate_final.txt).
