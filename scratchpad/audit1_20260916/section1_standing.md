### 1.3 The three standing reds the addendum names: all three are the GATE

The addendum asks each of these to be decided with evidence -- stale gate
against wrong product -- and the wrong side fixed, with a refuter. All three
came out the same way, and none of them is a product defect. In each case the
gate is asking a question that a landed ruling has already answered
differently, and in each case the product's own output says so.

**Standing red 3, `lodgen_stage_times.sh` -- 1 FAIL: `and the run wrote the
.lodo/.lodi pair --native asked for`.** The gate looked for
`<native>/Commonwealth.lodo` and `<native>/Commonwealth.lodi`, which is where
they were until lane LAYOUT1 (2026-09-16) moved every FO4CS file to
`<mod>/FO4CSLOD/<worldspace>/`. The bake writes them; the gate looks in the
parent. Decided by the bake's own tree: the run the gate had just made has both
files under `FO4CSLOD/Commonwealth/`, and its census names that root -- `layout
.../FO4CSLOD, N file(s), 0 outside`. The check above it in the same gate, which
reads the `stage times:` line, was green throughout, so the bake itself ran.

FIX (`tests/spells/lodgen_stage_times.sh`): a `pairdir()` helper resolves the
pair under the FO4CS layout with the flat path as a fallback, so the gate
follows the ruling instead of a hardcoded shape; a new check asserts the census
names ONE FO4CS root with zero files outside it, which is the layout contract
itself; and a refuter runs the same test against a tree whose `.lodo` has been
removed and requires it to read 0. **Before: 16 checks / 1 fail, 34 s. After:
18 checks / 0 fail, 39 s**, with `refuter: the same test reads 0 on a tree
whose .lodo is missing` green.

**Standing red 2, `lodgen_btofree.sh` -- 3 FAIL, legs (a), (b) and (c).** Two
different stale assumptions, both about files that landed AFTER the rung the
gate pins its bytes to.

Legs (a) and (b) compare a bake against `NifSkope.before_btofree1.exe` file by
file and failed with `0 differ, 0/1 only on one side` -- nothing differs; one
file exists on the new side only. The gate printed which:
`only in drop: nat/FO4CSLOD/Commonwealth/Commonwealth.4.-20.24.lodj`. That is
lane INCR1's per-chunk native cache, which the rung predates entirely. A file
that did not exist when the baseline was cut is not a byte change in an output.

Leg (c) compares the whole stock output and failed with
`DIFFERS: Commonwealth.lodb -> Commonwealth.lodb (716 vs 2161)`. The rung
writes the version 1 BINARY `LODB` container; this exe writes the version 2
PLAIN-TEXT record, so `cmp` can only ever say they differ. That is the bake
record's own format landing, not the stock path moving -- and the stock path is
exactly what this leg exists to measure. Measured on my own step-2 trees: the
stock record is 6,258 B of plain text with 52 `out` rows, 16 `switch` rows, 9
`chunk` rows and 4 `census` rows; nothing in it is a chunk file.

FIX (`tests/spells/lodgen_btofree.sh`): the three sweeps exclude those files BY
NAME and say so in the check text, and -- this is the part that keeps the
exclusion honest -- each exclusion is then ASSERTED on both sides. Leg (a) now
requires that this exe really did write the `.lodj` and that the rung wrote
none, so the exclusion is proved to cover a new output rather than to hide a
change; leg (b) requires the cache on both sides; leg (c) checks the record
separately, by its own format and size, instead of by `cmp`. Two refuters
append a single byte to a compared file and require the sweep to report it.
**Before: 21 checks / 3 fails, 123 s. After: 27 checks / 0 fails, 146 s.**

**Standing red 1, `lodgen_native.sh` section 5 -- 2 FAIL.** The section exists
to prove that the stock path does not move when `--native` is added. Its first
check does exactly that and was green: `25 stock files compared, 0 differ`.
Both failures are one claim about the bake RECORD --

```
FAIL the second ledger differs from the first ONLY in the command-line digest
     (5399992dfee2 -> 5b6569b33c87)
```

-- the first line printed by `lodgen_btofree_ledger.py` itself, the second the
suite's own verdict word for the same thing, which is why six `FAIL` lines
count as five failures in this gate.

THE PAIR THE CHECK ACTUALLY COMPARES is the stock bake against a
`--native --keep-bto` bake: check 4 spells `--keep-bto` so that there are chunks
on both sides. I reproduced it by hand on my own step-2 Sanctuary records and
got the gate's two digests exactly, `5399992dfee2 -> 5b6569b33c87`, so what
follows is measured on the gate's own question. Reduced to canonical rows, the
two records agree completely:

| | stock record | FO4CS record |
|---|---|---|
| `.DDS` sheets | 27 | 27 |
| `.BTR` terrain chunks | 9 | 9 |
| `.txt` manifests | 8 | 8 |
| `.BTO` object chunks | 8 | 8 (`--keep-bto` is spelled) |
| `.lodj` native cache | 0 | **9** |
| canonical rows shared | 52 | 52, none only on one side |
| **recorded digests that moved** | | **0** |

The `.lodj` rows are lane INCR1's per-chunk native cache, which the stock target
cannot have, and those are already checked-then-dropped by `--fo4cs-vs-stock`.
After that there is not one output file the two records disagree about. So the
red was never about an output at all.

WHAT IT WAS ABOUT. `keep` prints "differs ONLY in the command-line digest" but
runs a whole-document `==` over everything the record reader returns. On this
pair that document differs in ten places and not one is an output row:

| key | stock | FO4CS |
|---|---|---|
| `baked` | two clock readings, seconds apart | |
| `bytes` / `lineCount` | 6,258 / 90 | 10,984 / 113 |
| `target` | `stock` | `fo4cs` |
| `census` | 4 rows | 12 rows |
| `hashes` | 1 (`loadOrderHash`) | 5 (the four corpus digests too) |
| `endFiles` / `endBytes` | 52 / 11,512,374 | 11 / 226,572,422 |
| `switchTokens` | | `--native`, `--keep-bto` |
| `chunks` | the raw `outFiles` / `outDigests` lists | |

The last row is the one that proves this is the gate. Lane BAKEREC1 gave the
record reader `outFiles` and `outDigests` beside `out`; `canon_doc()` and
`strip_digests()` were written before that and only ever knew about `out`. So
every reduction this file exists for -- the `FO4CSLOD/` prefix off, the `../`
hops off, the digests held back so `explained()` can check them one at a time --
was being undone by the raw copies sitting untouched in the same dict, and the
comparison could not survive the LAYOUT1 move no matter what the bake did.

`baked` proves the rest of it in one line. A copy of one stock record with
nothing altered but its clock:

```
$ lodgen_btofree_ledger.py same self.lodb timeshift.lodb
  FAIL the two records name the same outputs, digest for digest
```

Two bakes never share a clock reading, so `keep` and `same` could not pass on
any real pair. They pass in the suite today only where they are skipped -- both
legs that would reach them hit the version-1 rung guard first -- which is why
this stayed invisible until an FO4CS pair had to be compared for real.

FIX (`tests/spells/lodgen_btofree_ledger.py`). Two small changes, both toward
the sentence the check already prints. `canon_doc()` drops the parallel
`outFiles`/`outDigests` lists and sorts the canonical rows, since the order two
bakes happen to write their outputs in is not a property of the bytes and a
sorted multiset still fails on a dropped or duplicated row. And a new `shape()`
sorts every field the reader returns into three groups named in the file:
`SHAPE_KEYS` -- worldspace, region, dim, load order, algorithm knobs, plugins,
resources, chunk rows -- which must always match; `RUN_KEYS`, the per-run
bookkeeping (the clock, the file's own size and line count, which exe, and the
command line, whose digest is the one thing these modes ask about directly);
and `TARGET_KEYS`, what the bake target decides, dropped only under
`--fo4cs-vs-stock`. Digests are untouched and still go one by one through
`explained()`.

The group lists cannot rot into a sweep: `shape()` returns every key that none
of the three names, and the caller goes red listing it. Measured refuter: a
record given a `somethingNew` field reports `unaccounted: ['somethingNew']`.

THE REFUTERS, all four run on the gate's own pair, each red:

| doctored | result |
|---|---|
| one `.BTR` digest in the FO4CS record, leading hex digit swapped 0<->1 | **FAIL** (the gate's own refuter) |
| one `.BTO` output row deleted (113 -> 112 lines) | **FAIL** |
| the stock record given one `.lodj` row | **FAIL**, naming `1 .lodj row(s)` |
| a record field the three groups do not name | **FAIL**, naming the field |

**Before: 31 checks / 5 fails. After: 31 checks / 4 fails, and those four are section 14.** The four remaining failures
are section 14, this lane's own new checks against C1 and C2, which stay red
until the source fix in section 6.
