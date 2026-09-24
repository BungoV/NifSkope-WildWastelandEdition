# Lane HORIZONOUT -- the baked-horizon far-shadow route is removed; identity is the route; keep only the scrappable bit

## Header
- Tree: `E:/Projects/NifskopeWildWastelandEdition`, branch main, ONLY build lane in the tree. Clock at launch 2026-09-19
  05:28 (`date` for every later timestamp). Exe at launch: `release/NifSkope.exe` expected 2026-09-18 23:47:33,
  22,949,376 B, sha1 b349f807426be700ed2ff9b54ee23e4fab3ba037 (HORIZON2's final; re-read, print, first line of the
  report). Rung ONCE before your first build: `release/NifSkope.before_horizonout.exe` (never delete any
  `release/NifSkope.before_*.exe`, `NifSkope.archlock1_rung.exe`, `NifSkope.at_0117.exe`, `NifSkope_inuse_*.exe`).
  Markers `scratchpad/horizonout_20260919/BUILDING` (touch FIRST, remove at DONE) / `DONE` (first word `horizonout`).
  Report `scratchpad/horizonout_20260919/lane_horizonout_report.md`, INCREMENTAL (section 0 inside ten tool calls);
  `PENDING.md` past half your context. Never commit, never `git stash`, never edit `WW_CHANGES.md` or `HANDOFF.md`.
- Game: `tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?` is ITS OWN command before every build and every exe
  run. Fallout4 up = no build, no exe run: do the rest, PENDING.md headed `BUILD PENDING`, stop; never wait-loop. A
  NifSkope with no `--port` is bungo's window: rename the exe aside as `NifSkope_inuse_<pid>.exe` at link time, never
  kill. A wedged harness NifSkope is ended by `NifSkope::open <scene>` as UTF-16LE UDP to its port. Headless runs:
  `--port <unused>` + `WW_WINDOW_AT=1960,40`, one at a time, absolute `E:/...` paths, scene file positional.
- Build: MSYS2 UCRT64, `export PATH=/c/msys64/ucrt64/bin:/e/Tools/GIT/mingw64/bin:$PATH`, `mingw32-make -f
  Makefile.Release -j8`, make's exit code is the gate; skill `nifskope-ww-build-verify` incl. object-vs-header check.
  ONE background waiter at a time. A script containing a backslash goes through the Write tool, never a heredoc.
- Read first: `CONSTITUTION.md`; `HANDOFF.md` top block -- the lines of 2026-09-19 (SUNSIM1, HORIZON4, the two RULED
  lines "we revert back to identity" / "horizon goes bye bye", LODLEVELS, IDENTPROX); root `MISTAKES.md` top 20;
  `scratchpad/horizon3_20260919/PENDING.md` + report s2, s4 (what HORIZON3 left UNBUILT in the tree: tier 2
  `--horizon-subdivide` + `.lodo` v5, tier 3 option parse, the scrappable bit `.lodi` v9 flag 0x0040, census
  `scrappablePlacements`, viewer channel `scrappable`); HORIZON1 + HORIZON2 reports (what they added: `.lodi` v8
  per-vertex horizon stream, role-7 terrain horizon sheet, `--horizon-refute`, `WW_HORIZON_REFUTE_DUMP`, viewer
  `horizon` / `horizonbin` channels + `WW_SUN`, gates `lodgen_horizon.sh`, docs s3.7 / s4.10-4.12, census words).
- Skills: `nifskope-ww-lodgen`, `nifskope-ww-render-shot`, `nifskope-ww-build-verify`, `ww-test-harness-add`,
  `ww-contract-provenance`.

## bungo's words (2026-09-19)
"As you can see, the end result is terrible." -- "So, for now, we revert back to identity data per LOD object from the
preauthored LODs" -- "We're not doing the horizon thing" -- "So yeah, horizon goes bye bye now, we're back to
identity". Measured reason: baked object horizons disagreed with a ray-cast sun on 50-58% of object pixels at low sun
(SUNSIM1); the identity far shadow map simulated at 64 u disagreed on ~9% (HORIZON4).

## The work (each step lands in the report before the next)
1. **Inventory first** (no edits): `git status`, `git log -5`, and a table of every horizon artefact in src/, tests/,
   docs/, res/, the two skill trees (`.claude/skills` here and `E:/Projects/Claude/.claude/skills`): file, lines,
   which lane added it, committed or uncommitted. Separate HORIZON3's uncommitted hunks into KEEP (scrappable bit:
   the ESM rule, the flag bit, census word, viewer channel, its docs) and DROP (tier 2, tier 3, `.lodo` v5).
2. **Remove the bake side**: a bake writes NO horizon data -- no per-vertex object stream, no role-7 terrain sheet,
   no horizon census words; switches `--horizon-*`, `--no-terrain-horizon`, `--horizon-refute` are gone (an unknown
   switch must fail by name as any other does). The marcher / `lodgenHorizonCastAt` / lattice code that nothing else
   uses is deleted; anything the sky/AO casts share STAYS and you prove the sky + AO streams are byte-identical
   before/after on chunk 4.4.-12 (bake with the rung exe vs your exe; list the only differing bytes).
3. **Format**: decide and document the `.lodi` version a default bake now writes: v7 layout + the scrappable flag
   bit (say the version number and why; the docs' version table is the authority). READERS stay tolerant: a v8 file
   with a horizon stream met in the wild still opens (stream skipped by length, named in the note line), never a
   crash; `--lodi-v6` / `--lodi-v7` ways back unchanged. `.lodo` stays v4.
4. **Viewer**: channels `horizon`, `horizonbin` and `WW_SUN` removed from the channel table, menus and skills;
   `identity`, `placement`, `sky`, `scrappable` stay. `scrappable` = magenta yes / grey no + note line.
5. **Gates**: `tests/spells/lodgen_horizon.sh` and its refuters retired (deleted, with the row removed from any board
   script); new small gate `tests/spells/lodgen_scrappable.sh`: rule count on the region matches the Python count,
   five named examples each way, a red control (flip the bit in a copy -> the census word moves), a v8-with-horizon
   fixture still opens. Neighbours before/after on your final exe at their standing counts: `lodi_v7.sh` 12/0,
   `lodl_channels.sh`, `lodgen_slab.sh` 16/0, `native_open.sh` 17/0/2, `render_shot.sh` 82/0 (re-base ONLY the rows
   that were horizon rows, list them), `lodl_open.sh` 23/0, `lodgen_native.sh`, `lodgen_defaults`.
6. **Pictures** `scratchpad/horizonout_20260919/images/`: chunk 4.4.-12 `identity` and `scrappable` channels, close +
   full framing, from the real viewer.
7. **Docs + skills**: horizon sections removed from `docs/LODGEN_NATIVE_LODO_LODI.md`, `LODGEN_TERRAIN_VT.md`,
   `LODGEN_CENSUS.md`, replaced by ONE short "history" paragraph (what was tried, the two numbers, why dropped, which
   rung exe still bakes it); `docs/FO4CS_IMPROVED_LOD_PLAN.md` far-shadow section rewritten to the identity route:
   the far shadow map keys on the `.lodi` GROUP id, self-shadow excluded by identity, trees cast from their authored
   3D LOD mesh with alpha test (cards never cast), the scrappable bit drops workshop-owned casters; skill text for
   `nifskope-ww-lodgen` + `nifskope-ww-render-shot` delivered AND written to both skill trees (hash both).
8. A MISTAKES.md entry: three lanes were spent on a baked route before anyone rendered it against a ray cast in a
   perspective view -- the rule: a lighting representation gets its perspective truth-vs-data picture in its FIRST lane.

## Rules
- Do NOT change the group/identity rule in this lane (a proximity-join ruling is pending with bungo).
- Authored LOD models only, `--road-detail 1`, masters ship OFF. No "fixed/final/true": mechanism + refuter. Every
  number from a named log.

## Report
0 exe at launch + rung; 1 inventory table; 2 bake removal + the byte-identity proof; 3 format/version + tolerant
reader proof; 4 viewer; 5 gates + neighbours; 6 build (mtime, size, sha1, `find src tests res -newer` empty,
object-vs-header); 7 pictures; 8 docs + skills (both trees, hashes); 9 WW_CHANGES paragraph + HANDOFF LANDED block for
the director to splice; 10 MISTAKES; 11 finished-work skill review. END with `DONE` and five plain sentences for
bungo, ending with: his open window needs a restart.
