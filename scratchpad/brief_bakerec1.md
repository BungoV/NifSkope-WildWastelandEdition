# Lane BAKEREC1 -- the bake record `FO4CSLOD/<ws>/<ws>.lodb`

## Header
- Tree: `E:/Projects/NifskopeWildWastelandEdition`, branch main. Exe at launch: read `release/NifSkope.exe`'s mtime and size
  yourself and write them in the report's first line (queue: ... -> BTOFREE1 -> LAYOUT1 -> BAKEREC1 (you) -> INCR1 -> PERF1
  -> AUDIT1, one lane at a time; LAYOUT1 has landed, so every FO4CS-target file already sits under `<out>/FO4CSLOD/<ws>/`).
  Rung ONCE before your first build: `release/NifSkope.before_bakerec1.exe` (never delete any `release/NifSkope.before_*.exe`).
  Markers `scratchpad/bakerec1_20260916/BUILDING` (touch FIRST) / `DONE` (first word `bakerec`). Report
  `scratchpad/bakerec1_20260916/lane_bakerec1_report.md`, INCREMENTAL; `PENDING.md` past half context. Never commit, never
  `git stash`.
- Game: `tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?` is ITS OWN command, run and read before every build and every
  exe launch; Fallout4 up = stop, PENDING.md (CONSTITUTION 6). A NifSkope with no `--port` is bungo's own window: rename the
  exe aside as `NifSkope_inuse_<pid>.exe` at link time, never kill it; never rename or delete a `NifSkope_inuse_*.exe` you did
  not create. `-no-gui` bakes need no GUI slot; a GUI harness is `--port <unused>` + `WW_WINDOW_AT=1960,40`, one at a time.
  Every path in argv and every WW_* path ABSOLUTE `E:/...`. Never bungo's installed Data/Terrain, never the whole Commonwealth:
  the fixture region is Sanctuary / chunk (-20,24) (`tests/spells/lodgen_native.sh`, `lodgen_defaults.sh`, `lodgen_layout.sh`).
- You are the ONLY lane in the tree. No mutex needed; leave none behind. ONE background waiter at a time.
- Read first: `CONSTITUTION.md`; `HANDOFF.md` top block (the 2026-09-16 LANDED lines incl. LAYOUT1's, the RULED 19:2x and
  19:3x lines = your ruling); `MISTAKES.md` (root, top entries) and `docs/MISTAKES.md`'s lodgen section;
  `docs/LODGEN_NATIVE_LODO_LODI.md` §8 and §8.1 IN FULL (the three corpus hashes and `loadOrderHash`: the law you extend, not
  replace); `docs/LODGEN_CENSUS.md` §6.1; `docs/LODGEN_LEDGER_FORMAT.md` (the existing ledger digests of assets: reuse, do not
  invent a second digest); `docs/FO4CS_IMPROVED_LOD_PLAN.md` §5.
- Skills (repo `.claude/skills`): `nifskope-ww-lodgen` (whole), `nifskope-ww-build-verify`, `ww-module-off-is-identical`,
  `ww-test-harness-add`, `ww-census-contract`, `ww-contract-provenance`, `ww-spec-gate-audit`.

## bungo's words, verbatim (2026-09-16)
- 19:1x: "Should we also save a file for each bake, or a list of plugins that were used for it?" -> "But also, all 3 sound
  good" (the three = bake record, incremental rebake, chunk-parallel passes).
- 19:1x: "So yeah, each bake needs to know plugins used or what's different, to even attempt a partial rebake"
- 19:1x: "lodb sounds good"  (LOD bake record)
- 19:3x: "The folder should be called FO4CSLOD maybe, so it'd be Data/FO4CSLOD"

## The file
`<out>/FO4CSLOD/<ws>/<ws>.lodb`, PLAIN TEXT (UTF-8, LF, one record per line, `key<TAB>fields`), one per worldspace, written
LAST by a bake (after every other file is closed and synced) so its presence means "this bake finished". A reader that finds
none does a full bake; a reader that finds one with an unknown line kind ignores that line (forward compatible), and an
unknown `version` refuses by name. Sections, in this order:
1. `lodb` version line: `lodb	1	<ws>	<utc ISO date>	<exe build stamp = NIFSKOPE_REVISION + WW_EDITION_VERSION,
   src/main.cpp:109-125>	<exe size>`.
2. `hash` lines: `loadOrderHash`, `pluginCorpusHash`, `objectCorpusHash`, `modelCorpusHash`, `cardCorpusHash` exactly as the
   `.lodo`/`.lodi` headers carry them (§8, §8.1), 16 hex digits each. The `.lodb` NEVER disagrees with the pair: gate (b).
3. `plugin` lines, ONE PER PLUGIN IN LOAD ORDER, the same list `EsmWorld::load` was given (`src/esmdata.cpp:873`
   `loadOrderHash()` walks it): index, lower-cased base file name, byte size, and a per-file FNV-1a 64 over the file's
   bytes (this is the new thing `loadOrderHash` does not have: an EDITED plugin of the same size is caught). Full path in
   a separate field (informational; never part of any hash: §8.1 law).
4. `resource` lines, one per mod folder / archive in the order the panel's OrderedPathList (`src/lodgenmanager.cpp:87`) or
   `--plugins-txt` / `--mo2` gave them: kind (folder / ba2 / bsa), path, and for an archive its size + mtime.
5. `switch` lines: every switch AS SPELLED for the bake, one per line, in the form `lodgen_defaults.sh` (a) already uses to
   spell a default bake (the way back to reproducing this bake exactly). Reuse that spelling code, do not write a second.
6. `chunk` lines, ONE PER CHUNK WRITTEN: chunk x, y, level, and a per-chunk INPUT hash = FNV-1a 64 over, in a fixed order:
   the hashes of every LAND record in the chunk's cells, every REFR/placement (form id, position, rotation, scale, base id,
   SCOL part) that reaches the chunk, every base's model + material + texture asset digests it uses (the ledger digests,
   `docs/LODGEN_LEDGER_FORMAT.md`), the land textures + their assets, and the switches that reach that pass. State the
   exact ordered byte recipe in `docs/LODGEN_BAKE_RECORD.md` (new doc) with the ww-spec-gate-audit skill, and prove it with
   the refuters in gate (d). INCR1 (next lane) skips a chunk whose input hash is unchanged: your recipe is its contract, so a
   change that MUST rebake a chunk must move the hash, and a change that must not (a plugin edit to a record no chunk reads,
   a file's mtime, a moved mod folder) must not.
7. `census` lines: every census line the bake printed, verbatim, prefixed `census<TAB>`.
8. `end` line: `end	<file count>	<total bytes>` over every file the bake wrote under `FO4CSLOD/<ws>/` (read back from disk,
   never from intent), so a truncated record is detectable.

## Landed since this brief was written (LAYOUT1, 2026-09-17 00:20; read its LANDED block in HANDOFF.md)
- Every FO4CS-target file already sits under `<out>/FO4CSLOD/<ws>/`; the root is composed by ONE function in
  `src/lodgenlayout.cpp`. Your `.lodb` path comes from that composer, never a second spelling; `tests/spells/lodgen_layout.sh`
  leg (e) greps for a second literal and its leg (a) lists every file the root may hold: add `.lodb` to its expected set.
- The bake ALREADY keeps a LEDGER of what it wrote (path + checksum; `docs/LODGEN_LEDGER_FORMAT.md`) that an existing
  `--incremental` path reads. LAYOUT1 fixed an entry with an empty checksum there. Your `.lodb` is the bake's receipt and
  the per-chunk INPUT hash table; the ledger is the OUTPUT digest table. Reuse the ledger's digest code for asset digests;
  do not write a second digest, and say in `docs/LODGEN_BAKE_RECORD.md` how the two relate (INCR1 reads both).
- Harness drivers: an MSYS2 bash DROPS the parent shell's variables (`.claude/skills/ww-panel-run-harness` section 9); run
  a panel harness from Git Bash with the variables set in that same shell.

## The work
1. Write it (a writer in its own `src/lodbfile.cpp|h`, called by the FO4CS-target job after the last file; the STOCK target
   writes none: byte-identical stock output, `lodgen_defaults.sh` (c)/(e)).
2. `lodgen --native-verify <lodo> <lodi> --native-verify-corpus` (`src/nifcli.cpp:2653`) reads the `.lodb` beside the pair
   when present and, on a `loadOrderHash` / `pluginCorpusHash` mismatch, NAMES THE PLUGIN: which index/file was added,
   removed, reordered, or edited (its per-file hash moved), instead of the bare hash pair. Without a `.lodb` the message is
   as today. New CLI verb `lodgen --bake-record <ws.lodb>` prints the record's summary (plugins, switches, chunk count) and
   diffs it against a given plugin list (`--plugins-txt` / `--mo2` / the positional list): the output INCR1 will act on.
3. The census gets one `bake-record:` line (register it in `docs/LODGEN_CENSUS.md` §6.1): path written, plugins, resources,
   chunks, bytes; read back from the file.
4. The panel: no new row; the record is always written by the FO4CS target (it is the bake's receipt, not a feature).
5. **Gates** (`tests/spells/lodgen_bakerec.sh` new): (a) a FO4CS bake on (-20,24) writes the record last, with every section,
   and `end` counts equal `find` over the folder; (b) the five hashes equal the pair's headers (decode with
   `lodgen_native_fields.py`); (c) plugin lines equal the list given, in order, sizes equal `stat`, per-file hashes equal an
   independent Python FNV-1a over the bytes; (d) REFUTERS for the chunk hash: (i) rebake with a plugin copy whose ONE placement
   in (-20,24) is moved by 1 unit (use `tools/esp_lib.py` or a byte patch of the REFR DATA) -> exactly the chunks that
   placement reaches change hash, every other chunk hash identical; (ii) the same plugin copy with a record no chunk reads
   edited (a GMST or a dialogue string) -> per-file hash moves, `pluginCorpusHash` per its own law, EVERY chunk hash
   identical; (iii) the mod folder renamed / a file touched -> record identical except the informational path field;
   (e) `--native-verify` with the plugin copy from (i) names that plugin and "edited"; with a plugin appended names it and
   "added"; (f) stock target byte-identical; (g) every existing harness the change reaches PASS (`lodgen_native.sh`,
   `lodgen_defaults.sh`, `lodgen_layout.sh`, `lodgen_btofree.sh`, panel self-test); say why the others are not run.
6. Docs: `docs/LODGEN_BAKE_RECORD.md` new (format, the chunk-hash recipe, the refusal words, provenance);
   `docs/LODGEN_NATIVE_LODO_LODI.md` §8.1 gains the pointer; `docs/FO4CS_IMPROVED_LOD_PLAN.md` §5 gains the row "the
   reader can name the plugin that went stale".
7. Pictures: the record opened in a text view (one screenshot), the `--native-verify` naming a plugin (terminal capture),
   `scratchpad/bakerec1_20260916/images/`.
8. Report + changelog text for the director to splice (WW_CHANGES entry + HANDOFF LANDED block; you edit neither).
   MISTAKES entries for your own mistakes at the top of `MISTAKES.md`. Then `DONE` with first word `bakerec` and the
   one-line verdict: every gate's count, and the exe's mtime/size.
