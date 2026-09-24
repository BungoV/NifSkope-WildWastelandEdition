# PBRLODFIX1 progress

- 03:56 started; exe e5320fdd; FO4 not running
- 03:58 current exe: 14 checks 5 failures (T2a fires, T2 metal, T3a emissive, T3 count, T3 role6); maskPbrm 0 on the with-bake
- 04:1x bisect: vtnormal1/defaults2/impostorlight1 FAIL 5 (maskPbrm 0); horizon1 (09-18 19:28) PASS 14/14 maskPbrm 6. Break is between 09-18 19:28 and 09-22 22:12, NOT vtnormal1. Fixture TXSTs mostly have TX00 and no MNAM (stem route should fire)
- (04:0x-04:1x read-back: earlier 04:1x-04:5x stamps in this file were typed, not read; build finished 04:15:57) bisect narrowed: before_gltfexport1 (09-19 09:00) maskPbrm 6 PASS; before_impostorshow (09-19 09:44) maskPbrm 0. Broken by the GLTFEXPORT1 build (or whatever built between).
- 04:4x ROOT CAUSE (code regression, not a stale harness): src/nifcli.cpp:7762 -- GLTFEXPORT1's hook-up (09-19) put gltfExportParseFlag() in the SHARED flag loop for every command, ABOVE lodgen's `--data-root` (7804). gltfExportParseFlag handles `--data-root` (gltfexportopts.cpp:213), so lodgen's lgDataRoot is never set: the bake never reads the fixture folder -> maskPbrm 0. Same shadow eats `collision --skeleton` (8361, takes no value; gltf's --skeleton swallows the next token). Fix: only call gltfExportParseFlag when cmd is gltf / gltf-export.
- settings isolation: -no-gui lodgen prints msnCacheDir (none) -> the harness does not inherit his profile; not the defect.
- 04:5x rung release/before_pbrlodfix1 (e5320fdd); fix01.py applied to src/nifcli.cpp (LF-only, +5 lines); building
- 04:16 BUILD-RC=0, only nifcli.o recompiled, exe 4d30baa9 MZ, newer than src/nifcli.cpp
- 04:16 lodgen_terrain_pbrm.sh on 4d30baa9: 14 checks 0 failures RESULT PASS (maskPbrm 6). Red control = same harness on the rung e5320fdd (identical tree minus fix01): 5 failures (run_current.txt)
- 04:16 collision --skeleton: rung SHADOWED ('gltf: --skeleton needs a value'), new exe ok, output byte-identical to before_gltfexport1
- 04:18 gltf_export_options.sh on 4d30baa9: rc 0, '0 row(s) not as registered' (66 PASS rows; the 3 FAIL lines are its registered floors)
- 04:18 lodgen_resources.sh on 4d30baa9: RESULT PASS
- 04:19 lodgen_terrain_vt.sh on 4d30baa9: 45 checks 0 failures RESULT PASS
- 04:24 pbr_shade_ab zero set (old = release/before_pbrlodfix1): SUMMARY 10 cases, 0 failures, 3 empty by the viewer -> PASS. No NifSkope left running. DONE.
