# AO2 deliverable text (lane AO2, 2026-09-26, branch ao2-20260926 from incr2-20260926 @6382a09a)

## HANDOFF text

### AO2 decal round -- stain sheets no longer shade the walls behind them (built, region-baked, NOT installed, not flown)
- Commits on ao2-20260926: 5ef03110 (src: lodgenao.h, lodofile.h/.cpp, lodgen.h/.cpp, nativeemit.cpp), c1a84b8c
  (docs s4.8 step 7).
- Cause: yes, a decal in front. The stain sheet (HitExtAStainsWall3x1_LOD) stands 5 u in front of the tall panel;
  about 3 of the panel's 8 AO rays hit the sheet's back. Its see-through switch lives in the LOD BGSM
  (HitTechStain_LOD: blend + test 134), and the AO caster took every triangle as solid.
- Fix: a blended material, or a decal that tests, blocks AO rays only where its texture's alpha is opaque. Walls and
  skybridges with the same switches still block where they draw (a flag-only rule would have stopped them; rejected).
  Alpha-tested-only rows (fences, tree cards: 64 rows) still cast as solid.
- Gates, Boston region: panel 28942 216 (was 163) against neighbours 211 / 218; seam census 0.2; a, b, s1-s3,
  ballpark, tower face, lone box PASS. 1.16% of vertex AO bytes move, almost all lighter.
- Refuter: WW_AO_OVERLAY_CASTERS=1 = the 3a36445d build's .lodi and .lodo byte for byte.
- Next: the overseer's combined whole-map bake + install; then bungo's eye and a flight.
- What would prove this wrong: bungo's eye on pics\tower_right_decal_5rows.png and pics\08_roads_decal_side.png.

### AO2 tower round -- kit pieces share their AO values (built, region-baked, NOT installed, not flown)
- Commits on ao2-20260926 (= main 422881d4 + 2): 3a36445d (src/nativeemit.cpp), f0dff006 (docs s4.8 step 6).
- Cause, measured in the bytes, both of bungo's tower spots: a wall or roof of kit pieces is several placements, and
  the weld and flat patches worked inside one placement only. Right tower: the tall panel read 115-135 at corners
  where its neighbours read 251-255 (its samples hit the back of an opaque stain overlay 5 u in front); left tower:
  a roof tile under a catwalk read 38 everywhere while its neighbours disagreed at shared corners. Not an empty
  value, not a 255 clamp; "missed by the weld" in effect.
- Fix: the weld pools corners of different placements; a corner on another piece's edge (T-junction) takes that
  edge's value. Patches still stay inside one placement.
- Gates, Boston region: tower panel step 122-132 -> 1.3 (<= 32), darkening close to old's (108/90 vs 90/93), seams
  190 -> 0, black tile 38 -> 92.5 (old 85.8), cross-piece jump 19.8 -> 0.2; a, b, s1-s3, ballpark, tower face,
  lone box PASS. Cost: kink inside one piece 7.5 -> 10.0.
- Refuter: WW_AO_WELD_ACROSS=0 = main's .lodi byte for byte.
- Next: the overseer's combined whole-map bake + install; then bungo's eye and a flight.
- What would prove this wrong: bungo's eye on pics\08_roads_AO_new.png and the two tower crops.

### AO2 -- native per-vertex AO + sky: open roofs no longer drawn black (built, baked, NOT installed, not flown)
- Commits: ee52efc0 (code: src/lodgenao.h, src/nativeemit.cpp, src/lodifile.h), 4b23daa9 + 22d83a48
  (docs/LODGEN_NATIVE_LODO_LODI.md s4.8, s4.10).
- Cause, measured on bungo's three circled spots in 08_boston_oblique_AO.png:
  (a) the Prudential roof was darkened by a far-ring stand-in: the native cast built one scene with every MNAM slot
  mixed, so a lower-ring lid 72 u above the roof took 136 of 136 rays;
  (b), (c) the cathedral wall and tower cap were darkened because the vertex corners sit inside the neighbouring towers
  and pinnacles (back faces), and the corner value spread across a face that is open in the middle.
  bungo's hypothesis holds for (b) and (c), not for (a).
- Fix:
  - One scene per chunk AND slot.
  - Each vertex is the area- and hat-weighted mean over k x k equal-area pieces of its triangles, with
    k = ceil(longest edge / 256), 1..4. The AO ray law (8 rays, 1 - 0.85 hits/8) is unchanged.
  - Sky (v7 stream) is horizon aware: 7 rings at the irradiance medians (7-79 deg), cos-weighted, 8 azimuths
    (56 rays), reach 10000 u, origin offset 2 u along the normal.
  - The .BTO colour B, the v5 placement AO, the .lodo selfAO and the 0x11 byte are untouched.
- Gates on the Boston region (-8 -12 3 -1), each red on the old code:
  - a: open roofs AO 237.5 / 13.4% below 201 -> 252.0 / 2.2%;
  - b: enclosed faces AO 73.2, 179 below open;
  - s1: sky-open 221 -> 252;
  - s3: low canyon 7 -> 48 below open;
  - c: whole map, every file except Commonwealth.lodi sha1-equal; inside the .lodi only vertexAo/vertexSky and the
    two CRCs move.
- Whole bake (not installed): scratchpad\ao2_20260926\whole\after\mod\FO4CSLOD\Commonwealth in the bake2 worktree.
  The overseer installs Commonwealth.lodi (+ the .lodb record) only. whole\before equals the installed files.
- Cost: the whole-map instances stage 1478 s -> 1810 s (+22%). The whole bake is 5448 s before and 5386 s after
  (noisy machine).
- Open:
  - The wall foot reads only 12 bytes darker than the wall top (was 33): the whole-triangle averaging flattens it.
    A least-squares vertex fit would keep it; bungo's call.
  - Sky is now physical for every facing, so walls read 99 -> 72 and soffits 74 -> 25; FO4CS consumers darken walls more.
  - Face step 256 leaves a mean 6 byte per-vertex sampling error against a dense reference (step 128: 3.6, +85% cast).
  - tests/spells/lodi_v7.sh G3 (stream vs the 0x11 byte) was written for the old law; it was not re-run and is
    expected to move.
- What would prove this wrong: bungo's eye on the after picture and the three crops in scratchpad\ao2_20260926\pics, then a flight.

### AO2 split-line round -- one AO value per surface point; AO off means off (built, baked, NOT installed, not flown)
- Commits: e5c0beb4 (viewer), 7e081591 (weld + under-ground), fc03aaa6 (library selfAO), e93e19b5 (docs),
  5bebb273 (flat patches), 2d8f5c9a (DROPPABLE foot variant, branch tip), 75226ac8 (docs).
- Cause of bungo's "split lines" (ballpark roof, tower faces, lone box): ee52efc0 gave each split copy of a corner
  (UV seam / smoothing copies) its own value, so one point of a flat face carried two bytes. Boston: 100,900 such
  pairs, mean jump 25.2 bytes. Not the viewer's lighting: the step is in the stored bytes.
- bungo's hypothesis "the triangles are geometry that's not connected" holds at all three spots: each line runs
  along an edge where the triangles share no vertex, only duplicate corners (ballpark 32, lone box 68, tower 136
  bytes apart), the box's diagonal included.
- Fix: copies within 0.5 u and 30 deg pool into one value (0-1 deg jump 25.2 -> 0.0); samples more than 32 u under
  the ground are used only when a vertex has nothing above ground (lone box foot 97 -> 141, diagonal 68 -> 11);
  bungo's "merge flat geometry": each flat patch (normals within 1 deg) takes one linear field, so no corner can
  draw a diagonal (ballpark 26 -> 9, tower 69 -> 2, box 11 -> 2; Boston kink share > 16: 27.5% -> 7.7%);
  the library selfAO uses the same face cast (the Charles bridge deck 38 -> 240-245); the viewer's AO off now
  draws no AO at all (bridge-deck gate 0.44 -> 1.00).
- Gates: ballpark and lone box PASS (fail on ee52efc0); earlier region gates a, b, s1-s3 all PASS; each knob's
  refuter reproduces the previous bake byte for byte.
- Foot variant for bungo to pick (2d8f5c9a, droppable): the patch plane fits the face samples, not the corners.
  Wall foot/top gap: installed 32.6, split-line fix 7.6, variant 13.6 bytes; lone box 255 -> 61 top to foot,
  smooth. Cost: ballpark step 37 (fails <= 32), kink share 7.7% -> 17.8%. Picture: pics\walls_AO_old_fix_foot.png.
- Rulings recorded: sky stream darker walls/soffits accepted; install = ONE combined whole-map bake (ROADS1 +
  FLAT1 + AO2) after merge.
- Card ruling (bungo, 2026-09-26): impostor tree cards get NO per-vertex AO, only the one placement AO value per
  tree. Per-corner AO on a flat card smears a gradient and doubles the card's self-shading. Cards stay on the
  one-value path.
- Whole bake: stopped at the coordinator's word; the overseer runs the one combined whole bake after the tower fix.
- Open: which fit ships (corner fit or foot variant). Sharp creases (> 30 deg) keep separate values by design.
- What would prove this wrong: the crops in scratchpad\ao2_20260926\pics (old | ee52efc0 | new), then a flight.

## WW_CHANGES text

### Native LOD objects: stains and glass no longer darken the walls behind them (AO2 decal round, 2026-09-27)
- The baked ambient shading now treats see-through overlays (stain sheets, window glass, other blended or decal
  materials) as see-through: they shade what is behind them only where their texture is opaque. Before, a stain
  sheet hanging in front of a wall darkened the wall behind it.
- Tuning knob for testing only (environment): WW_AO_OVERLAY_CASTERS=1 treats every material as solid again.

### Native LOD objects: walls and roofs built from kit pieces shade as one surface (AO2 tower round, 2026-09-26)
- Buildings assembled from several wall or roof pieces no longer show a hard step in the baked ambient shading
  where two pieces meet: pieces now share their shading at every point they touch, including where a short piece's
  corner sits on a long piece's edge.
- Tuning knob for testing only (environment): WW_AO_WELD_ACROSS=0 restores the per-piece shading.

### Native LOD objects: no more split lines in the baked ambient shading (AO2 split-line round, 2026-09-26)
- A corner a mesh stores as several copies (for its UVs or smoothing) now gets one shading value, and each flat face
  takes one smooth shading gradient (bungo's idea: merge flat geometry), so flat roofs, tower faces and boxes no
  longer show a hard line or a diagonal along their triangle edges. Creases keep their shade.
- Parts of a mesh buried under the terrain no longer darken the visible foot of a building.
- The model's own shading stored in the .lodo library uses the same method; open decks such as the Charles bridge no
  longer read dark.
- LOD viewer: "AO off" now draws no ambient shading of any kind.
- Impostor tree cards keep one shading value per tree, by design.
- Tuning knobs for testing only (environment): WW_AO_WELD_DEG, WW_AO_UNDER_TOL, WW_SELFAO_FACE,
  WW_AO_PATCH_DEG, WW_AO_PATCH_FIT.

### Native LOD objects: ambient shading no longer blackens open roofs (AO2, 2026-09-26)
- The per-vertex ambient occlusion baked into FO4CSLOD .lodi files now uses each LOD level's own geometry only.
  A coarse stand-in for a far ring no longer shades a roof it floats above.
- Each vertex is now shaded from samples spread across its faces, not from the corner alone. A corner tucked inside
  a neighbouring building no longer blackens a wall that is open in the middle.
- The per-vertex sky visibility stream now sees the sky down to the horizon (7 elevation rings, 8 directions,
  weighted by how much light each band of sky casts on a flat roof, reach 10000 units, as FO4CS Skylighting's default).
  Street canyons read darker than open roofs; walls and undersides read by the share of sky they can see.
- Only the .lodi changes; every other LOD file bakes byte-for-byte as before. Whole-map bake cost +22% on that stage.
- Tuning knobs for testing only (environment): WW_AO_FACE_STEP, WW_AO_FACE_MAX, WW_SKY_REACH, WW_AO_PROBE.

## MISTAKES text

### Heredoc backslash trap, four times in one lane (AO2, 2026-09-26)
- A bash heredoc (or a printf format) holding a literal backslash was written through the tool and lost or changed
  the backslash:
  - a C++ probe insertion;
  - the bake_region printf "\n";
  - gatec.py's '\\' became '\', a SyntaxError;
  - worstface.py's split('\\') became split('\'), a SyntaxError (split-line round).
- Rule: never put a backslash literal in generated code. Use os.sep, chr(92) or a raw file written with the Write
  tool, then run it.

### A brief's coordinates were taken on trust (AO2, 2026-09-26)
- The brief named world coordinates for the "right white tower", and they pointed at another building. Time went
  into probing the wrong placement.
- Rule: match a circled spot by projecting candidate placements into the picture's camera and checking the pixel,
  before measuring.

### Comment written from memory, not from the constants (AO2, 2026-09-26)
- A lodifile.h comment said the sky rings span "5 to 87 deg". The constants say 7.1 to 79.5. It was caught before
  the commit and corrected.
- Rule: derive numbers in comments from the code constants in the same turn.

### A finished bake was read off concatenated background output (AO2 split-line round, 2026-09-26)
- Two background bakes wrote into one reading; the tail of the older one was taken as the newer one's "rc=0".
- Rule: read the bake's own .out file and its rc line, never a merged task output, before using its files.

### make clean leaves this tree unbuildable (AO2 split-line round, 2026-09-26)
- After `make clean`, make stops on "No rule to make target GeneratedFiles/.obj/icon_res.o": the Makefile lists the
  object by a relative path but has its windres rule under the absolute one.
- Rule: after a clean, run `make -f Makefile.Release <absolute path>/GeneratedFiles/.obj/icon_res.o` once, then
  tools/ww_build.sh.

### A harness NifSkope exited 0 with no window, and a clean rebuild was blamed first (AO2, 2026-09-26)
- For about 30 minutes every harness launch (old and new exes, any folder) returned 0 at once with no log; 30 minutes
  later the same exes started normally. A clean rebuild (12 min) was done on the stale-build theory before a control
  run of the OLD exe in the same minute showed it failing too.
- Rule: when a harness launch fails, run the last known-good exe as a control in the same minute before touching the
  build.

### A sample filter was shipped to a region bake without a fallback (AO2 split-line round, 2026-09-26)
- The first under-ground rule dropped the buried samples outright. Vertices whose samples were all buried then had no
  weight and fell back to a cast at the vertex per copy, which re-split exactly the copies the weld had joined
  (10-20 deg jumps 1.3 -> 4.5). The spot gates passed; only the region census showed it.
- Rule: a filter on samples needs a stated fallback for the empty case, and the jumps census runs on every bake that
  touches the cast, not only the spot gates.

### A whole-map bake was started before the brief settled, then could not be stopped (AO2, 2026-09-26)
- The whole bake was launched with the weld + under-ground exe; minutes later the brief asked for flat patches.
  Stopping the task ended the shell but left the -no-gui NifSkope running (a kill was refused), so it ran 40 more
  minutes beside the region bakes and its output was thrown away.
- Rule: launch the hour-long whole bake only after every follow-up in the queue is in the exe; never stop a bake
  through its shell.

### The harness exit-0 flake came back (AO2, 2026-09-26, 19:33-19:48)
- Renders again returned 0 at once with an empty log; a control run of the exe that had rendered a minute before
  failed the same way, so no build was touched. Fifteen minutes later renders worked.

### A bare turn.sh call joined the machine lock's queue, twice (AO2 tower round, 2026-09-26)
- `turn.sh status` (21:02) and a bare `turn.sh` (22:56) both mean "acquire as anon": the first took the lock for six
  seconds; the second queued behind gpu1, and stopping its task did NOT end it: it took the lock at 23:00:51 and
  was released by hand at 23:00:59. turn.sh has no status command.
- Rule: read the lock with `cat <lane dir>/.ns_turn/who`; call turn.sh only as `acquire <who>` / `release <who>`;
  after stopping any task that may wait on the lock, read the holder again a minute later.

### A test exe folder was overwritten with another build (AO2 tower round, 2026-09-26)
- ns_x1's files were copied into ns_m, overwriting the merged-main exe that was to be the refuter's baseline.
- Rule: one folder per exe; copy only into a new folder, and check the sha1 after.

### The refuter was compared with a pre-merge baseline (AO2 tower round, 2026-09-26)
- The WW_AO_WELD_ACROSS=0 bake was first hashed against reg_patch1 (5bebb273, before the merge) and differed; the
  merge had moved 7 instances. A bake with the overseer's main exe (bb1ae6af) matched byte for byte.
- Rule: a refuter's baseline is baked by the exe of the commit the branch now sits on, never an older bake.

### A two-pass relaxation was taken as settled (AO2 tower round, 2026-09-26)
- The first T-junction build did two Jacobi passes; a chain of three pieces left the tower step at 20 bytes, and the
  per-corner printout showed the corner off the edge's line. Settling in place to 0.1 byte took it to 1.3.
- Rule: an iterative fix stops on a measured change bound, not on a pass count.

### A flag-only rule was nearly shipped (AO2 decal round, 2026-09-27)
- What happened: the first fix skipped every material row with a blend or decal switch. It was built and baked
  (23,547 vertices moved) before its reach was measured. A census of each row's drawn opacity showed warehouse
  walls (decal + test, 100% opaque) and skybridges (blend + test, 54%) under the same switches.
- Rule: before a rule keyed on a material switch ships, measure what the switch catches across the region (the
  drawn surface each row covers), not only the one case in the picture.

### The stock caster was edited and broke the byte-exact refuter (AO2 decal round, 2026-09-27)
- What happened: the alpha test was added inline to the shared rayHit, which no alpha scene uses. With the rule
  off, 6 placement AO values still moved (max 7 bytes): the compiler folded the float maths differently.
- Rule: when the refuter must be byte for byte, leave the stock hot path alone; add the new test in a separate path.

### Two build errors from where code was placed (AO2 decal round, 2026-09-27)
- What happened: the alpha loader was first written above the texture type's declaration (not declared), then
  inside lodgen.cpp's unnamed namespace (internal linkage, undefined at link).
- Rule: in lodgen.cpp, check which namespace a spot is in before adding an exported function there.
