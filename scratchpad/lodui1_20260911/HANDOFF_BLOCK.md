- LODUI1 LANDED AND GATED, EXE FREE. `release/NifSkope.exe` **2026-09-11
  13:24:12, 21,234,176 B** (ROADS1's was 12:19:06, 21,180,928). Rollback rung
  `release/NifSkope.before_lodui1.exe` (12:42, 21,180,928 — md5
  `9ca009cb289858c17d87546a216c45a6`, equal to the launch exe byte for byte).
  Markers: `scratchpad/lodui1_20260911/DONE` in, `BUILDING` gone. Report
  `scratchpad/lane_lodui1_report.md`; entry text
  `scratchpad/lodui1_20260911/WW_CHANGES_ENTRY.md`; **five MISTAKES entries NOT
  appended by the lane** — `scratchpad/lodui1_20260911/MISTAKES_ENTRIES.md`, the
  director splices. ONE build (13:09:49) plus **THREE counted relinks** (13:13:51
  the two checks the first gate run turned red; 13:22:11 and 13:24:12 both for
  the panel grab, which twice could not reach the rows it was offered as proof
  of), each declared in the report's section 4. `qmake` was re-run with the
  build: `src/lodgenmanager.cpp` gained a NEW include (`nativeemit.h`) that the
  frozen dependency list did not name.

  **WHAT SHIPPED, against his four rulings of that morning.**
  (06:4x, the five .lod types) Under **FO4 Community Shaders** the panel offers
  `.lodl`, `.lodt`, the new `.lodo`/`.lodi` pair and the `.lodm` sidecars, and
  four legacy rows are hidden whole: the `.btr` section (with its terrain-texture
  bake, cover rows and identity channels), the `.bto` head, the object atlas and
  "Chunk textures from the pyramid". The **Stock engine** shows all four again
  and loses the native head. The object SETTINGS did not split: the section's
  header carries two check boxes and one is offered at a time, so every row below
  stays reachable under both targets.
  (07:0x–07:2x, trees only) **`LodgenTreesOnlyCheck`, ON by default**; off is the
  old empty-far-slot rule and nothing else. `--candidates all` is **retired** and
  refuses by name in the CLI and in `tools/bake_impostor_cards.sh`. The three
  path tests now have ONE definition (`lodgenIsTreeModel`).
  (07:3x, 512 px) in the card resolution list and in the driver's `TILE` list;
  the cost line is computed and moves.
  (07:4x) "Cards from ring" is now **"Tree cards from ring"** and is tree-only in
  effect in both states of the toggle.
  **THE NATIVE ROW EXISTS AND IS WIRED** — the item NATIVE1a and NATIVE1b both
  closed on. A GUI-driven one-chunk Sanctuary run wrote
  `Terrain\Commonwealth.lodo` **9,657,316 B** and `.lodi` **37,248 B**.
  **THE FOUR STAGE TIMES ARE DELIVERED**, in the panel's new result line and on
  the command line, from one formatter.

  **GATES.** New `lodgen_stage_times.sh` **16/0 PASS** and `lodgen_panel_run.sh`
  **125/0 PASS** (the panel driven to completion twice, floor 125 MEASURED);
  `lod_generation.sh` **116/0 PASS** (was 97/0 on the rung, floor raised 74 →
  116). Baselines all held on the final exe: `lodgen_native.sh` 18/0,
  `lodgen_terrain.sh` 26/0, `lodgen_terrain_vt.sh` 41/1 (V9b, red on the rung
  too), `lodgen_roads.sh` 11/0, `lodgen_ground_cover.sh` 29/5,
  `lodgen_terrain_pbrm.sh` 14/0, `lodl_open.sh` 23/0, `lodl_write.sh` PASS,
  `ui_align.sh` 11/0, `lodgen_merge.sh` / `lodgen_identity.sh` /
  `lodgen_card_arrays.sh` / `lodgen_texture_arrays.sh` /
  `lodgen_impostor_cards.sh` PASS. Consistency: the exe is newer than all 10
  changed files and the six objects that include `lodgen.h` are newer than it;
  `res/style.qss` and `release/style.qss` byte-identical.

  **THE STAGE TIMES ON THE SANCTUARY REGION**, measured, one chunk at dim 4:
  GUI object run `landscape 0.0, meshes 3.8, textures 0.2, impostors 0.0`; GUI
  heightmap run `landscape 1.1, meshes 0.0, textures 0.0, impostors 0.0`; the CLI
  region bake `landscape 0.0, meshes 4.1, textures 0.4, impostors 0.0`. The
  one-page bake instruction for the full Commonwealth, with those numbers and an
  extrapolation labelled as an estimate, is
  `scratchpad/lodui1_20260911/BAKE_INSTRUCTION.md`.

  **THREE CALLS FOR BUNGO.** (1) **The FO4CS target still writes `.BTO` chunk
  files** — not as an offered output, but because the texture arrays, the card
  arrays, the shape merge and the far-ring cut all read them back, and FO4CS's
  Improved LOD module reads them today. The summary line says so in words. Stop
  writing them, or keep them until the runtime reads the pair? (2) **Where does
  the ground cover live** — carried over from TERRAIN-R, still open, and
  `--vt-cover-in-color` is still INI-only with no row, as the brief instructed.
  (3) **The Trees-only row is ON by default in the panel, but `--candidates`
  still defaults to `missing` in the CLI and the driver** — kept that way so no
  existing gate moved; the bake instruction names `CANDIDATES=trees` explicitly.
  Make them agree, or leave the CLI's old default alone?

  **TWO REDS / OWED.** (a) The panel grab reaches the target row, the five
  outputs and the native row, but **Trees only, 512 px and "Tree cards from ring"
  are below the fold** in it — they are proven by the harness counts and by the
  cost line printed in the log, not by the picture. A fourth relink would buy a
  second grab scrolled to the impostor rows. (b) Gate L3 was pre-registered as
  "a bake at 512 writes a 4096-wide sheet"; what is true is **4096 on the
  frame's LONG side**: `TreeMapleForest1.nif` at `OCT=8` gives 2176 x 4096 at
  512 px and 1152 x 2048 at 256 px, because a thin tree gets a narrower frame
  from its own silhouette (the row's tooltip already says so). The number moves
  with the row, which is what the gate was for.

  **PICTURES.** `scratchpad/lodui1_20260911/images/panel_fo4cs.png`,
  `panel_stock.png` (in-app dock grabs, 497 x 741, the progress pane hidden for
  the grab so the settings get the height) and `result_line.png` (the pinned
  action bar with the four stage times in it).

  **NEW FILES.** `tests/spells/lodgen_panel_run.sh`,
  `tests/spells/lodgen_stage_times.sh`.

  **RESTART: YES.** Anyone holding a NifSkope window from before 13:24:12 needs
  to restart it. No NifSkope was running at any point during this lane except its
  own harnesses and two headless card bakes, one at a time, on the second
  monitor; none is running at its close; the game was down at every launch.
