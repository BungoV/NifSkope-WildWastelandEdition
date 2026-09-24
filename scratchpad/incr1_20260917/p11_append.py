#!/usr/bin/env python
# INCR1 -- append sections 11, 12, 13 and 14 to the lane report. Pure LF in,
# pure LF out, measured by byte count both sides; refuses if the sections are
# already there so a second run cannot double them.
#
#   python p11_append.py <lane_incr1_report.md> <s12_body.txt>
import io
import sys

LF = chr(10)

BODY_A = """## 11. The neighbour gates, measured: before and after

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
"""

BODY_B = """## 13. Changelog addendum (for the director, after section 8)

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
"""


def main():
    rep, s12 = sys.argv[1], sys.argv[2]
    old = io.open(rep, "rb").read().decode("utf-8")
    assert chr(13) not in old, "report already has CR"
    assert "## 11." not in old, "section 11 already present"
    assert "## 12." not in old, "section 12 already present"
    mid = io.open(s12, "rb").read().decode("utf-8")
    assert chr(13) not in mid, "section 12 body has CR"
    for part in (BODY_A, BODY_B):
        assert chr(13) not in part
    out = (old.rstrip(LF) + LF + LF
           + BODY_A.rstrip(LF) + LF + LF
           + mid.rstrip(LF) + LF + LF
           + BODY_B.rstrip(LF) + LF)
    io.open(rep, "wb").write(out.encode("utf-8"))
    b = io.open(rep, "rb").read()
    sys.stdout.write("report %d bytes, CR %d, LF %d%s"
                     % (len(b), b.count(chr(13).encode()),
                        b.count(LF.encode()), LF))
    return 0


if __name__ == "__main__":
    sys.exit(main())
