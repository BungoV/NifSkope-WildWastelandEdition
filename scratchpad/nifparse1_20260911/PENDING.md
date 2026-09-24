# NIFPARSE1 — resume (written 2026-09-11 16:4x, while CODE-ONLY)

Written early on purpose. The lane is **not finished**: it is blocked on the
build slot (lane CARDS-AGG holds it and `scratchpad/cards_agg_20260911/DONE`
does not exist yet). Everything below is on disk and checked; nothing has been
built, nothing has been applied, nothing is committed.

## Read first

1. `CONSTITUTION.md`
2. `scratchpad/brief_nifparse1.md`
3. `scratchpad/lane_nifparse1_report.md` — sections 0 (pre-registered gates) and
   1 (the inventory by call path, and the three candidates C1/C2/C3)
4. `scratchpad/lane_bakeperf1_report.md` §3.4–§3.10 — the crash, the five
   relinks, the stage table and the memory numbers this lane has to beat
5. Skills: `nifskope-ww-crash-diagnose`, `ww-parallelise-a-stage`,
   `ww-anchored-hookup`, `nifskope-ww-build-verify`, `nifskope-ww-lodgen`,
   `ww-test-harness-add`, `fo4cs-census-field`

## On disk, NOT built, NOT applied

| file | state |
|---|---|
| `src/nifparsestress.h` / `.cpp` | **new.** The discriminator: the model layer on N threads with no resource layer in the picture. `g++ -fsyntax-only` **RC=0** (MSYS2 UCRT64, project include set). Not in `NifSkope.pro` yet |
| `tests/spells/parse_stress.sh` | **new.** Drives it: S1 the sabotage floor FIRST, then S2/S2b/S3/S4. Never run |
| `scratchpad/nifparse1_20260911/hookup.py` | **7 edits**, `--check` green: *7 anchors, all matched once, nothing written*, CR 0 before and after on `NifSkope.pro` and `src/nifcli.cpp`. **NOT applied** — `NifSkope.pro` is CARDS-AGG's |
| `scratchpad/nifparse1_20260911/fixes.py` | **25 edits** over `src/message.cpp`, `src/gamemanager.{h,cpp}`, `src/data/nifvalue.cpp`, `src/model/nifmodel.cpp`, `src/xml/nifexpr.cpp`, `src/lodgenparallel.{h,cpp}`. `--check` green: *25 edits, all anchors matched once, nothing written*, CR unchanged on all eight. **NOT applied — gate N1 first** |
| `scratchpad/nifparse1_20260911/relink_sym.sh` | the diagnostic relink (symbols back). A LINK, therefore a build |
| `scratchpad/nifparse1_20260911/gdb3.sh` | three gdb runs on the 25-chunk region at 16 chunk threads |
| `scratchpad/nifparse1_20260911/loop20.ps1` | gate N2's twenty-run loop, per region, NTSTATUS decoded per run |
| `scratchpad/nifparse1_20260911/fix_anchor.py` | rebuilt one anchor from the file's own bytes after it was typed wrong twice; kept as the worked example the amended skill points at |
| `scratchpad/nifparse1_20260911/syntax.log` | the syntax gate's artefact: `g++ 16.1.0`, `SYNTAX-RC=0` |

## RE-CHECK BOTH SCRIPTS FIRST - another lane is editing one of their files

`src/nifcli.cpp` changed under this lane at 15:46 while CARDS-AGG was alive, so
the hook-up anchors can go stale at any moment. Both scripts were re-run
immediately after that edit and were still green - hookup.py 7 of 7, fixes.py
25 of 25, nothing written - but a resume runs BOTH `--check`s again before
anything else, and treats a refusal as "the anchor moved", not as a bug.

## The order to resume in, and why that order

1. **Wait for `scratchpad/cards_agg_20260911/DONE` and for no `scratchpad/*/BUILDING`.**
   Read CARDS-AGG's DONE line for the exe it leaves; rung it ONCE to
   `release/NifSkope.before_nifparse1.exe` and record its md5. Put up
   `scratchpad/nifparse1_20260911/BUILDING`. Check `Fallout4.exe` is down
   (`Get-CimInstance Win32_Process`, not `tasklist | grep`).
2. **GATE N1 BEFORE ANY FIX.** `bash scratchpad/nifparse1_20260911/relink_sym.sh`
   (counted relink 1, symbols on, ~26 MB, `nm | wc -l` must be in the tens of
   thousands — a stripped exe gives `?? ()` and that is not a stack), then
   `bash scratchpad/nifparse1_20260911/gdb3.sh 3`. Three stacks; the report says
   whether they agree. `thread apply all bt` matters more than the faulting
   thread: what the other workers are in IS the diagnosis.
3. **Apply the hook-up and build the discriminator.**
   `python scratchpad/nifparse1_20260911/hookup.py --apply`, then **qmake before
   make** (two new sources join the project), then
   `bash tests/spells/parse_stress.sh`. **S1 must go red before S2 is believed.**
   - S2 red at 16 threads with no resource layer in the picture ⇒ **C1**, the
     model layer, and the fix belongs there.
   - S2 green ⇒ **C1 is refuted**, and the bake's fault is C2 (the shared
     `BA2File` freed by `close_archives()`) or C3 (`QMessageBox` built on a
     worker). `fixes.py`'s F1 and F2 are exactly those two.
4. **Only then** `python scratchpad/nifparse1_20260911/fixes.py --apply`, and
   remove the parse mutex in `lodgenLoadModel` (`src/lodgen.cpp:2067`) — that
   file is CARDS-AGG's, so it goes in a second `--check` hook-up, written at
   the time, never a hand edit.
5. **The gates**, in this order: `loop20.ps1` on both regions (20 runs each),
   then identity with BAKEPERF1's `bake_diff.py` **shown red first** on a
   flipped byte and on a deleted file, then the stage table (medians of 3,
   alternating), then peak working set, then the ~100-chunk run
   (`--terrain-region 0 -12 39 27` = 10×10 chunks at dim 4), then the lodgen
   suites, then the editor harnesses.
6. **N5's sweep**: the model headers reach roughly 81 translation units. Check
   every object that includes a changed header is newer than it — not one, all.

## What is red / unknown right now

* **Gate N1 is not discharged.** The fault is NOT named. Section 1.4 lists three
  candidates and the experiment that separates them; no fix may be claimed
  before that experiment has run.
* No build, no relink, no bake, no harness has been run by this lane.
* `src/message.cpp` and `src/gamemanager.{h,cpp}` belong to no live lane but are
  not this lane's either; that is why their edits are in a refusing script.
* Whether making the parser safe produces a SPEED-UP is unmeasured. BAKEPERF1's
  numbers say the chunk fan-out with the parse serialised was 1.5–1.8× slower
  and peaked at 21.9 GB on 25 chunks; the memory cap rule for
  `--chunk-threads`'s default is still to be derived from the 100-chunk point.

## Nothing left running

No NifSkope instance was started by this lane. `Fallout4.exe` was down at every
check. No `BUILDING` marker is up for `nifparse1_20260911`.
