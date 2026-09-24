# HANDOFF.md block owed by lane BAKEPERF1 (2026-09-11)

Text for the director to splice into `HANDOFF.md`. Every number below is
measured; nothing is left to fill in.

---

- **BAKEPERF1 LANDED AND GATED, EXE FREE.** `release/NifSkope.exe`
  **2026-09-11 14:48:52, 21,261,312 B** (LODUI1's was 13:24:12, 21,234,176).
  Rollback rung `release/NifSkope.before_bakeperf1.exe` (13:24:12, 21,234,176 B,
  md5 `edd2a99ea9fcbeb21b18db05c8b0dbb3` — equal to the launch exe byte for
  byte). Markers: `scratchpad/bakeperf1_20260911/DONE` in, `BUILDING` gone.
  Report `scratchpad/lane_bakeperf1_report.md`; entry text
  `.../WW_CHANGES_ENTRY.md`; **three MISTAKES entries NOT appended by the lane**
  — `.../MISTAKES_ENTRIES.md`. **ONE build (13:59:25 → 14:02:15, qmake re-run
  for two new sources) plus FIVE counted relinks**, each declared in the
  report's §3: 1 the array-pseudonym race, 2 the nested fan-out, 3 the
  crash-dialog fix + the whole texture budget, **4 the fallback (chunk fan-out
  opt-in) + serialised NIF parsing**, 5 the persistent fan-out pool. A SIXTH
  link was taken and discarded for diagnosis only —
  `make LFLAGS="-Wl,-subsystem,windows -mthreads"` to put the symbol table back
  for one gdb run; the shipped exe is stripped as the project's flags say.

  **WHAT BUNGO ASKED, AND THE ANSWER.** *"bake time, anything we can do to speed
  it up? use my system to its fullest here?"* — **not on the chunk queue yet,
  and the blocker is named: the NIF parser is not thread-safe.** Building
  NifModels on worker threads faults; five runs of five on the nine-chunk
  Sanctuary region ended in `STATUS_HEAP_CORRUPTION`, and a symbolised stack put
  it in `NifItem::deleteChildItems()` under `BaseModel::~BaseModel()` inside
  `lodgenLoadModel`, with every other worker in the same parser. Serialising the
  parse makes the fan-out survive and it is then **1.8x SLOWER** than one
  thread, because parsing is the part that cannot overlap. So the machinery
  ships switched off behind one number and the default bake is byte for byte the
  bake that has always run.

  **WHAT SHIPPED.** `src/lodgenchunkpass.{h,cpp}` (new) — the chunk queue the
  panel and the CLI had written out twice, now one function, with results
  retiring **in job order** so `writtenBto` (the atlas, texture arrays, merge,
  far-ring cut, card arrays all read it in order), the printed lines, the panel
  preview, the progress bar and the `.lodo`/`.lodi` accumulator can never see
  completion order. `src/lodgenparallel.{h,cpp}` (new) — `--threads N` (the
  general fan-out budget, default the machine, used by the BC1/BC3/BC4 block
  encoders), `--chunk-threads N` (**default 1**), a persistent pool, a bounded
  **writer thread** that drains before the run returns, and the peak working
  set. `nativeemit` gained a per-job **journal** replayed in job order.
  `gamemanager` locks the process-wide NifModel resource map; `nifmodel` locks
  the array-pseudonym tables; `nifcli` calls `SetErrorMode` so a headless crash
  never shows a dialog.

  **GATES. Byte identity GREEN, all four, both regions** — Sanctuary 9 chunks
  60 files / 24,975,886 B and downtown Boston 25 chunks 163 files / 77,591,755 B,
  the rung against the shipped default AND the serial queue against
  `--chunk-threads 16`, plus the literal `--threads 1` form. The comparator was
  shown RED first on one flipped byte and on one missing file. Stability
  `--chunk-threads 16`: **5 of 5 clean** after relink 4 (5 of 5 faulted before
  it). **TWELVE HARNESSES, ELEVEN GREEN, ONE UNMOVED RED**:
  `lodgen_stage_times` 16/0, `lodgen_terrain` 26/0, `lodgen_native` 18/0,
  `lodgen_identity` PASS, `lodgen_merge` PASS, `lodgen_texture_arrays` PASS,
  `lodgen_card_arrays` PASS, `lod_generation` **116/0** (floor 116),
  **`lodgen_panel_run` 125/0** (the panel driven to completion twice on the
  rewritten chunk loop), `ui_align` 11/0, `water_ui` 82/0 (floor 72); the one
  red is `lodgen_terrain_vt` **41/1**, the same `V9b` check at the same count
  that was red on the rung. **AND THE HEADLINE NUMBER: no regression and no
  speed-up** — three alternating runs a side give medians 16,040 ms (rung) vs
  16,130 ms (shipped), 0.6 percent apart against a 2.3 s within-exe spread.
  Stage times, memory and the Commonwealth extrapolation: report §3.8–§3.11.

  **FIVE REDS.** (R1) **the NIF parser is not thread-safe** — a lane of its own,
  and the one thing that unlocks the machine; (R2) the chunk fan-out is correct
  and byte-identical but SLOWER, so it ships off; (R3) a pre-existing defect
  found while auditing: `src/model/nifmodel.cpp:1421` copy-assigns
  `multiArrayPseudonyms1` INTO the global `arrayPseudonyms` instead of
  re-pointing a reference, so the first multi-array row displayed replaces the
  application's singular-name table for the session — UI path, deliberately not
  fixed here; (R4) the pyramid tile loop is still serial, same blockers; (R5) no
  panel row for either thread number, as the brief instructed.

  **OWED, best first.** Hoist `lodgenBuildObjectChunk`'s per-chunk `modelCache`
  to the RUN — every chunk re-parses every model it needs, it is byte-identical
  because the loader is a pure function of the path, and it speeds up the SERIAL
  path, which is the one that ships. Then batched card bakes: **one process,
  many models**, not K models per frame (the frame's arithmetic is a world
  measurement off viewport pixels — lane CARDORTHO's trap), worth a process
  start plus the hook's fixed 1200 ms sleep per card.

  **NEW FILES.** `src/lodgenchunkpass.{h,cpp}`, `src/lodgenparallel.{h,cpp}`,
  two skills in the REPO tree (`nifskope-ww-crash-diagnose`,
  `ww-parallelise-a-stage`) for the director to mirror.

  **RESTART: YES.** Anyone holding a NifSkope window from before **14:48:52**
  needs to restart it.
