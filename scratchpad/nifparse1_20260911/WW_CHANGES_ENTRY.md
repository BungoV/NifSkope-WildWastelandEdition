# WW_CHANGES.md entry owed by lane NIFPARSE1 (BUILD PENDING)

Text for `WW_CHANGES.md`. **Not spliced by this lane** (CONSTITUTION 8: lanes
deliver changelog text, the director splices; `WW_CHANGES.md` is mixed line
endings and stays so — splice it in binary, match neighbours, never normalise).

Marked BUILD PENDING because **nothing here has been compiled or run**. If the
build lands, the entry is rewritten with the gate numbers and this version is
discarded.

---

## 2026-09-11 — The bake's thread-safety blocker, re-opened and given an experiment

BUILD PENDING — written, `--check` green, **not built, not applied**.

The previous round shipped the LOD generator's chunk fan-out switched off with
the blocker named as *"the NIF parser is not thread-safe"*. That came from one
stack taken in the middle of a whole chunk bake, where the parser, the plugin
reader, the texture cache, the archive layer and the message sink are all live
at once — and for a heap-corruption fault a stack names where the damage was
detected, not where it was done.

Following the calls out of the model loader instead of stopping at the file
boundary turned up two unsafe things, **neither of them in the parser**:

* **One shared archive index, freed while it is read.** Every chunk worker's
  texture and material lookup funnels into one `GameResources`, whose
  `init_archives()` / `close_archives()` delete and rebuild its `BA2File` with
  no lock; the self-healing retry inside `get_file` calls `close_archives()` at
  any moment, and `findFile` hands out `std::string_view`s into the buffers that
  `delete` frees. The bake's warm-up covers first use and only first use.
* **A message path that builds a window from a worker thread.** `Message::append`
  and `Message::message` construct `QMessageBox` widgets on whatever thread calls
  them and append them to an unguarded static list. Not reachable from the
  `-no-gui` command line — the widget-building handler is installed only when
  the application really is a `QApplication` — but **live for the LOD Generation
  panel**, where one missing texture in a chunk worker builds a widget off the
  GUI thread.

Both sit on the path the earlier bisect had already implicated: mesh reads never
enter the game manager at all, while `.dds`, `.bgsm` and `.pbrm` do — and the
two bisect variants that came back clean, `--no-tex-dir` and `--no-roads`, are
exactly the two that stop entering it.

**New: the model layer on N threads, with nothing else in the picture.**
`src/nifparsestress.{h,cpp}` and `tests/spells/parse_stress.sh` read the NIFs
once up front and then build, load, walk and destroy documents on N threads with
no plugin reader, no texture cache, no archive lookup and no file I/O in the
threaded region. It is the experiment that tells "the parser" apart from "the
resource layer", and it can come out either way. It is not a crash detector
only: every worker must reproduce the single-threaded reference digest of what
it read back, so corruption that happens not to fault still fails. Its floors
run first — one flipped byte for one worker has to go red before any green is
believed — and a fixture that did not load, a walk that reached too few items,
or a load count short of `threads x reps x files` are named failures rather than
silent passes.

**Also found, and fixed in the same prepared set:** the block-tree name column's
`QHash & map = arrayPseudonyms; map = multiArrayPseudonyms1;` does not re-point
the reference — it copy-assigns into the application's global singular-name
table, so the first multi-array row ever displayed replaced that table for the
rest of the session.

Ways back, unchanged and exact: `--chunk-threads 1` is the bake that has always
run, and `--threads 1` turns the general fan-out off as well.
