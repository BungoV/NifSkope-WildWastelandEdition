"""Pick a reference the bake DEMONSTRABLY DRAWS, and say which cell it lives in.

    python b_pickref.py <manifest.txt> <esm> <x0> <y0> <x1> <y1>

Intersects the base bake's manifest (the refs that reached a `.BTO`) with the
plugin's own REFRs in the given cell range, and prints the first match as
`<cx> <cy> <formid>` for b_esmedit.py's `moveid` mode.

This exists because gate B3's first run moved the first REFR in a cell and that
ref was not drawn: the arm's floor was the ledger file alone.
"""
import sys

sys.path.insert(0, 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/land1_20260912')

import struct  # noqa: E402

import b_esmedit as E  # noqa: E402


def main(argv):
    man, esm = argv[0], argv[1]
    x0, y0, x1, y1 = (int(v) for v in argv[2:6])

    drawn = set()
    for line in open(man, encoding='utf-8', errors='replace'):
        if line.startswith('#'):
            continue
        f = line.split()
        # data rows only: 'I', 'A', 'M' rows are group/layer/material lines.
        if not f or f[0] in ('I', 'A', 'M'):
            continue
        if len(f) >= 2 and f[-1] == '-1':
            drawn.add(f[-2].lower())

    want = {(x, y) for x in range(x0, x1 + 1) for y in range(y0, y1 + 1)}
    buf, found = E.scan(esm, want, types=(b'REFR',))
    for cell, _t, rs, _sz, fl, _st in found:
        if fl & E.COMPRESSED:
            continue
        form = struct.unpack_from('<I', buf, rs + 12)[0]
        if ('%08x' % form) in drawn:
            print('%d %d %08x' % (cell[0], cell[1], form))
            return 0
    print('REFUSED: no drawn, uncompressed REFR in that cell range')
    return 2


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
