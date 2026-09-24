"""AUDIT1: replace the standing-red-1 paragraph of section 1.3.

The draft was written from the .lodj half of the difference and claimed the
mode fixed the red. The gate was then run and it did not. This rewrite is the
measured account: the pair the gate actually compares, the ten keys that differ
on it, and the two gate defects behind them.
"""
import io
import os
import sys
import tempfile

P = ('E:/Projects/NifskopeWildWastelandEdition/scratchpad/audit1_20260916/'
     'section1_standing.md')
HEAD = '**Standing red 1, `lodgen_native.sh` section 5 -- 2 FAIL.**'

NEW = """**Standing red 1, `lodgen_native.sh` section 5 -- 2 FAIL.** The section exists
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

**Before: 31 checks / 5 fails. After: %(after)s.** The four remaining failures
are section 14, this lane's own new checks against C1 and C2, which stay red
until the source fix in section 6.
"""


def main():
    after = sys.argv[1] if len(sys.argv) > 1 else '31 checks / 4 fails'
    s = io.open(P, encoding='utf-8', newline='').read()
    if s.count(HEAD) != 1:
        print('ABORT: the standing-red-1 head appears %d times' % s.count(HEAD))
        return 1
    if 'canonical rows shared' in s:
        print('ABORT: section 1.3 already carries the rewrite')
        return 1
    body = NEW % {'after': after}
    out = s[:s.index(HEAD)] + body
    if out.count('\r'):
        print('ABORT: CR in the result')
        return 1
    d = os.path.dirname(P)
    f = tempfile.NamedTemporaryFile('w', encoding='utf-8', newline='', dir=d,
                                    delete=False, suffix='.tmp')
    f.write(out)
    f.close()
    os.replace(f.name, P)
    print('section1_standing.md: %d -> %d bytes, CR %d'
          % (len(s), len(out), out.count('\r')))
    return 0


if __name__ == '__main__':
    sys.exit(main())
