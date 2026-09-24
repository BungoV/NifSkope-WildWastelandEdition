# HANDOFF.md top-block text from lane RESUME3 (delivered, NOT applied)

Splice as one block. Every figure here is measured and timestamped; the full
detail is in `scratchpad/lane_resume3_report.md`.

---

## Lane RESUME3 -- 2026-09-11, one build for NIFPARSE1 and SPLAT1 phase B

**Two blockers closed with measurements, not theories.**

### 1. The bake's heap fault is NOT the NIF parser, and it is fixed

For a week the LOD generator's chunk fan-out has been off with the blocker
recorded as "the NIF parser is not thread-safe". It is not.

* **The experiment.** `NifSkope -no-gui parsestress` runs the model layer ALONE
  on N threads -- no plugin reader, no texture cache, no archive lookup, no file
  I/O in the threaded region -- and every worker must reproduce the
  single-threaded digest of what it read back. **20 consecutive runs at 16
  threads: 10,240 loads, 0 mismatches, 0 faults**, with both sabotage floors
  seen red first.
* **The fault.** With BAKEPERF1's containment mutex removed the 9-chunk
  Sanctuary region faulted **3 of 5** at `--chunk-threads 16`. Four symbolised
  stacks: two of them INSIDE `cliMessageHandler`, the headless CLI's own message
  handler, which wrote through `err()` -- a function-local `static QTextStream`
  with no lock. `GameResources::get_file` calls `qWarning()` on every miss and
  the road pass misses the same `.bgsm` on every placement, so sixteen workers
  grew one `QString` buffer at once. The other two stacks are an innocent
  `QList` reallocation that reached the corrupt heap first.
* **The fix**: the handler writes with `std::fputs` to `stderr` under its own
  mutex (the CRT locks the `FILE *`). One thread produces exactly the same bytes
  in the same order -- the way back is exact.
* **The gate**: `--chunk-threads 16`, **20 of 20 clean on Sanctuary and 20 of 20
  on Boston**, byte-identical to the serial bake on both regions.
* **THE DEFAULT STAYS `--chunk-threads 1`** and the reason is speed and memory,
  not safety. Medians of 3, alternating: Sanctuary 17.9 s / 1.75 GB serial
  against 40.5 s / 5.80 GB at 16 (2.26x slower); Boston 128 s / 3.76 GB against
  156 s / **25.1 GB** (1.22x slower). The texture stage is where it goes
  backwards -- 5.8 -> 31.9 s and 66.1 -> 89.4 s -- because sixteen workers keep
  sixteen cold texture caches. The source comments and usage text that said the
  fan-out was off because the parser faults were rewritten. `--chunk-threads 0` now means the smaller
  of the cores and what free memory holds, and the bake census says which bound
  decided it.
* Also landed: the shared archive index behind one read/write lock;
  `Message::append` / `message` refuse to build a `QMessageBox` off the GUI
  thread (live for the LOD Generation panel, not for the command line); and the
  block-tree name column no longer copy-assigns into the application's global
  pseudonym table.
* **NOT landed, deliberately, with the number that says why**: the lazy
  `NifValue::type()` re-init and the four shared `QRegularExpression`s. The
  experiment put 10,240 loads through both and neither moved. Named as latent in
  the report; the prepared edits are still in
  `scratchpad/nifparse1_20260911/fixes.py`.

### 2. The landscape textures were baked 6x too large -- fixed, default moved

* The bake tiled every landscape texture at 2,048 world units a repeat. The
  engine's own repeat is **341.3333** = 128 / 0.375, read out of `Fallout4.exe`
  1.10.155 (`fLandTextureTilingMult` 1.5f, one code reference, the 17x17
  quadrant loop). **6.0000x too coarse.**
* All **fourteen** sampling sites -- colour, mask and emissive, stock bake and
  pyramid -- read one value now. `_msn` is byte-identical at both tilings.
* `--land-tiling 2048` is the exact way back, byte-identical to the previous exe
  on every file of both test tiles.
* Real-bake numbers, chunk (-20,24): local variance **76.22 -> 12.34** against
  vanilla's 19.81 (SPLAT1 predicted 12.93 offline, 4.6 % out), mean abs RGB vs
  vanilla **16.33 -> 14.33** of 255. Chunk (-20,20): 74.06 -> 25.83 and
  20.43 -> 18.92. On that second tile the prediction was 14.60, and the
  discriminator says why: the offline model draws no roads, and against the
  `--no-roads` bake the same tile reads **17.00 vs 14.60, +16.4 %**, inside the
  gate. The tiling's own effect agrees with the prediction to **2.4 %**.
* **It does not close the colour error against vanilla.** That is the grading
  (x0.82-0.83), still open as "splat calibration vs vanilla grading".
* `docs/LODGEN_TERRAIN_VT.md` 2.5 rewritten with `T` and its provenance; its
  VCLR range corrected from 249..255 to the measured 203..255.

### The chain, in one line

Every harness the change reaches at its baseline -- `lodgen_terrain` 26/0,
`lodgen_terrain_vt` 41/1, `lodgen_ground_cover` 29/5, `lodgen_terrain_pbrm`
14/0, `lodgen_native` 18/0, `lodgen_stage_times` 16/0, `lodgen_panel_run`
125/0, `lod_generation` 116/0, `lodl_open` 23/0, `ui_align` 11/0, `water_ui`
82/0, the five card/array suites PASS, `lodgen_octahedral`'s one pre-existing
`F1 worst 1.88` -- plus all five editor harness logs BYTE-IDENTICAL to the
rung's. **One count moved: `lodgen_roads.sh` 11/0 -> 11/1**, and it is the
tiling's doing in a way worth knowing: bar 2 asks the road to correlate with
vanilla at least 80 % as well as the surrounding GROUND does; the road term did
not move (0.3065 -> 0.3061) and the ground's improved 17 % (0.3442 -> 0.4024),
so the bar rose past a signal that stood still. Nothing was changed to make it
green. **Our road raster is now the worse-matching part of the sheet** -- owed to
a roads lane.

### What is owed to bungo

1. **A restart of his open NifSkope window** -- the exe was relinked.
2. Two pictures: `scratchpad/resume3_20260911/images/cmp_tiling_fixed.png` (the
   tiling, real bake, vanilla beside it) and
   `scratchpad/pic_grass_20260911/images/cmp_grass_tint.png` (the grass tint at
   the corrected tiling).
3. The open item that did NOT close: splat calibration vs vanilla grading.

### State

* Nothing committed. **389 uncommitted paths** in the tree (243 under
  `src/ docs/ tests/ res/ .claude/` plus `NifSkope.pro`); fifteen are this
  lane's: `NifSkope.pro`, `src/lodgen.{h,cpp}`, `src/nifcli.cpp`,
  `src/message.cpp`, `src/gamemanager.{h,cpp}`, `src/model/nifmodel.cpp`,
  `src/lodgenparallel.{h,cpp}`, `src/lodgenchunkpass.cpp`,
  `docs/LODGEN_TERRAIN_VT.md`, `tests/spells/lodgen_terrain_model.py`, the two
  amended skills, plus NIFPARSE1's new `src/nifparsestress.{h,cpp}` and
  `tests/spells/parse_stress.sh`.
* Exe: `release/NifSkope.exe` **2026-09-11 19:08:42, 21,435,904 B, md5
  `847236ecb8f83d64b25b8a2ceeec91d3`** -- one build (16:53:23) plus **five**
  counted relinks, each `BUILD-RC=0`. Exe newer than every changed file (0
  STALE) and **0 of 246 object dependency blocks stale**. Rung `release/NifSkope.before_resume3.exe` 16:20:02, 21,419,520 B,
  md5 `3ebf175826feee6c6545cf0873635acc` (== CARDS-AGG's DONE line).
* Report `scratchpad/lane_resume3_report.md`; resume
  `scratchpad/resume3_20260911/PENDING.md`; ledger text in that folder
  (`WW_CHANGES_ENTRY.md`, `MISTAKES_ENTRIES.md`, this file).
* Skills amended and mirrored to the live tree:
  `nifskope-ww-crash-diagnose` (the `_NO_DEBUG_HEAP=1` rule, the MSYS2-only
  toolchain rule, "point it where the fault is", and a new section 7 "separate
  the stage from the pipeline") and `ww-anchored-hookup` (anchors are read from
  the file, never from a report -- prose normalises punctuation).
