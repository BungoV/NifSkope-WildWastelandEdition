## HANDOFF text
FOG1 (2026-09-24, Opus 5.5) -- weather fog in the Scene popup. BUILT + GATED, NOT FLOWN BY BUNGO.
release/NifSkope.exe 18:45:55 sha1 629a3911d8a1c33c1879d9762fcfc0ced86c2ebf; rung release/before_fog1 = 45108d83.
Commits (local, not pushed): 5928bb5 model+CLI, 031c521 shaders+stage, 4a4d24b Fog row+live leg, 96bfe94 gates, 52f6fad skill,
  8bfb374 fog as its own program (Fog off = pre-fog shader), 9c754f3 gate section legacy, 85f052a skill.
* Scene popup, the existing "Fog" heading now holds a live Fog checkbox (was a placeholder). OFF as shipped,
  persisted under Settings/Render/Scene/Lookdev Fog, harness scopes isolate it; greyed outside Lookdev.
  Divergence from the brief wording ("Sky group"): the row sits under the ruled UI's own Fog heading.
* Model (src/esmweather.cpp wwFogAt): FNAM 18 floats (short FNAM copies [8..11] into [14..17]), NAM4 32
  scales; day weight on the climate TNAM widened by fDaytimeColorExtension (2.0); near/far/power/max/
  height mids+ranges/HDS blended linearly; the four NAM0 fog rows (1/12/17/18) blended in CIELab on the
  sky clock's keys x NAM4 on the same keys, then pow 2.2; cb12[41..46] packed as spec_fog.md.
* Shader (res/shaders/lookdev_fog.glsl): spec 2.4 line for line (ramp, two-plane height blend, max clamp,
  near escape x66.67, HDS weight, fog-sun term with fDirectionalFogPower 8 -- sun colour/intensity INFERRED
  from the lookdev sun). Applied per fragment on the lookdev ground, pbrm_default and the legacy path
  (Lookdev only). The legacy path fogs through its own program, fo4_fog.prog (fo4_default.frag with
  WW_FOG defined; the loader drops an included file's #version), swapped in by name only while a draw
  fogs: the fog code merely present in fo4_default, switched off, moved 783 px of the zero set. SPACE: linear light, before exposure and the view transform (studioOutput); in the legacy
  fo4_default path that is lin = c^2 (its tonemap squares), fog, then sqrt back. World z is measured from
  the lookdev ground plane. Sky dome, sun, moon, clouds are NOT fogged (ruling).
* Hour row drives it live (echo in the PBRM census: `fog=on fogw= near= far= ... drew=on(groundz= scale=)`).
* CLI: `weather --fog [--fog-probe "d,z;..."]` prints the record, per-hour fog, cb12 rows and probes.
* Gate: tests/spells/pbr_fog1_gates.sh, 64 checks 0 failures (CLI 117/117 probed fragments = the judge;
  shader alpha 6/6 +-1, colour 4/4 +-2 incl. 07:00 between keys and 19:30 dusk, height 2/2, distance
  scale exact, sky identical on/off, the model inside 3000 u untouched, OFF = before_fog1 byte for byte on
  3 framings pinned + unpinned, legacy path 5/5, live leg 14/0). 14/14 reds FAIL.
  Regression on this exe: legacy zero set vs before_fog1 10 cases 0 failures; R1 48/0, R2a PASS,
  R2b PASS (live 14/0), R3 PASS, R4 PASS, WX1 71/0.
* Seen at the preview's scale: vanilla day fog starts at 3000 u and the lookdev ground is an ~8192 u quad,
  so at view 8 noon fog changes 0 px and night 1 level. From 20000 u it is plain (fog1_far_sheet.png).
  Night fog colour is near-black (NAM0 night bytes ^2.2, no eye adaptation in the preview).
* Owed: bungo's in-app look at dawn/noon/night. Any open window of his predates this build -- restart it.

## WW_CHANGES text
### Weather fog in the Scene window (lane FOG1, 2026-09-24)
- **Fog row** in the Scene popup. It switches on live and is off by default; the choice is remembered.
- **Fog:** the weather's own fog for the chosen hour, day and night blended the way the game blends
  them: its distances, thickness and height fade, and its four fog colours, on the ground and the model.
  The sky is never fogged.
- The fog follows the hour row as you drag it.
- `weather --fog` on the command line prints the same fog numbers for any hour and distance.
- Fog off gives exactly the picture the previous build gave.

## MISTAKES text
- 2026-09-24 FOG1: ran `find /e/Projects -maxdepth 5` to locate two skills -- a search over every project
  root, against search-lean. It ran past 120 s and was stopped. The skills live in the repo's
  .claude/skills and ~/.claude/skills; list those two folders.
- 2026-09-24 FOG1: a Python heredoc turned the `\` of a bash line continuation into `\n` text, so
  `env` got an argument `n` and two shots died rc=127. Known rule (patch scripts via the Write tool);
  broken again for a one-line insert. Fixed with the Edit tool.
- 2026-09-24 FOG1: the first straight-down distance probe was centred at x 5000, outside the ~8192 u
  lookdev ground, so it photographed the Lookdev cube and "failed". Read the framing before the number.

- 2026-09-24 FOG1: the Fog-OFF identity gate covered only the PBR fixture, so the fog code sitting
  (switched off) in fo4_default.frag was caught by the legacy zero set at regression time, not by the
  lane's own gate: 783 px on GRailCurveR01. Code behind a false uniform is not "off" to the driver;
  an off path that must stay byte-identical goes under #ifdef in a variant program. Gate added (legacy).
- 2026-09-24 FOG1: ran pbr_shade_ab.sh with a RELATIVE --out: every shot, old arm too, came back
  "NO PICTURE" rc=0. Give it an absolute path (the regress runner did).
- 2026-09-24 FOG1: a Python splice sent through a bash heredoc turned `'\n'` into a real newline
  inside C++ source (glcontext.cpp); caught before building, fixed with the Edit tool. Second time
  in this lane -- C++/shader escapes go in with the Edit tool only.

## Skill review
- nifskope-ww-pbr-shade-ab (repo .claude/skills): added section 2d "The FOG1 gates" (then the legacy
  section, fognoswap, and the rule: byte-identical off paths go under #ifdef in a variant program) (commands, sections,
  the probe modes and what each reads, the 13 reds, the ~8192 u ground limit for geometry probes, why fog
  barely shows at view 8 and the 20000 u framing). Description names pbr_fog1_gates.sh.
- nifskope-ww-build-verify: `tools/ww_build.sh <sources>` worked first time both builds; nothing new.
- nifskope-ww-render-shot / ww-test-harness-add: the fog leg followed the existing weather-leg pattern;
  nothing new needed.
- search-lean: followed except once (MISTAKES above); the rule is right as written.
- Drift noted, not fixed: nifskope-ww-pbr-shade-ab exists only in the repo copy; the E:\Projects\Claude
  skill tree has build-verify and render-shot but not it. No new skill: the per-lane gate pattern is
  already the shade-ab skill's job.
