# Lane BTOFREE1 -- the FO4CS target writes no `.BTO` into the mod folder

## Header
- Tree: `E:/Projects/NifskopeWildWastelandEdition`, branch main. Exe at launch: read `release/NifSkope.exe`'s mtime and size
  yourself and write them in the report's first line (the queue runs NATIVEVIEW2 -> VT1 -> GENSMALL1 -> NATIVE1c -> BTOFREE1,
  one lane at a time, so the exe you find carries the lanes before you). Rung ONCE before your first build:
  `release/NifSkope.before_btofree1.exe` (copy of the exe on disk at that moment; never delete any `release/NifSkope.before_*.exe`).
  Markers `scratchpad/btofree1_20260916/BUILDING` (touch FIRST) / `DONE` (first word `btofree`). Report
  `scratchpad/btofree1_20260916/lane_btofree1_report.md`, incremental; `PENDING.md` past half context. Never commit, never `git stash`.
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
- Also read: `src/lodgenmanager.cpp` around the line that says the `.BTO` chunks are read back by the pair, the arrays, the
  cards, the merge and the far-ring cut (~2575 and the readers at ~2585 / ~3077 / ~3155 as DEFAULTS1 left them); the object
  pass in `src/lodgen.cpp` (where a chunk's `.BTO` is written and where the FO4CS target branches); `docs/LODGEN_TEXTURE_ARRAYS.md`,
  `docs/LODGEN_CARD_SHEETS.md`, `docs/LODGEN_MANIFEST_FORMAT.md` (what reads a `.BTO` back). Plan §5 row 7 and §6 (k).

## bungo's words, verbatim
- 2026-09-12 18:3x: "essentially, no legacy vanilla file types are now used by us or baked in the FO4CS lod bake" -- "Except
  the data we're reading from for the bakes".
- 2026-09-16 11:1x: "Also do the parked" (BTOFREE1 was the parked proposal; the director's earlier recommendation to wait for the
  FO4CS readers is overruled by this).

## The work
1. Under the FO4CS target the object pass builds every `.BTO` chunk in a SCRATCH folder (under the bake's own work dir, named in
   the census line), every read-back (arrays, cards, merge, far-ring cut, the pair) reads from there, and the scratch is removed
   after the pair and the sidecars are written. The mod folder receives only our types: `.lodl .lodt .lodo .lodi .lodm`, the
   arrays / heightmap DDS, the manifest sidecar (bungo's open call; keep it). Way back `--keep-bto` (panel row "Keep legacy
   .BTO chunks", OFF): the `.BTO`s land in the mod folder exactly as today, byte-identical.
2. The STOCK target does not change at all (byte-identical whole output on (-20,24) at dim 4/8/16/32; `lodgen_defaults.sh`).
3. The census line names where the chunks were built and that they were dropped, with the count and the bytes freed
   (written AND moving: `--keep-bto` reads dropped 0).
4. **Gates**: `lodgen_btofree.sh` new: (a) FO4CS default bake -> zero `*.BTO` under the mod folder (refuter: the rung leaves
   3060 on a Commonwealth run; on the region the count you measure), and the native pair + arrays + cards are byte-identical to
   the rung's (they must not notice); (b) `--keep-bto` == rung bytes; (c) stock target == rung bytes; `lodgen_native_baseline
   --check` 25 files 0 differ; `lod_generation.sh` self-test (the new row counted with a floor, `nifskope-ww-panel-style`);
   `lodgen_defaults.sh` 28/0.
5. **Docs**: plan §5 row 7 closed and §6 (k) marked ruled with the date; `nifskope-ww-lodgen` skill's target section.
6. **Report sections**: `## 0. Exe at launch`, `## 1. Where the chunks go now`, `## 2. Gates`, `## 3. Build and chain`,
   `## 4. Owed / red / bungo's calls`, `## 5. Mistakes`, `## 6. Skill review`.

## Documents in `scratchpad/btofree1_20260916/`
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

## ADDED 2026-09-16 13:5x by the director (bungo: "Do what you think is correct")

- **Own the native_open.sh object-coverage failure.** One check fails on every exe since before NATIVEVIEW2 (IoU 0.8179
  between the `.lodi` object coverage and the `.BTO`'s; identical on the rung exe). You are rebuilding the `.BTO`
  read-back path this check compares against, so you stand where the cause is. Either the failure vanishes with the
  scratch-folder `.BTO` (then say why, with the numbers) or it survives, and then you find the cause and fix it or, if
  it is a wrong check, rewrite the check with a measured floor and show it failing on broken input. Either way
  native_open.sh must end at 14 checks 0 failures, or the report names exactly what is still red and whose it is.

