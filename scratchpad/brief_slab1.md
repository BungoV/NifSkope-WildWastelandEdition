# Lane SLAB1 -- the object term of `--terrain-object-ao` stops turning a deck into a solid block

## Header
- Tree: `E:/Projects/NifskopeWildWastelandEdition`, branch main, ONLY lane in the tree (no mutex; leave none). Exe at
  launch: `release/NifSkope.exe` 2026-09-18 05:18:11, 22,663,680 B, sha1 a6d213e23258... (read it yourself, write it in
  the report's first line). Rung ONCE before your first build: `release/NifSkope.before_slab1.exe` (never delete any
  `release/NifSkope.before_*.exe`, `release/NifSkope.archlock1_rung.exe`, `release/NifSkope.at_0117.exe` or a
  `NifSkope_inuse_*.exe`). Markers `scratchpad/slab1_20260918/BUILDING` (touch FIRST) / `DONE` (first word `slab`).
  Report `scratchpad/slab1_20260918/lane_slab1_report.md`, INCREMENTAL (a section per finished step; the director
  reads it while you work); `PENDING.md` past half your context. Never commit, never `git stash`, never edit
  `WW_CHANGES.md` or `HANDOFF.md` (you deliver their text in the report).
- Game: `tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?` is ITS OWN command, run and read before every build and
  every exe launch; Fallout4 up = stop, write PENDING.md (CONSTITUTION 6). A NifSkope with no `--port` is bungo's own
  window: rename the exe aside as `NifSkope_inuse_<pid>.exe` at link time, never kill it. A GUI/headless run is
  `--port <unused>` + `WW_WINDOW_AT=1960,40`, one at a time, second monitor only. Every path in argv and every WW_*
  path ABSOLUTE `E:/...`. Batch mode resolves a RELATIVE path against the exe's folder (nifcli.cpp:179).
- Build: MSYS2 UCRT64 shell, `export PATH=/ucrt64/bin:/e/Tools/GIT/mingw64/bin:$PATH` before
  `mingw32-make -f Makefile.Release -j8` (the link runs `git`). Gate = make's own exit code (`nifskope-ww-build-verify`).
  ONE background waiter at a time. Read the clock with `date` for every timestamp you write.
- Read first, in this order: `CONSTITUTION.md`; `HANDOFF.md` top block -- the DIRECTOR HOTFIX 7d, 7c, 7b, 7 and 6
  entries (2026-09-18) and the GROUND1 LANDED block further down (grep `GROUND1`); root `MISTAKES.md` top 12 entries
  (the units trap of 05:0x and the "file byte is not the pixel" trap of 05:1x are yours to not repeat);
  `WW_CHANGES.md` hotfix 7 entry and its three addenda (top of file); `src/lodgen.cpp` 8146-8420 IN FULL
  (`class LodgenObjectHeightField`: CELL 128, `NONE`/`SENTINEL_TEST`, `gather`, `topAt`, `dump`, and the 8-direction
  march at ~8355 that reads `topAt` with `dist` 128..2048 x1.5 and takes `maxSlope`), the three use sites ~8963
  (chunk bake gather), ~9730 (census print), ~11523 (pyramid gather, once for all levels), and the census clause
  ~11987-12005; `docs/LODGEN_TERRAIN_VT.md` and `docs/LODGEN_CENSUS.md` where they name `objAo*`.
- Skills (repo `.claude/skills`, all present): `nifskope-ww-lodgen`, `nifskope-ww-build-verify`,
  `nifskope-ww-render-shot`, `ww-module-off-is-identical` (the OFF arm), `ww-texel-picture` (the mask-B crop),
  `ww-census-contract` (any new census word), `ww-test-harness-add`, `ww-control-calibration` (the refuter side of
  every number). Write a skill for any repeatable procedure you invent, under
  `scratchpad/slab1_20260918/skills_proposed/<name>/SKILL.md`, WITH frontmatter; the director places it.

## bungo's words, verbatim
- 2026-09-18 05:0x, over the chunkD3 AO picture: "why is this area so dark on the AO?" (the ground under the overpass).
- 2026-09-18 05:2x, when the cause was named: "Schedule that as the next thing in the next session."
- Standing: `--road-detail 1` always; masters ship OFF (`--terrain-object-ao` ships OFF and stays OFF; the strength is
  his dial); no default moves; no format change; the FO4CS target (`--native`, `Data/FO4CSLOD/`) is the ruled pipeline.

## The defect, as measured by the director (hotfix 7c/7d, 2026-09-18)
`LodgenObjectHeightField` keeps ONE float per 128 x 128 world-unit square: the highest object Z over that square. The
march at ~8355 treats every square whose top is above the sample as an occluder rising from the ground, so an overpass
deck reads as a solid block from the ground up, and the land beneath it darkens to the floor (chunkD3 mask-B values
16..255, mean 134.1, black under the deck) while the pier and the rocks -- shaded by the .lodi/.BTO 8-ray triangle
cast -- read grey. The terrain term (the ESM heightfield) is right and is not touched.

## The work (in this order; each step lands in the report before the next starts)
1. **Measure first, on the exe at launch, before any source change.** Re-bake chunk 4.4.-12 with chunkD3's recipe
   into `scratchpad/slab1_20260918/before/`: `--terrain-region 4 -12 7 -9 --dim 4 --vt-finest 1 --vt-content 512
   --msn-cache "E:/Projects/Fallout 4 Mods/mods/Upscaled Terrain Normals/<the folder that holds *_msn.DDS>"
   --road-detail 1 --terrain-object-ao 0.5` plus whatever chunkD2/D3 carried (`scratchpad/viewfix_20260917/chunkD3/`
   and `chunkD2/`: read their logs and reproduce the switch set; state every switch in the report; if a switch cannot
   be recovered, say so and bake without it on BOTH sides). Verify the msn cache first: every `*_msn.DDS` there is
   16,777,364 B, count them, and quote the census `--msn-cache` hit line. Confirm the before bake's census matches
   chunkD3's (`objAoPlacements 8693 objAoMeshes 908 objAoTriangles 96009 objAoTexels 4162845`) or explain the delta.
   Then the NUMBER: a Python reader that decodes the finest mask sheet's B channel (reuse `tests/spells/lodgen_vt_check.py`
   -- its BC1 decoder and `Lodv` reader; one reader per format, never a second) and reports the mean over (a) a
   rectangle you define IN WORLD UNITS under the deck near 24900,-41300 (read the placement list for the overpass REFRs
   to bound it, and print the rectangle), (b) the whole chunk, (c) a control rectangle of open ground with no object
   over it. Write the script to `scratchpad/slab1_20260918/mask_b_mean.py` and its three numbers to the report.
2. **The slab lattice.** Store min Z beside max Z per square (a second float plane, or a pair; the `dump` path writes
   what it wrote before unless you deliberately extend it -- if you extend it, the dump is a debug file, say so). In
   the march, a square occludes the sample only where the sample's own height lies BELOW that square's slab, i.e. the
   blocked thing is an elevation band [minZ, maxZ] per direction, not everything above the horizon: a sample under a
   deck whose min Z is above the sample is under a ceiling, and a ceiling attenuates by its angular cover, it does not
   read as a wall. Design the ceiling term yourself and WRITE THE FORMULA in the report before the code: what a sample
   directly under an infinite slab reads (must not be 0 -- sky light still enters from the open sides), what a sample
   beside a solid wall reads (unchanged from today), and what a sample under a deck edge reads between them. A model
   whose triangles reach the ground (a pier, a wall, a rock) has min Z at ground and behaves exactly as today. Object
   term only. `strength` semantics unchanged. No new switch unless the ceiling needs a dial, in which case it is a
   sub-toggle inside `--terrain-object-ao`, documented, default = the new behaviour, and the report's divergence row
   says what the old block-everything behaviour is worth keeping for.
3. **Both gather sites** (~8963 chunk, ~11523 pyramid) carry the min plane; the pyramid's coarser levels inherit through
   the existing box filter, nothing new there. The census (~9730, ~11987) gains ONE word that proves the slab path ran:
   e.g. `objAoSlabSquares N` = squares whose min Z is more than one cell above the terrain under them (the count that
   is 0 on a chunk with no elevated deck and > 0 on 4.4.-12), through `ww-census-contract`; `docs/LODGEN_CENSUS.md`
   updated with provenance.
4. **OFF is identical** (`ww-module-off-is-identical`): a bake of the same region WITHOUT `--terrain-object-ao` on the
   rung and on your exe is byte-identical, every file under the output root (list them, count them, `cmp` each). A bake
   WITH it must differ ONLY in the mask sheets (and their CRCs) -- name every file that differs and why.
5. **The witness.** Re-bake `after/` with the identical command. `mask_b_mean.py` on both: (a) under the deck must
   RISE, (c) the open-ground control must NOT move (say by how much, 0 expected), (b) whole-chunk mean reported. The
   pier's own shadow column beside the deck (define a fourth small rectangle at the pier foot) must NOT brighten -- that
   is the refuter that the ceiling did not become "ignore objects". Then re-render the two crops with
   `scratchpad/viewfix_20260917/render_slots_e.sh` (args `close 24900 -41300 2600 8 450`, and the `full` framing the
   hotfix 7c pictures used -- read `images/*.log` there for its args) with `SHEETS=<your after bake's FO4CSLOD/Commonwealth>`,
   `WW_LODL_AO=1` and `WW_RENDER_FLAT=1`, into `scratchpad/slab1_20260918/images/`, before and after from the same
   framing, and one side-by-side per framing with the measured pixel-difference percentage in the caption
   (`ww-texel-picture` for the mask-B crop: a texel-level crop of the sheet under the deck, before/after, same texels).
   Quote the viewer's "WW_LODL_AO terrain from mask sheet B: ... values a..b, mean m" note line for both.
6. **Gate.** New `tests/spells/lodgen_slab.sh` (through `ww-test-harness-add`), floors on every check: (a) OFF identity
   of step 4 (must go RED when one byte of a mask sheet is flipped -- show it); (b) census word written and MOVES
   (0 on a flat chunk, > 0 on 4.4.-12); (c) `mask_b_mean.py` under-deck mean on the after bake above a bar you set
   from the measurement, open-ground control within 1 of the before, pier-foot within 2 of the before; (d) a synthetic
   control: build a `LodgenObjectHeightField` by hand in a small C++ or Python harness -- one flat slab 200 units up over
   open ground, and one wall from the ground -- and pin: under the slab centre > 0.3 (state your formula's value),
   beside the wall == today's value to 1e-6 (compute today's with the rung's formula), under the slab edge strictly
   between. That is the check that fails on the OLD code: run it against the old formula and show it red. Then the
   neighbours, one at a time, before/after on your final exe, in a table with WHOSE every red is: `lodgen_terrain_vt.sh`
   (45/0 expected), `lodgen_terrain.sh` (26/0), `lodgen_ground_cover.sh` (known 4-7 grass reds, not yours),
   `lodgen_native_baseline.sh --check` (25 files 0 differ), `lodgen_defaults.sh` (28/0), `lod_generation.sh`,
   `lodgen_identity.sh`. Any red you make is yours.
7. **Docs.** `docs/LODGEN_TERRAIN_VT.md` (the object-AO paragraph: slab, ceiling formula, the census word) and
   `docs/LODGEN_CENSUS.md`, each line with provenance (`ww-contract-provenance`); `lodgen --help` if a dial was added.

## Gates (pre-registered; a gate invented after the numbers is not a gate)
- G1 OFF identity: every output file byte-identical rung vs new exe without the switch, and the flipped-byte refuter red.
- G2 Under-deck mask-B mean RISES from the before bake; open-ground control moves 0; pier-foot rectangle does not
  brighten by more than 2. All three numbers quoted, rectangles printed in world units.
- G3 Synthetic slab/wall/edge harness: wall unchanged to 1e-6, slab centre > 0.3, edge between; red on the old formula.
- G4 Census word present, moves, and `docs/LODGEN_CENSUS.md` names it.
- G5 Neighbour counts unchanged except where you explain them; `lodgen_terrain_vt.sh` 45/0.
- G6 Pictures: before/after crops from the same framing, pixel-difference percentage in the caption, the texel crop.

## Rules (what the lane may not touch)
- No format change to any file (`.lodt`, `.lodl`, `.lodo`, `.lodi`, `.lodj`, `.lodb`); the mask sheet's meaning is the
  same, only its values under a ceiling move. No default moves. `--terrain-object-ao` stays OFF and its strength
  semantics stay. The terrain term, the roads, the erosion, the vertex-AO cast of hotfix 7 (`src/nativeemit.cpp`,
  `src/lodgenao.h`) are not touched. Object term only: `LodgenObjectHeightField`, its march, the two gathers, the census.
- Never present a file byte as a pixel: every picture claim is traced to the sheet texel or the shader uniform.
- Units: the field is in WORLD units; if you copy any constant from a caster, copy its units (MISTAKES 05:0x).

## Report (`scratchpad/slab1_20260918/lane_slab1_report.md`, incremental, sections in order)
0 exe at launch (mtime, size, sha1), rung name; 1 the before measurement (switch set, msn-cache count, census match,
the three rectangles and their means); 2 the ceiling formula, written BEFORE the code, with the three canonical values;
3 the code (functions touched, lines); 4 OFF identity table; 5 the witness (numbers before/after, the note lines, the
pictures with captions); 6 the gate (counts, each refuter shown red); 7 neighbours before/after with owners; 8 build
(mtime, size, rung, sha1; `find src tests res -newer release/NifSkope.exe`); 9 docs touched with provenance; 10 text
for the director to splice: a WW_CHANGES paragraph for a reader who never saw the code (numbers in it) and the HANDOFF
LANDED block; 11 divergence rows for bungo (any dial, anything you chose that he might rule on), written before the
thing stands; 12 MISTAKES entries (root MISTAKES.md: write them yourself at the top, the moment you recognise one);
13 the finished-work skill review (CONSTITUTION 1a: skills loaded, skills wished for, skills written).
END with `DONE` (first word `slab`) and, for bungo, five plain sentences: what moved, the under-deck number before and
after, what did not move, what is his to rule on, and that his open window needs a restart.
