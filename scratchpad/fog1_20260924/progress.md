# FOG1 progress

- 2026-09-24 17:25 start. Precheck: no Fallout4/NifSkope running; E: 167G free; release exe sha1 45108d83 (matches brief). Read CONSTITUTION, brief, chain rules, spec_fog, PBRWX1 text, skills pbr-shade-ab + build-verify.
- 17:29 rung release/before_fog1 made from release/ (exe sha1 45108d83, shaders identical).
2026-09-24 17:44 step: all FOG1 sources written (esmweather fog model + CLI, lookdev_fog.glsl, ground/fo4/pbrm shaders, lookdevstage fog uniforms + pins + echo, Scene Fog row, scenetest fogLeg); building
2026-09-24 17:47 build RC=0 first try; CLI fog matches fog_model_out (12:00 d64750 alpha 0.600183, 06:00 near 1900 w 0.5, 01:00 d4096 0.237877); committed 3 source commits 4a4d24b 031c521 5928bb5 
2026-09-24 17:55 gates written (tests/spells/pbr_fog1_gates.sh/.py); partial runs green: cli 117/117 probes, alpha 6/6, colour 4/4, height 2/2, geo + distance scale (R 63->126 for +1000 units), sky identical; full green run started
2026-09-24 18:00 GREEN: pbr_fog1_gates 59 checks 0 failures (green/); reds started
2026-09-24 18:08 13/13 reds FAIL (reds/summary.txt); rebuilt 18:08 for a header comment (fog probe scale); final green + regress (zero, r1..r4, wx1) running on it
2026-09-24 18:14 final green 59/0 on be0704e3; skill 2d committed 52f6fad; DELIVERABLE_TEXT drafted; regress running
2026-09-24 18:46 regress: r1 48/0, r2a/r2b/r3/r4 PASS, wx1 71/0, but ZERO SET FAIL (embedded_lit 783 px max 56, pistol_10mm 5 px max 1) with fog OFF. Bisect: old fo4_default.frag PASS; include-only PASS; fog line alone FAIL; probe alone FAIL; fogOn renamed (C++ never sets it) still FAIL -> the driver compiles the shader differently when the dead-at-runtime fog code is present. #ifdef WW_FOG guards PASS.
2026-09-24 18:46 fix: fo4_default.frag fog under #ifdef WW_FOG; new fo4_fog.frag (#define WW_FOG + #include fo4_default.frag; the loader now drops an included file's #version) + fo4_fog.prog (no conditions); Renderer::setupProgram swaps fo4_default<->fo4_fog by name when wwLookdevFogWanted (red fognoswap). New gate section `legacy`. Built 18:45:55 sha1 629a3911; zero set + full gates + reds fognoswap/fogleak running.
2026-09-24 19:16 zero set 10/0 after the fix (zero2/); gates 64/0 (full run 63/64, far framing reframed FOV 8 -> legacy 9/9); reds fognoswap + fogleak FAIL; regress2 R1 48/0 R2a R2b R3 R4 PASS WX1 71/0; commits 8bfb374 9c754f3 85f052a; DONE.md written
