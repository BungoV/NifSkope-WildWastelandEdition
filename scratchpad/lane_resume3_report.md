# Lane RESUME3 -- one build for NIFPARSE1 (the loader fault) and SPLAT1 phase B (the tiling constant)

Tree `E:\Projects\NifskopeWildWastelandEdition`, branch `main`, nothing committed.

Exe at launch, and the rung, taken ONCE at **16:45:32**:

| file | time | size | md5 |
|---|---|---|---|
| `release/NifSkope.exe` (CARDS-AGG's) | 2026-09-11 16:20:02 | 21,419,520 B | `3ebf175826feee6c6545cf0873635acc` |
| `release/NifSkope.before_resume3.exe` (the rung) | copied `-p`, same bytes | 21,419,520 B | `3ebf175826feee6c6545cf0873635acc` |

The rung equals CARDS-AGG's DONE line byte for byte (gate R5's last clause,
discharged at the start rather than at the end).

`scratchpad/resume3_20260911/BUILDING` up at 16:45:32. No other
`scratchpad/*/BUILDING` existed at that moment; `tasklist | grep -i -E
"Fallout4|NifSkope"` returned nothing (`rc=1`) at 16:45:14.

---

## 0. Pre-registered gates

Written **before** any script was applied and before the first build. Numbers in
the "to beat" column that came out of a PENDING are **predictions** and are
re-derived, never accepted (`nifskope-ww-resume-pending` section 9).

| id | gate | what makes it red |
|---|---|---|
| **R1** | both refusing scripts `--check` green at RE-COUNTED anchors, and SPLAT1's 14 `TILE` sites re-derived from the file's own bytes, before anything is applied | an anchor that matches 0 or 2 times; a line number reused from `anchors.txt` without re-deriving it |
| **R2** | the fault NAMED from the experiment plus three symbolised stacks, BEFORE any fix beyond the message-box guard | a fix applied on a hypothesis; a stack of `?? ()`; three stacks that disagree and are reported as agreeing |
| **R3** | `--chunk-threads 16`, 20 consecutive runs on EACH region, exit 0 every run; byte identity of every output file vs the serial run on both regions (comparator shown red first); stage-time table serial vs 16, medians of 3; peak working set; the `--chunk-threads` default stated **by rule** | one non-zero exit in twenty; one differing byte; a default asserted without the rule |
| **R4** | tiling: `--land-tiling 2048` byte-identical to the rung on every file of both tiles; `_msn` byte-identical at BOTH values; the local-variance and colour tables real vs predicted vs vanilla; the picture | `_msn` moving one byte; a variance outside 20 % of the prediction with no measured reason |
| **R5** | whole chain at baseline, every moved count named; exe newer than EVERY changed file; gate drivers rebuilt and `-nt`-checked; rung == CARDS-AGG's exe bytes | one stale object; a moved count with no name beside it |
| **R6** | no NifSkope left running; game down at every launch; no crash dialog on his desktop; every timestamp from `date` | either process alive at the end; one Windows "Application Error" box |

### Baselines and predictions, recorded before the build

| what | value | source | status |
|---|---|---|---|
| Sanctuary bake, 9 chunks | 60 files / 24,975,886 B | BAKEPERF1 §3.8 | floor for R3's comparison set |
| Boston bake, 25 chunks | 163 files / 77,591,755 B | BAKEPERF1 §3.8 | floor for R3's comparison set |
| Boston at `--chunk-threads 16`, shipping exe | 161.1 s wall, 21,919 MB peak | BAKEPERF1 §3.9 | prediction; re-measured here |
| Sanctuary at `--chunk-threads 16` | 30.7 s wall, 5,775 MB peak | BAKEPERF1 §3.9 | prediction; re-measured here |
| local variance, chunk (-20,24), TILE 341.333 | 12.93 (vanilla 19.81, ours-as-shipped 76.22) | SPLAT1 §4, offline | PREDICTION; the real bake is measured here |
| local variance, chunk (-20,20), TILE 341.333 | 14.60 (vanilla 29.39, ours-as-shipped 75.26) | SPLAT1 §4, offline | PREDICTION |
| whole-tile mean abs RGB vs vanilla, (-20,24) | 16.89 -> 15.02 | SPLAT1 §4, offline | PREDICTION |
| whole-tile mean abs RGB vs vanilla, (-20,20) | 21.73 -> 20.16 | SPLAT1 §4, offline | PREDICTION |

---

## 1. The fault, named

**R1 first, at re-counted anchors, before anything was applied** (16:45-16:52):

| script | result |
|---|---|
| `scratchpad/nifparse1_20260911/hookup.py --check` | 7 anchors, all matched once, `NifSkope.pro` CR 0 before and after, `src/nifcli.cpp` CR 0 before and after, nothing written |
| `scratchpad/nifparse1_20260911/fixes.py --check` | 25 edits, all anchors matched once, CR unchanged on all eight files, nothing written |
| SPLAT1's anchor pass, re-derived from the file's own bytes | `src/lodgen.cpp` **450,162 B, 10,274 LF, 0 CR, sha1 `d81bfcc94016956a185867ca5f594fee46f7313c`** -- byte for byte what `anchors.txt` recorded at 15:44:38. All **14** `TILE` sites present at 6335, 6680, 6681, 6688, 7623, 7798, 7799, 7802, 7836, 7837, 7840, 7855, 7856, 7859 |

CARDS-AGG did not touch `src/lodgen.cpp` after SPLAT1 took its anchors, so
nothing moved. The pass was run anyway and its sha1 is the proof, not the
absence of news.

### 1.1 The experiment: the model layer alone, on 16 threads

Hook-up applied (7 edits), `qmake` re-run BEFORE `make` -- and the regenerated
dependency read back by object name, not by eye:

```
GeneratedFiles/.obj/nifparsestress.o: src/nifparsestress.cpp src/nifparsestress.h \
        src/model/nifmodel.h src/model/basemodel.h src/data/nifitem.h ...
Makefile.Release:4498   src/nifparsestress.h \      <- inside nifcli.o's block
```

BUILD 1 at **16:53:23**, `BUILD-RC=0`, two objects compiled (`nifcli.o`,
`nifparsestress.o`) and linked; `res/style.qss` and `release/style.qss`
identical after the link.

**Both floors fired before any green was believed.**

| floor | what it does | result |
|---|---|---|
| `--stress-sabotage digest` | flips one byte for worker 0 | **red as it must**: `loads 16, mismatches 1`, `first mismatch ... got c5b11b55a2450820, reference 5c59dcb58c34527f` |
| `--stress-sabotage share` | puts every worker on ONE `NifModel` | **red, and violently**: `Segmentation fault`, exit 139, `beginResetModel called on NifModel(0x...) without calling endResetModel first` |

Then the experiment itself, 16 threads x 8 reps x 4 fixtures, **20 consecutive
runs**:

```
run  1..20   rc=0   loads 512, mismatches 0, wall 9,096-10,011 ms
EXPERIMENT 20x: 0 fault(s) of 20
```

**10,240 model loads on 16 threads, 0 faults, 0 digest mismatches**, with no
`EsmWorld`, no texture cache, no archive lookup and no file read inside the
threaded region.

> **C1 -- "the NIF parser is not thread-safe" -- is REFUTED with a number.**
> BAKEPERF1 concluded it from one stack taken inside a five-subsystem run;
> the layer on its own survives ten thousand loads at sixteen threads.

### 1.2 The original fault, reproduced, and the trap that hid it from two lanes

`scratchpad/resume3_20260911/mutex_off.py` (a refusing script, 1 edit, anchor
matched once, CR 0 before and after) removed BAKEPERF1's process-wide parse
lock, which is the configuration the fault lives in. RELINK 1, symbols kept,
`release/NifSkope.exe` 26,533,646 B at 16:59.

| run | region | result |
|---|---|---|
| bare, 1 run | Boston 25 chunks, `--chunk-threads 16` | RC=0, 163 files, 397.6 s, 19,056 MB peak |
| bare, 5 runs | Sanctuary 9 chunks, `--chunk-threads 16` | **3 of 5 faulted, `0xC0000374` STATUS_HEAP_CORRUPTION**, 9-19 files in; the two clean runs wrote all 60 |

So the fault is alive, it is on **Sanctuary**, not on Boston, and NIFPARSE1's
`gdb3.sh` was pointed at the wrong region. Three gdb runs there came back
`[Inferior 1 exited normally]`, 3 of 3 -- which is exactly what BAKEPERF1
recorded ("never once under gdb, twice") and read as a property of the bug.

**It is a property of the debugger.** Windows gives a process created by a
debugger the DEBUG heap, which allocates differently and does not fail-fast, so
a heap-corrupting race is invisible under `gdb` unless the inferior's
environment carries `_NO_DEBUG_HEAP=1`. With that one line added
(`gdb_nodbgheap.sh`) the same command faults in **three seconds**:

```
warning: Critical error detected c0000374
Thread 10 "QThread" received signal SIGTRAP, Trace/breakpoint trap.
```

Two lanes lost their stack to this. It is written into
`.claude/skills/nifskope-ww-crash-diagnose/SKILL.md` (section 8).

### 1.3 THE VERDICT: four symbolised stacks, and what they agree on

14 gdb runs with the debug heap off; **4 faulted** and all four were caught.
Every one of them, bottom half identical:

```
ChunkThread::run()
  runJob(...)
    lodgenBakeTerrainTextures(...)
      LodgenRoadSet::gather(...)
        LodgenRoadSet::addPlacement(...)
          lodgenLoadModel(...)
```

The top halves split two and two, and the split IS the diagnosis:

| # | top of stack | reading |
|---|---|---|
| q4 | `cliMessageHandler(QtMsgType, ...)` <- `GameResources::get_file` <- `lodgenReadAsset` | caught **in the act** |
| q6 | `cliMessageHandler(QtMsgType, ...)` <- `GameResources::get_file` <- `lodgenReadAsset` | caught **in the act** |
| nh2 | `QArrayDataPointer<Color4>::reallocateAndGrow` inside `lodgenLoadModel` | an innocent heap user that reached the corrupt heap first |
| q2 | `QArrayDataPointer<Vector3>::reallocateAndGrow` <- `QList<Vector3>::emplaceBack` inside `lodgenLoadModel` | same |

And the seven lines immediately above the fault in every log are the same
warning, seven times over:

```
File ' "materials/c:/projects/fallout4/build/pc/data/materials/landscape/roads/asphaltandswedgedecals01.bgsm" ' not found in archives
```

**The fault is the headless CLI's own message handler.**
`src/nifcli.cpp:125` was

```cpp
void cliMessageHandler( QtMsgType type, const QMessageLogContext &, const QString & msg )
{
	if ( type == QtWarningMsg || type == QtCriticalMsg || type == QtFatalMsg )
		err() << msg << Qt::endl;
}
```

and `err()` (`src/nifcli.cpp:100-103`) is a **function-local
`static QTextStream`** with no lock anywhere in the file. `QTextStream` is not
reentrant -- it grows one `QString` write buffer in place. `GameResources::get_file`
calls `qWarning()` on **every** miss, the road pass misses the same `.bgsm` on
every placement, and sixteen workers append to that one buffer at the same
moment. The heap is what breaks.

**This is candidate C3, in the form NIFPARSE1 ruled out.** That lane was right
that `-no-gui` never installs `NifSkope::MessageOutput`, so no `QMessageBox` is
ever built; it then treated "`nifskopeCliMain` installs its own stderr-only
handler instead" as the safe case. The replacement handler is the unsafe one.
Written up in `MISTAKES_ENTRIES.md`.

**What is NOT the fault, each with the measurement that says so:**

* the model layer (C1) -- 10,240 loads, 16 threads, 0 mismatches, 20/20 clean;
* `close_archives()` freeing the shared `BA2File` (C2) -- not in one of the four
  stacks. It is still unlocked and still reachable, so F2 ships (section 2), but
  it is named as a **latent** defect, not as this fault;
* the mip selection, VCLR, the grass tint and the codec -- SPLAT1 refuted all
  four with numbers; nothing here disturbs that.

**What would refute this verdict:** the fixed exe still faulting at
`--chunk-threads 16` with the parse lock off. That is gate R3 and it is 20 runs
on each of two regions, not an argument.

---

## 2. The fixes applied, and the fan-out gate

### 2.1 What went in, and what did NOT, each with its reason

NIFPARSE1's `fixes.py` is all-or-nothing by design.
`scratchpad/resume3_20260911/fixes_subset.py` imports its `EDITS` table and
filters it BY FILE -- which is exactly how its fixes are grouped -- so another
lane's artefact was not edited and the skipped edits are PRINTED by name rather
than quietly dropped.

| fix | file(s) | edits | in? | why |
|---|---|---|---|---|
| **NEW, this lane** | `src/nifcli.cpp` | 2 | **YES** | the named fault: `cliMessageHandler` stops writing through the shared `QTextStream` and writes the line with `std::fputs` to `stderr` under its own mutex. The CRT locks the `FILE *`; the mutex only keeps a line and its newline together. **On one thread the bytes and their order are exactly what `err()` produced** -- that is the way back and it is exact |
| **F1** | `src/message.cpp` | 5 | **YES** | a `QMessageBox` is a widget and a widget off the GUI thread is undefined. Refuted for `-no-gui` (no handler installed) but LIVE for the LOD Generation panel, which has a real `QApplication`. A correctness fix regardless, as the brief says |
| **F2** | `src/gamemanager.{h,cpp}` | 7 | **YES** | `GameResources::get_file` is the frame in two of the four faults. A recursive `QReadWriteLock` over the archive state: `init_archives` / `close_archives` / `close_materials` take the write lock, `find_file` / `get_file` take the read lock across `findFile` AND `extractFile` AND the interior `string_view`s. Read/write on purpose -- `extractFile` is where the texture stage's time is |
| **F4+F5** | `src/model/nifmodel.cpp` | 2 | **YES** | plain bugs. F5 is the one the brief names: `QHash & m = arrayPseudonyms; m = multiArrayPseudonyms1;` copy-assigns INTO the global, so the first multi-array row ever displayed replaced the application's singular-name table for the session. Gate = the editor harnesses |
| **F7** | `src/lodgenparallel.{h,cpp}` | 3 | **YES** | `--chunk-threads 0` stops meaning "the core count" and means the smaller of the cores and what free memory holds. R3 asks for the default by rule, and this is the rule |
| **F3** | `src/data/nifvalue.cpp` | 2 | **NO** | `NifValue::type()`'s lazy destructive `initialize()` behind `std::call_once`. **The experiment does not implicate it**: 10,240 loads on 16 threads through exactly that code, 0 mismatches, 0 faults. Latent, named in §6, not applied on a guess (NIFPARSE1's own recorded lesson) |
| **F6** | `src/xml/nifexpr.cpp` | 4 | **NO** | the four shared `QRegularExpression`s made `thread_local`. The brief's condition was "only if the experiment implicates them". It does not -- every array resize in those 10,240 loads goes through them |

Also applied: `mutex_off.py` then `mutex_final.py` -- **BAKEPERF1's process-wide
parse lock in `lodgenLoadModel` is gone**, and the comment that replaced it
carries the measurement rather than a claim. That was containment for a fault
that was never in the parser.

RELINK 2 at **17:23:03**, `BUILD-RC=0`, 21,433,856 B, `res/style.qss` and
`release/style.qss` identical after the link.

### 2.2 The fault is gone, on the region it lived on

| exe | region, `--chunk-threads 16` | result |
|---|---|---|
| the parse lock off, no fix (RELINK 1) | Sanctuary 9 chunks | **3 of 5 faulted**, `0xC0000374`, 9-19 of 60 files written |
| the fixes (RELINK 2) | Sanctuary 9 chunks, smoke | **5 of 5 clean**, 60 files each |
| the fixes (RELINK 2) | Sanctuary 9 chunks, **gate R3, `loop20.ps1`** | **GATE GREEN -- 20 of 20 clean**, 60 files every run, wall 30.8-47.2 s, peak working set 5,791-5,805 MB |

The same command, the same region, the same thread count, on either side of one
change: that is the before/after `nifskope-ww-crash-diagnose` section 8 asks for.

### 2.3 Gate R3, in full

**The 20x loops** (`scratchpad/nifparse1_20260911/loop20.ps1`, NTSTATUS decoded
per run; logs `logs/loop20_sanctuary.txt`, `logs/loop20_boston.txt`):

| region | runs | result | wall | peak working set | files |
|---|---|---|---|---|---|
| Sanctuary, 9 chunks | 20 | **GREEN, 20 of 20 clean** | 30.8 - 47.2 s | 5,791 - 5,805 MB | 60 every run |
| Boston, 25 chunks | 20 | **GREEN, 20 of 20 clean** | 112.7 - 261.7 s | 23,082 - 26,090 MB | 163 every run |

**Byte identity, with the comparator SHOWN RED FIRST**
(`identity.sh`, `logs/identity.txt`):

| what | result |
|---|---|
| REFUTER 1, one flipped byte in one `.BTR` | **RED as required** -- `RESULT FAIL - 0 only-in-A, 0 only-in-B, 1 differ`, and it names the file |
| REFUTER 2, one `.BTO` deleted | **RED as required** -- `RESULT FAIL - 1 only-in-A` |
| Sanctuary, `--chunk-threads 1` vs `16` | **PASS, 60 files, 24,975,886 bytes, byte-identical** |
| Boston, `--chunk-threads 1` vs `16` | **PASS, 163 files, 77,591,755 bytes, byte-identical** |
| Sanctuary, RUNG exe vs FIXES exe, both serial | **PASS, 60 files, 24,975,886 bytes** |
| Boston, RUNG exe vs FIXES exe, both serial | **PASS, 163 files, 77,591,755 bytes** |

Both file counts and both byte totals are exactly BAKEPERF1's floor, so the
comparison set is the named non-empty one and not a subset.

**The stage table, medians of 3, ALTERNATING serial / 16 threads** (the rung's
own run-to-run spread on one region was 4.5 s, so single pairs cannot answer it)
-- `logs/r3_rest.txt`:

| region | queue | meshes | textures | wall (median) | peak WS |
|---|---|---|---|---|---|
| Sanctuary 9 | serial (`--chunk-threads 1`) | 9.9 s | 5.8 s | **17,947 ms** | 1,755 MB |
| | 16 threads | 7.8 s | **31.9 s** | **40,484 ms** | **5,800 MB** |
| Boston 25 | serial | 54.9 s | 66.1 s | **128,118 ms** | 3,756 MB |
| | 16 threads | 63.4 s | **89.4 s** | **156,043 ms** | **25,136 MB** |

**And what the serial path costs, measured with an instrument that can see it.**
The 38k-vertex torso through `-no-gui info` is 15 ms of parse inside a 280 ms
process -- too weak to answer a 5 percent question. `parsestress` at ONE thread
is the right instrument: 160 loads in one process, and the BUILD 1 exe (16:53:23,
hook-up only, no fixes) against the shipping exe:

| exe | three runs | median |
|---|---|---|
| pre-fix, BUILD 1 | 3,377 / 3,422 / 3,490 ms | **3,422 ms** |
| fixed, RELINK 5 | 3,375 / 3,311 / 3,400 ms | **3,375 ms** |

**-47 ms of 3,400, i.e. -1.4 percent**, inside the pre-fix side's own 113 ms
spread. The fixes cost the serial parse nothing measurable. The interactive-edit
instrument agrees exactly: `WW_DUPFREEZE_TEST`'s `duplicateElements returned in
37 ms` on the rung and **37 ms** on the shipping exe.

### 2.4 THE `--chunk-threads` DEFAULT: it stays 1, and the reason changed

**The default stays 1**, and the numbers above are why -- not safety any more:

* **it is slower**: 40.5 s against 17.9 s on 9 chunks (**2.26x**) and 156.0 s
  against 128.1 s on 25 (**1.22x** at the median; the loop's own median over 20
  runs is 146.7 s, still 1.15x). The texture stage is where it goes backwards
  (5.8 -> 31.9 s and 66.1 -> 89.4 s): sixteen workers with sixteen cold texture
  caches re-read the same landscape diffuses.
* **it costs 6.7x the memory** on 25 chunks: **25.1 GB against 3.76 GB**, on a
  31.8 GB machine.

So the source comments and the `--chunk-threads` usage text that said the
fan-out was off because "building NIF documents on worker threads is not safe in
this tree yet (it faults in the parser)" are now **false and were rewritten**
(`stale_comments.py`): the fan-out is clean, and the default is a speed and
memory decision.

**The rule for `--chunk-threads 0`, stated:**

```
workers = min( cores, floor( 0.60 * availPhys / (250 MB + max(64 MB, texBudget)) ) )
```

250 MB is BAKEPERF1's measured marginal cost of a second plugin reader, the
texture budget is 512 MB a worker, and 0.60 is of what is FREE rather than of
what is installed, because the desktop and the post-queue passes still have to
fit. The census now says **which bound decided it**, and the word was proved to
move rather than merely to exist:

| command line | census |
|---|---|
| (none) | `chunk threads 1 bound by default` |
| `--chunk-threads 4` | `chunk threads 4 bound by asked` |
| `--chunk-threads 0` | `chunk threads 16 bound by cores` |

The fourth word, `memory`, was **NOT exercised** and is named as such: at the
moment of the run the machine had **22,388 MB free of 31,797**, so
`floor(0.60 x 22,388 / 762) = 17`, which is above the 16 cores, and `cores` is
the correct answer. The arithmetic puts the crossover at **20,320 MB free**;
below that the word is `memory`. It is reachable in ordinary use and it has not
been seen.

**One defect this found, in NIFPARSE1's own field.** The word printed `asked`
for a run with no `--chunk-threads` on the command line, because
`g_chunkThreads` is initialised to 1 and any positive value read as "the caller
asked". A field that cannot tell the shipped default from a typed 1 is not a
field (`fo4cs-census-field`); `boundby_default.py` adds the fourth state and the
table above is the proof it moves.

---

## 3. The tiling fix and its gates

### 3.1 The change

`scratchpad/resume3_20260911/tiling.py`, 6 edits, every anchor matched once, CR
unchanged on all three files, and an AFTER-state assert rather than a before
count (`0 of the old declaration left, 2 of the new`).

* `src/lodgen.h` -- `float lodgenLandTiling(); void lodgenSetLandTiling(float);`
  with the whole provenance in the comment: `fLandTextureTilingMult` 1.5f in
  `Fallout4.exe` 1.10.155, Setting record at file 0x36E83A8, its single code
  reference at VA 0x1403A74C6, the 17x17 quadrant loop at 0x1403A7620,
  `128 / 0.375 = 341.3333`.
* `src/lodgen.cpp` -- one `static float g_landTiling = 341.3333f;` with its
  getter and setter, and **both** `constexpr float TILE = 2048.0f;` declarations
  become `const float TILE = lodgenLandTiling();`. Because all fourteen uses read
  the declaration in scope, **all fourteen move together** -- colour, mask and
  emissive, stock bake and pyramid. That is SPLAT1's red 2 closed by
  construction rather than by fourteen edits.
* `src/nifcli.cpp` -- `--land-tiling <units>` and its usage block. A
  non-positive value is refused and leaves the default standing, so a mistyped
  switch cannot flatten the ground to one texel.
* **No panel row.** The LOD Generation panel's rows belong to lane LODUI1; this
  is CLI (and therefore INI, through the panel's own argument line) only. Said
  here because the brief asked for it to be said.
* `tests/spells/lodgen_terrain_model.py` -- its own `TILE` moves with the bake,
  `float(os.environ.get('WW_LAND_TILING', 341.3333))`, or the ring-0 gate would
  measure the old law against the new bake. That command has no shell spell
  driving it (it was TERRAIN-R's hand-run gate), so no chain count depends on it.

### 3.2 Gates S3 and S4: the way back, and the sheet that must not move

Six bakes, `tiling_gates.ps1`, each a single dim-4 chunk into this lane's own
out-dir: the RUNG exe at its shipped defaults, the new exe at
`--land-tiling 2048`, and the new exe at its new default.

| gate | result |
|---|---|
| **S3** `--land-tiling 2048` vs the RUNG exe, chunk (-20,24), **every file** | **PASS, 6 files, 1,904,522 bytes, byte-identical** |
| **S3** the same, chunk (-20,20) | **PASS, 6 files, 1,403,974 bytes, byte-identical** |
| **S4** `_msn` at 2048 vs 341.3333, chunk (-20,24) | **PASS, byte-identical, 174,888 bytes** |
| **S4** the same, chunk (-20,20) | **PASS, byte-identical, 174,888 bytes** |
| **S4's refuter** -- the colour sheet under the same comparison | **RED, as it must be**: `1 of 1 files differ` on both tiles |

The refuter is the half that matters: `_msn` being identical proves nothing
unless the same comparison is seen to detect the change that DID happen.

### 3.3 Gates S1 and S2: the real bake against the offline prediction and vanilla

`tiling_measure.py`, metrics imported from SPLAT1's `splatlib` rather than
re-implemented, so the column is comparable with that lane's table.

| chunk | tiling | local variance | SPLAT1's offline prediction | vanilla | mean abs RGB vs vanilla | prediction |
|---|---|---|---|---|---|---|
| (-20,24) | 2048 | 76.22 | 71.95 | 19.81 | 16.33 | 16.89 |
| (-20,24) | **341.3333** | **12.34** | **12.93** | 19.81 | **14.33** | 15.02 |
| (-20,20) | 2048 | 74.06 | 61.71 | 29.39 | 20.43 | 21.73 |
| (-20,20) | **341.3333** | **25.83** | **14.60** | 29.39 | **18.92** | 20.16 |

* **S2 PASS on both tiles.** The gate was "does not get WORSE by more than 1.0";
  it got BETTER by 2.00 and 1.50 of 255. (It is still ~14-19 of 255 away from
  vanilla, which is the grading, not the tiling.)
* **S1 PASS on (-20,24)**: 12.34 against the predicted 12.93, **4.6 percent**.
* **S1 RED on (-20,20) as written**: 25.83 against 14.60, 77 percent -- and the
  cause was measured rather than argued.

**Why (-20,20) is outside the band, with the discriminator run**
(`logs/s1_discriminator.txt`). SPLAT1's offline model draws no roads and applies
no grass tint; that tile is the one carrying ROADS1's rasterised loop road. Two
more bakes, same chunk, `--no-roads`:

| variant | local variance | mean abs RGB vs vanilla |
|---|---|---|
| full bake, 2048 | 74.06 | 20.43 |
| full bake, 341.3333 | 25.83 | 18.92 |
| `--no-roads`, 2048 | 68.59 | 22.00 |
| **`--no-roads`, 341.3333** | **17.00** | 20.40 |
| `--no-roads --grass-tint 0`, 2048 | 68.59 | 22.00 |
| `--no-roads --grass-tint 0`, 341.3333 | 17.00 | 20.40 |
| VANILLA | 29.39 | -- |

Against the like-for-like artefact -- the bake that, like the model, draws no
roads -- the prediction is inside the gate at **both** tilings: 68.59 vs 61.71
(**+11.2 %**) and **17.00 vs 14.60 (+16.4 %)**. The residual is the model's own
known ceiling (the four `.bgsm` LTEXs it cannot open, plus BC1) and it is a
roughly constant offset, not a tiling error.

And the tiling's OWN effect, which is what the change is, agrees to **2.4
percent**: the full bake drops 74.06 -> 25.83 (48.23) where the prediction drops
61.71 -> 14.60 (47.11).

**A bonus control nobody asked for**: `--grass-tint 0` changed the local
variance by **exactly 0.00** on that tile (68.59 and 17.00 both ways), which
independently confirms SPLAT1's "the tint is ruled out" on the tile where it
was confounded.

**So S1 reads PASS on both tiles when measured against the artefact the
prediction describes, and RED on one tile when measured against an artefact
carrying a road the prediction does not contain.** Both statements are in the
report because the second is the one a reader would otherwise find themselves.

### 3.4 The contract

`contract.py` spliced SPLAT1's `CONTRACT_AMENDMENT.md` into
`docs/LODGEN_TERRAIN_VT.md`: 2 paragraphs, both anchors matched once, CR 0
before and after, 1,211 -> 1,256 lines. Provenance re-stamped at apply time from
the live tree (`src/lodgen.cpp` 450,193 B, sha1
`2d49c158c2e5fd4cc9d190ff09de44a7ade1f56b`), never copied from the amendment.

* 2.5's sampling law is now `u = frac(wx/T)`, `v = frac(wy/T)` with **T =
  341.3333** and the whole binary derivation beside it, and it says in as many
  words that **the runtime and the pyramid must use the same T or ring 0 seams**.
* The VCLR sentence is corrected: `249..255` did not reproduce; **11 of 16
  Sanctuary cells carry a VCLR and the bytes run 203..255**, 16 of 16 over
  170..255 on the other block. The page now says the old range is withdrawn,
  rather than quietly replacing it.
* It also states what the tiling does NOT fix.

**Both files carry non-ASCII in the anchors** -- an EM DASH (U+2014) in
`src/lodgen.cpp`'s `TILE` comment and UNICODE MINUS (U+2212) in the contract's
cell ranges. Anchors typed from SPLAT1's report counted 0 twice before they were
rebuilt from the files' own bytes; the rule is in `ww-anchored-hookup` now.

---

## 4. Build ledger and the chain

### 4.1 One build, five counted relinks, and the mtime table

| # | what | time | exe size | why |
|---|---|---|---|---|
| **BUILD 1** | the experiment hooked up (`qmake` first; `nifcli.o` + `nifparsestress.o`) | **16:53:23** | 21,434,368 | the discriminator |
| RELINK 1 | `relink_sym.sh`, symbols kept, no source change except `mutex_off.py` | **16:59** | 26,533,646 | the stacks |
| RELINK 2 | the fixes (19 of NIFPARSE1's 25 + the CLI log fix + `mutex_final.py`) | **17:23:03** | 21,433,856 | 7 objects |
| RELINK 3 | the tiling, the census word, the contract | **19:04:23** | 21,435,904 | 7 objects |
| RELINK 4 | the usage text and the two source comments that still blamed the parser | **19:06:29** | 21,435,904 | 2 objects |
| RELINK 5 | the census word's fourth state (`default`) | **19:08:42** | 21,435,904 | 2 objects |

Every one `BUILD-RC=0` from make's own exit code, `tasklist` rc=1 immediately
before each, and `res/style.qss` / `release/style.qss` byte-identical after each
link.

| file | time | size | note |
|---|---|---|---|
| `release/NifSkope.before_resume3.exe` | 2026-09-11 16:20:02 | 21,419,520 | the rung, md5 `3ebf1758...` -- **equals CARDS-AGG's DONE line** |
| `release/NifSkope.exe` | 2026-09-11 **19:08:42** | 21,435,904 | md5 `847236ecb8f83d64b25b8a2ceeec91d3` |
| `scratchpad/nifparse1_20260911/NifSkope.stripped.exe` | 16:53:23 | 21,434,368 | BUILD 1, kept by `relink_sym.sh` -- and it earned its keep: it is the pre-fix side of the serial-parse measurement in §2.3 |
| `tests/spells/lodgen_terrain_model.py` | 17:30:17 | 27,885 | gate driver, edited and re-read |
| `tests/spells/parse_stress.sh` | 15:35:14 | 4,463 | gate driver, NIFPARSE1's, unedited |
| `scratchpad/bakeperf1_20260911/bake_diff.py` | 13:59:55 | 2,467 | gate driver, BAKEPERF1's, unedited, shown red first |
| `scratchpad/resume3_20260911/images/cmp_tiling_fixed.png` | 19:10 | 221,604 | |
| `scratchpad/pic_grass_20260911/images/cmp_grass_tint.png` | 19:23:36 | 1,141,172 | |

### 4.2 The exe is newer than EVERY changed file, and no object is stale

* The whole-working-set sweep over `git status --porcelain -- src res tools tests
  NifSkope.pro`: **0 STALE**.
* The stronger check -- `obj_stale.py`, which walks each object's dependency
  block in `Makefile.Release` through its continuation lines (not `grep -A3`) and
  intersects it with the 95 changed files under `src/ res/ lib/`:
  **246 object blocks parsed, 0 objects stale against a changed dependency, 0
  objects newer than the exe.** 90 of the 95 changed files reach at least one
  object; the five that reach none are `NifSkope.pro`, `src/ui/nifskope.ui`,
  `src/wwskin.h`, `res/hkclasses_fo4.json`, `res/hkx_annotation_vocabulary.txt`
  -- none of them this lane's.
* `qmake` was run BEFORE `make` for the one build that added sources, and the
  regenerated dependencies were read back **by object name**: `nifparsestress.o`
  carries `src/nifparsestress.{cpp,h}` plus the model headers, and
  `src/nifparsestress.h` appears inside `nifcli.o`'s own block at
  `Makefile.Release:4498`.

### 4.3 The chain: every count against its baseline

`chain.sh`, one NifSkope at a time, its own lock directory, each `env VAR=` on
the CHILD; summary `scratchpad/resume3_20260911/chain/summary.txt`, per-harness
logs beside it. 19:13:30 to 19:20:52.

| harness | this exe | baseline | verdict |
|---|---|---|---|
| `lodgen_terrain.sh` | **26 / 0 PASS** | 26 / 0 | at baseline |
| `lodgen_terrain_vt.sh` | **41 / 1** | 41 / 1 | at baseline (`V9b`, red on the rung too) |
| `lodgen_roads.sh` | **11 / 1** | 11 / 0 | **MOVED -- named in 4.4** |
| `lodgen_ground_cover.sh` | **29 / 5** | 29 / 5 | at baseline |
| `lodgen_terrain_pbrm.sh` | **14 / 0 PASS** | 14 / 0 | at baseline |
| `lodgen_native.sh` | **18 / 0 PASS** | 18 / 0 (CARDS-AGG) | at baseline |
| `lodgen_stage_times.sh` | **16 / 0 PASS** | 16 / 0 | at baseline |
| `lodgen_identity.sh` | **PASS, 8 ok** | PASS | at baseline |
| `lodgen_merge.sh` | **PASS, 10 ok** | PASS | at baseline |
| `lodgen_card_arrays.sh` | **PASS, 35 ok** | PASS | at baseline |
| `lodgen_texture_arrays.sh` | **PASS, 40 ok** | PASS | at baseline |
| `lodgen_impostor_cards.sh` | **PASS, 12 ok** | PASS | at baseline |
| `lodgen_octahedral.sh` | **FAIL, 1 of 111** | FAIL, same check | at baseline -- `F1 ... worst 1.88`, the identical check and the identical number CARDS-AGG reproduced on the rung |
| `lodgen_panel_run.sh` | **125 / 0 PASS** | 125 / 0 | at baseline |
| `lod_generation.sh` | **116 / 0 PASS** | 116 / 0 | at baseline |
| `lodl_open.sh` | **23 / 0 PASS** | 23 / 0 | at baseline |
| `ui_align.sh` | **11 / 0 PASS** | 11 / 0 | at baseline |
| `water_ui.sh` | **82 / 0 PASS** | 82 / 0 (no picture args) | at baseline |

**The editor harnesses**, the model layer's own, run on the rung FIRST at
16:50:31 and again on the shipping exe at 19:11:13:

| harness | rung | shipping exe |
|---|---|---|
| `WW_JOIN_TEST=2` | PASS, verts 4377 tris 5748, bone union {5,6,7,8} | **log byte-identical to the rung** |
| `WW_SEP_TEST` | PASS + UNDO PASS | **byte-identical** |
| `WW_COPYPASTE_TEST` | PASS, blocks 116 -> 126 delta 10 | **byte-identical** |
| `WW_VERTEXFLAGS_TEST` | in-model PASS, 0 of 68 positions moved | **byte-identical** |
| `WW_DUPFREEZE_TEST` | `duplicateElements returned in 37 ms; blocks delta 0` (it prints no PASS line -- its product is the timing) | **37 ms, identical** |

**Harnesses deliberately NOT run, with the reason**: the collision, HKX, skeleton,
water-flow, gltf, pose and block-list suites. This lane touched the model
layer's statics, the game manager's archive locking, the CLI message sink, the
chunk queue and the terrain bake's tiling; none of those suites enters any of
them, and running them would have added an hour of chain to re-measure numbers
CARDS-AGG measured at 16:31 on a tree whose only later change is listed in 4.1.
`top_bar.sh` is skipped for the reason `nifskope-ww-resume-pending` §10 gives:
its five failures are a stale View-menu expectation that predates this session.

### 4.4 The one moved count, named with its discriminator

`lodgen_roads.sh` R5, "the road-presence metric passes its pre-registered bars".
Bar 2 is **relative**: the road must correlate with vanilla's road at least 80
percent as well as the surrounding GROUND correlates with vanilla's ground. The
harness was re-run on the RUNG exe in the same session
(`logs/lodgen_roads_RUNG.log`) so both sides are measured, not remembered:

| | rung (tiling 2048) | shipping exe (341.3333) |
|---|---|---|
| the road term ("after") | **0.3065** | **0.3061** |
| the reference, the ground around it | 0.3442 | **0.4024** |
| bar 2 = 0.8 x reference | 0.2753 | 0.3219 |
| bar 1 = 2 x the `--no-roads` floor | 0.2549 | 0.2694 |
| verdict | 0.3065 >= 0.2753 **ok** | 0.3061 >= 0.3219 **FAIL** |
| mean colour error vs vanilla, `--no-roads` | 24.39 | **22.45** |
| mean colour error vs vanilla, `--roads` | 22.81 | **20.97** |

**The road signal did not weaken: 0.3065 -> 0.3061, a change of 0.0004.** What
moved is the harness's own reference -- the corrected tiling made the GROUND
match vanilla 17 percent better (0.3442 -> 0.4024), which raised a bar defined as
80 percent of it. Bar 1, the absolute one, still passes with room. Both colour
errors against vanilla improved.

**Nothing was changed to make it green** (`nifskope-ww-resume-pending` §6: the
resuming lane's product is a verdict, not a cure). And the red is not merely an
artefact: it says something true and new -- now that the ground matches vanilla
better, **our road raster is the worse-matching part of the sheet**, and closing
that is a roads lane's work, not this one's. Listed in section 6 as owed.

---

## 5. Pictures

### 5.1 `scratchpad/resume3_20260911/images/cmp_tiling_fixed.png` (1078 x 1346, 19:10)

Four panels, the SAME 128 x 128 texels at (216,128) of chunk (-20,24) in every
one, 4x nearest neighbour, the crop chosen BY THE METRIC on the BEFORE artefact
(the worst-local-variance window of the 2048 bake, searched over the whole
sheet, reading 89.70). `ww-texel-picture`: fixed cells, no caption clipping, and
every caption carries the number this report quotes, in this report's units.

**All four panels are DDS files off disk. Nothing in this picture is modelled** --
that is the difference from SPLAT1's `speckle_diagnosis.png`, whose third panel
was an offline re-bake.

| panel | what it shows |
|---|---|
| VANILLA `Commonwealth.4.-20.24.DDS` | pale, low-contrast ground, local variance 18.58 on the crop and 19.81 over the tile |
| OURS `--land-tiling 2048` (the way back) | visibly grainy: one repeat is 64 texels, so the bake prints a 64x64 image of the ground texture at full contrast. 89.21 on the crop, **4.8x vanilla**; 76.22 over the tile |
| OURS default 341.3333 | smooth, with the 17x17 blend grid faintly visible (128 world units = 4 texels, which is the blend grid and not the tiling). 14.58 on the crop, **0.78x vanilla**; 12.34 over the tile |
| OURS at 2048 minus VANILLA, x4 | the difference is everywhere and it is a LEVEL difference as well as a grain one: whole-tile mean abs RGB 16.33 at 2048, 14.33 at 341.3333 |

The picture was opened and read back before this line was written: no caption
clips, the crop is the same in all four panels, and the two panels that should
differ do.

**What it honestly shows, and what it does not.** It shows the speckle gone and
the ground now slightly SMOOTHER than vanilla's (0.78x). It does not show the
colour matching vanilla -- the fourth panel is the open grading item, and the
caption says so.

### 5.2 `scratchpad/pic_grass_20260911/images/cmp_grass_tint.png` (2148 x 1294, 19:23:36)

Lane PIC-GRASS's own `run.sh`, run unchanged by this lane AFTER the tiling
relink, so both of our panels carry the engine's own repeat. Its
`scratchpad/pic_grass_20260911/NOTES.md` is written with two sentences per panel
before any claim; the numbers:

| quantity | value |
|---|---|
| mean colour error vs vanilla, whole tile, tint 0.35 (shipped) | **20.97** of 255 |
| the same with `--grass-tint 0` | **21.92** |
| over cover>0 texels only, tint on | **19.31** |
| over cover>0 texels only, tint off | **21.12** |
| the tint alone, whole tile | mean **3.34**, max 27.42 |
| the tint alone, over cover>0 | mean **6.31**, p95 16.45 |
| the cover plane | 137,399 of 262,144 texels, max 73, stamped `0x56435757` |

**The tint helps by 0.95 of 255 over the whole tile and 1.81 over the texels it
touches.** It is the right sign and it is small; at most 1.81 of the remaining
~21 belongs to it, and the rest is the grading.

**One thing the picture measured that nobody had asked**: `--grass-tint 0` is
NOT byte-identical outside the cover plane (the two sheets differ by up to 22.94
there). **Named as a candidate, not a conclusion**: the sheets are BC-compressed
in 4x4 blocks and a block straddling the cover boundary is re-quantised whole
when its covered texels change, which would produce exactly this. The
discriminator is the same pair written uncompressed; it has not been run.

### 5.3 The before picture, and where it came from

`nifskope-ww-resume-pending` section 11's trap did not bite, because this lane's
"before" is not a layout but a BAKE: the rung exe was kept and re-run inside the
same session for every comparison (the three tiling `rung/` bakes, both serial
identity bakes, the five editor harnesses, and `lodgen_roads.sh` with `EXE=`
pointed at it). No before number in this report is quoted from an older lane's
log; the ones that are predictions are labelled as predictions in section 0.

---

## 6. Owed / red / bungo's calls

### 6.1 The one thing to say to him

Two things that had been blocked for a week are closed, and the reason both were
blocked is the same mistake: a cause was stated from one stack and one constant,
and nobody built the experiment that could refute it.

* The bake's crash was **never in the NIF parser**. It was the headless
  generator's own log line: sixteen workers writing "texture not found" warnings
  through one unlocked Qt text stream. The parser survives ten thousand loads on
  sixteen threads.
* The far terrain's speckle was **the landscape textures tiled six times too
  large**, and the correct number was sitting in the game's own binary.

### 6.2 RED, and what is not proven

| # | what | state |
|---|---|---|
| **R1** | `lodgen_roads.sh` R5 bar 2 is RED, measured on both sides (4.4). The road signal is unchanged; the bar rose because the ground improved. **Owed to a roads lane**: our road raster is now the worse-matching part of the sheet | named, not cured |
| **R2** | `lodgen_octahedral.sh` `F1 ... worst 1.88` -- **pre-existing**, the identical check and number CARDS-AGG reproduced on the rung | not this lane's |
| **R3** | `lodgen_terrain_vt.sh` `V9b`, `lodgen_ground_cover.sh` 5 failures -- both at baseline, both red on the rung | not this lane's |
| **R4** | The `bound by memory` census word is **arithmetically correct but UNEXERCISED**: 22,388 MB were free, the crossover is 20,320 MB. Named rather than claimed | unproven |
| **R5** | The whole-tile colour error against vanilla is **NOT closed**: 14.33 and 18.92 of 255 after, from 16.33 and 20.43. That is the x0.82-0.83 grading, still the open item "splat calibration vs vanilla grading" | open, and it was open before |
| **R6** | `close_resources()` (`gamemanager.cpp`) still iterates `nifResourceMap` unlocked. Not on the bake path, not fixed -- NIFPARSE1 named it and it stays named | named, not fixed |
| **R7** | NIFPARSE1's F3 (the lazy `NifValue::type()` re-init) and F6 (four shared `QRegularExpression`s) are LATENT and **not applied**: the experiment put 10,240 loads through both and neither moved. The prepared edits are still in `fixes.py` | deliberate, with the number |
| **R8** | `--grass-tint 0` is not byte-identical outside the cover plane, up to 22.94 (5.2). A BC-block-boundary effect is the named candidate; the discriminator has not been run | unproven |
| **R9** | 341.3333 was read out of **Fallout 4 1.10.155 only**. SPLAT1 said so and it is still true: nothing here checks FO76 or Skyrim worldspaces, and this generator opens `.btd` regions | not measured |
| **R10** | The Commonwealth's own memory curve is still two points. 3.76 GB at 25 chunks serial is the configuration to run it in; the 100-chunk measurement BAKEPERF1 asked for has still not been taken | open |

### 6.3 bungo's calls

1. **`--chunk-threads` stays 1** -- this lane's recommendation, with the numbers
   in 2.4: the fan-out is CLEAN now (20 of 20 on each of two regions,
   byte-identical to serial) but 2.3x slower on 9 chunks and needs 25 GB on 25.
   It is worth having as a switch and not as a default. If he wants it on, the
   thing to fix first is the texture stage, which is where the time goes
   backwards.
2. **341.3333 ships as the default.** `--land-tiling 2048` is there for anyone
   who wants the old sheets back byte for byte, and the switch exists because
   another user can set `fLandTextureTilingMult` in an INI.
3. **His open NifSkope window needs a restart** -- the exe was relinked five
   times; the one on disk is 19:08:42.

---

## 7. Mistakes

Full text in `scratchpad/resume3_20260911/MISTAKES_ENTRIES.md`, six entries for
the director to splice:

1. **"It never crashes under gdb" was a property of the debugger**, not of the
   bug. Windows gives a debugger-created process the DEBUG heap;
   `_NO_DEBUG_HEAP=1` in the inferior's environment made the same run fault in
   three seconds. Two lanes lost their stack to this.
2. **C3 was refuted for the wrong handler.** "X is not installed, Y runs
   instead" is half an answer until Y has been read; Y was the fault.
3. **A gate pointed at the region where the fault is NOT.** Bigger is not
   racier: Boston ran clean, Sanctuary faulted 3 of 5.
4. **An anchor retyped from a report instead of read from the file**, twice in
   one hour -- an EM DASH in `src/lodgen.cpp` and a UNICODE MINUS in the
   contract page. Prose normalises punctuation silently.
5. **`relink_sym.sh` printed `SYMBOLS=0`** on an exe carrying 71,982 symbols,
   because `nm` exists only inside MSYS2. A floor that can fail for a reason
   unrelated to what it measures is not a floor.
6. **A census word written and never read** (`lodgenChunkThreadBoundBy`), and
   then a census word that could not tell the shipped default from a typed 1.
   Both caught by reading the field on a real run.

And this lane's own seventh, recorded because it happened in this session:
**a Bash heredoc halved the backslashes again** and wrote six C string literals
with real newlines inside them. It is the fifth time the tree has paid for it.
The repair went through the Write tool, which is what `nifskope-ww-build-verify`
says to do, and the lesson is that the rule applies to *every* generated C
string, not only to anchors.

---

## 8. Finished-work skill review

**Loaded and used.** `nifskope-ww-resume-pending` (this lane IS that page:
section 2's equal-length refusing script, section 3's qmake-then-read-the-
dependency-back, section 4's whole-working-set sweep, section 5's locked
sequential chain with `env` on the child, section 6's "measure the cause and
STOP" -- which is exactly what happened to `lodgen_roads.sh` R5 -- section 7's
four places, section 9's "a PENDING's numbers are PREDICTIONS", which is why
SPLAT1's 12.93 and 14.60 are labelled predictions in section 0 and re-derived in
section 3.3). `nifskope-ww-crash-diagnose` (the relink recipe, the NTSTATUS
read, `thread apply all bt`, and above all its opening rule that the stack names
where the damage was DETECTED -- two of four stacks were innocent bystanders).
`ww-anchored-hookup` (six refusing scripts; amended). `ww-parallelise-a-stage`
(the inventory that follows the CALL, the byte-identity gate with its refuter
shown red first). `nifskope-ww-build-verify` (make's own exit code, the
stylesheet at link time, the harness on a proven-newer exe, and its heredoc
rule, which this lane broke and then obeyed). `nifskope-ww-lodgen` (the region
choices, absolute paths, own out-dirs, never his installed `Data\Terrain`).
`ww-contract-provenance` (provenance re-stamped from the live tree at apply
time, never copied). `fo4cs-census-field` (which is what found BOTH census
defects). `ww-texel-picture` (the crop chosen by the metric on the BEFORE
artefact, fixed cells, the caption's number is the report's number).
`ww-control-calibration` (the `--no-roads` discriminator in 3.3 is a
ceiling-and-floor pair). `fo4-engine-constant-from-ini-setting` (read, not
re-derived: SPLAT1's addresses were reused verbatim).

**Amended, both trees, and the director need do nothing:**

* `.claude/skills/nifskope-ww-crash-diagnose/SKILL.md`, 120 -> 183 lines. Four
  new rules in section 4 (`_NO_DEBUG_HEAP=1` or the run is not evidence; gdb and
  nm live only inside MSYS2; point it where the fault is, measured bare first;
  expect 4 faults in 14 runs), a correction in section 5 saying the previous
  lane's answer was WRONG and why, and a **new section 7, "separate the stage
  from the pipeline"** -- the page NIFPARSE1 recommended and could not write
  because it had no numbers. It has them now.
* `.claude/skills/ww-anchored-hookup/SKILL.md`, 182 -> 207 lines. Section 5
  gains "an anchor is read out of the file, never out of a report" with both
  code points named, and "two declarations with identical text need a non-local
  anchor, and the assert goes on the AFTER state".

Both were copied to `E:\Projects\Claude\.claude\skills\` and `diff -q` says
SAME, so the two trees do not drift (CONSTITUTION 1a).

**Wished for, and worth writing before its second use** -- a page for
**subsetting another lane's all-or-nothing refusing script**. `fixes_subset.py`
imports the other lane's `EDITS` table, filters it by file, re-runs the same
anchor and CR asserts, and PRINTS THE EDITS IT LEFT BEHIND by name. Three things
in that are general and were worked out from scratch here: you do not edit
another lane's artefact to select from it; the selection key is the FILE, because
that is how fix sets are grouped in practice; and a skipped edit must be printed,
or "we applied the subset the verdict supports" is indistinguishable from "we
forgot some". The brief said "apply only the fixes the verdict supports" and the
mechanism for doing that did not exist.

**Declined, with the reason.** No skill for the tiling measurement itself -- it
is one constant in one generator and it will not recur. No skill for the region
choices or the bake runner: `nifskope-ww-lodgen` carries both, and BAKEPERF1's
`bake_run.ps1` / `bake_diff.py` / `loop20.ps1` were reused unchanged rather than
retyped, which is the whole point of keeping a lane's scripts in the repo.
