"""Lane PRTP1 -- the LIGHT census, read straight out of the plugin (the gate side).

Independent of NifSkope: its own GRUP walk (no import from the generator).
One pass over the whole plugin:
  - every LIGH base: DATA radius, colour, flags, falloff exponent, FOV, FNAM fade;
  - every CELL: exterior (world, XCLC x,y) or interior (EDID), XCLL present,
    LTMP lighting template;
  - every REFR whose base is a LIGH: its cell, position, XRDS radius override,
    XLIG block present.
Writes <out>.tsv (one row per light ref, sorted) and prints the census.

Usage: python light_census.py <plugin.esm> <out-prefix>
"""
import struct
import sys
import zlib
from collections import Counter

GRUP = b'GRUP'
COMPRESSED = 0x00040000
DELETED = 0x00000020
INIT_DISABLED = 0x00000800


def fields(buf, off, size):
    end = off + size
    big = 0
    while off + 6 <= end:
        t = buf[off:off + 4]
        sz = struct.unpack_from('<H', buf, off + 4)[0]
        off += 6
        if t == b'XXXX':
            big = struct.unpack_from('<I', buf, off)[0]
            off += sz
            continue
        if big:
            sz, big = big, 0
        yield t, buf[off:off + sz]
        off += sz


def rec_fields(buf, off, size, flags):
    if flags & COMPRESSED:
        data = zlib.decompress(buf[off + 4:off + size])
        return list(fields(data, 0, len(data)))
    return list(fields(buf, off, size))


def cstr(p):
    return p.split(b'\0')[0].decode('cp1252', 'replace')


def main():
    path, out = sys.argv[1], sys.argv[2]
    buf = open(path, 'rb').read()
    lighs = {}        # form -> dict
    cells = {}        # form -> dict
    worlds = {}       # form -> edid
    refs = []         # (cellForm, refForm, baseForm, pos, xrds, xlig, flags)
    pending = []      # REFR (cell, form, flags, off, size) -- base resolved after the walk

    stack = []        # (end, label, gtype)
    off = 24 + struct.unpack_from('<I', buf, 4)[0]
    n = len(buf)
    while off + 24 <= n:
        while stack and off >= stack[-1][0]:
            stack.pop()
        t = buf[off:off + 4]
        size = struct.unpack_from('<I', buf, off + 4)[0]
        if t == GRUP:
            label = struct.unpack_from('<I', buf, off + 8)[0]
            gtype = struct.unpack_from('<i', buf, off + 12)[0]
            stack.append((off + size, label, gtype))
            off += 24
            continue
        flags, form = struct.unpack_from('<II', buf, off + 8)
        doff = off + 24
        if t == b'LIGH':
            d = {'edid': '', 'radius': None}
            for ft, p in rec_fields(buf, doff, size, flags):
                if ft == b'EDID':
                    d['edid'] = cstr(p)
                elif ft == b'DATA' and len(p) >= 16:
                    d['radius'] = struct.unpack_from('<I', p, 4)[0]
                    d['color'] = tuple(p[8:11])
                    d['flags'] = struct.unpack_from('<I', p, 12)[0]
                    if len(p) >= 28:
                        d['falloff'], d['fov'], d['near'] = struct.unpack_from('<3f', p, 16)
                elif ft == b'FNAM' and len(p) >= 4:
                    d['fade'] = struct.unpack_from('<f', p, 0)[0]
            lighs[form] = d
        elif t == b'WRLD':
            for ft, p in rec_fields(buf, doff, size, flags):
                if ft == b'EDID':
                    worlds[form] = cstr(p)
        elif t == b'CELL':
            d = {'edid': '', 'xy': None, 'xcll': False, 'ltmp': 0, 'world': None, 'interior': False}
            for gend, label, gtype in stack:
                if gtype == 1:
                    d['world'] = label
            for ft, p in rec_fields(buf, doff, size, flags):
                if ft == b'EDID':
                    d['edid'] = cstr(p)
                elif ft == b'XCLC' and len(p) >= 8:
                    d['xy'] = struct.unpack_from('<ii', p, 0)
                elif ft == b'XCLL':
                    d['xcll'] = True
                elif ft == b'LTMP' and len(p) >= 4:
                    d['ltmp'] = struct.unpack_from('<I', p, 0)[0]
                elif ft == b'DATA' and len(p) >= 1:
                    d['interior'] = bool(p[0] & 1)
            cells[form] = d
        elif t == b'REFR':
            cell = None
            for gend, label, gtype in stack:
                if gtype in (6, 8, 9, 10):
                    cell = label
            pending.append((cell, form, flags, doff, size))
        off = doff + size

    for cell, form, flags, doff, size in pending:
        if flags & DELETED:
            continue
        fl = rec_fields(buf, doff, size, flags)
        base = next((struct.unpack_from('<I', p, 0)[0] for ft, p in fl if ft == b'NAME' and len(p) >= 4), None)
        if base not in lighs:
            continue
        pos = next((struct.unpack_from('<3f', p, 0) for ft, p in fl if ft == b'DATA' and len(p) >= 12), (0.0, 0.0, 0.0))
        xrds = next((struct.unpack_from('<f', p, 0)[0] for ft, p in fl if ft == b'XRDS' and len(p) >= 4), None)
        xlig = any(ft == b'XLIG' for ft, p in fl)
        refs.append((cell, form, base, pos, xrds, xlig, flags))

    def cell_key(c):
        d = cells.get(c)
        if d is None:
            return 'unknown:%08X' % (c or 0)
        if d['interior']:
            return 'int:%s' % d['edid']
        return 'ext:%s:%d,%d' % (worlds.get(d['world'], '%08X' % (d['world'] or 0)), *(d['xy'] or (0, 0)))

    rows = []
    for cell, form, base, pos, xrds, xlig, flags in refs:
        L = lighs[base]
        rows.append('%s\t%08X\t%08X\t%s\t%.1f\t%.1f\t%.1f\t%s\t%s\t%s\t%s\n' % (
            cell_key(cell), form, base, L['edid'], pos[0], pos[1], pos[2],
            L['radius'], '' if xrds is None else '%.1f' % xrds, int(xlig),
            int(bool(flags & INIT_DISABLED))))
    rows.sort()
    with open(out + '.tsv', 'w', encoding='utf8') as fh:
        fh.write('cell\tref\tbase\tbaseEdid\tx\ty\tz\tradius\txrds\txlig\tinitDisabled\n')
        fh.writelines(rows)

    interiors = [c for c in cells.values() if c['interior']]
    per_cell = Counter(cell_key(r[0]) for r in refs)
    lf = Counter()
    for L in lighs.values():
        f = L.get('flags', 0)
        lf['shadowSpot' if f & 0x400 else 'shadowHemi' if f & 0x800 else 'shadowOmni' if f & 0x1000
           else 'spot' if f & 0x4000 else 'box' if f & 0x20000 else 'omni'] += 1
    print('LIGH bases %d (%s)' % (len(lighs), ' '.join('%s=%d' % kv for kv in sorted(lf.items()))))
    print('cells %d: interior %d (XCLL %d, LTMP %d), exterior %d' % (
        len(cells), len(interiors), sum(c['xcll'] for c in interiors), sum(1 for c in interiors if c['ltmp']),
        len(cells) - len(interiors)))
    print('light refs %d: interior %d, exterior %d, unknown cell %d; XRDS %d, XLIG %d, initially disabled %d' % (
        len(refs), sum(1 for k in per_cell.elements() if k.startswith('int:')),
        sum(1 for k in per_cell.elements() if k.startswith('ext:')),
        sum(1 for k in per_cell.elements() if k.startswith('unknown')),
        sum(1 for r in refs if r[4] is not None), sum(1 for r in refs if r[5]), sum(1 for r in refs if r[6] & INIT_DISABLED)))
    print('cells with lights %d; most lit: %s' % (len(per_cell), ', '.join('%s=%d' % kv for kv in per_cell.most_common(4))))


if __name__ == '__main__':
    main()
