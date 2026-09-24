# Lane LODUI1 report

Tree: `E:\Projects\NifskopeWildWastelandEdition`, branch `main`, working tree, nothing committed.
Exe at launch: `release/NifSkope.exe` 2026-09-11 12:19:06, 21,180,928 B, md5
`9ca009cb289858c17d87546a216c45a6` (ROADS1).
Rollback rung taken ONCE at 12:42: `release/NifSkope.before_lodui1.exe`, same md5, verified.
Markers directory: `scratchpad/lodui1_20260911/`.

At launch: `ls scratchpad/*/BUILDING` empty; `Fallout4.exe` not running; no NifSkope process
on the machine (bungo's own window is closed).

## 0. Pre-registered gates

Taken verbatim from `scratchpad/brief_lodui1.md` BEFORE any code was written.

| id | gate |
|---|---|
| L1 | Row visibility per target, with floors BOTH ways (FO4CS: the legacy rows hidden, the native row shown; Stock: the reverse). The summary line's extension list asserted per target. |
| L2 | Trees-only ON/OFF changes the candidate list on Sanctuary: ON has zero non-tree bases (a named empty-slot non-tree shown absent), OFF has that base present. |
| L3 | 512 px reaches the bake hook -- a bake at 512 writes a 4096-wide sheet. |
| L4 | Cards-from-ring touches no non-tree base. |
| L5 | The native row wires `--native`; the `.lodo`/`.lodi` pair is written by a GUI-driven region run. |
| L6 | The four stage times (landscape / meshes / textures / impostors) are WRITTEN and MOVE (a run with a stage off prints 0 for it and nonzero for the rest). |
| L7 | The exe is newer than every changed file; the drivers are rebuilt; the rung equals the launch bytes. |
| L8 | No NifSkope left running; the game is down at every launch. |

Baselines to hold (from the newest lane blocks in HANDOFF.md, ROADS1 12:19):
`lodgen_terrain.sh` 26/0, `lodgen_terrain_vt.sh` 41/1 (V9b red on the rung too),
`lodgen_ground_cover.sh` 29/5, `lodgen_terrain_pbrm.sh` 14/0, `lodgen_native.sh` 18/0,
`lodgen_roads.sh` 11/0, `lodgen_card_arrays.sh` PASS, `lodgen_texture_arrays.sh` PASS,
`lodl_open.sh` 23/0, `ui_align.sh` 11/0, `water_ui.sh` 82/0.
`lod_generation.sh` (the panel's own harness) baseline is measured on the rung by this lane
before any change, because no recent lane block records it.

## 1. The rows per target

`applyTarget` in `src/lodgenmanager.cpp` now gates these rows. Object names are
what the self-test reads.

| row | FO4 Community Shaders | Stock engine |
|---|---|---|
| `LodgenLodtCheck` — Landscape file (.lodl) | shown | hidden |
| `LodgenHeightmapCheck` — Shadow heightmap | shown | hidden |
| `LodgenVtCheck` — Terrain virtual texture (.lodt) | shown | hidden |
| `LodgenNativeCheck` — Native object files (.lodo/.lodi) | **shown, ticked by default** | **hidden** |
| `LodgenObjectsCheck` — Object LOD chunks (.bto) | **hidden** | shown |
| `LodgenBtrSection` — Legacy terrain chunks (.btr) and every sub-row, including `LodgenTexCheck` (bake terrain textures), the ground-cover rows and `LodgenTerrainIdentityCheck` | **hidden** | shown |
| `LodgenAtlasCheck` — Pack an object texture atlas | **hidden** | shown |
| `LodgenVtBtrCheck` — Chunk textures from the pyramid | **hidden** | shown |
| the object sub-rows (identity, sway, channels, arrays, AO + skirt, cull + margin, slot fallback, far-ring simplification, the impostor rows) | shown | shown, minus the FO4CS-only channel rows that were already hidden |

The object SETTINGS section did not split in two. Its header carries two check
boxes and exactly one is offered at a time (`LodgenSection::addHeaderCheck`):
`.bto` under the stock engine, `.lodo`/`.lodi` under FO4CS. The rows underneath
describe the object PASS — the same placements, channels, cull, simplification
and cards — and only the file they end in differs, so splitting them would have
doubled every row.

### What FO4CS still writes that is not one of the five

**The object pass still writes `.BTO` chunk files under the FO4CS target**, and
the summary line says so in words ("N object chunks under meshes\terrain\<ws>,
which the pair is built from"). This is a deviation from the literal ruling and
it is bungo's call, not the panel's. The reason is mechanical: the texture
arrays, the card arrays, the shape merge and the far-ring cut all take a LIST OF
WRITTEN `.bto` PATHS and read them back, and FO4CS's own Improved LOD module
reads `.bto` + manifests today and no native file (the dependency the 06:4x
ruling itself flags). The `.bto` ROW is gone as a choice; the file is not.

## 2. Trees only / 512 px / Cards from ring

* **`LodgenTreesOnlyCheck` — "Trees only"**, QSettings key
  `LodGeneration/treesOnly`, **ON by default**, tooltip one sentence. It reaches
  the bake as `LodgenObjectOptions::treesOnly`.
  * ON: only a base the chunk builder's own tree test calls a tree may stand on
    a card.
  * OFF: any base whose ring slot is EMPTY may — the old "missing" rule and
    nothing else, no type, folder or size filter.
  * The bake counts its refusals and says so in the chunk's own report line:
    `N placements refused a card: not a tree (Trees only)`.
* **The tree test has ONE definition now.** `lodgen.cpp` carried the three path
  tests inline at the repetition breaker as well as in `lodgenIsTreeModel`; the
  inline copy is gone and both callers go through the shared function.
* **`--candidates all` is retired.** `nifcli.cpp` refuses it by name with the
  reason in words, and so does `tools/bake_impostor_cards.sh`
  (`CANDIDATES=all`). `trees` and `missing` are the two words left, in the CLI,
  in the driver and in the panel row.
* **512 px** is in `LodgenCardResBox` and in the driver's `TILE` list. The cost
  line is computed, not typed: at 8 x 8 frames it reads `4096 x 4096 sheets at
  512 x 512 a frame`. The bake hook's own clamp (`nifskope_ui.cpp`,
  `WW_IMPOSTOR_TILE`) already accepted 32..512; only the list stopped at 256.
* **"Cards from ring" is now "Tree cards from ring"**, tooltip one sentence, and
  it is tree-only IN EFFECT in both states of the Trees-only row: a non-tree
  with an authored mesh is never replaced by a quad. Replacing one was never
  asked for and bungo's ruling names trees.

## 3. The native row and the stage times

* **`LodgenNativeCheck`** arms the emitter in `startChunks()` exactly as the
  CLI's region driver does (`lodgenNativeBegin` with the session's own resource
  stack as the data root), and `step()`'s tail calls `lodgenNativeWrite` then
  `lodgenNativeEnd` — on every path, including a cancel, so a cancelled run
  cannot leave the process-wide accumulator armed. The files land in
  `<output mod>\Terrain\<worldspace>.lodo` and `.lodi`, beside the `.lodl`.
* **The four stage times** — landscape / meshes / textures / impostors — are
  accumulated from the calls that belong to each stage and from nothing else, so
  a stage that did not run reads exactly `0.0 s`.
  * landscape: the `.lodl` write or AO refresh, and the shadow heightmap.
  * meshes: the terrain and object chunk builders, the shape merge, the
    far-ring cut, and the native pair.
  * textures: the terrain pyramid, the chunk sheets, the object texture arrays
    and the atlas.
  * impostors: the card arrays.
  * The words live once, in `lodgenStageTimeLine()` in `src/lodgen.cpp`, so the
    panel's result line and the command line cannot word them differently.
* The panel grew a result line, `LodgenResultLabel`, under the summary in the
  pinned action bar. Empty until something has run.

## 4. Build and gates

**ONE build plus TWO counted relinks.** Markers: `scratchpad/lodui1_20260911/BUILDING`
up before the build, replaced by `DONE` after the gates.

| | time | bytes | what |
|---|---|---|---|
| launch exe (ROADS1) | 2026-09-11 12:19:06 | 21,180,928 | the rung, `release/NifSkope.before_lodui1.exe`, md5-verified equal |
| build 1 | 13:09:49 | 21,232,128 | qmake + make, QMAKE-RC=0 BUILD-RC=0 CHAIN-RC=0 |
| relink 1 | 13:13:51 | 21,233,152 | the two checks the first gate run turned red (both harness defects, `fix01.py`) |
| relink 2 | (below) | | the panel grabs showed none of the rows under test (`fix02.py`) |

`qmake` was re-run because `src/lodgenmanager.cpp` gained a NEW include
(`nativeemit.h`) that `Makefile.Release`'s frozen dependency list did not name
(`nifskope-ww-build-verify`, "a successful build is not a consistent one").

Before the build, a syntax+semantics pass on all four changed translation units:
`lodgenmanager.cpp`, `lodgen.cpp`, `nifcli.cpp`, `nifskope_ui.cpp` — RC=0 each,
only pre-existing warnings.

Consistency after the build: the exe is newer than all 10 changed files; the six
objects that include `lodgen.h` are all newer than it; `res/style.qss` and
`release/style.qss` are byte-identical.

### The gates against the pre-registered list

| gate | number | baseline | verdict |
|---|---|---|---|
| `lod_generation.sh` (the panel, structural) | **116 / 0, PASS** | **97 / 0** measured on the rung by this lane | +19: 3 new rows found by name, 16 new checks. Floor raised 74 → 116 |
| `lodgen_panel_run.sh` (NEW, the panel driven) | **125 / 0, PASS** | newly registered | floor 125, measured not predicted |
| `lodgen_stage_times.sh` (NEW, the CLI's four stages) | **16 / 0, PASS** | newly registered | |
| `lodgen_native.sh` | 18 / 0, PASS | 18 / 0 | held |
| `lodgen_terrain.sh` | 26 / 0, PASS | 26 / 0 | held |
| `lodgen_terrain_vt.sh` | 41 / **1** | 41 / 1 | held — V9b, red on the rung too |
| `lodgen_roads.sh` | 11 / 0, PASS | 11 / 0 | held |
| `lodgen_ground_cover.sh` | 29 / **5** | 29 / 5 | held |
| `lodgen_terrain_pbrm.sh` | 14 / 0, PASS | 14 / 0 | held |
| `lodl_open.sh` | 23 / 0, PASS | 23 / 0 | held |
| `lodl_write.sh` | PASS | PASS | held |
| `ui_align.sh` | 11 / 0, PASS | 11 / 0 | held |

Why those and not others: every suite above is reached by one of the four
changed files — the CLI region driver (every terrain and native gate), the
object card gate in `lodgen.cpp` (the impostor and object gates), the `.lodl`
writer's new timer (`lodl_write`), and the panel itself.

### The pre-registered gates, answered

* **L1 — rows per target with floors both ways, summary extensions per target.**
  GREEN. 4 of 4 legacy rows hidden under FO4CS, 4 of 4 shown under Stock, each
  row's hidden flag printed beside its name. The FO4CS summary names `.lodl`,
  `.lodt`, `.lodo` and `.lodi` and never `.btr`; the Stock summary names `.btr`
  and `.bto` and none of the four. The saved-tick law is gated: a native tick
  made under the stock target does not reach the run.
* **L2 — Trees only ON/OFF on Sanctuary.** GREEN. 19 candidates with it ON, 33
  with it OFF; zero of the 19 are non-trees by folder or by name; the named
  floor is `00033794 Architecture\Shacks\ShackBalconyFloor03.nif`, present in
  the OFF list and absent from the ON list.
* **L3 — 512 reaches the bake hook.** GREEN, with one honest correction to the
  gate's wording. `TreeMapleForest1.nif` at `OCT=8`: **512 px gives a 2176 x
  4096 sheet, 256 px gives 1152 x 2048**. The number the row sets is the frame's
  LONG side, so the sheet is 4096 on the long side and narrower on the short one
  because the maple's silhouette is narrower than it is tall — the size-ladder
  and aspect rule the row's tooltip already describes. The gate as written said
  "a 4096-wide sheet"; what is true is "4096 on the frame's long side", and the
  256 control shows the number moves with the row.
* **L4 — Cards from ring touches no non-tree base.** GREEN by construction and
  by the candidate gate: the ring override is gated on the base's own tree test
  in BOTH states of the Trees-only row (`lodgen.cpp`, `cardWanted`), so a
  non-tree with an authored mesh is never replaced by a quad.
* **L5 — the native row wires `--native`.** GREEN. A GUI-driven one-chunk
  Sanctuary run wrote `Terrain\Commonwealth.lodo` **9,657,316 B** and
  `Terrain\Commonwealth.lodi` **37,248 B**.
* **L6 — the four stage times, written and moving.** GREEN both ways, in the
  panel and on the command line.
  * GUI run 1 (one object chunk): `landscape 0.0, meshes 3.8, textures 0.2, impostors 0.0`
  * GUI run 2 (the shadow heightmap only): `landscape 1.1, meshes 0.0, textures 0.0, impostors 0.0`
  * CLI region bake: `landscape 0.0, meshes 4.1, textures 0.4, impostors 0.0`
  * CLI heightmap bake: `landscape 1.1, meshes 0.0, textures 0.0, impostors 0.0`
* **L7 — the exe newer than every changed file, drivers rebuilt, rung equals the
  launch bytes.** GREEN. The rung is md5-identical to the launch exe; no gate in
  this lane runs a standalone driver binary.
* **L8 — no NifSkope left running, game down at every launch.** GREEN, checked
  before the build, before each relink and at the close.

## 5. Pictures

`scratchpad/lodui1_20260911/images/`, all in-application dock grabs through
`WW_LODGEN_SHOT` (never a desktop capture), 497 x 741, taken on the 13:24:12
exe. The progress pane is hidden for the duration of the grab and put straight
back, because the settings could otherwise never get more than about 270 px of
a 741 px dock — the map alone has a 160 px minimum.

* **`panel_fo4cs.png`** — the panel under **FO4 Community Shaders**. Visible,
  in order: Source, Plugins, Resources, Worldspace `Commonwealth (0000003c)`,
  Output mod, the **What to generate** heading, **Target = FO4 Community
  Shaders**, **Landscape file (.lodl)** ticked with its two radios and its two
  numbers, **Shadow heightmap (.HeightMap.dds)** ticked with its Size row, and
  **Native object files (.lodo/.lodi)** ticked with **Identity channels and
  manifests** under it. There is no `.bto` row, no `.btr` section and no atlas
  row anywhere above them. The pinned summary reads: *Terrain\Commonwealth.lodl
  (about 36 MB); ...HeightMap.dds (6144 x 6144, 72 MB); Terrain\Commonwealth.VT.*.lodt
  (5 levels, 12276 tiles, about 1.59 GB); Terrain\Commonwealth.lodo and
  Terrain\Commonwealth.lodi; 3060 object chunks under meshes\terrain\Commonwealth,
  which the pair is built from; the object texture arrays and their .lodm.*
* **`panel_stock.png`** — the same panel under **Stock engine**. The `.lodl`,
  heightmap and native rows are gone; **Object LOD chunks (.bto)** is the head,
  with Bake vertex AO, the AO skirt, the buried cull and its margin, far-ring
  simplification and its four ratios, **Pack an object texture atlas** — which
  FO4CS does not show — and the Impostor cards row. The summary reads *3060
  chunks (object .bto and terrain .btr) under meshes\terrain\Commonwealth.*
* **`result_line.png`** — the pinned action bar after a driven run, 521 x 88:
  the "Will write to Lodgen_LODUI1_run" sentence and, under it, **`stage times:
  landscape 1.1 s, meshes 0.0 s, textures 0.0 s, impostors 0.0 s`** — the
  heightmap-only run, which is the half of the moving proof that shows three
  stages at zero.

**What the pictures do NOT show, said plainly:** Trees only, the 512 px entry
and "Tree cards from ring" are below the fold in `panel_fo4cs.png` — they sit
inside the object section, under the rows that are in frame. They are proven by
the harness counts and by the cost line printed into the log
(`cost at 8x8/256: 2048 x 2048 ...`, `cost at 8x8/512: 4096 x 4096 ...`), not by
a picture. A fourth relink would buy a second grab anchored on those rows.

## 6. Owed / red / bungo's calls

### His calls

1. **The FO4CS target still writes `.BTO` chunk files.** Not as an offered
   output — the row is gone — but the files are. The texture arrays, the card
   arrays, the shape merge and the far-ring cut all take a list of written
   `.bto` paths and read them back, and FO4CS's Improved LOD module reads them
   today and reads no native file (the dependency the 06:4x ruling itself
   flags). The summary line says so in words before anything runs. Stop writing
   them under FO4CS, or keep them until the runtime reads the pair?
2. **The Trees-only row is ON by default in the panel, but `--candidates` still
   defaults to `missing`** in `nifcli.cpp` and in `tools/bake_impostor_cards.sh`.
   Kept that way deliberately so no existing gate moved; the bake instruction
   names `CANDIDATES=trees` explicitly. Make the two defaults agree, or leave
   the command line's alone?
3. **Where the ground cover lives** — carried from TERRAIN-R, untouched here.
   `--vt-cover-in-color` stays INI-only with no row, exactly as the brief said.

### Red / owed

* **The panel grab does not reach the impostor rows** (section 5). One relink
  away.
* **Gate L3's wording was optimistic and the measurement corrects it.** 512 px
  gives 4096 on the frame's LONG side (2176 x 4096 on the maple), not a square
  4096 sheet; 256 px gives 1152 x 2048. The number moves with the row, which is
  the property the gate exists to prove, and the short side narrowing with the
  silhouette is the documented size-ladder behaviour.
* **The panel's cost line is an UPPER BOUND, not a prediction.** It prices a
  square silhouette (`4096 x 4096 ... 74.67 MB`); the maple's real sheet is
  2176 x 4096, a little over half that. That was already true at 64/128/256 and
  this lane did not change it, but 512 makes the gap large enough to matter.
  Worth a line of arithmetic in a later lane, or a word in the tooltip.
* **The future per-object card picker is PARKED**, on bungo's own word, and was
  not built.
* `WW_CHANGES.md`, `HANDOFF.md` and `MISTAKES.md` were **not edited by this
  lane** — the text is in `scratchpad/lodui1_20260911/`, for the director to
  splice.

## 7. Mistakes

Five, in `scratchpad/lodui1_20260911/MISTAKES_ENTRIES.md`, each written the
moment it was recognised:

1. A heredoc halved a backslash in an anchor — the exact trap the
   `ww-anchored-hookup` skill had warned about ten minutes earlier.
2. Indentation typed off a Read (whose line-number prefix is a tab) instead of
   off the bytes; three Edit refusals.
3. A new spell's check-count floor was PREDICTED (128) rather than measured
   (125), so the spell failed on its own arithmetic while every check passed.
4. A picture was taken and moved past without being looked at; it showed none of
   the rows it was offered as proof of. Twice, which is why there were three
   relinks and not one.
5. Two checks whose premise this lane changed were not re-aimed before the
   build, and turned red on the first gate run.

## 8. Finished-work skill review

### Skills loaded and used

* **`nifskope-ww-panel-style`** — every new control went through the shared
  helpers (`wwMatchFieldStyle` on the combo entry, the label | field grid, one
  setting per row, the name-only label with the explanation in a one-sentence
  tooltip), and the self-test counts each of them. The two new check boxes and
  the new label kept the existing counts green: numbers >= 13, group boxes 0,
  selectors >= 6 all matched, dashed labels 0, untipped outputs 0.
* **`nifskope-ww-build-verify`** — the gated chain with make's own exit code,
  the interactive-window discriminator (`--port`), the syntax pass on all four
  changed translation units BEFORE the build, `test exe -nt source`, the
  stylesheet compare, and the "a successful build is not a consistent one"
  header sweep, which is what caught that `Makefile.Release` did not name
  `nativeemit.h` for `lodgenmanager.o` and made the `qmake` re-run non-optional.
* **`ww-anchored-hookup`** — every multi-line edit to `src/nifskope_ui.cpp` went
  through a refusing script with `--check` first, exact-once anchors carrying the
  real line ending, and a CR-count assert either side. Four scripts, twelve
  anchors, CR 0 throughout.
* **`ww-test-harness-add`** — the arming shape, the log ending in PASS/FAIL, the
  spell, and a floor on the other side of every new count.
* **`fo4cs-census-field`** — the four stage times were treated as census words:
  written and moving, no duplicate key, a default that does not flatter. One
  DIVERGENCE, stated: the skill says a metric must refuse as `n/a` rather than
  print a number on no data; these print `0.0 s`, because "this stage did not
  run" is exactly what 0.0 means here and it is what both halves of the moving
  gate rest on.
* **`nifskope-ww-lodgen`** — the CLI's shape, the byte-identity habits and the
  GUI harness rules (one instance, second monitor, `--port`).
* **`ww-retire-a-surface`** — for `--candidates all` and the `.bto` row: the
  inventory before the deletion, the refusal in words rather than a silent
  downgrade, and the harnesses re-aimed rather than edited to agree.

### Skills WRITTEN

* **`ww-panel-run-harness`** (new, in the REPO tree at
  `.claude/skills/ww-panel-run-harness/SKILL.md`) — drive a panel to completion
  inside its own `WW_*_TEST`: why a reading harness cannot prove a row is wired,
  gating the leg on its own variable with its own spell, pumping the event loop
  against the Cancel button rather than a private `running` flag, **putting the
  user's QSettings group back because the action button saves it**, the two-run
  pairing that gives every field its zero, measuring the input headlessly first
  to decide whether an in-app leg is reasonable at all, and printing the same
  numbers on the command line so the cheap gate can own the arithmetic. This was
  the largest thing re-derived from first principles in this lane, and lane
  BAKEPERF1 needs exactly it to put before/after stage times on a board.

### Skills AMENDED (repo tree — the director mirrors them to the live tree)

* **`nifskope-ww-panel-style`** gains *"Arrange the dock before you grab it"*:
  the width, then HIDING the pane that steals the height (arguing with the
  splitter does not work — the map's 160 px minimum wins), then scrolling to the
  row under test with an anchor chosen from what is visible at the current
  target, then looking at the image. This cost two relinks in this lane.
* **`ww-test-harness-add`** gains two sections: *"a check-count floor is
  measured, never predicted"* (the 128-vs-125 failure) and *"after changing a
  default, sweep the harness for stale premises"* (the two checks whose English
  my change made false, one going red and one going green for the wrong reason).

### Declined

* A skill for "add a row to the LOD Generation panel" — `nifskope-ww-panel-style`
  plus `fo4cs-menu-row`'s sibling already cover the mechanics, and the parts that
  were specific here (which QSettings key, which `want*()` predicate, which
  sync lambda) are one file's structure and are documented in that file's own
  comments. It will not be re-derived from memory.

## 9. What this lane changed on disk (nothing committed)

Tracked, modified:
`src/lodgen.h`, `src/lodgen.cpp`, `src/nifcli.cpp`, `src/lodgenmanager.cpp`,
`src/nifskope_ui.cpp`, `tests/spells/lod_generation.sh`,
`tools/bake_impostor_cards.sh` — 7 files, CR 0 in every one before and after.

New:
`tests/spells/lodgen_panel_run.sh`, `tests/spells/lodgen_stage_times.sh`,
`.claude/skills/ww-panel-run-harness/SKILL.md`.

Amended in the REPO skill tree (the director mirrors them to the live tree):
`.claude/skills/nifskope-ww-panel-style/SKILL.md`,
`.claude/skills/ww-test-harness-add/SKILL.md`.

Scripts and text kept in the repo, not `%TEMP%`:
`scratchpad/lodui1_20260911/` — `splice_selftest.py`, `splice_runleg.py`,
`fix01.py`, `fix02.py`, `fix03.py`, `selftest_block.cpp`, `runleg_block.cpp`,
`logs/`, `images/`, `BAKE_INSTRUCTION.md`, `WW_CHANGES_ENTRY.md`,
`HANDOFF_BLOCK.md`, `MISTAKES_ENTRIES.md`.

`NifSkope.pro` was NOT touched — no new translation unit. `qmake` was still
re-run, for the new `nativeemit.h` include in `lodgenmanager.cpp`.
