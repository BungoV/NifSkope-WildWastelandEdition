# Lane LAYOUT1 -- every FO4CS-target output under ONE root: `<out>/FO4CSLOD/`

## Header
- Tree: `E:/Projects/NifskopeWildWastelandEdition`, branch main. Exe at launch: read `release/NifSkope.exe`'s mtime and size
  yourself and write them in the report's first line (the queue is NATIVEVIEW2 -> VT1 -> GENSMALL1 -> NATIVE1c -> BTOFREE1 ->
  LAYOUT1 (you) -> BAKEREC1 -> INCR1 -> PERF1 -> AUDIT1, one lane at a time; the exe you find carries the lanes before you).
  Rung ONCE before your first build: `release/NifSkope.before_layout1.exe` (never delete any `release/NifSkope.before_*.exe`).
  Markers `scratchpad/layout1_20260916/BUILDING` (touch FIRST) / `DONE` (first word `layout`). Report
  `scratchpad/layout1_20260916/lane_layout1_report.md`, INCREMENTAL; `PENDING.md` past half context. Never commit, never
  `git stash`.
- Game: `tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?` is ITS OWN command, run and read before every build and every
  exe launch; Fallout4 up = stop, PENDING.md (CONSTITUTION 6). A NifSkope with no `--port` is bungo's own window: rename the
  exe aside as `NifSkope_inuse_<pid>.exe` at link time, never kill it; never rename or delete a `NifSkope_inuse_*.exe` you did
  not create. `-no-gui` bakes need no GUI slot; a GUI harness is `--port <unused>` + `WW_WINDOW_AT=1960,40`, one at a time.
  Every path in argv and every WW_* path ABSOLUTE `E:/...`. Never bungo's installed Data/Terrain, never the whole Commonwealth:
  the fixture region is Sanctuary / chunk (-20,24) (`tests/spells/lodgen_native.sh`, `lodgen_defaults.sh`).
- You are the ONLY lane in the tree. No mutex needed; leave none behind. ONE background waiter at a time: stop a waiter
  before starting the next; never stack polling loops.
- Read first: `CONSTITUTION.md`; `HANDOFF.md` top block (the 2026-09-16 LANDED lines, the RULED 19:3x line = your ruling);
  `MISTAKES.md` (root, top entries) and `docs/MISTAKES.md`'s lodgen section; `docs/LODGEN_NATIVE_LODO_LODI.md` §3, §4 and
  §12; `docs/LODGEN_TERRAIN_VT.md` (the `--vt DIR` row and everything that spells `Terrain/`); `docs/LODGEN_CARD_SHEETS.md`;
  `docs/LODGEN_MANIFEST_FORMAT.md`; `docs/LODGEN_CENSUS.md` §6.1.
- Skills (repo `.claude/skills`): `nifskope-ww-lodgen` (whole), `nifskope-ww-build-verify`, `ww-module-off-is-identical`,
  `ww-test-harness-add`, `ww-census-contract`, `ww-contract-provenance` (for every doc line you change), `ww-spec-gate-audit`.

## bungo's words, verbatim (2026-09-16)
- 19:2x: "shouldn't all these files sit under a new single directory then?"
- 19:3x: "The folder should be called FO4CSLOD maybe, so it'd be Data/FO4CSLOD, sound fine?" -> director: fine, recorded.
- Standing (2026-09-12 18:3x): "essentially, no legacy vanilla file types are now used by us or baked in the FO4CS lod bake".

## The ruling, as paths (relative to the output folder the panel / `--out` names; that folder IS the mod's `Data`)
| today | after you |
|---|---|
| `Terrain/<ws>.lodl` | `FO4CSLOD/<ws>/<ws>.lodl` |
| `Terrain/<ws>.VT.<dim>.lodt` (every level) | `FO4CSLOD/<ws>/<ws>.VT.<dim>.lodt` |
| `Terrain/<ws>.lodo` + `Terrain/<ws>.lodi` | `FO4CSLOD/<ws>/<ws>.lodo` + `.lodi` |
| `Textures/Lodgen/Aggregate/<ws>/...` | `FO4CSLOD/<ws>/Aggregate/...` |
| cards: the panel's impostor folder, game path `Data\Textures\Lodgen\Cards\<id>_oct.*` | `FO4CSLOD/Cards/<id>_oct.*`, game path `Data\FO4CSLOD\Cards\<id>_oct.*` (cards are per tree, shared by every worldspace) |
| the manifest sidecar (bungo's open call: KEPT) | beside the files it describes under `FO4CSLOD/<ws>/` |
| (BAKEREC1, next lane) | `FO4CSLOD/<ws>/<ws>.lodb` -- you write NOTHING there, but nothing you do may make that spot awkward |

STAYS WHERE IT IS (do not touch): the STOCK target's `meshes/terrain/<ws>/` + `textures/terrain/<ws>/` (engine paths) and
`--keep-bto`'s `.BTO`s; the far HeightMap DDS `Textures/Terrain/<ws>/<ws>.HeightMap.*.dds` (a SHIPPED FO4CS reader composes
that path from loose files; moving it is a later FO4CS change, his call); every READ path (vanilla `Textures/Terrain`,
`Textures/LOD`, `meshes/LOD`, the MNAM files).

## Where the paths are spelled today (start here; find the rest with grep, and list every hit you changed in the report)
- `src/lodgenmanager.cpp:2829` (`.lodl` path), `:2981` (`nativeDir = outputDir() + "/Terrain"`), `:2996` (`cardDir =
  impostorEdit`: under the FO4CS target the card output defaults to `<out>/FO4CSLOD/Cards` when the field is empty; a
  filled field still wins, say so in the tooltip), `:2939-2940` (stock meshDir/texDir: unchanged).
- `src/lodgen.cpp:2906` and `:3038` (the GAME-RELATIVE card path strings written INTO the files), `:3284` (aggregate dir),
  `:11438` (vt dir); `src/lodtfile.cpp:1803`; `src/nativeemit.cpp:993-994` (pair paths from `s.outDir`); `src/nifcli.cpp:2955`,
  `:3237`, `:3247` (`--vt` output), `:4086` (the aggregate path printed).
- Gates that spell a path (re-base every one; counts are grep hits): `tests/spells/lodgen_byte_gate.sh` 6,
  `lodgen_card_arrays.sh` 3, `lodgen_terrain_vt.sh` 4, `lodgen_terrain.sh` 2, `lodgen_farring.sh` 1, `lodgen_roads.sh` 1,
  `lodgen_terrain_pbrm.sh` 1; plus any `.py` reader that composes one. `lodgen_native.sh` / `lodgen_native_baseline.sh` /
  `lodgen_defaults.sh` / `lodgen_btofree.sh` read the pair by path: check them too.
- Docs that spell a path: `docs/LODGEN_BTD_FORMAT.md`, `LODGEN_LEDGER_FORMAT.md`, `LODGEN_NATIVE_LODO_LODI.md`,
  `LODGEN_PLAN.md`, `LODGEN_TERRAIN_VT.md`, `LODGEN_CARD_SHEETS.md`, `LODGEN_CENSUS.md` (§6.1 census lines), and
  `docs/FO4CS_IMPROVED_LOD_PLAN.md` §5 (where the reader is told to look). Every changed line carries provenance
  (`ww-contract-provenance`).

## The work
1. Move every FO4CS-target writer to the table above. ONE function composes the root (`fo4csLodRoot( outDir, ws )` or the
   like) and every writer calls it; no second spelling anywhere (gate (e) below proves it with grep).
2. The game-relative strings written INTO files (card `texPath`, `.lodm` `source`/paths, manifest rows) change with the folder.
   Everything else in every file is byte-identical to the rung's output: prove it by diffing the rung's bake and yours with
   the path strings normalised (a Python diff that rewrites `Textures\Lodgen\Cards` -> `FO4CSLOD\Cards` in the OLD file and
   then requires byte equality; and a refuter: the same diff WITHOUT the rewrite must fail on exactly the path bytes).
3. The stock target: byte-identical whole output on (-20,24) at dim 4/8/16/32 against the rung (`lodgen_defaults.sh` (c) and
   (e) as they are). `--keep-bto` under FO4CS: the `.BTO`s still land in `meshes/terrain/<ws>/` exactly as BTOFREE1 left them.
4. The census names the root: the `native:` line (or a new `layout:` word, register it in `docs/LODGEN_CENSUS.md` §6.1 with
   the `ww-census-contract` skill) prints the absolute root every file went under, read back from the paths actually written,
   never from the setting. The panel's LOD Generation page shows the root under the output field (label only, no blurb).
5. Empty-folder hygiene: a FO4CS bake creates NO `Terrain/`, `Textures/Lodgen/` or `meshes/terrain/` folder it does not fill
   (BTOFREE1 already removes its scratch; you remove nothing that was there before the bake).
6. **Gates** (`tests/spells/lodgen_layout.sh` new, plus the re-based ones): (a) FO4CS default bake on (-20,24) -> every
   expected file present at its new path and NOTHING of ours outside `FO4CSLOD/` except `Textures/Terrain/<ws>/*.HeightMap.*`
   (refuter: the rung's exe fails this on every row); (b) the normalised byte-identity of step 2 with its refuter; (c) the
   stock target untouched (step 3); (d) `--keep-bto` placement; (e) `grep -rn "Textures/Lodgen\|\"/Terrain\"\|/Terrain/" src/`
   returns only READ paths and the HeightMap writer, listed by line in the gate so a new spelling fails it; (f) the census
   root word present and equal to the folder that exists; (g) every re-based harness PASS on the new exe, and the panel
   self-test. Record every count.
7. Pictures: one native-view screenshot of the (-20,24) pair opened from its NEW path (`lodgen_native.sh` route), one
   `.lodt` sheet render from its new path, one panel screenshot showing the root label; `scratchpad/layout1_20260916/images/`.
8. Report + changelog text for the director to splice (WW_CHANGES entry + HANDOFF LANDED block; you edit neither file).
   MISTAKES entries for your own mistakes at the top of `MISTAKES.md`. Then `DONE` with first word `layout` and the
   one-line verdict: every gate's count, and the exe's mtime/size.
