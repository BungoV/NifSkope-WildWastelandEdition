# Lane VT1 -- the pyramid-assembled sheet agrees with the direct bake under the ruled land default

## Header
- Tree: `E:/Projects/NifskopeWildWastelandEdition`, branch main. Exe at launch: `release/NifSkope.exe` 2026-09-12 21:52:25,
  22,280,192 B (DEFAULTS1's). Rung ONCE before your first build: `release/NifSkope.before_vt1.exe` (copy of the exe on disk at
  that moment; never delete any `release/NifSkope.before_*.exe`). Markers `scratchpad/vt1_20260912/BUILDING` (touch FIRST) /
  `DONE` (first word `pyramid`). Report `scratchpad/vt1_20260912/lane_vt1_report.md`, incremental; `PENDING.md` past half
  context. Never commit, never `git stash`.
- Game: `tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?` is ITS OWN command, run and read before every build and every
  exe launch; Fallout4 up = stop, PENDING.md (CONSTITUTION 6). A NifSkope with no `--port` is bungo's own window: rename the exe
  aside as `NifSkope_inuse_<pid>.exe` at link time, never kill it; never rename or delete a `NifSkope_inuse_*.exe` you did not
  create. `-no-gui` bakes need no GUI slot; a GUI harness is `--port <unused>` + `WW_WINDOW_AT=1960,40`, one at a time. Every path
  in argv and every WW_* path ABSOLUTE `E:/...`. Never bungo's installed Data/Terrain, never the whole Commonwealth.
- LANE NATIVEVIEW2 RUNS IN THIS TREE AT THE SAME TIME (it edits the renderer, `res/shaders/`, `src/btdterrain.cpp`; you edit
  `src/lodgen.cpp` / `src/lodtfile.cpp` and `tests/spells/lodgen_terrain_vt.sh`; neither touches the other's files). Builds and
  GUI harness runs are SERIALISED through one mutex: `mkdir scratchpad/ww_build_mutex` (atomic; poll 30 s while it exists, never
  delete another lane's), write your lane name into `scratchpad/ww_build_mutex/owner`, build + run your harnesses, then
  `rm -r scratchpad/ww_build_mutex`. The rung is copied from the exe on disk at YOUR first build (it may carry the other lane's
  link; your gates are bake byte-identities, which the renderer does not reach).
- Read first: `CONSTITUTION.md`; HANDOFF.md top block down to `LANDED 21:58`; `scratchpad/defaults1_20260912/lane_defaults1_report.md`
  §5 (the V9a finding, verbatim numbers: with the ruled land default the pyramid-assembled sheet and the direct bake differ on
  2 of 4 chunks by 4 and 27 bytes of 174,888; `--land-warp 0` alone restores identity, so does `--land-guide off`; hex and mip
  bias are innocent) and its `v9a/` + `v9a_probe.sh`; `tests/spells/lodgen_terrain_vt.sh` (V9a-1 and the four OLD land switches
  it now spells; V9c, red on the rung with identical numbers); `docs/LODGEN_TERRAIN_VT.md` §2.5h/§2.5i/§7a (the land sampler:
  footprint / stochastic hex / warp lattice / guide rules, and how a pyramid tile is assembled from chunk bakes); the LAND1 and
  GROUND1 entries in `WW_CHANGES.md`; `src/lodgen.cpp` around `g_landHexSize`, `g_landWarpLattice`, `g_landWarpOctaves`,
  `g_landGuideStrength`, `g_landGuideSlopeRef` and the land sampling that reads them; the `.lodt` pyramid writer in
  `src/lodtfile.cpp`.
- Skills (repo `.claude/skills`): `nifskope-ww-lodgen` (whole; its top says the defaults moved on 2026-09-12 and how to spell
  the way back), `nifskope-ww-build-verify`, `ww-module-off-is-identical`, `ww-test-harness-add`, `ww-anchored-hookup`,
  `ww-spec-gate-audit`.

## bungo's words (2026-09-12 22:0x)
Offered "VT1, the pyramid disagreement: under your new land default the pyramid-assembled sheet and the direct bake differ by a
few bytes on 2 of 4 chunks. A lane finds why and makes them agree, then the parked check goes back on the new look." -- "Do both now".

## The work
1. **Find the cause, with the texels.** Decode the two sheets that differ (BC1 blocks; the 4-byte and 27-byte differences are
   which blocks, which texels, which channel) and say what the warp / guide sampler does differently when a texel is produced
   through the pyramid path versus the direct chunk bake: the lattice phase keyed to chunk-local instead of world coordinates,
   the guide's slope neighbourhood cut at a tile border, the octave seed, a float summation order -- name it from the code and
   prove it with a probe that reproduces the exact differing bytes.
2. **Make them agree byte for byte under the ruled default**, in the sampler, so that a texel's value depends on its WORLD
   position only. If the honest fix is impossible without changing what the direct bake writes at the default, STOP: report the
   choice (which side moves, what it costs in bytes and in look) as bungo's call and land nothing -- a default's output moving is
   his ruling, not a lane's.
3. **Re-register V9a-1 on the new look**: `lodgen_terrain_vt.sh` drops the four spelled OLD land switches and passes on the bare
   default. Keep a second arm that runs the old switches too (both must pass), so the old look stays gated. The refuter: the rung
   exe fails the new arm with exactly the 4- and 27-byte differences.
4. **V9c, the seam, red on the rung with identical numbers**: measure the cause and report it with the numbers; fix it ONLY if
   it is the same root cause as item 1. Otherwise it goes under Owed with the candidates named.
5. **Gates**: `lodgen_terrain_vt.sh` (both arms), `lodgen_defaults.sh` (28/0 must hold: the default bake of (-20,24) at dim 4
   stays byte-identical to the rung's UNLESS your fix moves it -- then item 2's STOP applies), `lodgen_terrain.sh` 26/0,
   `lodt_write.sh`, `lodgen_native_baseline --check` (25 files 0 differ). Every count next to the exe timestamp; skipped ones named.
6. **Build**: the chain from `nifskope-ww-build-verify` (game check as its own command, mutex, rename aside, `make -j2` gated on
   make's rc, exe newer than EVERY changed file, stale-object check for every header touched, `make -n` zero compile lines).
7. **Documents in `scratchpad/vt1_20260912/`**: `WW_CHANGES_ENTRY.md` (starts `## 2026-09-12 — <title>`), `HANDOFF_BLOCK.md`
   (10-20 lines, plain language, a blank line after its lead line), `MISTAKES_ENTRIES.md` (and root `MISTAKES.md` at the TOP the
   moment recognised), `CHANGED_FILES.txt` (A/M + path, CR/LF byte counts before and after), the report: `## 0. The cause`
   (the bytes, the texels, the code line), `## 1. The fix`, `## 2. Gates`, `## 3. Build and chain`, `## 4. Owed / red / bungo's
   calls` (V9c; ground cover's missing frozen baseline if you touch that harness -- do not refreeze it, that is bungo's word),
   `## 5. Mistakes`, `## 6. Skill review` (amend `nifskope-ww-lodgen`'s land-sampler lines if the run disproved them; list the
   file). Docs: `docs/LODGEN_TERRAIN_VT.md` gets the rule "a texel depends on its world position only" beside the sampler.

## Rules
Writer only: `src/lodgen.cpp`, `src/lodtfile.cpp`, the harness, the doc. No defaults change (the four ruled values and every
other default stay; `--road-detail` 1 in every bake). No renderer change. No pictures owed unless a look moves (then one pair,
rung vs new, difference counted). Plain language; numbers beside floors; no "final/true" claims; incremental writes; if context
runs short, PENDING.md with exact resume steps and a DONE-so-far list. MISTAKES at the top of the root file by you, the moment.
