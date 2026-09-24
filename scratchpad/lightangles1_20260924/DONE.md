DONE e5320fddb24f85885835af3f03b46a4e0ee6d885

Lane LIGHTANGLES1, 2026-09-24 03:54 (date-read). release/NifSkope.exe 24,283,136 B, 03:48:01, sha1 e5320fdd.
Rung: release/before_lightangles1/ (exe ae101325 = PBRR1, shaders diffed identical).
Red build kept for re-runs: release/NifSkope.red_lightangles1.exe (a433c337 = harness + UNCHANGED lightingwidget.cpp).
NOT COMMITTED. bungo's open NifSkope (none was running at 03:54) needs a restart to have this.

Gate verdicts
- (a) tests/spells/light_angles.sh on e5320fdd: roundtrip 6 launches PASS, 56 checks 0 failures; picture leg PASS
  (noise 0 px, planted 180/90 vs 0/0 = 129,751 px, bar 1,000).
- (a) red, literal rung before_lightangles1 (ae101325), picture leg: 0 px moved -> FAIL (as required).
- (a) red, same harness on the unchanged code (a433c337), roundtrip: rt2..rt6 FAIL, each loaded 0,0 while the store held
  the saved value -> 5 failed launches (as required).
- (b) pbr_shade_ab --old release/before_lightangles1: 10 cases, 0 failures, 3 empty by the viewer -> PASS (all px=0,
  program/camera/PBRM censuses same).

Files: src/ui/widgets/lightingwidget.cpp (the fix), src/lightanglestest.cpp (new harness), src/nifskope_ui.cpp (+5 lines,
harness call), NifSkope.pro (+1 SOURCES line), tests/spells/light_angles.sh (new), .gitignore (+3 lines). All LF-only.
