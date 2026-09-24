# Lane GENSMALL1 -- the small generator items the FO4CS runtime will need (plan §5 rows 6, 8, 12, 13, 17)

## Header
- Tree: `E:/Projects/NifskopeWildWastelandEdition`, branch main. Exe at launch: read `release/NifSkope.exe`'s mtime and size
  yourself and write them in the report's first line (the queue runs NATIVEVIEW2 -> VT1 -> GENSMALL1 -> NATIVE1c -> BTOFREE1,
  one lane at a time, so the exe you find carries the lanes before you). Rung ONCE before your first build:
  `release/NifSkope.before_gensmall1.exe` (copy of the exe on disk at that moment; never delete any `release/NifSkope.before_*.exe`).
  Markers `scratchpad/gensmall1_20260916/BUILDING` (touch FIRST) / `DONE` (first word `small`). Report
  `scratchpad/gensmall1_20260916/lane_gensmall1_report.md`, incremental; `PENDING.md` past half context. Never commit, never `git stash`.
- Game: `tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?` is ITS OWN command, run and read before every build and every
  exe launch; Fallout4 up = stop, PENDING.md (CONSTITUTION 6). A NifSkope with no `--port` is bungo's own window: rename the exe
  aside as `NifSkope_inuse_<pid>.exe` at link time, never kill it; never rename or delete a `NifSkope_inuse_*.exe` you did not
  create. `-no-gui` bakes need no GUI slot; a GUI harness is `--port <unused>` + `WW_WINDOW_AT=1960,40`, one at a time. Every
  path in argv and every WW_* path ABSOLUTE `E:/...`. Never bungo's installed Data/Terrain, never the whole Commonwealth: the
  fixture region is Sanctuary / chunk (-20,24) as the earlier native lanes used it (`scratchpad/nativeview1_20260912/shots.sh`,
  `tests/spells/lodgen_native.sh`).
- You are the ONLY lane in the tree. No mutex needed; leave none behind.
- Read first: `CONSTITUTION.md`; `HANDOFF.md` top block (the 2026-09-16 lead line and the 2026-09-12 lines under it);
  `MISTAKES.md` (root, top entries) and `docs/MISTAKES.md`'s lodgen section; `docs/LODGEN_NATIVE_LODO_LODI.md` IN FULL
  (the `.lodo`/`.lodi` law; §11 deviations, §12 the free room); `docs/LODGEN_CENSUS.md` §6 (what the runtime needs from
  the file); `docs/FO4CS_IMPROVED_LOD_PLAN.md` §5 and §6 (what the generator owes, and bungo's rulings).
- Skills (repo `.claude/skills`): `nifskope-ww-lodgen` (whole), `nifskope-ww-build-verify`, `ww-module-off-is-identical`,
  `ww-test-harness-add`, `ww-anchored-hookup`, `ww-census-contract`, `ww-contract-provenance` (for every doc line you change),
  `ww-spec-gate-audit` (before reproducing any number a plan row states).

## bungo's words, verbatim (2026-09-16 11:1x)
"Also the small generator items" -- after the director listed them as: the decoder's cell rule, the loose-file whitelist that
cannot see `.pbrm`/`.lodm`, a panel and CLI row for the height sheet, the three census words the shadow rulings asked for, and
the asymmetric-drop proof. The `.lodo`/`.lodi` header fields (plan §5 rows 1, 2, 3, 5) are NOT yours: they ride with NATIVE1c,
which touches the writer's format anyway. The card layer (row 11) is its own lane later. Do not widen.

## The work
1. **The decoder's cell rule (plan §5 row 6).** `tests/spells/lodgen_native_decode.py` refuses the downtown-Boston pair at
   instance 3359 because a neighbour sits at 2.999985 cells from its chunk origin after quantisation and the decoder re-derives
   a different cell than the writer sorted on. Read the writer's cell rule in `src/lodofile.*` / `src/lodgen.cpp` (the sort law
   in NATIVE §2.1) and make the DECODER apply the writer's rule -- the same quantised position, the same floor -- so the pair
   decodes. The writer does not move (its bytes are pinned by `lodgen_native_baseline --check`). Gate: the Boston pair decodes,
   its occluder-box table reads back with the count the `native-occluders:` line printed; refuter: the old decoder on the same
   file refuses at 3359. If the honest answer is that the WRITER's rule is wrong (a neighbour really belongs to the other cell),
   STOP and report both readings with the numbers; the writer moving is bungo's call.
2. **The loose-file whitelist (row 8).** `--resource <dir>` goes through the vendored `BA2File` loose-file scan in
   `lib/libfo76utils/src/ba2file.cpp`; find the extension filter (grep the `.bgsm`/`.dds`/`.nif` list) and add `.pbrm` and
   `.lodm`. Gate: a scratch resource dir with one `.pbrm` and one `.lodm` beside a model -> the bake's material resolver finds
   them (the census line names the arm that served); refuter: the rung ignores both silently. Every other extension's behaviour
   byte-identical (a stock bake of (-20,24) `cmp`s against the rung).
3. **The height sheet row (row 12).** `--vt-height` exists (`src/nifcli.cpp:6983`, help at :6266) and the panel has
   `LodgenVtHeightCheck` (`src/lodgenmanager.cpp:1278`). The CLI TABLE in `docs/LODGEN_TERRAIN_VT.md` §5 does not list the flag
   although §2.2 names it: add the row with `ww-contract-provenance`. Verify the panel row and the CLI produce byte-identical
   output (`lod_generation.sh` panel-vs-CLI leg) and that the flag's default is OFF and pinned. No default moves.
4. **The three census words (row 17).** `docs/LODGEN_CENSUS.md` lacks: a shadow-caster count per SOURCE (bungo's 14:4x ruling
   of 2026-09-11), the shadow march's own cost and the far shadow map's own cost in milliseconds (his 15:0x ruling). Two of the
   three are RUNTIME words (FO4CS measures them); what the generator can promise is the bake-side denominator: per-source caster
   counts. Add to the census page (§1.2's six rules, `ww-census-contract`) the three rows with their read-from column, and add
   the per-source caster count to the bake's `native:` census line and to the `--native-mesh-report` sidecar -- written AND
   moving (a region with no trees reads 0 tree casters; refuter stated).
5. **The asymmetric-drop proof (row 13).** The claim: the 16-bit index cap silently drops 6.17 percent of chunk (-32,0) dim-32's
   placements on the STOCK path. Re-run it on the stock path (reproduce 6.17, `ww-spec-gate-audit` first: print the table, not
   the count) and measure the NATIVE path on the same chunk: the `.lodi` chunk table's instance count against the REFR walk. If
   native drops nothing, say so with the two numbers; if it drops, name what and why. No code change unless the native path
   drops -- then STOP and report, the fix is a format question.
6. **Gates**: `lodgen_native.sh`, `lodgen_native_baseline --check` (25 files 0 differ -- item 1 must not move the writer),
   `lodgen_native_decode.py` on every native fixture incl. Boston, `lodgen_defaults.sh` 28/0, `lod_generation.sh` (panel
   self-test count unchanged or higher), `lodgen_terrain_vt.sh` both arms, the new whitelist gate (`tests/spells/resource_ext.sh`).
7. **Report sections**: `## 0. Exe at launch`, `## 1. The cell rule` (the two readings, the numbers), `## 2. Whitelist`,
   `## 3. Height row`, `## 4. Census words` (the rows as added), `## 5. Asymmetric drop` (both tables), `## 6. Gates`,
   `## 7. Build and chain`, `## 8. Owed / red / bungo's calls`, `## 9. Mistakes`, `## 10. Skill review`.

## Documents in `scratchpad/gensmall1_20260916/`
`WW_CHANGES_ENTRY.md` (starts `## 2026-09-16 — <title>`), `HANDOFF_BLOCK.md` (10-20 lines, plain language, a blank line after
its lead line), `MISTAKES_ENTRIES.md` (and root `MISTAKES.md` at the TOP the moment recognised), `CHANGED_FILES.txt` (A/M +
path, CR/LF byte counts before and after: `b.count(b'\r')` in Python, never grep), the report with the numbered sections
below, the LAST always `## Skill review` (skills loaded, skills that should have existed, skills written -- write them the same
session at `<repo>/.claude/skills/<name>/SKILL.md` and list the file).

## Rules (all lanes)
No defaults change (bungo's four rulings of 2026-09-12 and every other default stay; `--road-detail` 1 in every bake). Every
behaviour a user can see has an exact way back pinned by a gate (`ww-module-off-is-identical`). Any field that ships is WRITTEN
and MOVES under a test (the three rules of 2026-09-04 21:33). Nothing stated as a cause without a measurement. Plain
language; numbers beside floors; no "final/true" claims; incremental report writes; if context runs short, PENDING.md with exact
resume steps and a DONE-so-far list. The build chain is `nifskope-ww-build-verify` (game check as its own command, rename aside,
`make -j2` gated on make's rc, exe newer than EVERY changed file via `git status --porcelain -- src res tools tests`, stale-object
check for every header touched, `make -n` zero compile lines); every harness count next to the exe timestamp; skipped harnesses
named with the reason. Tell the director in `HANDOFF_BLOCK.md` that bungo's open window needs a restart.
