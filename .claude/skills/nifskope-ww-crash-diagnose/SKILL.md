---
name: nifskope-ww-crash-diagnose
description: Name a crash in NifSkope Wild Wasteland Edition's own release exe (E:\Projects\NifskopeWildWastelandEdition) instead of guessing at it -- the one-minute diagnostic relink that puts the symbol table back, the gdb batch that catches a timing-dependent fault, reading a Windows NTSTATUS exit code, the crash-dialog rule that keeps a headless run off bungo's desktop, and the bisect shape that narrows a stage before any of it. Use the MOMENT release/NifSkope.exe faults, before writing a single fix.
---

# Diagnosing a crash in the release exe

Written from lane BAKEPERF1 (2026-09-11), which shipped **three fixes on three
hypotheses and got two of them wrong** before doing what is on this page. The
stack, once it existed, answered in one run.

## 0. FIRST: no dialog reaches his desktop

A crashing `-no-gui` run raises a Windows "Application Error" box. It is on the
primary monitor and it takes focus, so it breaks CONSTITUTION 6 twice. **Six of
them reached bungo in three minutes** during that lane's bisect.

* The exe arms itself: `SetErrorMode(SEM_FAILCRITICALERRORS |
  SEM_NOGPFAULTERRORBOX | SEM_NOALIGNMENTFAULTEXCEPT | SEM_NOOPENFILEERRORBOX)`
  at the top of `initModelLayer()` (`src/nifcli.cpp`) — already in the tree
  since 2026-09-11. Check it is still there before a crash campaign.
* A driver script arms itself too, because the error mode is **inherited by
  child processes**: dot-source
  `scratchpad/bakeperf1_20260911/no_crash_dialog.ps1` (a five-line
  `Add-Type` + `SetErrorMode`) before the first launch. `cmd /c start /b` does
  NOT do it — the mode must be set by an ancestor process.
* PowerShell note: `param(...)` must be the FIRST statement in a `.ps1`, so the
  dot-source line goes AFTER the param block, never before it.

## 1. Read the exit code as an NTSTATUS

PowerShell prints `$p.ExitCode` signed; convert with
`'0x{0:X8}' -f ([uint32]([int64]$c + 4294967296))` when it is negative.

| code | meaning | what it tells you |
|---|---|---|
| `0xC0000005` | access violation | a bad pointer, often a use-after-free |
| `0xC0000374` | **heap corruption** | two threads in one allocator or container, or a double free — the fault is reported LONG after the damage |
| `0xC0000409` | stack buffer overrun | |

`0xC0000374` means the stack you eventually get is where the damage was
*detected*, not where it was *done* — which is why the bisect in §2 comes first.

## 2. Bisect the STAGE before you read any code

Three runs a variant, never one: a race that fires four times out of four still
fires zero times out of one. Drop ONE thing per variant and tabulate:

```
full            crash crash crash
no --native     crash crash ok
no --tex-dir    ok ok ok          <- the stage
--no-ao         crash crash crash
meshes only     ok ok ok
```

That table cost fifteen minutes and removed four candidates. Vary the thread
count the same way (1 / 2 / 4 / N) and record where the boundary is.

## 3. THE DIAGNOSTIC RELINK — do this before the second hypothesis

`Makefile.Release` carries `LFLAGS = -Wl,-s`, so `nm release/NifSkope.exe` says
**"no symbols"** and every gdb frame in the exe prints `?? ()`. One relink puts
them back. It changes no source and no behaviour — only the file size:

```bash
cp -p release/NifSkope.exe scratchpad/<lane>/NifSkope.stripped.exe
MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc \
 'export PATH="$PATH:/e/Tools/GIT/cmd"; cd /e/Projects/NifskopeWildWastelandEdition \
  && rm -f release/NifSkope.exe \
  && make -j4 LFLAGS="-Wl,-subsystem,windows -mthreads" && nm release/NifSkope.exe | wc -l'
```

~1 minute, link only (21 MB -> 26 MB, 71,769 symbols). **Relink normally
afterwards** so the shipped exe matches the project's flags, and COUNT both
links in the report.

## 4. The gdb batch

```bash
gdb --batch -ex 'set pagination off' -ex 'set confirm off' \
    -ex run -ex 'info threads' -ex 'thread apply all bt 25' \
    --args release/NifSkope.exe -no-gui lodgen "<esm>" --worldspace 3C \
      --terrain-region <x0> <y0> <x1> <y1> ... > crash_gdb.txt 2>&1
```

* **`_NO_DEBUG_HEAP=1` OR THE RUN IS NOT EVIDENCE** (2026-09-11, lane RESUME3).
  Windows gives a process **created by a debugger** the DEBUG heap, which
  allocates and validates differently and does not fail-fast. A heap-corrupting
  race that is 3-of-5 bare is then **0-of-3 under gdb**, and two lanes in a row
  wrote that down as a property of the bug. It is a property of the debugger.
  Add one command and the identical run faults in three seconds:

```bash
gdb --batch -ex 'set pagination off' -ex 'set confirm off'     -ex 'set environment _NO_DEBUG_HEAP=1'     -ex run -ex 'info threads' -ex 'thread apply all bt 20' --args ...
#   warning: Critical error detected c0000374
#   Thread 10 "QThread" received signal SIGTRAP, Trace/breakpoint trap.
```

  A clean gdb run without this line is reported as "not measured", never as
  "did not reproduce".
* **gdb and nm live ONLY inside MSYS2 UCRT64.** From the outer Git-Bash
  `which gdb` and `which nm` are both empty, so a floor written as
  `SYMBOLS=$(nm release/NifSkope.exe | wc -l)` prints **0** on an exe carrying
  71,982 symbols and reads as a failed relink. Run the gdb/nm steps through
  `MSYSTEM=UCRT64 /c/msys64/usr/bin/bash -lc`, and print the tool's own path
  beside its answer.
* **POINT IT WHERE THE FAULT IS, measured bare first.** Making the run bigger is
  not the same as making it racier: with the parse lock off the 25-chunk Boston
  region ran CLEAN while the 9-chunk Sanctuary region faulted 3 of 5, because
  Sanctuary's road pass misses the same material on every placement and drives
  far more warnings per second. One five-run bare loop per candidate region
  costs less than three gdb runs at the wrong address.
* **gdb changes the timing** even with the debug heap off. Budget several runs
  and keep the faulting ones; 4 of 14 is a normal yield.
* `thread apply all bt` is the half that matters. The faulting thread says
  where it broke; **the other threads say what it collided with**. In BAKEPERF1
  all nine workers were inside the same parser, which is the whole diagnosis.
* Filter the noise: `grep -vE "^\[(New )?Thread|^\[Switching"`.

## 5. What the answer looks like, and what to do with it

The lane's stack was

```
#0-#4 NifItem::deleteChildItems()   #5 BaseModel::~BaseModel()
#6 lodgenLoadModel(...)             #7 LodgenRoadSet::addPlacement(...)
```

with the rest of the workers in `BaseModel::getItemInternal` /
`NifExpr::partition` / `NifModel::get<>`. That is a whole LAYER that is not
thread-safe, not a bug in one function — and the right answer was to contain it
(a mutex), turn the feature off by default, and say so, not to keep patching.

**A comment claiming thread safety is a claim to test, not evidence.**
`NifItem`'s slab pool says "Thread-safe (mutex) because the XML checker parses
NifModels on worker threads" and the pool is fine; everything around it is not.

**AND THAT ANSWER WAS WRONG** (2026-09-11, lane RESUME3). The layer was not the
fault. With a driver that runs the model layer ALONE on 16 threads
(`NifSkope -no-gui parsestress`, section 7) it did 10,240 loads over 20
consecutive runs with zero digest mismatches and zero faults, while the real
bake's four symbolised faults all sat under the CLI's own message handler
writing through an unlocked shared `QTextStream`. Read the two together:

* the stack names where the damage was **detected**. Two of those four faults
  were in an innocent `QList<Vector3>` reallocation that simply reached the
  corrupt heap first, and a lane that had taken only one of them would have
  accused the parser again;
* **when every worker is inside subsystem X, that is where the TIME goes, not
  necessarily where the bug is.** BAKEPERF1's "all nine workers were inside the
  same parser" was true and was not the diagnosis;
* the discriminator is a driver that contains ONE subsystem. Build it before
  the fix, not after.

## 7. Separate the stage from the pipeline

A fault inside a run where five subsystems are live cannot be attributed by
reading. Build a mode that runs ONE of them and nothing else --
`src/nifparsestress.{h,cpp}` is the worked example: it reads the fixtures once
on the calling thread and then builds, loads, walks and destroys documents on N
threads with no plugin reader, no texture cache, no archive lookup and no file
I/O inside the threaded region.

Its floors are the point, and they are run FIRST:

* `--stress-sabotage digest` flips one byte for one worker and must come back
  red, or the digest is hashing a constant;
* `--stress-sabotage share` puts every worker on ONE document -- the thing the
  layer is not allowed to survive -- and must fault. It segfaults, which is the
  floor under "N threads clean" meaning anything;
* the load count must equal `threads x reps x files`, and a fixture that did not
  load is a named failure, never a skip.

A green run of such a driver REFUTES the accusation. That is worth one build.

## 8. Before you believe the fix

Re-run the SAME stability measurement that produced the crash rate — five runs,
same region, same thread count — and put both numbers in the report (5 of 5
crashed / 5 of 5 clean). A fix that moves the crash later (BAKEPERF1's first
one moved it from 0-3 files in to 11) is progress, not a fix, and must be
described that way.
