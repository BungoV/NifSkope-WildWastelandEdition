# PBRR2A progress
- 04:25 start. Fallout4.exe not running, no NifSkope running. release exe sha1 4d30baa9 (PBRLODFIX1).
- 04:26 rung release/before_pbrr2a made (exe 4d30baa9 + shaders + runtime files, same layout as before_pbrlodfix1).
- 04:32 read briefs, design doc R2a row + s3-5 + RULINGS, CONSTITUTION 4-10, PBRR1/LIGHTANGLES1/PBRLODFIX1 texts, skills pbr-shade-ab + build-verify.
- 04:3x read code (stamp corrected: the earlier 04:50 was typed, not read off the clock): pbrm_default.frag/prog, fo4_default.vert, renderer.cpp setupProgramPBRM, gltexloaders texLoadPBRCubeMap (bsver>=151 gate), sfcube2 (normalised GGX filter, 7-level table r*(10-4r)), glview light uniforms, PBR menu in nifskope_ui.cpp. Found: vanilla mipblur_DefaultOutside1.dds is BGRA8 UNORM legacy header with the cube bits in caps (0x40FE08) and caps2=0 -> texLoadPBRCubeMap would refuse it; plan = rewrite to a DX10 B8G8R8A8_UNORM_SRGB header for the Studio copy.
- 04:42 (date) wrote src/gl/scenelighting.{h,cpp} (state, pins, settings, Studio cube header normaliser + cache), TexCache::loadStudioCube (public wrapper of texLoadPBRCubeMap), pbrm_default.frag R2a paths (linear, sun x PI, Studio cube pair, EV, Standard/AgX/PBR Neutral, sRGB encode in shader, legacy-mode Hable+sqrt out). Found: PBR Route View is in the RENDER menu (ui->mRender, title "Render"), not View.

05:02 NifSkope.pro registered (scenelighting.h/.cpp, ui/scenewindow.h/.cpp, scenetest.cpp; LF, 0 CRLF). Build running (compiled clean, linking). Written: tests/spells/pbr_r2a_fixtures.py (-> tests/fixtures/pbr_r2a_data, 4 cases), pbr_r2a_gates.sh + .py (ev, grey, srgbtag, cube, window, pictures; reds ev/grey/srgbtag/cubedecode/nolive/nosave).

05:47 gates1 (exe 09b57364): grey/cube PASS; ev0 timed out (no picture); srgbtag max 3/255 (decode-then-filter vs filter-then-decode); window geometry FAIL: the headless backstop NifSkope::wwPlaceHeadlessWindow forced the Scene window to WW_WINDOW_AT+WW_RENDER_SIZE on reopen. Fixes: backstop exempts a window with property wwOwnGeometry whose geometry is wholly on a non-primary screen (opacity/no-activate still applied); PBR program skips the hardware sRGB decode (EXT_texture_sRGB_decode) of a tagged base/emissive and restores it at the next setupProgram. Rebuilt b985beac.

05:54 gates2 (exe b985beac): R2A GATES PASS (ev median 2.0049, grey 99.25% at 188, srgbtag max|d| 0 Studio+Legacy, cube max 0.00102, window1+window2 PASS). Reds: ev, grey, srgbtag, cubedecode, nolive, nosave all BITE. Now: zero set.

06:05 zero set vs before_pbrr2a: 9/10 PASS first run; bgsm_duct old_a launch hung (no picture, the rung's first launch -- same shape as gates1 ev0); re-run bgsm_duct PASS (px=0). Zero set = 10/10 PASS (3 empty-by-viewer as standing). Now red shader + neighbour.

06:16 red shader BITES (4/4; E: full, 2 logs unwritten); neighbour pbr_r1_gates 48/48 PASS. DONE.md + DELIVERABLE_TEXT.md written. Lane DONE, exe b985beac.
