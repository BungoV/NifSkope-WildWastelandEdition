"""AUDIT1: replace C2's constructed evidence with a REAL aggregate bake.

The paragraph in 4.1 said the version-5 aggregate path could not be reached by a
bake in this lane and had to be reached by construction. That was true of the
step-2 trees and it is no longer true: lane SHOWCASE1 left a real ortho card
library on disk, `--aggregate` accepts it, and the bake writes exactly the file
C2 is about. Constructed evidence is replaced by the product's own output.
"""
import io
import os
import sys
import tempfile

P = ('E:/Projects/NifskopeWildWastelandEdition/scratchpad/audit1_20260916/'
     'lane_audit1_report.md')
HEAD = 'C2 -- `--aggregate` refuses without a real card library'
TAIL = 'prints `aggregateStride 0` for a version-5 file that carries aggregates, which is how this was first seen.'

NEW = """C2 -- and this one is no longer constructed at all: **the product writes the file itself, on a
default command line, and then misreads it.** `--aggregate` refuses without a real card library
(`src/nifcli.cpp:3857`), and lane SHOWCASE1 left one on disk -- 23 octahedral tree cards under
`scratchpad/showcase1_20260912/cards`, `oct 8`, `tile 512`, every one of them `projection ortho`
(independently decoded, section 3). Region (a) baked against it,
`--native <out> --impostors <cards> --aggregate` and no other switch, lands in
`scratchpad/audit1_20260916/bake/aggreal` at rc 0, and the census is happy:

```
aggregate cards: 23 tree bases with a usable ortho card set; refused 0 with no set in
  .../showcase1_20260912/cards, 0 baked through a perspective camera, 0 with no octahedral grid
```

The `.lodi` it wrote, read byte by byte at the header offsets in `src/lodifile.cpp:48-50`:

| field | offset | value |
|---|---|---|
| version | 0x04 | **5** |
| `offAggregates` / `offCovered` | 0xB0 / 0xB8 | 131,072 / 139,264 |
| `aggregateCount` / `coveredCount` | 0xC0 / 0xC4 | **97** / **3,423** |
| `aggregateStride` / `aggregateViews` | 0xC8 / 0xCA | **48** / 8 |
| `aggregateSwitch` / `aggregateBand` | 0xCC / 0xD0 | 96.0 / 1.2 |
| `offPlacementAo` / `placementAoCount` | 0xE4 / 0xEC | 155,648 / 3,526 |

Ninety-seven aggregates and 3,423 covered instances, in a file whose version is 5 because placement
AO has been on by default since DEFAULTS1. Every aggregate RULE in the reader is gated on
`h.version == LODI_VERSION_AGGREGATE`, which is 4. So on this file -- a real bake, shipped defaults,
no doctoring -- not one of them runs.

The product says so itself. `--native-verify` over that pair returns **rc 0**, and among the fields
it prints back is

```
lodi aggregateStride 0
```

while the bytes at 0xC8 hold 48. The reporting site (`src/lodifile.cpp:1289`) shares the
version-4 gate with the rules, so the exe's own report of the file contradicts the exe's own
bytes, on a file the exe had just written. That is C2 end to end, with no constructed input
anywhere in the chain.

The two doctored version-5 files of `lodgen_native.sh` section 14 stay as the gate, because a gate
needs an input that is WRONG and a real bake's aggregates are right: `aggregateViews 1` --
**accepted**, rc 0; an all-zero record with HEIGHT clear, no identity bit and no extent --
**accepted**, rc 0. The version-4 reader refuses both by name. The fix in section 6 must therefore
do two things and is measured on both: turn those two red, and leave `bake/aggreal` verifying."""


def main():
    s = io.open(P, encoding='utf-8', newline='').read()
    if s.count(HEAD) != 1 or s.count(TAIL) != 1:
        print('ABORT: anchors %d / %d' % (s.count(HEAD), s.count(TAIL)))
        return 1
    if 'bake/aggreal' in s:
        print('ABORT: the real-bake evidence is already in the report')
        return 1
    a = s.index(HEAD)
    b = s.index(TAIL) + len(TAIL)
    out = s[:a] + NEW + s[b:]
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
