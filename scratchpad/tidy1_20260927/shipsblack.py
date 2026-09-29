"""TIDY1 continuation: the new writer rule (lodgenEmissiveShipsBlack, src/lodgen.cpp) mirrored in Python and
applied to the `_g` sheets a real bake shipped, beside gates.py's own black test.

The writer's new test ENCODES the emissive with the same encoder it writes with, then asks whether every
16-texel BC1 block decodes black. The sheets under <bake>/mod ARE those encoded bytes (same inputs, same
encoder), so running the decode half on them predicts what the new exe decides per file.
    python shipsblack.py [bake root]      (default: GROUND1's bakes/on, read only)
"""
import os, struct, sys

ROOT = sys.argv[1] if len(sys.argv) > 1 else r"E:\Projects\NifskopeWWE-ground1\scratchpad\ground1_20260927\bakes\on"


def block_black_new(b):
    # the C++ rule: index 0 = c0, 1 = c1; 2 (and 3 in four-colour mode) mix both; 3 when c0 <= c1 is transparent black
    c0, c1, bits = struct.unpack_from('<HHI', b)
    for i in range(16):
        k = (bits >> (2 * i)) & 3
        black = (c0 == 0) if k == 0 else (c1 == 0) if k == 1 else ((k == 3 and c0 <= c1) or (c0 == 0 and c1 == 0))
        if not black:
            return False
    return True


def block_black_gate(b):
    # gates.py bc1_black: both end points 0
    c0, c1 = struct.unpack_from('<HH', b)
    return c0 == 0 and c1 == 0


rows = []
for dp, dn, fn in os.walk(ROOT):
    for n in fn:
        if not n.lower().endswith('_g.dds'):
            continue
        p = os.path.join(dp, n)
        data = open(p, 'rb').read()
        fourcc = data[84:88]
        off = 148 if fourcc == b'DX10' else 128
        if fourcc == b'DX10':
            fmt = struct.unpack_from('<I', data, 128)[0]
            if fmt not in (70, 71, 72):
                rows.append((n, len(data), 'not BC1 (dxgi %d)' % fmt, '', 0))
                continue
        blocks = [data[i:i + 8] for i in range(off, len(data) - 7, 8)]
        new = sum(1 for b in blocks if not block_black_new(b))
        gate = sum(1 for b in blocks if not block_black_gate(b))
        rows.append((n, len(data), 'DROP' if new == 0 else 'KEEP', 'black' if gate == 0 else 'lit', new, gate,
                     'all-zero-bytes' if not any(data[off:]) else ''))

rows.sort()
drop = [r for r in rows if r[2] == 'DROP']
print('%-52s %10s %5s %6s %9s %9s %s' % ('file', 'bytes', 'new', 'gate', 'lit(new)', 'lit(gate)', ''))
for r in rows:
    print('%-52s %10d %5s %6s %9d %9d %s' % r)
print('new rule: %d of %d dropped, %d B; kept: %s' % (len(drop), len(rows), sum(r[1] for r in drop),
      ', '.join(r[0] for r in rows if r[2] == 'KEEP')))
print('agree with gates.py on every file: %s' % all((r[2] == 'DROP') == (r[3] == 'black') for r in rows))
