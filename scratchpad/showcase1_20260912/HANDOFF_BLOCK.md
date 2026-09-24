### Lane SHOWCASE1 -- final Sanctuary chunk bake with every landed feature, 2026-09-12

Region Sanctuary, cells `-20 24 -9 35`, dim 4 = 9 chunks, plus far rings dim 8 /
16 / 32, the whole-worldspace `.lodl` and shadow heightmap, and a 23-tree card
bake. No code changes, no build, nothing committed; `release/NifSkope.exe` never
run (the lane used its own copy, sha1 `ba7585cb...`, which matches the brief).

Report: `scratchpad/showcase1_20260912/lane_showcase1_report.md` (436 lines,
sections 0-5). Pictures: `scratchpad/showcase1_20260912/images/`, eight PNGs,
`6_contact_sheet.png` is the index. Every bake's exact argv is in
`scratchpad/showcase1_20260912/logs/<name>.argv`; every render is one
`shot.sh` call and each PNG has a `.cam` sidecar with the camera census
written at the moment of the grab.

Five reds, in the report's section 3:
1. Both identity payloads live in vertex colour and NifSkope multiplies vertex
   colour into the diffuse, so the shipping bake cannot be photographed as it
   looks (green crushed to 2 of 255). A fourth `look` bake exists for pictures.
2. There is no env switch for an arbitrary camera rotation -- `WW_RENDER_VIEW`
   is an enum and `ViewUser=8` is the only oblique. One oblique shipped, not two.
3. Card `_fs.DDS` paths get `textures/` prefixed twice by the resolver.
4. `--native` fitted 99 occluder models and wrote 0 boxes.
5. **New:** `--no-identity` gates the whole manifest (`src/lodgen.cpp:3760`), so
   it silently disables the impostor card arrays too.

Open for bungo: `--land-sample`, `--land-detail-source erosion` vs the vanilla
`_msn` reuse, and whether the shipping profile should keep writing the object
index into vertex colour.
