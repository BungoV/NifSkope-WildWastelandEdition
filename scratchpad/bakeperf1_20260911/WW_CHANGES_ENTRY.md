# WW_CHANGES.md entry owed by lane BAKEPERF1 (2026-09-11)

Text for the director to splice. `WW_CHANGES.md` is mixed-ending and stays so —
match the neighbours, never normalise.

---

## 2026-09-11 — LOD generation: the chunk queue moved into shared code, and what the machine can and cannot be used for

bungo, that morning: *"bake time, anything we can do to speed it up? use my
system to its fullest here?"* This is the answer, and half of it is a refusal
with numbers.

**What the generator now has.**

* **One chunk queue, shared.** The panel built one chunk per event-loop tick and
  the command line had the same doubly-nested loop written out again. Both now
  call `lodgenRunChunkPass()` (`src/lodgenchunkpass.{h,cpp}`, new). Results
  retire on the caller's thread **in job order**, so everything downstream that
  depends on order — the `.BTO` list the atlas, the texture arrays, the shape
  merge, the far-ring cut and the card arrays all consume in list order, the
  printed `[n] <name>` lines, the panel's live preview and its progress bar, and
  the `.lodo`/`.lodi` accumulator — is fed from one place and never from
  whichever worker finished first.
* **`--threads N`** (`src/lodgenparallel.{h,cpp}`, new) is the generator's whole
  thread budget: the BC1/BC3/BC4 block encoders run their block rows over it.
  0 or absent means the machine. **1 is the exact way back.**
* **`--chunk-threads N`**, the chunk queue's own number, **default 1**. See the
  refusal below.
* **A writer thread with a bounded queue** for the chunk outputs, so a worker
  hands over bytes instead of waiting on the disk. `finish()` drains before the
  pass returns: nothing exits with a file still unwritten.
* **A bake census line**, from one formatter shared by the panel's result line
  and the command line: `bake census: threads N, chunk threads N, chunk jobs N,
  chunk workers N, peak working set N.NN GB (N bytes)`. The peak comes from
  `GetProcessMemoryInfo`, so "will the Commonwealth fit in 31 GB" is a number.
* **A headless run can no longer put a dialog on your screen.** The `-no-gui`
  path calls `SetErrorMode` at start-up; a crash still fails the run and still
  sets the exit code.

**THE REFUSAL, with numbers. The chunk queue does not go parallel yet, because
the NIF parser is not thread-safe.** Building NifModels on worker threads
faults: five runs out of five on the nine-chunk Sanctuary region ended in
`STATUS_HEAP_CORRUPTION`, and a diagnostic build with symbols put the fault in
`NifItem::deleteChildItems()` under `BaseModel::~BaseModel()` inside
`lodgenLoadModel`, with every other worker in the same parser
(`BaseModel::getItemInternal`, `NifExpr::partition`, `NifModel::get<>`).
Serialising the parse makes the fan-out survive — and then it is **slower**,
because parsing is most of what a chunk costs. So `--chunk-threads` defaults to
1, the default bake is byte for byte the bake that has always run, and the thing
that has to be fixed before the machine can be used on chunks is named.

**Byte identity is the gate and it holds.** Every output file of two whole
regions, hashed and compared by path and by content: the Sanctuary nine-chunk
region (**60 files, 24,975,886 bytes**) and downtown Boston's twenty-five
(**163 files, 77,591,755 bytes**). Byte-identical in all of: the rollback exe
against the new default, the new default against `--chunk-threads 16`, and the
literal `--threads 1` form. The comparator was made to go RED first, on one
flipped byte and on one missing file, so it is a gate and not a formality.

**Where the time actually goes, measured on the nine-chunk region** (medians of
three alternating runs a side, because the same exe varies by 2.3 s between
runs):

| | wall | meshes | textures |
|---|---|---|---|
| the previous exe | 16.04 s | 8.6 s | 5.6 s |
| this one, default | 16.13 s | 8.5 s | 5.4 s |
| this one, `--chunk-threads 16` | 30.7 s | 6.7 s | 23.4 s |

**No regression and no speed-up.** The default is the old speed to within
0.6 percent; the parallel path is 1.8x slower and costs 21.9 GB on a
twenty-five-chunk region against 31 GB of machine, which is the other reason it
is off. Twelve harnesses run, eleven green, and the one red
(`lodgen_terrain_vt` 41/1, check `V9b`) was already red on the previous exe.
