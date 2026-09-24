# RESUME3 -- FINISHED, nothing pending (final rewrite 19:35)

Kept because the brief asked for it and because the traps below are worth
reading before the next crash hunt. **There is nothing to resume**: every gate
ran, every document was written, nothing was committed (that is bungo's call).

Tree `E:\Projects\NifskopeWildWastelandEdition`, branch `main`.
`scratchpad/resume3_20260911/DONE` is the marker; `BUILDING` was removed at the
same moment.

## Where the lane ended

| gate | state |
|---|---|
| R1 both refusing scripts re-checked at re-counted anchors | **GREEN** |
| R2 the fault NAMED from the experiment + four symbolised stacks | **GREEN** -- the headless CLI's message handler writing through an unlocked shared `QTextStream` |
| R3 20x16 threads on both regions, identity, stage table, memory, the default by rule | **GREEN** -- 20/20 and 20/20, byte-identical to serial on both, the default stays 1 with the numbers |
| R4 the tiling change and its gates | **GREEN on S2/S3/S4/S6/S7; S1 PASS on one tile and RED on the other with the cause measured** (the offline prediction draws no roads) |
| R5 the chain, the exe-newer sweep, the drivers | **GREEN except one named moved count** (`lodgen_roads.sh` 11/0 -> 11/1) |
| R6 nothing left running, game down, no crash dialog, timestamps from `date` | **GREEN** |

## The exes

| file | time | size | md5 |
|---|---|---|---|
| `release/NifSkope.before_resume3.exe` (rung) | 2026-09-11 16:20:02 | 21,419,520 | `3ebf175826feee6c6545cf0873635acc` -- CARDS-AGG's DONE line exactly |
| `release/NifSkope.exe` (shipping) | 2026-09-11 **19:08:42** | 21,435,904 | `847236ecb8f83d64b25b8a2ceeec91d3` |
| `scratchpad/nifparse1_20260911/NifSkope.stripped.exe` | 16:53:23 | 21,434,368 | BUILD 1 -- **keep it**: it is the pre-fix side of the serial-parse cost measurement, and rebuilding it costs a build |

One build (16:53:23) plus five counted relinks (16:59 symbols, 17:23:03,
19:04:23, 19:06:29, 19:08:42), each `BUILD-RC=0`.

## What is in the tree, and which script put it there

| # | script | what |
|---|---|---|
| 1 | `scratchpad/nifparse1_20260911/hookup.py --apply` | 7 edits: the `parsestress` mode joins the build |
| 2 | `scratchpad/resume3_20260911/mutex_off.py` then `mutex_final.py` | BAKEPERF1's process-wide parse lock in `lodgenLoadModel` removed; the comment that replaced it carries the measurement |
| 3 | `scratchpad/resume3_20260911/fix_clilog.py --apply` | **THE FIX**: `cliMessageHandler` writes with `std::fputs` under its own mutex |
| 4 | `scratchpad/resume3_20260911/fixes_subset.py --files src/message.cpp,src/gamemanager.h,src/gamemanager.cpp,src/model/nifmodel.cpp,src/lodgenparallel.h,src/lodgenparallel.cpp --apply` | 19 of NIFPARSE1's 25 (F1, F2, F4, F5, F7). **F3 and F6 deliberately left behind** with the number that says why |
| 5 | `scratchpad/resume3_20260911/census_boundby.py` | the `bound by` word gets a reader on the `bake census:` line |
| 6 | `scratchpad/resume3_20260911/tiling.py --apply` | 6 edits: `lodgenLandTiling()`, both `TILE` declarations, `--land-tiling` and its usage |
| 7 | `scratchpad/resume3_20260911/contract.py --apply` | 2 paragraphs in `docs/LODGEN_TERRAIN_VT.md`, provenance re-stamped |
| 8 | `scratchpad/resume3_20260911/stale_comments.py --apply` | the two source comments that still blamed the parser |
| 9 | `scratchpad/resume3_20260911/boundby_default.py --apply` | the census word's fourth state, `default` |
| 10 | hand edits, both read back | `tests/spells/lodgen_terrain_model.py`'s own `TILE` (with a `WW_LAND_TILING` override) and six usage lines in `src/nifcli.cpp` repaired after a heredoc broke them |

## Ledger text delivered for the director to splice

* `scratchpad/resume3_20260911/WW_CHANGES_ENTRY.md` -- ONE replacement entry that
  **supersedes lines 79 through 168** of `WW_CHANGES.md` (both BUILD PENDING
  entries and the blank line between them; both measured LF-only, 0 of 35 and 0
  of 55 lines CRLF, and the file as a whole stays mixed at CR 19,020).
* `scratchpad/resume3_20260911/MISTAKES_ENTRIES.md` -- **7 entries**.
* `scratchpad/resume3_20260911/HANDOFF_BLOCK.md` -- the top block.
* `## Build (RESUME3)` appended to `scratchpad/lane_nifparse1_report.md` (394 ->
  463 lines) and `scratchpad/lane_splat1_report.md` (742 -> 835 lines).
* A dated paragraph appended to `scratchpad/lodui1_20260911/BAKE_INSTRUCTION.md`
  (164 -> 210 lines).
* `scratchpad/pic_grass_20260911/NOTES.md`, new.
* Skills amended AND mirrored to `E:\Projects\Claude\.claude\skills`, `diff -q`
  SAME on both: `nifskope-ww-crash-diagnose` (120 -> 183) and
  `ww-anchored-hookup` (182 -> 207).

## The traps this lane paid for, so the next one does not

* **`gdb` needs `set environment _NO_DEBUG_HEAP=1`** or a heap fault that is
  3-of-5 bare is 0-of-3 under the debugger. BAKEPERF1 and NIFPARSE1 both lost
  their stack to this and both wrote it down as a property of the bug.
* **`gdb` and `nm` exist only inside MSYS2 UCRT64.** `relink_sym.sh` prints
  `SYMBOLS=0` from Git-Bash on an exe carrying 71,982 symbols.
* **The fault is on SANCTUARY, not Boston.** Boston at 16 threads ran clean with
  the lock off; Sanctuary faulted 3 of 5. Bigger is not racier.
* **Anchors carry non-ASCII**: EM DASH (U+2014) in `src/lodgen.cpp`'s `TILE`
  comment, UNICODE MINUS (U+2212) in `docs/LODGEN_TERRAIN_VT.md`. Read the bytes.
* **A Bash heredoc halves backslashes** and will write a C string literal with a
  real newline inside it. Use the Write tool for any generated text containing a
  backslash, not only for anchors.

## Nothing left running

`tasklist | grep -i -E "Fallout4|NifSkope"` rc=1 at the end; `Fallout4.exe` was
absent at every check; every bake went through `bake_run.ps1`, which
dot-sources `no_crash_dialog.ps1`, and the exe sets the same error mode itself
in `initModelLayer()`. **No Windows "Application Error" box reached his
desktop**, including on the three deliberate faults. Six temporary bake trees
and the gdb work dirs were deleted; the ones kept on purpose are
`scratchpad/resume3_20260911/tiling/`, `serial_*`, `rung_*`,
`scratchpad/nifparse1_20260911/loop_r{1,2}_keep`, and
`scratchpad/pic_grass_20260911/out/`.

**bungo's open NifSkope window needs a restart** -- the exe on disk is 19:08:42.
