---
name: ww-parallelise-a-stage
description: Make one stage of a NifSkope Wild Wasteland pipeline use more than one core, and PROVE it -- the shared-state inventory that has to follow the call rather than the file, the byte-identity gate with its own refuter, deterministic retire order for everything downstream that reads a list, the difference between wall time and summed CPU time in a stage-time line, the persistent pool a per-call thread is not, and the rule that a fan-out nobody measured is not a speed-up. Use before writing any fan-out, and before believing one.
---

# Making a stage parallel, and proving it

Written from lane BAKEPERF1 (NifSkope WW, 2026-09-11), which parallelised the
LOD generator's chunk queue, found the parser was not thread-safe, shipped the
fan-out switched off, and caught its own BC-encoder fan-out being a 54 percent
REGRESSION. Every rule below is one of those.

## 1. The inventory comes first, and it follows the CALL

Before any code: list every cache, static, global and Qt object the region
touches, and for each say whether it is read-only after a warm-up, written per
item, or written per texel. **Follow the call, not the file.** BAKEPERF1's
inventory covered the generator's own caches and stopped at the boundary of
`NifModel` — and that is exactly where the crash was.

Four shapes that keep recurring in this tree:

* **A cache that hands out interior references.** `EsmWorld::lodBase()` returns
  `const X &` INTO a `QHash`; an insert by another thread rehashes and the
  reference is into a moved bucket. **A mutex around the insert does not fix
  this**, because the reference outlives the lock. The only answers are
  per-worker instances or returning by value.
* **A decompression scratch buffer.** `ESMFile`'s `zlibBuf` is rewritten by
  every compressed-record read and the record's `fileData` is repointed into
  it. One instance per worker, always.
* **An LRU whose evictor `delete`s.** `LodgenBakeCaches` frees textures; a
  pointer another worker is reading becomes a use-after-free. Per worker.
* **A lazy "build once on first use" static.** First use is the only moment it
  can race. WARM THEM ALL on the calling thread before the fan-out
  (`lodgenWarmSharedIndices()` is the pattern), including one throw-away
  instance of any class whose CONSTRUCTOR has first-time work.

And the rule that costs the most when it is missed: **a comment claiming thread
safety is a claim to test.**

## 2. Order is a separate problem from safety

Anything downstream that consumes a LIST in list order, or assigns indices from
arrival order, will silently change its output if results retire in completion
order. In this tree that is: the `.BTO` list (atlas, texture arrays, shape
merge, far-ring cut, card arrays), the manifest indices, the texture-array layer
assignment, the card-array packing, and the FO4CS-native accumulator, which
gives each new `(ref, part)` the next index in a vector.

Two mechanisms, both used by `lodgenchunkpass.cpp`:

* **Retire by index on the caller's thread.** Workers write
  `results[jobIndex]`; the driver walks 0..n-1, waits for each flag, and calls
  the caller's `retire`. Everything order-sensitive is fed from there.
* **Journal and replay** for a process-wide accumulator you cannot make
  per-worker: record the calls on the worker (`lodgenNativeJournalBegin`),
  replay them on one thread in job order. The accumulator then receives exactly
  the sequence a serial run would have made, so identity is by construction.

## 3. The gate is BYTE IDENTITY, and the gate needs a refuter

`--threads 1` (or whatever the off value is) must be **exact**: the same loop,
the same thread, ascending order, the same caches. Then:

* every output file of a whole region, hashed, compared by path AND content;
* the file COUNT printed beside the verdict, so a comparison over nothing reads
  as the red it is;
* **and the comparator run against a deliberately corrupted copy first**, to
  watch it go red on one flipped byte and on one missing file. A comparator
  nobody has seen fail is not a gate.
* Compare the NEW exe at its off value against the ROLLBACK RUNG as well. That
  is the gate that says the rewrite changed no arithmetic, and it is the one
  that passed in BAKEPERF1 while everything else was still on fire.

## 4. Stage times: wall, not summed CPU

Summing per-item elapsed across workers gives CPU time and makes a stage look
SLOWER the more cores it uses. Take the pass's own wall time and split it
between stages in the proportion the work had; say so in the code and in the
report. At one worker the two agree, so the serial numbers keep their meaning.

Print the peak working set beside them (`GetProcessMemoryInfo`) so "will this
fit in the machine" is a number and not a hope — N workers each with their own
plugin reader and texture cache is N times a large number.

## 5. A per-call thread is not a pool

`lodgenParallelFor` first created a `QThread` per call. The BC writers call it
once per MIP of every sheet — thousands of `CreateThread`s for block rows that
encode in less time than a thread takes to start — and the texture stage got
**54 percent slower**. Use a persistent `QThreadPool` with `setExpiryTimeout(-1)`,
and put a SIZE FLOOR under the fan-out (32 items in that case) so small work
stays on the calling thread.

Suppress nesting: a worker of an outer fan-out sets a `thread_local` flag and
any inner fan-out under it runs serially. Without that, an N-worker outer pass
spawns N x M threads and the heap does not survive it.

## 6. Measure it, then decide whether it ships

**A fan-out that nobody measured is not a speed-up.** Put the before/after stage
table in the report with the speed-up per stage, and if a stage got slower say
so as a red. BAKEPERF1's chunk fan-out was correct, byte-identical, and
**slower** once the unsafe layer under it had to be serialised — so it ships
behind its own number, defaulting to off, with the blocker named. CONSTITUTION
7: always leave a zero-effort fallback, and a serial thing that works beats a
parallel thing that faults.

## 7. Before the first crash campaign

Read `nifskope-ww-crash-diagnose`. In particular: arm the error mode so a
headless fault never puts a dialog on bungo's desktop, and take the diagnostic
relink-with-symbols BEFORE the second hypothesis, not after the fourth.
