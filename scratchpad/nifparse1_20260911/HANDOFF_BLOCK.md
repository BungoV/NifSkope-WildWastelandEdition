# NIFPARSE1 — HANDOFF block (BUILD PENDING draft, 2026-09-11 16:5x)

Text for `HANDOFF.md`'s top block. This is the BUILD PENDING version; if the
build slot frees and the gates run, it is rewritten with their numbers.

---

- **NIFPARSE1 ENDED BUILD PENDING (slot held by CARDS-AGG; game down
  throughout).** Nothing built, nothing applied, nothing committed. Resume
  `scratchpad/nifparse1_20260911/PENDING.md`; report
  `scratchpad/lane_nifparse1_report.md`; FOUR MISTAKES entries **not appended
  by the lane** at `.../MISTAKES_ENTRIES.md`.

  **WHAT IT FOUND, AND IT MATTERS BEFORE ANYONE BUILDS TO BAKEPERF1'S
  CONCLUSION.** *"The NIF parser is not thread-safe"* was drawn from one stack
  taken in the middle of a whole chunk bake — which is the parser and the plugin
  reader and the texture cache and the archive layer and the message sink at
  once — and for `STATUS_HEAP_CORRUPTION` a stack names where the damage was
  DETECTED, not where it was done. Following the call out of `lodgenLoadModel`
  instead of stopping at the file boundary turned up two unsafe things, **and
  neither is in the model layer**:

  1. `GameManager::GameResources::init_archives()` / `close_archives()`
     (`src/gamemanager.cpp:174-209`, `:254-262`) **delete and rebuild one shared
     `BA2File` with no lock**, and the self-healing retry inside `get_file`
     (`:310-316`) calls `close_archives()` — freeing an index other workers are
     reading, and dangling the interior `std::string_view`s `findFile` hands
     out. `lodgenWarmSharedIndices()` covers FIRST use and only first use.
  2. `Message::append` / `Message::message` **construct `QMessageBox` WIDGETS on
     whatever thread calls them** and append to an unguarded static vector
     (`src/message.cpp:160`, boxes at `:30/:51/:197`). Refuted for the
     `-no-gui` command line (the widget-building handler is installed only
     inside `qobject_cast<QApplication*>`, and `-no-gui` builds a plain
     `QCoreApplication`) — but **live for the LOD Generation PANEL**, where one
     missing texture in a chunk worker builds a widget off the GUI thread.

  And the reads line up with BAKEPERF1's own bisect: `lodgenReadAsset` serves
  `.nif` from `lodgenMeshArchives()` with const, pointer-based
  `findFile`/`extractFile` and **never enters `GameManager`**, while `.dds`,
  `.bgsm` and `.pbrm` fall through to `GameManager::get_file`. The two variants
  that came back CLEAN — `--no-tex-dir` and `--no-roads` — are exactly the two
  that stop entering it.

  **THE DELIVERABLE IS THE EXPERIMENT, and it is written and syntax-checked, not
  run.** `src/nifparsestress.{h,cpp}` (new) builds, loads, walks and destroys
  `NifModel`s on N threads from bytes read once up front — no `EsmWorld`, no
  texture cache, no archive lookup, no road gatherer, no file I/O in the
  threaded region. Red ⇒ the model layer really is the fault; green ⇒ BAKEPERF1's
  conclusion is wrong and the fix belongs in the resource layer. Every worker
  must also reproduce the one-thread reference DIGEST of what it read back, so
  silent corruption that does not fault still fails; the floors are a sabotage
  mode that must go red, a fixture that must load, an item count that must
  exceed 32, and a load count that must equal `threads x reps x files`.
  Driver: `tests/spells/parse_stress.sh` (S1 the floor FIRST, then S2/S2b/S3/S4).
  `g++ -fsyntax-only` RC=0.

  **TWO REFUSING SCRIPTS, both `--check` green, NEITHER APPLIED:**
  `hookup.py` — 7 anchors, all matched once, CR 0 both sides on `NifSkope.pro`
  (CARDS-AGG's) and `src/nifcli.cpp`; `fixes.py` — **25 edits** over
  `src/message.cpp`, `src/gamemanager.{h,cpp}`, `src/data/nifvalue.cpp`,
  `src/model/nifmodel.cpp`, `src/xml/nifexpr.cpp`, `src/lodgenparallel.{h,cpp}`, all
  matched once, CR unchanged on all eight. They are not applied because gate N1
  (the fault named from a symbolised stack) is not discharged, and BAKEPERF1's
  own third recorded mistake was shipping three fixes on hypotheses.

  **ONE THING IN THE FIX SET IS A BUG BUNGO CAN SEE**, and it is BAKEPERF1's red
  R3, left unfixed there: `src/model/nifmodel.cpp:1431` binds
  `QHash<QString,QString> & pseudonymMap = arrayPseudonyms;` and then assigns
  `pseudonymMap = multiArrayPseudonyms1;`, which **copy-assigns into the global**
  instead of re-pointing — so the first multi-array row ever displayed replaces
  the application's singular-name table for the rest of the session. The fix is
  a pointer; its gate is the editor harnesses, not the bake.

  **NOT MEASURED, and named as such:** the fault is NOT named — three candidates
  with the experiment that separates them, no cause stated. No build, no relink,
  no bake, no harness, no 20-run loop, no stage table, no memory point. Whether
  a thread-safe parser would produce a SPEED-UP at all is unmeasured: BAKEPERF1's
  numbers have the fan-out 1.5–1.8x SLOWER with the parse serialised and peaking
  at 21.9 GB on 25 chunks, and the per-worker cost (its own plugin reader plus
  its own texture cache) means the default can never simply be the core count —
  `lodgenChunkThreadMemoryCap()` is declared for that and its constant must come
  from the 100-chunk run, which has not happened.

  **SKILL AMENDED:** `.claude/skills/ww-anchored-hookup/SKILL.md` gained
  sections 5 and 5a — never anchor an `after` on a line ending in `{`, and measure a
  multi-line anchor's indentation with Python byte counts instead of typing it, and build it by READING the line out of the file.
  Repo tree only; **the director mirrors it to the live tree**.

  **RESTART:** not needed. No exe changed; bungo's window is unaffected.
  **Uncommitted:** this lane adds `src/nifparsestress.{h,cpp}`,
  `tests/spells/parse_stress.sh`, its own `scratchpad/nifparse1_20260911/`, and
  edits `.claude/skills/ww-anchored-hookup/SKILL.md`. Nothing committed
  (CONSTITUTION 8).
