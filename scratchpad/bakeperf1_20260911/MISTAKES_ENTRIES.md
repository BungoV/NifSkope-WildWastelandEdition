# MISTAKES entries owed by lane BAKEPERF1 (2026-09-11)

Text for `MISTAKES.md` at the repo root, newest first. The lane did not append
them itself (the file is shared); the director splices.

---

## 2026-09-11 — Six Windows crash dialogs reached bungo's desktop

**What was done.** Lane BAKEPERF1 ran a headless `-no-gui lodgen` bake five
times in a row to measure a crash rate, and then a five-variant bisect, three
runs each. Every crashing run raised a Windows "Application Error" dialog.
**Six of them appeared on his desktop in three minutes** and he had to ask what
they were.

**What was true instead.** A headless run must never put a window on any
screen. The rule in CONSTITUTION 6 is written as "every window on the second
monitor, never SetForegroundWindow" and a modal system error box obeys neither
— it is on the primary monitor and it takes focus. The rule covers dialogs the
process did not author, not only the ones it did.

**How it was found.** bungo saw them; the director relayed it. Event Log id
1000, `NifSkope.exe` faulting in `ntdll.dll` at offset `0xff509`, six pids.

**The rule that prevents it.** A lane that is deliberately provoking a crash
arms the error mode FIRST, and the exe does it for itself:
`SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX |
SEM_NOALIGNMENTFAULTEXCEPT | SEM_NOOPENFILEERRORBOX)` at the top of
`initModelLayer()` (`src/nifcli.cpp`), which is the first thing the `-no-gui`
path runs. Windows gives a child process the error mode of its creator, so a
driver script can arm it too (`scratchpad/bakeperf1_20260911/no_crash_dialog.ps1`)
— but only the in-process call covers a run somebody else launches. A crash
still fails the run and still sets the exit code; only the dialog goes.

---

## 2026-09-11 — The shared-state inventory missed the layer the crash was in

**What was done.** The lane's first deliverable was an inventory of every
cache, static, global and Qt object a bake touches (report §1), written before
any code, as its brief required. It named `EsmWorld`'s six lazy caches,
`ESMFile`'s decompression scratch, `LodgenBakeCaches` and its LRU, the three
archive indices, the FO4CS-native accumulator, and `NifModel`'s entry in
`GameManager::nifResourceMap`.

**What was true instead.** The inventory stopped at the boundary of
`NifModel` — it named the one global a NifModel's constructor touches and
treated the rest of the model layer as the caller's own object. The crash was
**inside** that layer: `setupArrayPseudonyms()`'s three process-wide hashes
(missed entirely), and then the parser itself, where nine workers faulted in
`NifItem::deleteChildItems()` under `BaseModel::~BaseModel()`.

**How it was found.** By crashing: 4 runs of 4, `STATUS_HEAP_CORRUPTION`, then
a diagnostic relink with the symbol table kept and a `gdb` backtrace.

**The rule that prevents it.** An inventory of shared state must follow the
call, not the file. Every library the parallel region CALLS gets the same
question asked of it — "what does this write that is not mine" — and a
comment claiming thread safety (`NifItem`'s pool says "Thread-safe (mutex)
because the XML checker parses NifModels on worker threads") is a claim to
test, not evidence. Where the answer cannot be established by reading, the
lane says so in the inventory instead of leaving the area unmentioned.

---

## 2026-09-11 — Three fixes shipped on hypotheses, two of which were wrong

**What was done.** After the first crash the lane fixed, in order: the
array-pseudonym race (relink 1), the nested BC fan-out (relink 2), and its own
division of the texture budget (relink 3). Each was described in its patch
script as the cause.

**What was true instead.** Only the first moved the failure at all (from 0–3
files written to 11). The other two changed nothing measurable: 5 runs of 5
still faulted. The actual cause needed a symbolised stack, which needed a
fourth relink.

**How it was found.** By re-measuring after each: the crash rate did not move.

**The rule that prevents it.** CONSTITUTION 4, rule 2 of 2026-09-04 21:33 —
nothing is stated as a cause without a measurement, and candidates are named as
candidates with the discriminator that separates them. **Get the stack first.**
For a stripped release exe that means one diagnostic relink with
`LFLAGS="-Wl,-subsystem,windows -mthreads"` (no source change, no behaviour
change), which cost one minute and answered in one run what three relinks and
an hour of reading had not. That relink is now the FIRST step of any
crash-in-the-release-exe investigation, not the last.
