# FIX1 deliverable text (lane FIX1, 2026-09-26, branch fix1-20260926)

## HANDOFF text

### Lane FIX1 -- the small fixes (2026-09-26, 02:13 to 04:10), branch fix1-20260926, not merged
- Fix 1, colour remap: the game uses a swap's colour-remap index only on a material with the palette flag
  (1.10.155: BGSModelMaterialSwap::Apply tests SLSF1 bit 4). No installed LOD material has that flag, so the two
  Nuka-World rows change nothing on screen. The census now says so (game-applied / ignored / unset). Gate GREEN:
  646 of 647 files byte-identical to the rung (the .lodb differs: paths, time). Commit 57c5f0c7. No install.
- Fix 2, DLC vs vanilla: no systematic gap (Far Harbor and Nuka-World landmarks within 6% brightness and 0.035
  saturation of vanilla; Acadia reads brighter, not greyer). Nothing to change.
- Fix 3, floating objects: `--land-fill-vanilla` (panel: with *Fill unpainted ground with vanilla's colour*) gives a
  landless cell the game's own terrain LOD heights (read as input only). Pre-war: floating placements 101 in 15
  cells -> 0; edge step 8664 -> 200 units; objects unchanged. Commit f4eabed2. INSTALL PENDING (game up):
  scratchpad/fix1_20260926/stage/install_fix3.sh (6 files, backup to replaced/, sha1 read-back; refuses if
  plugins.txt moved since the bake -- CORE.esp left the profile at 02:53). Glowing Sea's 8 grey cells: LAND
  present, grey is the plugin's own paint; no change.
- Fix 4, harness settings: the 11 render spells each force their own settings scope (seeded with the Game Manager
  state, wiped). Red before: impostor_draw under a hostile scope fails 16c and 16d; after, they pass. Remaining
  reds in 5 spells are not settings (listed in fix4/RESULT.md). Commit 5093f561. Follow-up: 12 more spells wipe a
  scope without the Game Manager seed (a first-install dialog on the primary monitor).
- Fix 5a: whole-map FORCE_CARD stays NO (contract §4.13). 5c: .lodj is our own per-chunk cache for
  `--incremental`; nothing in the game or FO4CS reads it; still written by default (ledger doc §4.1). Commit 992b6fb5.
- Fix 5b: a *Ground cover* row under the FO4CS target (label + control only). Gate in WW_LODGEN_TEST: red exe
  (test, old panel) 129/1, build 5 129/0. Commit 7dc3107a.
- Fix 5d NOT DONE, needs its own lane: `--incremental --native` works on one ring; the ruled FO4CS pipeline refuses
  on `--dim all`, on the region products (arrays, cards), and a replayed chunk would need its card-link lines.
- Fix 8: the synthetic native fixture now carries real triangle counts (12,20,12); native-verify rc 1 -> 0 and an
  independent decoder recount equals it. Commit fda01cbc. The committed expectation only GAINED a key.
- Pictures for fix 3 (untracked): fix3/pic_PW_off.png (flat plain, cliff at the LAND edge, objects hanging) and
  fix3/pic_PW_fix.png (the hills are there, the objects sit on them). Fix 1 and 2: no picture, no byte moved.

## WW_CHANGES text

### Far terrain under pre-war Sanctuary's hills (lane FIX1, 2026-09-26)
- New: `--land-fill-vanilla`. A cell with no landscape record now takes the game's own far-terrain heights, so
  objects out there stand on ground instead of floating thousands of units over a flat plain. The panel does it
  whenever *Fill unpainted ground with vanilla's colour* is ticked.
- New: *Ground cover* can be ticked under the Fallout 4 Community Shaders target (it was only reachable under the
  stock target).
- The material-swap census now says whether the game would actually use a colour-remap row.
- The test fixture for the native library carries real triangle counts.
- The render test spells no longer read your own NifSkope settings.

## MISTAKES text

### FIX1, 2026-09-26: a byte-identity gate compared two bakes across a load-order change
The first switch-off bake (03:02) differed from the rung bake (02:52) in the .lodo/.lodi header. Cause: CORE.esp
left the MO2 profile at 02:53 (plugins.txt mtime), and the load-order hash is in those headers. The rung was
re-baked on the current order. Rule: a byte-identity gate's two arms are baked back to back, and the lane records
the plugins.txt sha1 with each arm.

### FIX1, 2026-09-26: a harness scope seeded with Settings/Version only is a Game Manager first install
My fix 5b gate (03:18-03:19) wiped its scope and seeded only Settings/Version=1. NifSkope then treats the run as a
first install: an opaque "Initializing the Game Manager" dialog opens on the PRIMARY monitor before any harness
placement, and Game Folders stay empty (no archives). Found by the fix 4 helper. Rule: seed Game Manager Version 2
and the machine's game state (tests/spells/settings_scope_game.py), as the 11 fixed spells now do.

### FIX1, 2026-09-26: a self-test gate whose red arm cannot run the test
The first fix 5b gate used the pre-fix exe as its red arm, but the test lives in the exe, so the rung never ran
it. And the test read the row through a folded section (isVisibleTo), failing on correct code. Rule: the red arm
of an in-exe self-test is an exe WITH the test and WITHOUT the fix.
