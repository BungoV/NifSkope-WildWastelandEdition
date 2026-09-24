#!/usr/bin/env python3
"""Why the field-exact BSTriShape walk refuses some blocks: is the descriptor
wrong, or is there simply data AFTER the triangles (BSSubIndexTriShape
segments, BSDynamicTriShape vertices)?  Prints the dsize identity per block
type and, where it holds, how many bytes trail the triangle array."""
import collections
import os
import struct
import sys

import tangent_census as tc


def main():
    root = r'E:/Tools/Fallout 4/DataUnpacked/Data'
    dump = '../cellview3_20260919/dump_downtown.txt'
    seen, models = set(), []
    for ln in open(dump):
        if ln.startswith('#'):
            continue
        f = ln.split()
        if len(f) < 21:
            continue
        m = ' '.join(f[20:])
        if m not in seen:
            seen.add(m)
            models.append(m)
    cnt = collections.Counter()
    tail = collections.Counter()
    for m in models:
        p = os.path.join(root, 'meshes', m.replace('\\', os.sep))
        if not os.path.exists(p):
            d, n = os.path.split(p)
            for f in os.listdir(d):
                if f.lower() == n.lower():
                    p = os.path.join(d, f)
                    break
        bl = tc.blocks(p)
        if bl is None:
            continue
        for t, pp in bl:
            if t not in tc.TRISHAPES:
                continue
            try:
                q = 4
                nx = struct.unpack_from('<I', pp, q)[0]
                q += 4 + 4 * nx + 4
                q += 4 + 12 + 36 + 4 + 4
                q += 16 + 4 + 4 + 4
                v = struct.unpack_from('<Q', pp, q)[0]
                q += 8
                ntri = struct.unpack_from('<I', pp, q)[0]
                q += 4
                nv = struct.unpack_from('<H', pp, q)[0]
                q += 2
                ds = struct.unpack_from('<I', pp, q)[0]
                q += 4
            except Exception:                    # noqa: BLE001
                cnt[(t, 'short')] += 1
                continue
            sz = (v & 0xF) * 4
            ok = (ds == nv * sz + ntri * 6)
            cnt[(t, ok)] += 1
            if ok:
                tail[(t, len(pp) - (q + ds))] += 1
    for k, c in sorted(cnt.items(), key=repr):
        print('dsize identity %-24s %-6s %d' % (k[0], k[1], c))
    print()
    for k, c in sorted(tail.items(), key=repr)[:24]:
        print('trailing bytes %-24s %8d  x%d' % (k[0], k[1], c))
    return 0


if __name__ == '__main__':
    sys.exit(main())
