
## B2 The refusals -- gate `b2_refusals.sh`, RESULT PASS, 5 arms, 0 failures

A refusal is worth something only if it (1) names itself, (2) exits non-zero and
(3) **bakes nothing**. The third is the one a reader takes on trust and the one
that matters: a refusal that had already written half a chunk would leave an
output tree nobody could reason about. So every arm runs against an empty
out-dir and asserts it is still empty afterwards.

| arm | rc | files written | the sentence it printed |
|---|---|---|---|
| `no-ledger` | 1 | **0** | `--incremental has nothing to diff against -- no ledger at <dir>/Commonwealth.lodb` ... `run the same command once WITHOUT --incremental; every bake writes the ledger` |
| `wrong-shape` | 1 | **0** | `<ledger> describes ... and this run is a different shape` ... `an incremental run must cover exactly the region its ledger covers` |
| `switches` | 1 | **0** | `the switches differ from the ones the ledger was written with, so EVERY chunk is dirty and an incremental run would be a full run with extra bookkeeping` |
| `whole-region` | 1 | **0** | `--atlas, --arrays and --impostors each build ONE region-wide product out of the whole written .BTO list` ... `do them in a separate full pass` |
| `merge-ok` **(negative control)** | **0** | 149 | `incremental: 0 of 25 chunks dirty (0 inputs moved, 0 not in the ledger, 0 output lost, 0 by neighbour)` |

The fifth arm is the one that makes the other four mean anything. The merge is
**on by default**, so without a control proving the default command *runs*, all
four refusal arms would pass just as happily on a build that refused everything
-- which is exactly the build this lane had at 08:30. That is not hypothetical:
see `MISTAKES_ENTRIES.md`.

### B2.1 Two ways this gate was passing and failing for the wrong reasons

Both were caught by one cheap habit -- **printing the refusal sentence beside
every verdict instead of only PASS/FAIL** -- and both are worth more than the
verdict they corrected.

1. **Three arms failed on a stale region.** `REG` was still the 3x3 shape from
   before gate B3's regions were enlarged to 5x5, so every arm tripped the
   *wrong-shape* refusal before reaching the refusal it was testing. The run
   read `5 arms, 3 failures`, which looks like a broken feature and was a broken
   gate.
2. **The `whole-region` refusal was UNREACHABLE, and the arm was passing
   anyway** -- first on the wrong-shape message, then on the switches message.
   `--atlas` is not on the switch-digest skip list (correctly: it changes the
   output), so asking for `--incremental --atlas` against a ledger baked without
   `--atlas` refuses for the *switch* reason and the whole-region check is never
   reached. The arm now bakes a ledger **with** `--atlas` first -- which is the
   path a person would actually walk -- so the digests agree and the only thing
   standing between the run and a quarter-sized atlas is the refusal under test.

An arm that passes for a reason it did not ask about is not evidence. Two of the
five were doing that, and the gate said `PASS` for one of them.

---

## B3 Byte identity -- `b3_identity.sh`, 8 arms, 8 PASS, 0 FAIL

The promise is **the same bytes, not nearly the same bytes**. Every arm bakes
the same edited plugin twice -- once in full, once incrementally against the
previous ledger -- and compares the two output trees file by file and byte by
byte, **including `Commonwealth.lodb` itself**, with no exceptions (an exception
is where a bug would live).

Two regions of 5x5 chunks at dim 4, both with `--cover --roads --road-detail 1`:

* **region A** `-24 16 -5 35`, land guide **off** (the shipping default);
* **region B** `-16 0 3 19`, with Part A's winner **on**
  (`--land-guide aspecthex:1.0 --land-guide-scale 256`) -- so the gate covers
  both switch states of this lane's other feature.

| arm | dirty | inputs moved | output lost | by neighbour | floor: files the edit moved | verdict |
|---|---|---|---|---|---|---|
| `A/null` | 0 of 25 | 0 | 0 | 0 | 0 *(required: control)* | **PASS** |
| `A/land` | 9 of 25 | 1 | 0 | 8 | 5 (`.BTO`, `.BTR`, `.lodb`, 2 sheets) | **PASS** |
| `A/refs` | 9 of 25 | 1 | 0 | 8 | 3 (`.BTO`, manifest, `.lodb`) | **PASS** |
| `A/border` | 16 of 25 | 4 | 0 | 12 | 9 | **PASS** |
| `A/lost` | 6 of 25 | 0 | **1** | 5 | 0 *(required: inputs untouched)* | **PASS** |
| `B/null` | 0 of 25 | 0 | 0 | 0 | 0 *(required: control)* | **PASS** |
| `B/land` | 9 of 25 | 1 | 0 | 8 | 5 | **PASS** |
| `B/refs` | 9 of 25 | 1 | 0 | 8 | 3 (`.BTO`, manifest, `.lodb`) | **PASS** |

The edits are made by `b_esmedit.py` **in a real copy of `Fallout4.esm`**, not
by a loose-file override -- rows 1, 2, 3, 5 and 7 of the dependency map live in
the ESM and nothing else can reach them. `land` raises a gradient byte of a
**zlib-compressed** `LAND` record (37,019 of 37,020 are compressed) and fixes up
the GRUP size chain; `refs` moves a `REFR` 512 units in X; `border` edits four
cells on the region's own outer edge; `lost` deletes one finished `.BTO` from
the tree.

### B3.1 Every arm carries a floor, and one arm's floor was a lie

`incr == full` is **trivially true** for an edit that reached nothing, so each
arm separately asserts the edit moved the full bake's bytes at all, and an arm
whose floor is empty prints **VACUOUS**, never PASS.

That mechanism fired. The first run's two `refs` arms passed with a floor of
**exactly one file: `Commonwealth.lodb` itself**. The moved reference was not
drawn in LOD -- most are not, because an empty MNAM slot drops a ref at every
ring -- so the only thing the edit moved was the input digest, which is the
thing under test. **An arm whose only witness is the artefact being tested is
not a witness**, and it would have been reported as a pass by any reading of the
verdict line.

The fix was to move a reference the bake **demonstrably draws**: `b_pickref.py`
intersects the base bake's own `.BTO.manifest.txt` -- the list of references
that actually reached a chunk -- with the plugin's REFRs in an interior cell, and
`b_esmedit.py moveid` moves that form id. Re-run, both arms now move a real
`.BTO` and its manifest, and still come back byte-identical. The weak plugins are
kept beside the good ones as `*_refs_weak.esm`.

The `null` and `lost` arms invert the floor on purpose: `null` changes nothing,
so a full bake that *moved* would be a **BROKEN CONTROL**; `lost` deletes an
output without touching an input, so the full bake must also be unmoved and what
is on trial is whether the missing file comes back byte for byte. For `lost` the
non-vacuity is carried by the census line -- `1 output lost, 5 by neighbour` --
not by the file diff, and that is said here because the printed floor of `0`
would otherwise read as a vacuous arm.

### B3.2 The census line is what separates "it worked" from "it cheated"

Printed every run, beside every verdict:

```
incremental: 9 of 25 chunks dirty (1 inputs moved, 0 not in the ledger, 0 output lost, 8 by neighbour)
  (-20,20) inputs 0f0440cde767 -> 7bf6b0e2c603
```

`25 of 25` would mean the run is a full bake with extra bookkeeping -- still
correct, still byte-identical, and **not what you asked for**. Byte identity
alone cannot tell those apart; the census can, and it is how this lane found the
defect where the ledger's output digests were taken **before** the merge rewrote
every `.BTO`, so every subsequent run reported all its outputs "lost" and
silently rebaked everything while passing byte identity.

---

## B4 The ledger is deterministic -- `b4_ledger.py`, RESULT PASS, 0 failures

The whole gate rests on this: if the ledger carried a timestamp, a thread id or
an absolute path, B3 would have had to *except* it from the comparison.

* **Two independent full bakes of the same tree, minutes apart, write a
  byte-identical ledger** -- region A 13,402 B, region B 13,341 B. This is B3's
  null arm doing double duty and it is the load-bearing evidence.
* Header checks: `LODB` magic, version 1, `jsonLen + 16 == file size`, reserved
  word zero.
* **Chunk rows sorted by `(cy, cx)`**, not by the order the pass retired them --
  which is thread order and would differ run to run. 25 rows each.
* **Every output path relative**, none absolute, none escaping the ledger's own
  directory except the one documented `../tex/` hop, so a mod folder can be
  moved or copied without invalidating it. 148 output rows for region A, 150 for
  region B.
* No field whose name contains `time`, `date` or `thread`.

A check that fell out of the counts and is worth keeping: region A wrote **149
files and 148 output rows**, region B **151 and 150**. The difference is exactly
one -- the ledger, which is not its own output -- so **every file each bake
wrote is named in the ledger**. Nothing is tracked that was not produced and
nothing produced is untracked.

---

## B5 What it costs -- `b5_times.py` over the gate's own logs

No extra bakes were made for this table: gate B3 already ran a full bake and a
dirty rebake of the same region for every arm.

| arm | full (stage s) | incremental (stage s) | chunks | saved |
|---|---|---|---|---|
| A/base | 29.3 | -- | 25 rows | *(the full bake)* |
| A/null | 24.5 | 0.0 | 0 of 25 | +24.5 s (100 %) |
| A/land | 24.7 | 12.7 | 9 of 25 | +12.0 s (49 %) |
| A/refs | 29.2 | 11.0 | 9 of 25 | +18.2 s (62 %) |
| A/border | 24.0 | 19.1 | 16 of 25 | +4.9 s (20 %) |
| A/lost | 24.3 | 9.5 | 6 of 25 | +14.8 s (61 %) |
| B/base | 78.4 | -- | 25 rows | *(the full bake)* |
| B/null | 45.2 | 0.0 | 0 of 25 | +45.2 s (100 %) |
| B/land | 46.7 | 15.9 | 9 of 25 | +30.8 s (66 %) |
| B/refs | 54.8 | 18.6 | 9 of 25 | +36.2 s (66 %) |

**Read the column heading.** Those are *stage* seconds -- landscape, meshes,
textures, impostors -- and they do **not** include reading the plugin, indexing
the archives, the merge, the far-ring simplify or writing the ledger. `100 %` in
the null row means 100 % of the staged work, not of the clock, and reporting it
without this paragraph would have been the wall-clock lie this feature refuses
to tell.

So the clock was measured directly, region A, same exe, same command:

* **a full bake: 32 s wall** (27.1 s of it staged);
* **a null incremental run of the same region: 1 s wall** -- the price of
  parsing the plugin, reading the ledger and recomputing 25 input digests.

That 1 s is the **fixed cost every incremental run pays**, and it is also close
to the fixed cost every *ordinary* bake now pays, because the ledger is written
whether or not anybody ever uses it. Caveat stated rather than buried: the OS
file cache was hot from dozens of prior bakes of this same region, so 1 s is a
warm-cache number and a cold first run will be slower.

**The saving tracks the dirty fraction, which is the honest headline**: 9 dirty
chunks of 25 is 36 % of the work and buys back about half to two thirds of the
staged time. On a real worldspace a one-cell edit is a few chunks out of
hundreds, and the same arithmetic is worth hours -- but that is an extrapolation
from a region bake, and no whole-Commonwealth incremental run was made.
