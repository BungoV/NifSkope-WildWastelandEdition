# Lane PANEL1 -- every bake setting the CLI has becomes a row in the LOD Generation panel; the audit of what is missing

## Header
- Tree: `E:/Projects/NifskopeWildWastelandEdition`, branch main. Exe at launch: `release/NifSkope.exe` 2026-09-12 12:58:48,
  22,007,808 B, sha1 ba7585cba389c8b38f0e07c6c11063e0cec1124c. Rung ONCE before your first build: `release/NifSkope.before_panel1.exe`
  (never delete any `release/NifSkope.before_*.exe`). Markers `scratchpad/panel1_20260912/BUILDING` (touch FIRST) / `DONE`. Report
  `scratchpad/panel1_20260912/lane_panel1_report.md`, incremental; PENDING.md past half context. Never commit, never `git stash`.
- Game: `tasklist | grep -i Fallout4` before every build and every exe launch; up = stop, PENDING.md (CONSTITUTION 6). A NifSkope with
  no `--port` is bungo's own window: rename the exe aside as `NifSkope_inuse_<pid>.exe` at link time, never kill it.
- LANE SHOWCASE1 RUNS AT THE SAME TIME from its own copy of release/ (`scratchpad/showcase1_20260912/ns_run/`). It launches GUI
  instances with `--port`. ONE GUI HARNESS INSTANCE AT A TIME across both lanes: before every GUI launch of yours run
  `powershell -NoProfile -Command "Get-CimInstance Win32_Process -Filter \"Name='NifSkope.exe'\" | Select-Object ProcessId, CommandLine | Format-List"`
  and WAIT (poll 10 s) while a `--port` NifSkope that is not yours is up; never kill it. Its processes hold the COPY, not
  `release/NifSkope.exe`, so your link is not blocked by it.
- Read first: `CONSTITUTION.md`, HANDOFF.md top block (down to `BAKED 13:5x`), `src/lodgenmanager.cpp/.h` (the panel), `src/nifcli.cpp`
  (the `lodgen` option parser, ~line 6800-7000: every `--switch`), `src/lodgen.h` (`LodgenTerrainOptions`, `LodgenObjectOptions`,
  `LodgenCoverOptions`, `LodgenVtOptions`, `LodgenSimplifyOptions`), `docs/LODGEN_TERRAIN_VT.md` §2.5h/§2.5i/§7a, the LAND1 / GROUND1 /
  TERRAINFMT1 / ROADS3 blocks in WW_CHANGES.md (what each switch does and its default), `tests/spells/lod_generation.sh` and the
  `WW_LODGEN_TEST` self-test in `src/nifskope_ui.cpp`, `scratchpad/lodui1_20260911/BAKE_INSTRUCTION.md`.
- Skills (repo `.claude/skills`): `nifskope-ww-panel-style` (THE house rules; read whole), `nifskope-ww-build-verify`,
  `nifskope-ww-lodgen`, `ww-panel-run-harness`, `ww-module-off-is-identical`, `ww-test-harness-add`, `ww-spec-gate-audit`,
  `ww-anchored-hookup`. Palette = `wwskin.h` tokens only. No descriptions/blurbs in the panel: label + control, explanation in the tooltip.
  Uncertain design -> Blender's equivalent, state the divergence.

## bungo's words, verbatim (2026-09-12 15:4x)
"Erosion is a knob in the menu, corret?" -- it was not. Then: "What? They should all be configurable in the gen menu, anything else we're
missing in that menu?"

## The work
1. **The audit, first, as a table in the report (`## 0`)**: every `--switch` the `lodgen` sub-command parses in `src/nifcli.cpp`, one
   row each: switch | option field it sets | default | class | panel row (existing name, or NEW, or CLI-ONLY with the reason).
   Classes: BAKE SETTING (a user-facing choice about the output: terrain, roads, land detail, guide, erosion, object AO, sheets, cache,
   cover, VT, objects, cards, arrays/atlas, merge, far rings, water, incremental, identity, ...); PATH/SOURCE (out-dir, data-root,
   resources, card dir, msn cache -- already rows or need one); DIAGNOSTIC / CLI-ONLY (`--dump-*`, `--probe*`, `--verify*`, `--*-check`,
   `--selftest`, `--stress-*`, `--list-*`, `--native-fixture`, `--from-btd`, fixtures, the non-lodgen sub-commands). Every row's class is
   justified in one clause. bungo reads this table; it answers "anything else we're missing".
2. **Every BAKE SETTING and PATH/SOURCE switch without a row gets one**, in the house style: one setting per row, whole-word label,
   `wwMakeScrubField` numbers / `wwMatchFieldStyle` selectors / ticked boxes as Blender's, `wwGuardWheel`, tooltip naming what it does
   and its CLI switch, grouped under `wwHeading` sections that match the switch families (Terrain, Land detail, Roads, Erosion, Object
   occlusion, Sheets and cache, Objects, Impostor cards, Far rings, Water, Run). Folding `LodgenSection`s where a family is optional.
   Defaults = the CLI defaults exactly (`--road-detail` 1.0; erosion 0 off; land guide off; object AO off; sheet format legacy; identity
   ON as the code has it -- do NOT change any default, that is bungo's call). Values persist in QSettings like the existing rows. The run
   reads the row through the same `want*()`/option plumbing as the existing rows (tick AND visible), never the widget alone.
   Read the existing rows before adding: a setting that already has a row is not duplicated, and a row whose label is now wrong is fixed.
3. **The gate that matters**: the PANEL run and the CLI run of the one-chunk Sanctuary region (-20,24), dim 4, with the SAME settings, are
   BYTE-IDENTICAL for every file (BTR, BTO, manifest, sheets, pyramid, .lodm, ledger) -- twice: (a) every new row at its default, against
   `release/NifSkope.before_panel1.exe`'s CLI bake with no switches (nothing moves by default); (b) every new row set to a non-default
   value against the CLI bake with the matching switches on the NEW exe. Drive the panel run through the `WW_LODGEN_TEST` /
   `lod_generation.sh` harness route (`ww-panel-run-harness`), never by hand. A row whose non-default value produces a bake identical
   to the default is a row that reaches nothing -- that is the refuter for every row, and it is reported per row.
4. **Self-test floors**: `WW_LODGEN_TEST` counts numbers/headings/selectors with floors -- raise the floors to the new counts and add a
   per-row check that each new row exists, is in a scroll area, takes the wheel only when focused, and round-trips its value through
   QSettings. Run it against the old exe first to watch the new floors fail.
5. **Layout picture**: `SHOT=` (WW_LODGEN_SHOT) of the panel before (old exe) and after, plus one at the panel's minimum width; counts
   do not see a ragged column, look at the picture. Send nothing yourself; the director sends.
6. **Build**: the chain from `nifskope-ww-build-verify` (game check, rename aside, `make -j2` gated on make's own rc, exe newer than
   EVERY changed file via the `git status --porcelain -- src res tools tests` sweep, stale-object check for every header touched, then
   `MSYSTEM=UCRT64 ... make -n` must print ZERO compile lines). Harnesses that the change reaches: `lod_generation.sh`, `lodgen_terrain.sh`
   (the CLI defaults must not move: its byte-identity checks), `ui_align.sh` if the dock chrome moved; name the skipped ones with the
   reason. Read every count next to the exe timestamp. Counted relinks; at most the builds you need.
7. **Documents in `scratchpad/panel1_20260912/`**: `WW_CHANGES_ENTRY.md` (starts `## 2026-09-12 — <title>`, em dash), `HANDOFF_BLOCK.md`
   (10-20 lines, plain language), `MISTAKES_ENTRIES.md` (`## ` entries; also appended to root `MISTAKES.md` the moment recognised, newest
   first), `CHANGED_FILES.txt` (A/M + path, CR/LF byte counts before and after for each), the report with `## 0. Audit table`,
   `## 1. Rows added` (row | section | switch | default | gate a | gate b), `## 2. Build and chain`, `## 3. Pictures`, `## 4. Owed / red /
   bungo's calls`, `## 5. Mistakes`, `## 6. Skill review` (amend `nifskope-ww-panel-style` if the run disproved it; list the file).

## Rules
Stay out of `src/lodgen.cpp` behaviour: this lane adds plumbing from rows to option fields and nothing else; if a switch cannot reach the
panel run without a generator change, it goes under Owed with the reason. No new defaults. No tone or renderer changes. Plain language;
numbers beside floors; no "final/true" claims; incremental writes; if context runs short, PENDING.md with exact resume steps and a
DONE-so-far list.
