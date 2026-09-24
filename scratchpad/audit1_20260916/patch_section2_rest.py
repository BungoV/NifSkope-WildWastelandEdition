"""AUDIT1: section 2.2 and 2.3 -- the null incremental and the stock rung.

Both measured by scratchpad/audit1_20260916/bake_rest.sh, whose whole output is
bake_rest.log. Every number below is read out of that log or out of the trees it
left on disk.
"""
import io
import os
import sys
import tempfile

P = ('E:/Projects/NifskopeWildWastelandEdition/scratchpad/audit1_20260916/'
     'lane_audit1_report.md')
ANCHOR = '## 4. Diff review of the campaign'

NEW = """### 2.2 The null incremental on region (a), over this lane's own fresh bake

The addendum asks for a null `--incremental` over my own fresh bake on region
(a), byte identity confirmed, the census `native-library-build:` line read in
words, and the two wall clocks recorded.

It took two runs to get, and the first one is a confirmed bug rather than a
measurement. `bake_all.sh` spelled the flag LAST, `... --native <dir>
--incremental`, which C8 parses to an empty string; that run full-baked in 44 s
and said `native-library-build: rebuilt (not offered: this is not an
incremental bake)`. Nothing on the console distinguished it from a real
incremental run. The evidence is quoted in section 4.1 and it is mine: I read a
silent full bake as a null incremental, which is a MISTAKES entry.

Spelled properly -- `--incremental <the finished bake>`, over a byte-for-byte
copy of this lane's own `sanctuary_fo4cs` tree -- the run is
`bake/sanctuary_incr2`, log `bake/sanctuary_incr2.log`:

| | full bake | null incremental |
|---|---|---|
| wall clock | **45 s** | **30 s** |
| peak working set (the census's own number) | 1.86 GB | 1.88 GB |
| chunks baked | 9 | **0** (`chunk jobs 0, chunk workers 0`) |
| native cache | 9 written to `.lodj` | **9 replayed** (3,526 placements) |
| files written under the layout | 19 | **2** |
| output files byte-identical to the bake it replayed | | **55 of 55, 0 differ**, same file list |
| the bake record's `chunk`/`out` lines | | md5 `337ae4bf...` on **both** sides |

The record is compared by its content lines rather than by `cmp` because a
record records the RUN -- its own clock, its own switch paths -- so its bytes
must move. Its output rows must not, and they did not.

The census in its own words:

```
incremental: 0 of 9 chunks dirty (0 inputs moved, 0 not in the ledger,
             0 output lost, 0 by neighbour, 0 with no native chunk cache)
native cache: 0 chunk(s) written to .lodj, 9 replayed from cache
              (3526 placement(s)), 0 failure(s)
native-library-build: rebuilt (occluders are on and the per-model box is in
                      neither file)
```

**And that last line is a finding, not a formality.** Nothing was dirty and
every chunk was replayed, yet the object library was rebuilt from scratch: of
the 30 s, `models 18.0 s` and `ladder 8.5 s` are the rebuild, 26.5 s of a 28.2 s
mesh stage. The exe names the reason itself -- occluders are ON by default and
the per-model occluder box is stored in neither the `.lodo` nor the `.lodi`, so
there is nothing on disk to reload it from. This is the incremental twin of
PERF1 row C, and it is carried into section 5 as a design gap rather than a
defect: the library reuse that exists cannot be offered under the shipped
defaults, in a full bake OR in a null incremental, and the census says so in
words every time rather than quietly reusing nothing. It is measured, not read:
the two runs differ by 15 s, and the 15 s is the nine chunks, not the library.

### 2.3 The stock target against the rung exe: byte-identical on all three regions

The stock target is a hard byte gate, so all three regions were re-baked with
`release/NifSkope.before_perf1.exe` (22,534,144 B, 2026-09-17 08:55:33 -- the
build before the audited one) on the same command line without `--native`, and
compared file by file against this lane's own step-2 stock trees:

| region | rung wall | files | identical | differ | file-list mismatch |
|---|---|---|---|---|---|
| (a) sanctuary | 7 s | 53 | **52** | **0** | none |
| (b) coast | 11 s | 55 | **54** | **0** | none |
| (c) urban | 20 s | 55 | **54** | **0** | none |

The one file excluded from each row is the `.lodb` bake record, and it is
excluded for a stated reason rather than for convenience: the rung writes the
version 1 BINARY `LODB` container and this exe writes the version 2 plain-text
record (lane BAKEREC1), so `cmp` between them can only ever say they differ. The
same exclusion, with the same reason, is what `lodgen_btofree.sh` leg (c) makes
-- see section 1.3 -- and there it is asserted by format and size rather than
waved through.

Every other byte of the stock target is unchanged across the two builds, on a
wooded region, a region that is 44.9 % water and downtown Boston.

"""


def main():
    s = io.open(P, encoding='utf-8', newline='').read()
    if s.count(ANCHOR) != 1:
        print('ABORT: the section 4 anchor appears %d times' % s.count(ANCHOR))
        return 1
    if '### 2.2 The null incremental' in s:
        print('ABORT: section 2.2 is already in the report')
        return 1
    out = s.replace(ANCHOR, NEW + ANCHOR, 1)
    if out.count('\r') != s.count('\r'):
        print('ABORT: CR count moved')
        return 1
    d = os.path.dirname(P)
    f = tempfile.NamedTemporaryFile('w', encoding='utf-8', newline='', dir=d,
                                    delete=False, suffix='.tmp')
    f.write(out)
    f.close()
    os.replace(f.name, P)
    print('report: %d -> %d bytes, CR %d' % (len(s), len(out), out.count('\r')))
    return 0


if __name__ == '__main__':
    sys.exit(main())
