#!/usr/bin/env python3
"""Writes the shipped default ground-fog .recx files (docs/RECX.md) for every weather in the official masters.

  python tools/recx_weathers.py <game Data folder> [--out res/Data/Plugins/FO4CS/Weathers] [--table]

One file per weather RECORD (a DLC entry that only overrides a base-game weather is not a new record: its values
come from the winning entry in the load order below, its file names the defining plugin). The files hold only the
record's id, its EditorID and our values, computed here from the winning entry's fog scalars (FNAM) by one rule,
nothing tuned per weather:

  street haze   a = the records' fog alpha 3000 units away below the height band:
                f = clamp((3000 - near) / (far - near), 0, 1), a = min(max, f ^ power)   (0 when near = far = 0)
  strength      clamp(6 x -ln(1 - min(a, 0.99)), 0.25, 8)     the denser the weather's own haze, the denser its floor
  thickness     clamp(300 + 800 a, 300, 1000) game units        and the deeper the layer
  hours         by day (14:00) from the day scalars; when the night scalars give other values, they are keyed at
                02:00 and the reader interpolates between the two around the clock

Nothing else is read; no game bytes are written.
"""
import json
import math
import os
import struct
import sys
import zlib

MASTERS = ['Fallout4.esm', 'DLCRobot.esm', 'DLCworkshop01.esm', 'DLCCoast.esm', 'DLCworkshop02.esm',
           'DLCworkshop03.esm', 'DLCNukaWorld.esm']


def subrecords(data):
    out, i, big = {}, 0, None
    while i + 6 <= len(data):
        t = data[i:i + 4]
        n = struct.unpack_from('<H', data, i + 4)[0]
        if t == b'XXXX':
            big = struct.unpack_from('<I', data, i + 6)[0]
            i += 6 + n
            continue
        if big is not None:
            n, big = big, None
        out.setdefault(t, data[i + 6:i + 6 + n])
        i += 6 + n
    return out


def read_plugin(path):
    """(masters, [(formid, edid, fnam)] for every WTHR record)"""
    buf = open(path, 'rb').read()
    hsize = struct.unpack_from('<I', buf, 4)[0]
    head = buf[24:24 + hsize]
    masters, i = [], 0
    while i + 6 <= len(head):
        t, n = head[i:i + 4], struct.unpack_from('<H', head, i + 4)[0]
        if t == b'MAST':
            masters.append(head[i + 6:i + 6 + n].split(b'\0')[0].decode('cp1252'))
        i += 6 + n
    out = []
    off = 24 + hsize
    while off < len(buf):
        size, label = struct.unpack_from('<I4s', buf, off + 4)
        if buf[off:off + 4] == b'GRUP' and label == b'WTHR':
            o, end = off + 24, off + size
            while o < end:
                t, ds, flags, fid = struct.unpack_from('<4sIII', buf, o)
                data = buf[o + 24:o + 24 + ds]
                if flags & 0x00040000:
                    data = zlib.decompress(data[4:])
                f = subrecords(data)
                edid = f.get(b'EDID', b'').split(b'\0')[0].decode('cp1252')
                out.append((fid, edid, f.get(b'FNAM', b'')))
                o += 24 + ds
        off += size if buf[off:off + 4] == b'GRUP' else 24 + struct.unpack_from('<I', buf, off + 4)[0]
    return masters, out


def haze(near, far, power, mx):
    """the records' fog alpha 3000 units away, below the height band"""
    if near == 0 and far == 0:
        return 0.0
    f = min(max((3000.0 - near) / (far - near), 0.0), 1.0) if far != near else 1.0
    return min(mx, f ** power) if f > 0 else 0.0


def law(a):
    tau = -math.log(1.0 - min(max(a, 0.0), 0.99))
    return round(min(max(300.0 + 800.0 * a, 300.0), 1000.0)), round(min(max(6.0 * tau, 0.25), 8.0), 2)


def values(fnam):
    """(groundFog block, day haze, night haze)"""
    n = len(fnam) // 4
    F = list(struct.unpack_from('<%df' % n, fnam)) if n else []
    F += [0.0] * (8 - len(F))
    ad = haze(F[0], F[1], F[4], F[6])
    an = haze(F[2], F[3], F[5], F[7])
    td, sd = law(ad)
    tn, sn = law(an)
    g = {'thickness': td, 'strength': sd}
    if (tn, sn) != (td, sd):
        g['hours'] = {'2': {'thickness': tn, 'strength': sn}, '14': {'thickness': td, 'strength': sd}}
    return g, ad, an


def main():
    data = sys.argv[1]
    out = 'res/Data/Plugins/FO4CS/Weathers'
    if '--out' in sys.argv:
        out = sys.argv[sys.argv.index('--out') + 1]
    win = {}    # (defining plugin, local id) -> (edid, fnam, from plugin)
    counts = {}
    for m in MASTERS:
        p = os.path.join(data, m)
        if not os.path.exists(p):
            continue
        masters, recs = read_plugin(p)
        counts[m] = len(recs)
        for fid, edid, fnam in recs:
            idx = fid >> 24
            owner = masters[idx] if idx < len(masters) else m
            key = (owner, fid & 0xFFFFFF)
            prev = win.get(key)
            win[key] = (edid or (prev[0] if prev else ''), fnam or (prev[1] if prev else b''), m)
    os.makedirs(out, exist_ok=True)
    rows = []
    for (owner, lid), (edid, fnam, src) in sorted(win.items(), key=lambda kv: (MASTERS.index(kv[0][0]), kv[1][0].lower())):
        g, ad, an = values(fnam)
        name = edid or '%08X' % lid
        doc = {'entries': [{'name': name, 'weather': '%s|0x%08X' % (owner, lid),
                            'groundFog': g}]}
        with open(os.path.join(out, name + '.recx'), 'w', newline='\n', encoding='utf-8') as fh:
            fh.write(json.dumps(doc, indent='\t') + '\n')
        rows.append((name, owner, lid, src, g, ad, an))
    print('WTHR entries per master: ' + ', '.join('%s %d' % kv for kv in counts.items()) + '; total %d' % sum(counts.values()))
    print('unique weather records: %d; files written: %d -> %s' % (len(win), len(rows), out))
    if '--table' in sys.argv:
        print('| EditorID | record | haze day / night | thickness day / night | strength day / night |')
        print('|---|---|---|---|---|')
        for name, owner, lid, src, g, ad, an in rows:
            n = g.get('hours', {}).get('2', g)
            print('| %s | %s 0x%08X | %.3f / %.3f | %d / %d | %g / %g |' % (name, owner, lid, ad, an, g['thickness'], n['thickness'], g['strength'], n['strength']))


if __name__ == '__main__':
    main()
