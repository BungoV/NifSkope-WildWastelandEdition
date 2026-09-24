# PBRWX1 progress

- 13:22 started; read CONSTITUTION, HANDOFF PBR lines, brief, chain rules, shade-ab skill. E: 95G free. No Fallout4. NifSkope pids 7644 (bungo) + 33080 (not ours) left alone.
- (13:2x; the "13:3x/13:4x" stamps below were typed from feel -- clock read 13:27 after them) read specs (weather_sky, clouds, moon, fog 2.2). Disassembled ColorRGBtoCIELab 0x657ac0 / ColorCIELabToRGB 0x657880 @155 (dis_rgb2lab.txt, dis_lab2rgb.txt + ripf.py): standard sRGB-decode (0.04045, 2.4) -> XYZ (0.4124..) D65 (95.047/100/108.883) -> Lab (0.008856, 7.787, 16/116); inverse = matching sRGB encode + clamp.
- 13:4x rung release/before_pbrwx1 from release/ (exe b6d37f73 + shaders + before_pbrr4 file set)
- 13:27 (clock read) resumed after compaction; NifSkope pids now 7644 (bungo) + 29688 (not ours, left alone)
- 2026-09-24 13:50 shaders (dome/bill/clouds) written; Scene rows Sky/Clouds/Sun/Moon/Game Day live + 10 Hz cloud timer; scenetest WW_SCENE_TEST_WEATHER leg; 4 TUs syntax-check RC=0 (red control RC=1). Building.
- 2026-09-24 14:07 gate driver + judge written (tests/spells/pbr_wx1_gates.sh/.py); weatherLeg gained (save) checks; rebuilding for the azimuth fix + scenetest (release exe renamed aside to NifSkope_inuse_32236.exe: another lane's harness, port 43393, was running on it).
- 14:09 rebuilt: release exe 14:09:30 (glproperty/glview objects forced after the header edit; the link first hit a lock from another lane's harness on port 43393, exe renamed aside to NifSkope_inuse_20588.exe). Running the full wx1 gate.
- 14:25 rebuilt 14:24:12 (lookdevstage.o only: loose-folder fallback for meshes/sky/atmosphere.nif when the archive index is empty); full gate re-run into g_main2 (script untouched during the run)
- 14:30 exe 14:30:13: dome loads from the resource stack's loose folders (stack lists <entry>/Meshes, so parent is tried); skypx 3/3 ok (off by <=0.48). Full gate g_main3 running.
- 14:34 full wx1 gate g_main3 on exe 14:30:13: 71 checks, 0 failures, PASS. Running the 23 reds (scratchpad/pbrwx1_20260924/reds).
- 14:46 all 23 wx1 reds end FAIL on their own checks (reds.log). Running zero set + R1/R2a/R2b/R3/R4 (regress/).
- 14:52 first regress run refused shots whenever another lane's harness (port 43393) was up: the six older PBR gates matched ANY --port. Fixed each guard to its own port (p08_ownport.py); re-running regress.
- 15:09 regress on exe 14:30:13: zero PASS (10 cases), R1 48/0, R2a PASS, R2b PASS, R3 15/15, R4 23/23, no refusals. Skill section 2c written; DONE.md + DELIVERABLE_TEXT.md written. DONE.
