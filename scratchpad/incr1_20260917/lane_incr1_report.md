# Lane INCR1 -- `--incremental` on the RULED (FO4CS) pipeline

Report written incrementally, a section per finished step.

**Exe at launch:** `E:/Projects/NifskopeWildWastelandEdition/release/NifSkope.exe`,
**2026-09-17 03:56:42**, **22,477,824 B** (ARCHLOCK1's; read off disk with
`stat`, not copied from the brief). Rung taken before the first build:
`release/NifSkope.before_incr1.exe`.

Lane started 2026-09-17 04:58 (`date`-read). Only lane in the tree; no mutex.

---

## 1. Measured FIRST, on the exe at launch, before any source change

### 1(a) B3's arms, as they stand, on the STOCK target

Driver `scratchpad/incr1_20260917/s1_b3_stock.sh`, comparer
`scratchpad/incr1_20260917/incr_compare.py` (which uses the tree's ONE record
reader, `tests/spells/lodb_read.py`, never a parser of its own).

**Region:** `-24 16 -13 27`, dim 4 -- **9 chunks**, `(-24,-20,-16) x (16,20,24)`,
`--cover --roads --road-detail 1`, plugin a copy of `Fallout4.esm` at one fixed
path (the switch digest covers the argument vector, so a second path would fire
the "switches differ" refusal instead of the experiment).

Every edit is made in the **corner** chunk `(-24,16)`. B3 itself used 5x5 and
said why: the widening marks a chunk dirty when a chunk within one cell of it
is, which for dim-4 chunks is all eight neighbours, so an edit in the MIDDLE of
a 3x3 dirties all nine and the arm cannot tell a working diff from a full bake
wearing one. A corner has three neighbours inside the region, so the dirty set
is a strict subset.

| arm | edit | dirty | inputs moved | output lost | by neighbour | full | incr | output files differing | record | verdict |
|---|---|---|---|---|---|---|---|---|---|---|
| null | none (control) | 0 of 9 | 0 | 0 | 0 | 11 s | 1 s | **0** | DIFFERS | **FAIL (record only)** |
| land | `VHGT` gradient byte, cell (-23,17) | 4 of 9 | 1 | 0 | 3 | 10 s | 6 s | **0** | DIFFERS | **FAIL (record only)** |
| refs | drawn `REFR` 0x0007FB53 moved 512 u in X, cell (-21,19) | 9 of 9 | 4 | 0 | 5 | 10 s | 11 s | **0** | DIFFERS | **FAIL (record only)** |
| border | `VHGT` on the region's own outer cell (-24,16) | 4 of 9 | 1 | 0 | 3 | 10 s | 5 s | **0** | DIFFERS | **FAIL (record only)** |
| lost | `Commonwealth.4.-24.16.BTO` deleted under the record | 4 of 9 | 0 | **1** | 3 | 11 s | 5 s | **0** | DIFFERS | **FAIL (record only)** |

Floors (the edit moved the full bake's bytes at all):

* `land` 3 output files (`.BTR`, `.DDS`, `_data.DDS` of (-24,16));
* `refs` 2 output files (`.BTO` and its manifest) -- a reference the bake
  **demonstrably draws**, picked out of the base bake's own manifest by
  `b_pickref.py` (B3.1's rule: an arm whose only witness is the artefact under
  test is not a witness);
* `border` 3 output files;
* `null` and `lost` invert the floor on purpose (the full bake must NOT move);
  `lost`'s non-vacuity is the census line `1 output lost, 3 by neighbour`.

**The finding, and it is the lane's first: every OUTPUT file is byte-identical
in all five arms -- and every arm still fails, on the bake record alone.**
Two separate causes, both measured:

1. **A v2 record cannot be compared across two output directories at all.** The
   record carries the argument vector verbatim (`switch` lines), so a full bake
   into `.../land/full/obj` and an incremental bake into `.../land/incr/obj`
   disagree on four `switch` lines before anything under test is reached. This
   is BAKEREC1's own leg-(h) lesson (MISTAKES.md 2026-09-17 01:1x, entry 3:
   "leg (h) first baked into two DIFFERENT out-dirs ... so it was measuring the
   folder name") -- it was never applied to B3, because B3 predates the v2
   record. The `switches` DIGEST is equal on both sides
   (`f613fda236b23169e9865f5f5c6cd2f8fd1271e8`), which is the digest doing
   exactly what it promises.
2. **Even in ONE directory the two records differ by construction**, in exactly
   two places and no others: the two extra `switch` tokens `--incremental` and
   its directory, and the one extra `census incremental: ...` line.

So B3, promoted verbatim, would be red for ever on a bake whose outputs are
perfect. Step 4's gate bakes full and incremental into **the same out-dir**
(the second bake replaying over a restored copy of the base tree) and masks
exactly those two record differences, with a refuter that proves the mask
cannot swallow a chunk digest or an output digest.

`refs` dirtying **9 of 9** is not a defect: the moved reference sits in cell
(-21,19), and the input digest covers each chunk's one-cell ring, so four
chunks' digests genuinely moved and the widening took the rest. The digest is
conservative by construction.

### 1(b) The refusal on the ruled pipeline, quoted

A full FO4CS bake of the same nine chunks first (`--native <dir>`, 81 s,
`Commonwealth.lodo` 225,399,755 B + `Commonwealth.lodi` 201,987 B, 5,379
instances), then the same command with `--incremental`:

```
refused: --native builds ONE .lodo/.lodi pair for the whole region out of the
placements the chunk pass hands it, so an incremental run would write a pair
covering only the chunks it rebaked and say nothing about the rest.
  bake without --incremental, or drop --native from this run and do the pair in
  a separate full pass.
```

`rc=1`, nothing baked (21 files before and after). **This is every command bungo
would type**, because since BTOFREE1/LAYOUT1 the ruled pipeline IS `--native`.

Note for the order of refusals: on a tree with no record the `NO_LEDGER` refusal
fires first, so the `--native` one is only reachable after a full FO4CS bake --
which is exactly the real case.

### 1(c) What the diff itself costs

Three runs each, same machine, same region, warm:

| what | seconds |
|---|---|
| full stock bake, 9 chunks | 10.5 (10, 10, 11) |
| full FO4CS bake, 9 chunks | 81.2 |
| `--incremental` over an unchanged tree (ESM load + 9 chunk digests + record write) | 0.93 (0.922, 0.956, 0.911) |
| ESM load alone (a `WRONG_SHAPE` refusal, which fires after the load) | 0.40 |
| **the diff alone** (9 chunk digests + the record write) | **0.53, i.e. 0.06 s a chunk** |

**5.0 %** of a stock bake of the same region, **0.65 %** of the FO4CS bake of
the same region. The diff is not bookkeeping: it costs about one part in twenty
of the cheap target and one part in 150 of the ruled one.

---

## 3. The fifth volatile field, and leg (f)'s count (done FIRST, so the neighbours are green before my own change lands)

### 3.1 The fifth volatile thing

`lodgenPeakWorkingSetLine()` (`src/lodgenparallel.cpp` ~223-241) is printed
INLINE in the chunk-pass census line (`lodgenchunkpass.cpp:352`), which the
record carries verbatim:

```
census	bake census: threads 16, chunk threads 1 bound by default, chunk jobs 9,
chunk workers 1, peak working set: 2.28 GB (2449879040 bytes), bto built in the
mod folder, 9 chunk(s), 0 dropped, 0 bytes freed, layout n/a
```

It measures THIS MACHINE AT THIS MOMENT, not the bake's inputs. Two bakes of one
tree differ in it by megabytes, and four gates were red on this one clause.

Applied `ww-volatile-field-law`: masked, never dropped, the named field only, in
BOTH implementations.

| where | what changed |
|---|---|
| `src/lodbfile.cpp` `lodbNormalise()` | a fifth branch: in a `census` line containing `peak working set: `, the clause from `peak working set: ` to the next comma (or end of line) becomes `<volatile>` |
| `tests/spells/lodb_read.py` `normalise()` | the same branch, same bounds; `VOLATILE_FIELDS` grows from four entries to five |
| `src/lodbfile.h` | the block comment's list gets the fifth row; "every line but four" -> "but five"; and `lodbNormalise`'s own doc comment said **"the three lines/fields"** above a function that already masked four -- exactly the bug the law's step 4 names -- now "the FIVE" |
| `src/nifcli.cpp` ~4494 | the same stale count in the record-writer's comment ("the three things `src/lodbfile.h` names") corrected to five; and ~4606's "`lodbNormalise()` **drops** it" corrected to "MASKS it" |
| `docs/LODGEN_BAKE_RECORD.md` section 3 | heading and lead count five; a table row for the clause; a paragraph naming where it came from |

THE MASK'S BOUND IS A FACT ABOUT THE WRITER, NOT A HOPE: neither spelling
`lodgenPeakWorkingSetLine()` can produce -- `N.NN GB (N bytes)` or `not available
on this platform` -- contains a comma, so the clause cannot run past its own end
into the bto disposition or the layout counts that follow it on the same line.

A STATED DIVERGENCE from the law's step 1, which says a volatile thing goes on a
line of its own and never inline among facts that do not move. This one stays
inline: the chunk-pass census line is a line bungo reads at the end of every
bake, and splitting it would change a census wording for a comparison's
convenience. The divergence is written into `LODGEN_BAKE_RECORD.md` section 3,
not left in a commit message.

Proven on the two records already on disk (`work/s1/null/{full,incr}`), both
halves of the law's step 5:

```
A norm: census	bake census: ... chunk jobs 9, chunk workers 1, peak working set: <volatile>, bto built ...
B norm: census	bake census: ... chunk jobs 0, chunk workers 0, peak working set: <volatile>, bto built ...
raw differ: True
```

-- the clause is gone, and `chunk jobs 9` vs `chunk jobs 0` (which is CONTENT:
the incremental run really did no chunks) still shows.

### 3.2 The two normalisers were never compared

Found while doing the above, and it is a bigger hole than the field itself:

```
$ grep -rn "lodbNormalise" src/ | grep -v lodbfile
src/nifcli.cpp:4606:   * `lodbNormalise()` drops it, which is what makes ...   <- a COMMENT
```

`lodbNormalise()` had NO CALLER anywhere in `src/`. The law's step 3 says the
mask is written twice, in two languages, BECAUSE two implementations of one rule
is the only way a wrong mask is caught -- but nothing in this tree ever held the
two against each other. Every gate used the Python half alone. A field masked in
one half and not in the other would have shipped unnoticed, which is exactly how
this lane would have shipped a fifth mask in Python and forgotten C++.

Closed with two lines on the `--bake-record` read path (no new switch):

```
bake-record normalisedLines <n>
bake-record normalisedSha1  <sha1 of the normalised text, UTF-8, LF>
```

Empty lines are dropped before masking and the join ends in exactly one newline,
which is what `lodb_read.normalise()` does, so the two agree on the TEXT, not
merely on the rule. My gate compares the two digests; they disagreeing is a
defect in whichever half was changed alone. Documented in
`LODGEN_BAKE_RECORD.md` sections 3 and 4.

### 3.3 `lodgen_layout.sh` leg (f): `census 79, on disk 80`

`LODGEN_BAKE_RECORD.md` section 2.8 states the law for the same count on the
`end` line: "Counted by walking the record's own directory tree at write time,
THE RECORD ITSELF EXCEPTED (it is not written yet)". The layout census line is
composed by `lodgenLayoutCensusLine()` inside the chunk-pass census, before the
record exists, so the record cannot be in the number it carries either. The law
is not silent, so the GATE's count is what was wrong.

Leg (f) now subtracts the record and SAYS HOW MANY IT TOOK OUT, so the exception
cannot quietly swallow a second file, plus a new check that there is at most one
record under the root:

```
(f) 1 bake record excepted from the disk count (section 2.8: it is not written
    when the census line is composed)
(f) it counts what is on disk (79 file(s), 80 on disk less 1 record)
```

### 3.4 `lodgen_bakerec.sh` leg (e) is red for a DIFFERENT reason -- measured, not assumed

The brief attributes `lodgen_bakerec.sh` legs (e) AND (h) to the peak working
set. Ran the gate on the exe at launch before touching anything (21 checks, 3
failures, matching ARCHLOCK1's table) and read the log:

* (h) IS the volatile field: "kinds that differ: baked, census; census lines that
  are not stage times: 1" -- that one census line is the peak working set.
* (e)'s two failures are NOT. The `--native-verify` refusal against an edited
  plugin does not name the plugin at all:

  ```
  native REFUSED .../Commonwealth.lodo is STALE: objectCorpusHash 0x6324ed6a...
  in the file, 0x9e97ef9d... from .../moved.esm -- a placement, a base's MNAM or
  a SCOL part changed since the bake
  ```

  `lodgenNativeVerify()` has three staleness branches. The `pluginCorpusHash` and
  `loadOrderHash` ones append `lodbNameTheStalePlugin()`; the `objectCorpusHash`
  one -- THE BRANCH AN EDITED REFR TAKES, which is the common case and the one
  the gate exercises -- did not. One call, three branches, two of them wired.
  Fixed in `src/nativeemit.cpp` ~1569.

So the peak working set is three of the four gates' red, not four, and leg (e)
was a second defect hiding behind the first attribution.

---

## 2. Step 2 -- making the FO4CS target incremental-capable

### 2.1 THE DIVERGENCE ROW FOR BUNGO (written before the file was written)

| | |
|---|---|
| **What** | A NEW FILE FORMAT: `<ws>.<dim>.<cx>.<cy>.lodj`, one per baked chunk, written under `Data/FO4CSLOD/<worldspace>/` beside `<ws>.lodo`, `<ws>.lodi` and the `.lodb` bake record. Plain text, UTF-8, LF, `key<TAB>fields`, like every other text file this tree writes. |
| **Why** | `--incremental` refused `--native`, so it refused the whole ruled FO4CS pipeline: every command you actually type was on the refusal list. The `.lodo`/`.lodi` pair is aggregated from the arrivals of ALL chunks; a chunk that is skipped contributes nothing, so a skipped chunk's contribution has to be read back from somewhere. |
| **Why not from the previous `.lodi` (the route with no new file)** | Four things the aggregate needs are provably not in that file. (1) `NativePlacement::model`, the path of the mesh the chunk drew -- never written; `src/lodifile.cpp` mentions `baseName` only inside refusal text (lines 291, 294). Without it the occluder lookup `models[foldPath(p.model)]` cannot be done at all. (2) The model-space occluder box: the file holds the WORLD-space, 0.999-shrunk box with the quantised scale already folded in, and only for the at most `LODI_OCCLUDERS_PER_CELL = 4` per cell that survived the writer's cull (`src/lodifile.cpp:466`). (3) `NativePlacement::isTree` -- the aggregate's tree list keys on it (`src/nativeemit.cpp:1092`) and no instance flag carries it. (4) The lighting and AO ACCUMULATORS: the file carries an 8-bit mean, not the sum and the count, so a kept chunk's and a rebaked chunk's contributions cannot be recombined. Route (i) is closed; it cannot be made byte-identical, only nearly right, and nearly right is not a bake. |
| **What is in it** | Not the raw journal. `lodgenNativeLighting()` is called once per VERTEX (`src/lodgen.cpp:4134`), so nine chunks is on the order of a million events. The file holds the chunk's REDUCTION: every placement it emitted, in emission order, with the full `NativePlacement` including the model path; then one row per lit placement carrying the exact `double` sums and the vertex count; then the same for placement AO. Every float is a hex BIT PATTERN (`%08x` / `%016x`), because the whole point is that the rebuilt arrivals are bit-identical and a decimal round trip is not. |
| **Size** | About 300 bytes a placement. The 9-chunk `--terrain-region -24 16 -13 27` bake makes 5,379 instances, so on the order of 1.6 MB; the whole Commonwealth would be some tens of MB against a `Commonwealth.lodo` of 225,399,755 bytes. It is the smallest file in the folder by two orders of magnitude. |
| **Where it shows up** | In the bake record's `out` rows and in the layout census like every other output, so the census floor counts it and `lodgen_layout.sh` sees it. Deleting one is self-healing: a clean chunk whose `.lodj` is missing is marked DIRTY and rebaked. |
| **The exact way back** | `--no-native-cache`. With it, not one `.lodj` is written, the record's `out` rows are what they were before this lane, and `--incremental` refuses `--native` exactly as it did (CONSTITUTION 10). |
| **The one case it refuses rather than guesses** | An arrival is keyed `(refForm, scolPart)` ACROSS chunks, so a placement that two chunks both light has sums built from both, and `(prev + a1) + a2` is not `prev + (a1 + a2)` in floating point. Those are counted at bake time (`lodgenNativeSharedArrivals()`) and printed in the census; an incremental `--native` run refuses in words when the count is not zero. |
| **What does NOT become incremental** | `--impostors` stays on the refusal list: card sets are aggregated per region from the whole written `.BTO` list, a different mechanism, and since BTOFREE1 those `.BTO`s live in a scratch folder and are dropped. `--atlas` and `--arrays` stay refused: stock-only sheets, not the ruled pipeline. |
| **Your commands** | Unchanged. `--road-detail 1`, `--native <dir>`, everything under `Data/FO4CSLOD/`. Nothing moves, nothing is renamed, no default changes. |

### 2.2 What was built

| File | What changed |
|---|---|
| `src/nativeemit.h` / `.cpp` | `lodgenNativeJournalWriteCache()` reduces one chunk job's journal to its contribution and writes the `.lodj`; `lodgenNativeReplayCache()` puts it back. No new translation unit, so `NifSkope.pro` and `Makefile.Release` are untouched. `Arrival` gained `litCx/litCy/litSeen/shared` and `State` a `sharedArrivals` counter -- the one case the reduction cannot reproduce, counted rather than hoped through. |
| `src/lodgenchunkpass.h` / `.cpp` | three options: `nativeAllJobs`, `nativeReplayCached`, `nativeJournalSink`. The pass walks a cursor over the full queue and replays a cached chunk at exactly the point its own job would have been retired, in BOTH the one-thread and the fan-out path, and in the zero-dirty-chunks early return. A sink also forces journalling on at one thread, where the emitter would otherwise be spoken to directly and there would be nothing to write. All three unset is today's behaviour, call for call. |
| `src/nifcli.cpp` | `--no-native-cache` (the exact way back); the `--native` refusal narrowed to that flag; the cache written on every `--native` bake and listed in the record's `out` rows; a clean chunk with no `.lodj` on disk marked dirty; the shared-arrival refusal; a `native cache:` census line; a seventh field on the `incremental:` census line. |
| `tests/spells/lodj_read.py` | the Python reader, so a gate never needs a binary reader. |

### 2.3 Proved on a real bake, not argued

`scratchpad/incr1_20260917/s2_proof.sh` -> `s2_proof.txt`, region `-24 16 -17 23` (4 chunks, dim 4), one out-dir, three runs:

| | dirty | from cache | `Commonwealth.lodo` sha1 | `.lodi` sha1 |
|---|---|---|---|---|
| A full | -- | -- | `094b818c...40b9` | `48f037cb...8011` |
| B incremental, nothing changed | 0 of 4 | 4 (2,243 placements) | **identical** | **identical** |
| C incremental, one `.lodj` deleted | 1 of 4 | 3 (1,791 placements) | **identical** | **identical** |

C is the leg that matters: one chunk rebuilt from the plugin beside three replayed from cache, and the pair still byte-for-byte what a full bake writes. The first run of leg C was a FALSE PASS -- it grepped for the census wording, and the census said "4 of 4 chunks dirty", so nothing was cached and the mixed path never ran. It now reads the two numbers out and fails on them. That false pass found a real defect, below.

### 2.4 The defect leg C found: a lost cache widened

The widening exists because the terrain ring and the AO skirt each reach one cell, so a chunk whose INPUTS moved changes what its neighbours draw. A `.lodj` is not an input to anything -- it is this tree's record of what a chunk once emitted -- but it is a tracked output, so losing one took the ordinary "output is missing or edited" path and dragged all three neighbours in with it: deleting one cache file rebaked the whole region. The diff now keeps two lists, and the widening is seeded only from chunks that lost something that is not a cache. Measured before: `1 of 4 dirty` became `4 of 4`. After: `1 of 4`.

### 2.5 The size, measured

658,970 bytes of `.lodj` for 2,243 placements = **294 bytes a placement**, against a `Commonwealth.lodo` of 225,399,755 bytes. The cache is 0.29 percent of the pair it rebuilds.

### 2.6 THE UNCOMFORTABLE MEASUREMENT: what this actually saves

| run | wall | `stage times: meshes` | textures |
|---|---|---|---|
| A full, 4 chunks | 72.0 s | 64.9 s | 3.2 s |
| B incremental, 0 chunks dirty | 70.0 s | 68.9 s | 0.0 s |
| C incremental, 1 chunk dirty | 75.5 s | 72.6 s | 1.1 s |

Doing NO chunk work at all saved 2 seconds of 72. The reason is not the cache: it is `lodgenNativeWrite()`, which builds the `.lodo` library from the worldspace's FULL base census and is chunk-INDEPENDENT by design (spec 8, 9.3 -- that is what makes mesh ids worldspace-stable in a one-chunk bake). Its ladder census for this run reports 3,666 meshes laddered over 4,400,225 full-detail triangles, and it runs whole whatever the dirty set is.

Against step 1's 9-chunk figure (meshes 72.7 s) and this 4-chunk one (64.9 s), five extra chunks cost 7.8 s, so a chunk is about 1.6 s and the FIXED cost is about 58 s -- **roughly 80 percent of an FO4CS bake, and 90 percent of a small one.** So this lane makes the FO4CS target incremental-CAPABLE and CORRECT, and the wall-clock prize is not here. It is in the library: the `.lodo` is a pure function of the base census and the three corpus hashes the record already stores, so an incremental run whose hashes match could reuse the previous `.lodo` verbatim instead of rebuilding it -- proved possible by the very fact that A, B and C wrote the same 225 MB file three times. That is a lane of its own and is NOT attempted here; what is here is the measurement that says where to point it.


---

## 4. The gate, finished

`tests/spells/lodgen_incremental.sh` arm (f) was the one red left, and it was
MINE, in the harness, not in the product: the replacement one-liner handed the
Windows python a Git-Bash `/e/...` path for `sys.path` and for the record, so
python died before it printed anything and the arm compared a digest against an
empty string. Two fixes, both in the script:

* the paths go over in WINDOWS form (`win "$R/tests/spells"`, and the record's
  directory through the same helper) and arrive as `sys.argv`, not spliced into
  a shell-quoted `-c` string;
* the digest is taken INSIDE python over `t.encode()`, never over a pipe. A
  text-mode stdout on Windows turns every LF into CRLF, which is how this arm
  first came up red with 77 lines agreeing and the sha1 not.

Cross-checked on a SECOND record, one neither half had seen:

| | lines | sha1 |
|---|---|---|
| C++ `lodbNormalise()` | 96 | `f605736012794cf5b5844b1d247a051608b16d02` |
| `lodb_read.normalise_file()` | 96 | `f605736012794cf5b5844b1d247a051608b16d02` |

## 5. `lodgen_layout.sh` leg (c): it was measuring the linker

Leg (c) bakes the STOCK target with the rung exe and with the new one and
byte-compares the whole tree. Four of its five failures were the same file at
four dims:

    DIFFERS: Commonwealth.lodb            (dim 4, 8, 16, 32)

The record's first line is

    lodb<TAB>2<TAB>Commonwealth<TAB>0.3.3+720762a<TAB>22477824
                                    exe version   exe SIZE IN BYTES

so two different exes cannot write the same record bytes however identical the
LOD. That leg went red the moment this lane's exe grew and would go red again on
any lane that adds a line of code. It is not a defect and it never was.

The record is now EXCEPTED from the byte comparison, the exception is printed
with its reason, and `reccmp()` compares the record's SUBSTANCE instead: the
normalised text minus the `lodb` header line and minus the `census` lines
(wall-clock timings), which leaves every claim the bake made about the files it
wrote. It has a floor -- at least one `chunk` row and one `out` row, or the
comparison is vacuous and fails -- and a third answer, rc 3 NOT COMPARABLE,
because `release/NifSkope.before_layout1.exe` predates lane BAKEREC1 and writes
the v1 BINARY container. Patch A treated that as a difference; it is a ratified
format change, so it is reported and counted separately, together with a line
saying where the record IS still measured on this exe (`lodgen_bakerec.sh`, and
`lodgen_incremental.sh` arm (d)).

Proved on real files, both refuters fire:

| pair | verdict |
|---|---|
| v1 container vs v2 text | rc 3, NOT COMPARABLE, names the magic `LODB` |
| a v2 record against itself | rc 0, 91 lines, 9 chunk rows, 54 out rows |
| one `out` digest zeroed | rc 1, prints the row |
| one `chunk` row deleted | rc 1, prints the row and the length |

## 6. The panel row: NO ROW, and the reason

The brief said: if the change is the panel passing the out-dir through, add the
row; if it is more than that, a row for bungo with the reason, and no row.

**It is more than that. There is no row.** Read `src/lodgenmanager.cpp`:

* the panel builds its own `LodgenChunkPassOptions` and calls
  `lodgenRunChunkPass()` directly (`runChunkQueue()`, around line 3187). It does
  not go through the CLI's lodgen command at any point.
* `#include "lodbfile.h"` appears in **exactly one** translation unit,
  `src/nifcli.cpp`. The panel never writes a bake record, so there is nothing
  for it to read back. `--incremental` without a record is a refusal by design.
* the whole incremental machinery -- finding the record, the shape and switch
  checks, the five refusals, the input digests, the two dirty lists, the
  widening, the `.lodj` hooks and the record write -- is roughly 400 lines
  written INLINE inside the CLI's lodgen function, using that function's locals.

So a panel row would need three things the tree does not have: the panel
writing a record (which means assembling its own switch digest over its 57
switches, its load order, its plugin list and its per-chunk output lists), the
panel reading one back, and the selection code living somewhere both callers
can reach.

**The row for bungo:** the panel gets `--incremental` after a lane that lifts
the ledger out of `nifcli.cpp` into its own translation unit -- one entry point
that takes the jobs, the switches and the out-dir and hands back the dirty jobs,
and one that takes the outcomes and writes the record. The CLI then calls it
where it has the inline code today, and the panel calls the same two functions.
That is a lane, not a row, and putting a row on the panel before it exists would
give a switch that refuses every time it is ticked.

## 7. Docs

| file | what changed |
|---|---|
| `docs/LODGEN_LEDGER_FORMAT.md` | §4 table: `--native` off the whole-region refusal (it fires only with `--no-native-cache` now), two new inline refusals; new **§4.1** the `.lodj` cache with the hex rule, the shared-arrival refusal and the measured size; new **§4.2** the two dirty lists and the measurement that forced them; §5 the census line's fifth field and the new `native cache:` line; §6 the six-arm gate table |
| `docs/LODGEN_BAKE_RECORD.md` | §2.6: a `--native` bake's `out` rows include one `.lodj` a chunk, and being listed is what makes a lost one self-healing; §7 retitled "For the lane after INCR1", with what INCR1 did and the `.lodo`-reuse prize it left |
| `docs/LODGEN_CENSUS.md` | §6.1 gains `incremental:` (which was never in the inventory) and `native cache:`, both in house style with HOW IT MOVES and their gate arms |
| `docs/MISTAKES.md` | a refusal aimed at a corner case landed on the only pipeline anybody bakes -- and its second half, that the cost model was never measured either |
| `MISTAKES.md` | six of mine: editing a running harness; a leg that grepped census prose and passed on a run proving the opposite; hashing a pipe on Windows; handing a Windows python a Git-Bash path; a byte gate measuring the linker; and three one-liners |
| `src/nifcli.cpp` | `--no-native-cache` reaches `lodgen --help`, under the `--incremental` paragraph |

## 8. Changelog text for the director to splice

*(I do not edit `WW_CHANGES.md` or `HANDOFF.md`. This is the text, written for a
reader who never saw the code.)*

### For `WW_CHANGES.md`

**Incremental rebakes now work on the FO4CS target (2026-09-17).**

`--incremental` rebakes only the chunks whose inputs changed since the last
bake. Until today it refused to run at all on the FO4CS target -- the one
everybody uses, `--native <dir>`, everything under `Data/FO4CSLOD/` -- so in
practice it only ever ran on the stock engine target.

The reason was real. FO4CS builds ONE object library and ONE instance table for
the whole region, assembled from the placements every chunk hands it as the bake
walks them. Skip a chunk and its objects are simply not in the library -- and
the result still loads, still passes its own verifier, and is quietly missing
part of the worldspace. Refusing was the safe answer; it just happened to refuse
everything.

A skipped chunk now speaks for itself. Every FO4CS bake writes one small extra
file per chunk, `<worldspace>.<dim>.<x>.<y>.lodj`, holding exactly what that
chunk contributed: each object it placed, in the order it placed them, and the
lighting it accumulated, stored as exact bit patterns rather than rounded
decimals. An incremental bake replays each skipped chunk's file in that chunk's
own turn, so the library comes out in the same order it would have come out of a
full bake. Proved on a real region: a bake with nothing changed, and a bake with
one chunk rebuilt beside three replayed, each produced a library and an instance
table **byte for byte identical** to the full bake's.

Three things worth knowing:

* The cache files cost about 0.29 % of the library they rebuild (294 bytes a
  placement, measured). They are listed in the bake record like any other
  output, so deleting one just rebakes that one chunk and writes it again.
* **This is about correctness, not speed.** On a four-chunk region, skipping
  every chunk saved 2 seconds out of 72, because roughly 80 % of an FO4CS bake
  is building the object library itself, which is built from the whole
  worldspace's census and does not care which chunks were rebaked. Reusing that
  library is the next piece of work, and this one is its precondition.
* `--no-native-cache` is the exact way back: no cache files, and an incremental
  FO4CS bake then refuses exactly as it did before today.

The census now prints, every incremental run, how many chunks were skipped for
want of a cache file, and a `native cache:` line saying how many chunks were
written, how many were replayed and how many placements those carried.

### For the `HANDOFF.md` LANDED block

**LANDED -- INCR1, `--incremental` on the FO4CS target (2026-09-17 06:34).**
`release/NifSkope.exe` 22,534,144 B. New file format `.lodj` (per-chunk cache,
divergence row stated before it was written); `--no-native-cache` is the way
back; `--incremental --native` no longer refuses. Two dirty lists: a lost cache
dirties one chunk, not the ring. New gate `tests/spells/lodgen_incremental.sh`,
six arms with floors. `tests/spells/lodgen_layout.sh` leg (c) stopped
byte-comparing the bake record, which stamps the exe's size. NO panel row: the
panel has no ledger at all (`lodbfile.h` is included by `nifcli.cpp` only) --
row for bungo in the lane report section 6. Wall clock: 2 s of 72 on four
chunks; the prize is reusing the `.lodo`, next lane.

## 9. The build

Game check as its own command before it; `Fallout4.exe` down, no NifSkope of
bungo's (no `--port`-less window) on the machine.

**`release/NifSkope.exe` 2026-09-17 06:34, 22,534,144 B.** Verified stale-free:
the exe is newer than every source this lane touched (`src/nativeemit.cpp`,
`src/nativeemit.h`, `src/lodgenchunkpass.cpp`, `src/lodgenchunkpass.h`,
`src/nifcli.cpp`) **and** than the director's three
(`src/nifskope.h`, `src/nifskope.cpp`, `src/nifskope_ui.cpp` -- his "Open Here"
drop option, recompiled from his in-tree sources by this make and NOT reverted),
and `make -n` now proposes **0** compiles.

One thing worth writing down, because it cost a build: **`git` is not on the
PATH of the MSYS2 login shell in this tree.** Every object compiles, and then
the LINK rule dies at `git rev-parse --short HEAD > release/build_rev.txt` with
`git: command not found`, which reads like a build break and is not one. The
working invocation is

    export PATH=/ucrt64/bin:/e/Tools/GIT/mingw64/bin:$PATH
    mingw32-make -f Makefile.Release -j8

(`/mingw64/bin/git` in Git Bash is `E:\Tools\GIT\mingw64\bin\git.exe`; MSYS2's
own `/mingw64` is a different tree and has no git.)

**Relinked at 07:33:21**, same 22,534,144 B, after the one-line
`lodgenNoteLayoutFile()` call in the `.lodj` writer (section 11, red row 3):
`GeneratedFiles/.obj/nativeemit.o` recompiled at 07:32:41 and the link rule
ran. Every gate in section 11 except `lodgen_defaults.sh` was measured on
THAT exe.

## 10. The neighbour gates, and why each is reached

BEFORE figures are ARCHLOCK1's recorded table for the **2026-09-17 03:56:42**
exe (22,477,824 B) -- this lane's launch exe, and the one the director states is
lodgen-identical to the 05:42 one I took the rung from. They are cited rather
than re-measured because my own BEFORE run of `lodgen_layout.sh` was
contaminated by an edit I made to the harness while it was running.

| gate | why this lane reaches it |
|---|---|
| `lod_generation.sh` | step 5 read the panel driver and decided **no row**. This gate counts the panel's rows and self-tests them, so it is what proves no row appeared by accident. |
| `lodgen_defaults.sh` | a new switch (`--no-native-cache`) was added to the parser and an existing refusal was narrowed. A new switch must move no default, and this gate is the one that bakes the ruled defaults and compares stock outputs byte for byte against the rung. |
| `lodgen_native.sh` | `src/nativeemit.cpp` is the file this lane changed most -- `Arrival` grew four fields, `lodgenNativeLighting` now counts cross-chunk sharing, and the whole cache reader/writer lives there. It owns the `.lodo`/`.lodi` pair. |
| `lodgen_bakerec.sh` | the record now carries one `.lodj` `out` row per chunk, and this lane changed the Python half of the record's two-normaliser cross-check (`lodb_read.py --normalise` writes bytes now). |
| `lodgen_layout.sh` | the `.lodj` is a **new file written under `FO4CSLOD/<ws>/`**, which is exactly what this gate measures, and this lane rewrote its leg (c). |
| `lodgen_btofree.sh` | the incremental output loop now walks **every** output and decides dirtiness from which one was lost. Whether a `.BTO` is on disk at all is `--keep-bto`'s business, so the two features meet in that loop. |
| `lodgen_incremental.sh` | this lane's own gate. |

## 11. The neighbour gates, measured: before and after

**BEFORE** is ARCHLOCK1's recorded table, taken on the 03:56:42 / 22,477,824 B
exe -- quoted, not re-measured by me. **AFTER** is this lane's exe,
`release/NifSkope.exe` 2026-09-17 07:33:21, 22,534,144 B, one gate at a time
via `scratchpad/incr1_20260917/run_gates2.sh`, logs in `gates_after2/`. The one
exception is `lodgen_defaults.sh`, whose AFTER row was measured on the 06:34
exe and is labelled as such; the only change between those two exes is the
`.lodj` writer's layout-census call, and that gate asserts nothing about file
counts, census clauses or the bake record.

| gate | before (checks/fail/s) | after (checks/fail/s) | verdict |
|---|---|---|---|
| `lod_generation.sh` | 128 / 0 / 14 | 128 / 0 / 9 | PASS both |
| `lodgen_defaults.sh` | 28 / 0 / 937 | 28 / 0 / 873 (06:34 exe) | PASS both |
| `lodgen_native.sh` | 69+44+87+52+17+15+25, 2 FAIL in section 5 / 222 | same sections, same 2 FAIL rows / 216 | RED, pre-existing, widened by me |
| `lodgen_bakerec.sh` | 21 / 3 / 478 | 21 / **0** / 461 | FAIL -> PASS |
| `lodgen_layout.sh` | 22 / 5 / 966 | 23 / **0** / 893 | FAIL -> PASS |
| `lodgen_btofree.sh` | 21 / 1 / 195 | 21 / **3** / 193 | RED, 1 pre-existing + 2 MINE |
| `lodgen_incremental.sh` | did not exist | 10 checks in 6 arms / 0 / 301 | PASS |

### Why each neighbour is reached

- **`lod_generation.sh`** -- the lodgen PANEL. I decided against a panel row
  (section 6), so this gate is the proof I did not move the panel while
  deciding that.
- **`lodgen_defaults.sh`** -- masters ship off. A new switch that quietly moved
  a default would show here as a byte difference between switch settings.
- **`lodgen_native.sh`** -- every line I wrote lands in the native emitter.
- **`lodgen_bakerec.sh`** -- the record gains an `out` row per `.lodj` and the
  chunk-pass census line grows a clause, and the record is what
  `--incremental` reads back.
- **`lodgen_layout.sh`** -- `.lodj` is written INTO the FO4CS target, so the
  layout gate owns it like every other output there.
- **`lodgen_btofree.sh`** -- it compares the whole output tree against a rung
  exe, so any new file in the tree is its business by construction.
- **`lodgen_incremental.sh`** -- mine.

### The four red rows, named

**(1) `lodgen_native.sh` section 5 -- `the second ledger differs from the first
ONLY in the command-line digest` and `every recorded chunk digest alike`.**
Same two rows, same section, before and after: 2 FAIL rows on the 03:56:42 exe
and 2 on this one. Reproduced by hand with
`tests/spells/lodgen_btofree_ledger.py keep` on a real stock/native pair:

```
stock rows 24   native rows 24
only in the NATIVE record (4):  Commonwealth.4.{-20.16,-20.20,-24.16,-24.20}.lodj
only in the STOCK  record (4):  Commonwealth.4.{-20.16,-20.20,-24.16,-24.20}.BTO
```

`explained()` gives up the moment `sorted(ma) != sorted(mb)`, so the file
LISTS, not the digests, are what fails it. The `.BTO` half is BTOFREE1's
(`--native` stopped shipping `.BTO`); the `.lodj` half is mine, and it is a
second reason for a row that was already red for the first. Not mine in
origin, widened by me. Section 5 asks two DIFFERENT targets to list the same
files, which is the thing that actually needs fixing, and it is not a one-line
fix: it needs a decision about what "the stock path is untouched" means once
the two targets legitimately write different file sets.

**(2) `lodgen_bakerec.sh` leg (h) -- FIXED, and it was not the mask.** The line
that differs between two bakes is the `census<TAB>bake census: ... peak working
set: 2.28 GB (2449879040 bytes), ...` one. The mask DOES reach it: the first
sub-check on the same pair says the two normalised records are byte-identical,
5647 vs 5647 bytes. What was stale is the FLOOR beside it in
`tests/spells/lodgen_bakerec_gate.py` (`cmd_identical`), which decided by
prefix -- a differing `census` line was tolerated only when its last field
started with `stage times:` -- so the fifth volatile thing failed a floor under
a mask that covers it. It now asks the mask about the one line
(`lodb_read.normalise(ra[i]) != lodb_read.normalise(rb[i])`), so it follows the
mask when the mask grows and still goes red for a census line the mask does not
cover. Refuter `scratchpad/incr1_20260917/p9_refuter.py`, two synthetic pairs:
before the fix pair 1 (differs only where the mask reaches) was RED; after, pair
1 is GREEN and pair 2 (`merged: 124 shapes -> 121` against `-> 118`) is still
RED. Gate: 21/3 -> 21/0.

**(3) `lodgen_layout.sh` leg (f) -- FIXED, and it was mine.** `census 79, on
disk 81 less 1 record = 80`: the `.lodj` writer never told the layout census it
had written a file. Every other writer calls `lodgenNoteLayoutFile()` --
`src/nativeemit.cpp:1323-1324` for the pair, `src/lodtfile.cpp:1811`,
`src/nifcli.cpp:4828` for the record -- and the cache did not, so the census
undercounted by one per `.lodj`. One line in `src/nativeemit.cpp` after the
cache's `f.close()`. The same leg had a SECOND off-by-one before this lane (the
bake record is not on disk when the census line is composed), which is what
leg (f)'s new `1 bake record excepted` row accounts for. AFTER: `ok (f) it
counts what is on disk (80 file(s), 81 on disk less 1 record)`. Gate 22/5 ->
23/0, the extra check being that exception row.

**(4) `lodgen_btofree.sh` legs (a)(b)(c) -- rows, not fixes.** (a) and (b) read
`0 differ, 0/1 only on one side` and name it: `only in drop:` /
`only in keep: nat/FO4CSLOD/Commonwealth/Commonwealth.4.-20.24.lodj`. Nothing
DIFFERS; a file exists on this exe's side that does not exist on the rung's.
That is the new file in the default output set, which is bungo's call, so it is
the divergence row in section 12.1 and I do not decide it. (c) `DIFFERS:
Commonwealth.lodb -> Commonwealth.lodb (716 vs 2161)` is BAKEREC1's v1 binary
container against the v2 plain-text record, and it was already the single red
row on the 03:56:42 exe (`gates_new/lodgen_btofree.log:59`, 21 checks, 1
failures) -- pre-existing, not mine.

### My gate, arm by arm, with floors

`tests/spells/lodgen_incremental.sh`, 10 checks in 6 arms, 0 failures, 301 s,
region `-24 16 -17 23` dim 4:

| arm | what it measures | floor |
|---|---|---|
| (a) | a null `--incremental` over THE ruled FO4CS target rewrites the same `.lodo`/`.lodi` | the pair's two sha1s, byte for byte, 4 chunks cached |
| (b) | one cell edited: 1 chunk rebuilt beside 3 replayed gives the SAME pair; and a DELETED `.lodj` heals itself | the same two sha1s again, and a lost cache must not seed the widening |
| (c) | the stock target's chunks and sheets on disk | file for file |
| (d) | the record's SUBSTANCE: 41 lines, 4 chunk rows, 24 out rows | a deleted chunk row AND a changed `out` digest must both go red |
| (e) | `--no-native-cache` is the exact way back | 0 `.lodj` on disk, `--incremental` then refuses rc=1, and the refusal NAMES the flag |
| (f) | the C++ and the Python normaliser on the same file | the same TEXT, not merely the same rule: lines=77 sha1=e24f50debe92c5ff9fedcac36c2dbec4844b5cb4 on both sides |

## 12. Divergence rows for bungo

Both rows are written BEFORE the thing they describe is allowed to stand, per
the standing rule. Neither is mine to decide.

### 12.1 A NEW FILE in the default output set: `<ws>.<dim>.<cx>.<cy>.lodj`

**What changed.** Every bake that emits the FO4CS aggregate now also writes one
small plain-text file per chunk next to the pair, under `Data/FO4CSLOD/<ws>/`.
It holds that chunk's REDUCTION -- its placements in emission order and its
per-objectIndex lighting and AO sums as exact IEEE hex bit patterns -- so a
later `--incremental` run can replay a clean chunk instead of re-baking it. A
9-chunk Sanctuary region writes 9 of them; measured, a placement costs 294
bytes, so four chunks cost about 640 KB against a 215 MB `.lodo`.

**Why it is a divergence.** The ruled default pipeline's output set is what
ships. `.lodj` is written BY DEFAULT, on every bake, whether or not anybody
ever passes `--incremental`. A new file in the shipped folder is your call, not
a lane's.

**What it costs if it stays.** Nothing at run time -- the game never reads it
and `--native-verify` ignores it. It costs disk (a fraction of a percent of the
pair), and it costs two harness rows: `lodgen_btofree.sh` legs (a) and (b)
compare the whole output tree against a rung exe that predates the file, so
they read `0/1 only on one side` and stay red until that rung is retaken.

**The alternative, stated plainly.** Write the cache only when the run was
asked to be incremental -- only under `--incremental`, or behind a
`--native-cache` switch that ships OFF like every other master. The price is
exact and one-time: the FIRST `--incremental` run after a full bake would find
no cache, every chunk would count as "not in the ledger", and that one run
would rebuild the whole region. From the second run on the behaviour is
identical. So the choice is "one wasted full bake, ever" against "a new file in
the shipped folder, always".

**What exists today either way.** `--no-native-cache` turns the writing off and
makes `--incremental` refuse rather than silently do a full bake under an
incremental's name, so the escape hatch is already in the exe, in `lodgen
--help` and in `docs/LODGEN_LEDGER_FORMAT.md` 4.1.

### 12.2 `--native` came OFF `--incremental`'s whole-region refusal list

**What changed.** `--incremental` used to refuse the `--native` target outright,
so it refused THE ruled default pipeline: the one target anybody bakes was the
one target it would not touch. It now accepts it, and refuses only when
`--no-native-cache` is also given (no cache to replay from).

**Why it is a divergence.** It changes what a switch does to the ruled
pipeline rather than adding a switch. No default MOVED -- `--incremental` is
still opt-in and still off -- but the set of runs it accepts grew, and the
census prints two more lines (`incremental:` and `native cache:`) on those
runs.

**The refuter that keeps it honest.** `tests/spells/lodgen_incremental.sh` arm
(d): a null incremental over an untouched tree must reproduce the full bake's
own bake record under the five-volatile mask, and arm (f) cross-checks that
normalisation against a second implementation in Python. If replay ever drifts
from a real bake, those two go red before anything ships.

## 13. Changelog addendum (for the director, after section 8)

Two more landings after the first gate sweep, each with a gate that was red
before and green after:

- `lodgen`: the per-chunk native cache now reports itself to the layout census,
  so the FO4CS target's `layout <root>, N file(s)` clause counts `.lodj` like
  every other file it writes (`lodgen_layout.sh` leg (f): 22/5 -> 23/0).
- harness: `lodgen_bakerec.sh` leg (h) no longer decides by prefix which
  `census` lines two bakes of one tree may differ on; it asks the mask about
  the one line, so the `peak working set:` clause stops failing a floor that
  the mask already covers (21/3 -> 21/0).

## 14. The pictures

- `E:/Projects/NifskopeWildWastelandEdition/scratchpad/incr1_20260917/images/native_full_vs_incremental.png`
  -- the same cells of the FO4CS object library from a full bake and from a
  null `--incremental`, same camera, 1024x1024 each. MEASURED: 0.000 % of
  pixels differ, and both `.lodi` are sha1
  `48f037cbfcae66f03e7739bbb0c1d0924a2f8011`. Nothing to see is the whole
  result.
- `E:/Projects/NifskopeWildWastelandEdition/scratchpad/incr1_20260917/images/incremental_census.png`
  -- one cell (-22,18) edited on a 16-chunk region: `incremental: 4 of 16
  chunks dirty (1 inputs moved, 0 not in the ledger, 0 output lost, 3 by
  neighbour, 0 with no native chunk cache)` and `native cache: 4 chunk(s)
  written to .lodj, 12 replayed from cache (7657 placement(s)), 0 failure(s),
  0 arrival(s) lit by more than one chunk`. The FIRST attempt framed only 4
  chunks, where the one-cell widening reaches every chunk, so it honestly read
  "4 of 4 dirty, 0 replayed" under a TYPED heading claiming the opposite. The
  caption is now read out of the census lines it sits above
  (`pics_compose.py`), and the region is big enough to have an interior.
  Script: `scratchpad/incr1_20260917/pics2.sh`.
