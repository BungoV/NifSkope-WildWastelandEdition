"""AUDIT1: put the RECORDED evidence for C8 next to the claim.

The paragraph already says this lane's own null-incremental bake full-baked. The
bake record proves it in its own words, which is better than my recollection of
it, so the tokens and the two census lines go in verbatim.
"""
import io
import os
import sys
import tempfile

P = ('E:/Projects/NifskopeWildWastelandEdition/scratchpad/audit1_20260916/'
     'lane_audit1_report.md')
ANCHOR = ("It has a MISTAKES entry of its own (mine, for reading a silent full "
          "bake as a null incremental) and a gate written against it, "
          "`lodgen_incremental.sh` arm (g).")
ADD = """

The bake record of that run is the evidence, in the product's own words. Its
last two `switch` rows, read back out of
`bake/sanctuary_incr/FO4CSLOD/Commonwealth/Commonwealth.lodb`, are

```
--native   E:/.../bake/sanctuary_incr
--incremental
```

-- the flag recorded with nothing after it, which is the empty value C8
describes reaching the incremental driver. The same run's census then says

```
native cache: 9 chunk(s) written to .lodj, 0 replayed from cache (0 placement(s))
native-library-build: rebuilt (not offered: this is not an incremental bake)
```

so the exe did not merely fail to reuse the previous bake: it never saw an
incremental bake at all. Wall clock 44 s against the full bake's 45 s, over a
tree that was a byte-for-byte copy of a finished bake of the same region. A run
that reused everything and a run that reused nothing are one second apart, which
is why this is worth a refusal rather than a warning -- there is no number on
the console that would have told me."""


def main():
    s = io.open(P, encoding='utf-8', newline='').read()
    if s.count(ANCHOR) != 1:
        print('ABORT: the C8 paragraph anchor appears %d times' % s.count(ANCHOR))
        return 1
    if 'never saw an\nincremental bake at all' in s:
        print('ABORT: the evidence is already in the report')
        return 1
    out = s.replace(ANCHOR, ANCHOR + ADD, 1)
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
