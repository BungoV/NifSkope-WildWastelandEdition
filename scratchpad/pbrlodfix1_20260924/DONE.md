DONE 4d30baa9 (release/NifSkope.exe sha1 4d30baa941da526b4a7c835e6924e19c595d9dec; rung release/before_pbrlodfix1 = e5320fdd)

Code regression fixed in src/nifcli.cpp (gltf flag parser scoped to gltf/gltf-export). Not committed.
- lodgen_terrain_pbrm.sh: 14 checks, 0 failures, RESULT PASS (run_fixed.txt)
- red control (rung e5320fdd = tree minus the fix): 14 checks, 5 failures, RESULT FAIL (run_current.txt)
- collision --skeleton: rung SHADOWED, new ok, byte-identical to before_gltfexport1
- gltf_export_options.sh: rc 0, 0 row(s) not as registered
- lodgen_resources.sh: RESULT PASS; lodgen_terrain_vt.sh: 45 checks, 0 failures, RESULT PASS
- pbr_shade_ab zero set: SUMMARY 10 cases, 0 failures, 3 empty by the viewer -> PASS
Finished 04:24.
