# Lane HORIZON4 -- the baked far shadows look terrible against a ray cast: find what it takes to make them match, prove it in the simulator FIRST, then write the bake repair

## Header
- Tree: `E:/Projects/NifskopeWildWastelandEdition`, branch main. Clock at launch: 2026-09-19 03:45 (`date` for every
  later timestamp). The tree holds lane HORIZON3's UNBUILT, uncommitted edits (lodofile/lodifile/esmdata/nativeemit/
  lodinative/nifcli .cpp + headers; `scratchpad/horizon3_20260919/PENDING.md`). HORIZON3 is parked at BUILD PENDING. You
  work ON TOP of those edits: never revert, reformat or "tidy" them; list every file you touch that HORIZON3 also touched.
- Fallout4 IS UP (pid 17248). NO build, NO exe run, no wait-loop on the game. Python is free to run. `tasklist | grep -i
  -E "Fallout4|NifSkope"; echo rc=$?` as its own command before anything exe-shaped. You end at `PENDING.md` headed
  `BUILD PENDING` with the exact build/gate commands; the director resumes you when the game is down.
- Markers `scratchpad/horizon4_20260919/BUILDING` (touch FIRST) / `DONE` (first word `horizon4`). Report
  `scratchpad/horizon4_20260919/lane_horizon4_report.md`, INCREMENTAL (section 0 inside your first ten tool calls).
  Never commit, never `git stash`, never edit `WW_CHANGES.md` or `HANDOFF.md`. Syntax gate while the game is up:
  `scratchpad/horizon3_20260919/syn.sh` style `g++ -fsyntax-only` on every TU you change.
- A script containing a backslash goes through the Write tool, never a heredoc (MISTAKES.md).
- Read first: `CONSTITUTION.md`; root `MISTAKES.md` top 20; `scratchpad/sunsim1_20260919/report.md` IN FULL (the
  simulator you reuse: `scene.py`, `shade.py`, `render.py`, `cams.py`, `run.py`, `control.py`); HORIZON2's report s1-s3
  (the third witness); HORIZON3's report s1 (edge/face tables, the 60 interior-shadow cases) and s2 (tier 2);
  `lodgenHorizonCastAt` and the terrain sheet marcher in src; `docs/LODGEN_NATIVE_LODO_LODI.md` s3.7, s4.10-4.12.
- Skills: `nifskope-ww-lodgen`, `nifskope-ww-render-shot`, `ww-control-calibration`, `ww-texel-picture`,
  `ww-contract-provenance`.

## bungo's words (2026-09-19 03:4x), on seeing SUNSIM1's left/right pairs
"As you can see, the end result is terrible." He judges by the perspective pictures: the right panel must look like
the left panel. That is the acceptance test. Numbers support the pictures, they do not replace them.

## What SUNSIM1 measured
Lit/shadow disagreement, baked vs ray cast: objects 50-58% at sun elevation 5-15 (coin flip), terrain 5-48% (street
view at 5 deg: the bake darkens the whole street, the ray cast shows lit strips between buildings). 65.0% of stored
object horizon bytes are zero; 1,320 of 2,449 placements all-zero; 60.7% of LOD vertices have downward normals and the
`nz <= -1e-3` rule in `lodgenHorizonCastAt` stores 0 in all 16 bins for them. ROT180 control moves terrain 31.76 ->
64.11% but objects only 28.76 -> 31.56%. The decode is sound (2.94 deg, r 0.946 where data exists).

## The work (each step lands in the report before the next)
1. **Ceiling experiments in Python, before any C++.** Replace the RIGHT panel's input with horizon data you compute
   yourself from the ray caster, in the file's own quantisation, one change at a time, and re-run the four cameras at
   120/5, 120/15, 120/30, 240/15. One table, disagreement split terrain/objects, a picture pair per row for the street
   and east cameras:
   - O1 objects: the stored bytes as they are (the baseline, must reproduce SUNSIM1's numbers);
   - O2 objects: no down-normal zero rule (every vertex marched; say what origin offset you used and why -- a
     double-sided card is seen from both sides, so a normal-side offset is wrong for half the viewers; try no offset
     with a self-hit epsilon, and a two-sided offset taking the lower skyline);
   - O3 = O2 + vertices inserted on edges over 512 u and 256 u (HORIZON3 tier 2, simulated);
   - O4 = O2 + per-pixel horizons on faces (the tier 3 ceiling: what a face sheet at 64 u texels would give);
   - O5 = O2 with 32 and 64 azimuth bins; and with the sun's bin interpolated between neighbours vs nearest vs max;
   - T1 terrain as stored; T2 the sheet at 64 u and 32 u texels; T3 32/64 bins; T4 bin value = skyline at the bin
     CENTRE vs the MAX over the bin's 22.5 deg (say which the bake does today and show the street case under each);
     T5 interpolation between bins at read time; T6 receiver height offset.
   State for every row what it costs in bytes on the 33,123-placement region (HORIZON3 s1's byte model).
2. **Name the causes, ranked by how much of the picture each one repairs**, each with its row. If the best
   reachable row still looks wrong in the street picture, say so plainly and say what representation would not
   (e.g. a far shadow map rendered at runtime from the LOD meshes instead of baked horizons) -- bungo would rather
   hear that now than after three more lanes.
3. **Write the C++ for every repair that needs no ruling** (a wrong rule is a repair: no knob, no toggle). Anything
   that changes file size or format meaningfully (bin count, texel size, tier 3) stays a knob at its present default
   and goes into ROWS FOR BUNGO with its picture. Syntax-gate each TU. Way-back flags unchanged.
4. **The gate**: `tests/spells/lodgen_sunsim.sh` -- runs the simulator against a fresh bake of chunk 4.4.-12 and
   fails over a disagreement ceiling per camera/sun that you set from step 1's chosen row + margin; the ROT180
   control must move OBJECTS by at least 20 points (today it moves 2.8: that is the red proof the gate bites).
   Written now, run after the build.
5. `PENDING.md` headed `BUILD PENDING`: the build order with HORIZON3 (one build carries both lanes; HORIZON3's gate
   runs first, then yours), the re-bake recipe, the gate, the pictures to make (the same 20 pairs as SUNSIM1, before
   and after, side by side).

## Rules
- Authored LOD models only, never decimate; inserting vertices is allowed. `--road-detail 1`. Masters ship OFF; an
  owed ruling never ships as a default. No "fixed/final/true": mechanism + refuter. Every number from a named log.
- The truth cast's limits (no trees, no neighbour chunks, 16 u footprint) stay stated beside every number.

## Report
0 state at launch; 1 the ceiling table + picture pairs; 2 causes ranked; 3 the C++ (files, lines, overlap with
HORIZON3); 4 the gate; 5 PENDING; 6 ROWS FOR BUNGO in plain words with pictures; 7 MISTAKES entries; 8 skill/doc text
for the director. End (at park time) with five plain sentences for bungo.
