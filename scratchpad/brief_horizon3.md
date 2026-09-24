# Lane HORIZON3 -- objects receive far shadows cleanly (three tiers) + the scrappable flag for the far-map consumer

## Header
- Tree: `E:/Projects/NifskopeWildWastelandEdition`, branch main, ONLY lane in the tree; starts AFTER HORIZON2 lands (the
  director launches you and fills in the exe line). Exe at launch: `release/NifSkope.exe` = HORIZON2's final: 2026-09-18 23:47:33, 22,949,376 B, sha1
  b349f807426be700ed2ff9b54ee23e4fab3ba037 (re-read and print it yourself; first line of the report). Rung ONCE before your first build:
  `release/NifSkope.before_horizon3.exe` (never delete any `release/NifSkope.before_*.exe`,
  `release/NifSkope.archlock1_rung.exe`, `release/NifSkope.at_0117.exe`, or a `NifSkope_inuse_*.exe`). Markers
  `scratchpad/horizon3_20260919/BUILDING` (touch FIRST, REMOVE when you write DONE) / `DONE` (first word `horizon3`).
  Report `scratchpad/horizon3_20260919/lane_horizon3_report.md`, INCREMENTAL (section 0 inside your first ten tool
  calls); `PENDING.md` past half your context. Never commit, never `git stash`, never edit `WW_CHANGES.md` or `HANDOFF.md`.
- Game: `tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?` is ITS OWN command before every build and every exe
  run. Fallout4 up = no build and no exe run: do everything else, then PENDING.md headed `BUILD PENDING`, stop; never
  wait-loop on the game. A NifSkope with no `--port` is bungo's window: rename the exe aside as
  `NifSkope_inuse_<pid>.exe` at link time, never kill it; a wedged harness NifSkope (has `--port`) is ended by sending
  `NifSkope::open <scene>` as a UTF-16LE UDP datagram on its port (src/main.cpp IPCsocket), never by kill. Headless
  runs: `--port <unused>` + `WW_WINDOW_AT=1960,40`, one at a time, second monitor only, every path ABSOLUTE `E:/...`,
  every gate script passes the scene file positionally.
- Build: MSYS2 UCRT64, `export PATH=/c/msys64/ucrt64/bin:/e/Tools/GIT/mingw64/bin:$PATH`, `mingw32-make -f Makefile.Release -j8`,
  make's exit code is the gate; `nifskope-ww-build-verify` INCLUDING its object-vs-header check. ONE background waiter
  at a time. `date` for every timestamp.
- Read first: `CONSTITUTION.md`; `HANDOFF.md` top block (HORIZON2, HORIZON1, LODIV7 LANDED blocks); root `MISTAKES.md`
  top 20; `scratchpad/horizon1_20260918/lane_horizon1_report.md` s1 (the edge table: p50 239.7 u, p90 512, p99 1,158.7,
  max 5,476; 41,042 of 250,320 triangles have an edge over 512 u, 7,926 over 1,024, 664 over 2,048; top offenders
  `edges_urban.json`), s2 (the v8 stream), s12 (rows); HORIZON2's report in full (the terrain fix and its third
  witness -- you reuse that witness); `docs/LODGEN_NATIVE_LODO_LODI.md` s2 (`.lodo` vertex layout), s4 (the cold
  record `refFormId`/`scolPart`/`identity`, flags), s4.9/s4.10, s4.11 if HORIZON2 added one; `docs/FO4CS_IMPROVED_LOD_PLAN.md`
  s9 (the consumer contract you extend); the horizon marcher for object vertices in the sources the reports name.
- Skills: `nifskope-ww-lodgen`, `nifskope-ww-render-shot` (the `horizon` rows), `nifskope-ww-build-verify`,
  `ww-test-harness-add`, `ww-control-calibration`, `ww-texel-picture`, `ww-contract-provenance`.

## bungo's words (2026-09-18 23:5x .. 2026-09-19 00:0x)
Asked whether the object pictures were optimal: no -- the gradient across a big quad is an artifact. Asked whether the
data could be stored per edge; the director's answer, which he accepted with "Do that": three tiers -- per vertex on
short edges (as now), vertices INSERTED along long edges at bake time (subdivision, NOT decimation: the authored LOD
models rule stands, no mesh loses a triangle), and a small per-face horizon texture on big faces where an interior
shadow never reaches an edge; plus one bit per placement saying whether it is workshop-scrappable so the far-map
consumer can drop settlement junk. FO4CS takes it from there; our side is the bake, the file, the viewer, the gate and
the contract. STANDING: every new master ships OFF; an owed ruling never ships as a default -- both thresholds are
knobs with default 0 (= tier 1 only), and your report gives bungo the table he rules from.

## The work (each step lands in the report before the next)
1. **Measure before designing**, on the 33,123-placement urban region (HORIZON1's population) with the existing Python
   readers: (a) per candidate edge threshold 256 / 384 / 512 / 768 / 1,024 u: edges over it, inserted vertices at one
   sample per `threshold` units, triangles added by the fan split, bytes added (16 B a vertex + the `.lodo` vertex);
   (b) per candidate face-area threshold (a face = a planar run of coplanar adjacent triangles inside one placement;
   define it and print the rule) 256² / 512² / 1,024² u²: faces over it, texels at 64 u and 128 u, bytes; (c) the
   shadow-interior case: for the 20 largest faces, cast the third witness from HORIZON2 at 64-u samples across the
   face at sun elevations 5/15/30 and report how many have a lit/dark boundary that touches NO edge of the face (that
   is the count that justifies tier 3 at all; if it is ~0, say so and tier 3 becomes a documented non-need, not code).
   One table per (a)/(b)/(c). Numbers from named logs.
2. **Tier 2 in the bake**: `--horizon-subdivide <u>` (default 0 = off). At bake, before the horizon march, every
   `.lodo` edge longer than `<u>` in world units gets vertices inserted at even spacing (both triangles sharing the
   edge are fan-split; shared edges split once; welded, no T-junctions -- the refuter counts T-junctions and must
   read 0). Inserted vertices inherit interpolated position/normal/UV/AO/sky and get their OWN horizon march. The
   `.lodo` gains the vertices in a way the v7-era readers refuse by name (bump the version the docs say to bump, way
   back byte-identical at 0). Triangle count added is a census word (`horizonSubdivTriangles`, `horizonSubdivVertices`).
3. **Tier 3 in the bake**, ONLY if step 1(c) justifies it: `--horizon-face-sheet <area u²>` (default 0 = off). For
   each face over the threshold, a horizon texture at 64 u/texel (16 bins as four RGBA), stored in a new `.lodi`/`.lodo`
   stream with a per-face UV rectangle; the viewer samples it in the fragment where a vertex carries a face index.
   If 1(c) says no, write the section "tier 3 not needed on this population, and the number that says so" and skip.
4. **The scrappable bit**: research the rule from the ESM through the existing record readers and the xEdit
   definitions (`reference_xedit_fo4_definitions` in the director's memory names the cache): a REFR is workshop-
   scrappable when its base object is the created object of a COBJ scrap recipe (the workshop scrap keyword /
   formlist -- name the exact records and formIDs you used) AND the REFR lies inside a workshop's location or linked
   build area (name which). Print the rule, the count on the region, and five named examples each way. Store it as one
   bit in the cold record's flags (say which bit; the doc's flag table), census word `scrappablePlacements`; the viewer
   channel `scrappable` (magenta = yes, grey = no) with a note line.
5. **Refuters and the gate** `tests/spells/lodgen_horizon3.sh`: G1 way back byte-identical with both knobs at 0 and the
   bit present-but-derived (a v8 bake from the rung exe vs this exe at knobs 0: the only diff is the flag bit and the
   census words, listed); G2 T-junctions 0, welded vertex count equals the predicted count from step 1(a) for the
   chosen threshold; G3 the inserted vertices' horizons within 2 deg of the HORIZON2 third witness at 20 sampled
   inserted vertices, control red (a rotated bin); G4 the picture pair: the HORIZON1 close framing at sun 120,15 with
   subdivide 0 vs 512 -- pixel-diff > 0 on the big quads named in HORIZON1 s1 and the gradient measured shorter (the
   luminance ramp across the tall building's wall spans fewer pixels; print both spans); G5 neighbours at standing
   counts (`lodgen_horizon.sh` at HORIZON2's count, `lodi_v7.sh` 12/0, `lodl_channels.sh`, `lodgen_slab.sh` 16/0,
   `native_open.sh` 17/0/2, `render_shot.sh` 82/0, `lodl_open.sh` 23/0, `lodgen_native.sh`).
6. **Pictures** `scratchpad/horizon3_20260919/images/`: close + full at sun 120,15 and 240,15 for subdivide 0 / 512 /
   256; the tall building's wall as a texel crop with the ramp span in the caption; `scrappable` channel full framing.
   `ww-texel-picture` captions: the number in the caption is the number in the report.
7. **Docs**: format sections for the new vertices/stream/bit (provenance per line), census words, the knobs in
   `nifskope-ww-lodgen`, the viewer rows in `nifskope-ww-render-shot` (delivered as text), and the consumer contract
   `docs/FO4CS_IMPROVED_LOD_PLAN.md` s9 extended: how a consumer reads tier 2 (nothing new: more vertices), tier 3 (the
   face sheet sampling rule), and the scrappable bit (drop from the far-map caster set when the workshop owns it).

## Rules
- Subdivision only ever ADDS vertices and triangles; no LOD mesh loses geometry. `--road-detail 1`. Masters OFF.
- No "fixed/final/true"; mechanism + refuter. Every number in the report is from a log you name.

## Report (`lane_horizon3_report.md`, incremental)
0 exe at launch + rung; 1 the three measurement tables; 2 tier 2 design + format; 3 tier 3 (or the non-need section);
4 the scrappable rule + counts + examples; 5 gate counts and each refuter shown red; 6 neighbours; 7 build (mtime,
size, sha1, `find src tests res -newer` empty, object-vs-header check); 8 pictures + captions; 9 docs + skill text;
10 WW_CHANGES paragraph + HANDOFF LANDED block for the director to splice; 11 ROWS FOR BUNGO: the threshold table he
rules from (edge threshold vs bytes/triangles/ramp span; face threshold vs bytes), in plain words; 12 MISTAKES entries
written; 13 finished-work skill review. END with `DONE` (first word `horizon3`) and five plain sentences for bungo,
ending with: his open window needs a restart.
