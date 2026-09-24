# LIGHTANGLES1 progress
- 03:32 started; briefs read
- 03:40 rung release/before_lightangles1 made: exe sha1 ae101325 (= PBRR1 ae101325), shaders copied and diffed identical
- 03:45 read: lightingwidget.cpp load :78/:80 reads "Lighting/Declination"/"Lighting/Planar Angle"; save :125/:126 writes
  "Settings/Render/Lighting/..." (only via the Save Lighting action, aSaveLighting). Writers of the old key: none in src/
  (search: src/ui/widgets, src/gl, src/*.cpp). Other readers of the members: glview.cpp paintGL fromEuler (only when
  Frontal Light is off), rotateLight (wraps to [-180,180] with rint -> ties to even, so +-180 both stay), Shift+L/T keys,
  impostorpreviewtest WW_IMPOSTOR_LIGHT (sets members, not persisted).
- Modulo defect CONFIRMED by arithmetic: runtime range is [-180,180] inclusive -> saved range [-720,720]; load's
  tmp % 720 folds +-720 (= +-180 deg, the light from the opposite side) to 0. Fix: load uses rotateLight's own wrap.
- Plan gate (a): new harness TU src/lightanglestest.cpp (WW_LIGHTANGLES_TEST), chained save->restart->read launches in a
  scratch WW_SETTINGS_SCOPE; red = the same harness built against the unchanged lightingwidget.cpp FIRST (rung code), plus a
  picture leg that runs on the literal rung exe (planted keys, Frontal Light off, render vs 0/0 render).
- 03:44 harness src/lightanglestest.cpp + spell tests/spells/light_angles.sh written; hook-up applied (fix01_hookup.py, CR 0 both); qmake RC 0. RED BUILD started (harness + UNCHANGED lightingwidget.cpp)
- 03:44 .gitignore: +3 lines (blank, comment, tests/fixtures/pbr_data/), LF-only kept (CR 0, LF 73->76); git status no longer lists it; 0 files of it tracked; check-ignore names line 76
- 03:46 RED build done: BUILD-RC=0, exe a433c337 03:45:41 copied to release/NifSkope.red_lightangles1.exe (harness + unchanged lightingwidget.cpp). Literal-rung picture leg: noise 0 px, planted 180/90 vs 0/0 = 0 px -> FAIL (red as expected)
- 03:47 RED roundtrip on NifSkope.red_lightangles1.exe: rt1 save-only PASS, rt2..rt6 FAIL (each: loaded 0,0; the stored keys held the saved values, legacy keys absent) = 5 failed launches. Fix applied (fix02_load.py, +752 B, CR 0). GREEN BUILD started
- 03:49 GREEN build: BUILD-RC=0, exe e5320fdd 03:48:01 (lightingwidget.o 03:47:58). Gate (a) on it: roundtrip rt1..rt6 all PASS (7+11+11+11+8+8 = 56 checks, 0 failures; +-180 both angles come back exact; out-of-range plant 1000/-1600 -> -110/-40), picture leg noise 0 px, planted 129,751 px > bar 1000 -> PASS. Gate (b) pbr_shade_ab started
- 03:54 Gate (b) pbr_shade_ab --old release/before_lightangles1: SUMMARY 10 cases, 0 failures, 3 empty by the viewer -> PASS (every case px=0, censuses same). Scope key removed, no NifSkope left running.
- 03:55 DONE.md (DONE e5320fdd) + DELIVERABLE_TEXT.md written. Lane closed; nothing running.
