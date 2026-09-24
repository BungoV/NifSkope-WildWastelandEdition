- - PBRWX1 DONE 15:0x 2026-09-24 PBRWX1:
  PBRWX1 (2026-09-24, Opus 5.5) -- weather preview in the Scene popup. BUILT + GATED, NOT FLOWN BY BUNGO.
  release/NifSkope.exe 14:30:13 sha1 45108d839c259f56880c57eb9201390f50f44fff; rung release/before_pbrwx1 = b6d37f73.
  * Scene popup, new "Sky" group: Sky, Clouds, Sun, Moon checkboxes + Game Day. All live, all OFF as shipped,
    persisted under Settings/Render/Scene/Lookdev Sky|Sun|Clouds|Moon|Game Day (harness scopes isolate them).
  * Sky: the WTHR sky colours for the hour, blended between time-of-day keys in CIELab, drawn on the vanilla
    Atmosphere dome (x the hour's IMSP SkyScale). With Sky off the Lookdev cube is the backdrop as before.
  * Sun: disc on the measured arc (600 / -325 / -150), disc fade 0.15 h, colour extension 2.0 h; Sun ON drives
    the W1 light direction, so the lit side and the disc agree. Sun OFF = W1's own light.
  * Clouds: every drawn WTHR layer ({0,1,2,3,4,5,12,14,15} for CommonwealthClear) through the resource
    manager, per-layer colour and alpha for the hour, scrolling in real seconds (QNAM/RNAM speeds).
  * Moon: Secunda only (the climate's moons byte), position on the arc, phase from Game Day (8 phases,
    4 days each), alpha fades at dusk/dawn; visual only, no light.
  * Rulings kept: sky not fogged, nothing HDR. Fog is the next lane.
  * CLI: `weather --sky` prints the engine clock, sky colours, cloud rows and moon for any --hour/--day.
  * Gate: tests/spells/pbr_wx1_gates.sh, 71 checks 0 failures; all 23 reds FAIL on their own checks. Zero set
    PASS (10 cases), R1 48/0, R2a PASS, R2b PASS (W1 included, live 14/0), R3 15 sections PASS, R4 23 sections PASS.
  * The six older PBR gates (shade_ab, r1..r4) now only refuse on THEIR OWN port's leftover NifSkope; they
    refused every shot whenever another lane's harness was up.
  * Owed: bungo's in-app look at dawn/dusk/night; his open window (pid 7644) predates this build -- restart it.
