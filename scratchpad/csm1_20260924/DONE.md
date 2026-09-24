DONE -- lane CSM1, cascaded sun shadows in the PBR renderer (2026-09-24 21:3x).

Release exe: release/NifSkope.exe 20:36:11, sha1 53637ec69b804e34e635ce6f2b371f678ec47a8d
Commits (main, not pushed): a4df29e 786d0f5 0b0ceba 2dd262e 488a99a 47b2cad 71f96c1

* CSM1 gate: 25 gates, 25 PASS (full/). 14 of 14 reds FAIL (run_all.txt).
* OFF identity: 6/6 on the PBR fixture, 0 px differ; legacy zero set 10 cases, 0 failures (zero.log).
* Regressions (regr.txt + regr_*.log): FOG1 64/0, WX1 71/0, R1 48/0, R2a PASS, R2b PASS, R3 15/0, R4 23/0.
* Pictures: CSM1_pictures.png; pics/pic_{8,12,16.5}_{on,off}.png, pics/pic_cascades.png (A|B), pics/pic_cascades_far.png (B|C).
* Ledger text: DELIVERABLE_TEXT.md. Skill: E:\Projects\Claude\.claude\skills\ww-shadow-map-judge\SKILL.md; repo skill section 2e.
* bungo's window (pid 23560) predates this exe: he must restart it to get shadows.
