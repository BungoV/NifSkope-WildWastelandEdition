## HANDOFF text
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

## WW_CHANGES text
### Weather preview in the Scene window (lane PBRWX1, 2026-09-24)
- **Sky, Clouds, Sun, Moon and Game Day rows** in the Scene popup. Each part switches on live and is off
  by default; the choice is remembered.
- **Sky:** the weather's sky colours for the chosen hour, blended the way the game blends them, on the
  game's own sky dome. With Sky off the Lookdev backdrop is unchanged.
- **Sun:** the sun disc follows the game's arc for the hour and fades in and out at the same times the
  game does. With Sun on, the model is lit from where the disc is.
- **Clouds:** the weather's cloud layers with their own colours, fades and scrolling speed, in real time.
- **Moon:** the moon rides the same arc at night, with its phase set by Game Day. It gives no light.
- `weather --sky` on the command line prints the same sky, sun, cloud and moon numbers for any hour.
- Every part off gives exactly the picture the previous build gave.

## MISTAKES text
- 2026-09-24 PBRWX1: timestamps in progress.md were typed from feel ("13:3x/13:4x") and the clock then read
  13:27. Rule already exists: run `date` in the same turn as any timestamp.
- 2026-09-24 PBRWX1: edited tests/spells/pbr_wx1_gates.sh while bash was running it. bash reads a script as
  it goes, so the running copy hit "FR: command not found" and a syntax error and died with no verdict.
  Never edit a driver mid-run; copy it aside if it must change.
- 2026-09-24 PBRWX1: the sky dome was never pixel-tested before the full gate. Every harness shot drew the
  Lookdev cube because the dome mesh was "not found": a fresh settings scope has no archive index and
  GameManager::folders() returns empty too, and the first loose-folder fix joined the path to the
  resource stack's <entry>/Meshes folder (meshes/meshes/...). The census line `drew=sky:refused(...)` said
  so from the first run. Read the census drew= line before believing a sky picture.
- 2026-09-24 PBRWX1: renamed held exes aside under made-up suffixes (NifSkope_inuse_wx1b/c/d) instead of
  the holder's pid, and trusted `( : >> exe )` as a "free to link" test -- the link then died
  "Permission denied" with another lane's harness on it. Name the rename by pid; rename whenever any
  NifSkope runs from release/.
- 2026-09-24 PBRWX1: the six older PBR gates' guard matched ANY `--port` NifSkope, so another lane's
  harness made the zero set and R1/R2a refuse shots and report FAIL. Guards now match their own port.

## Skill review
- nifskope-ww-pbr-shade-ab: added section 2c "The WX1 gates" (commands, sections, the 23 reds, the
  echo living in the PBRM census, the missing archive index in a fresh scope and the <entry>/Meshes
  join, measured moon and probe numbers, never edit a running driver, the append test is not a lock
  test). The description names the weather gates. 98 -> 141 lines, plus the own-port guard.
- nifskope-ww-build-verify: its "is the exe held" discriminator (Get-Process Path) reports the LAUNCH
  path even after the file was renamed; worth a line there (not edited here -- it lives in the Claude
  project skills, outside this repo).
- nifskope-ww-render-shot: nothing new needed.
