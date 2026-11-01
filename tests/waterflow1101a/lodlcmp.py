"""Lane FIX1101A flow: is <new> the <old> .lodl with only the WATER FLOW stage changed?

  T  the bytes before the water sections (header up to 0xA0, the cell table and the whole terrain pyramid: every byte
     from 0x110 up to the first water section offset) are equal, and the header's non-water words too;
  P  the water planes decoded: shore, surface and sky planes equal sample for sample; the body-ID plane equal except
     where a sea texel of <old> became a NEW body (id > the old body count: the channel split) in <new>;
  F  the flow plane: changed texels counted (report), and on the old bodies' tables: the count of bodies, the new ones.
usage: python -I lodlcmp.py <new.lodl> <old.lodl>     rc 0 = T and P hold
"""
import struct
import sys
import zlib

import numpy as np


def read(path):
    data = open(path, 'rb').read()
    h = data[:0x110]
    assert h[:4] == b'LODT'
    ver, = struct.unpack_from('<I', h, 4)
    x0, y0, x1, y1, spc = struct.unpack_from('<iiiiI', h, 8)
    cx, cy = x1 - x0 + 1, y1 - y0 + 1
    (oBody, nBody, recB, oName, nName, bodyS, oBodyP, flowS, flowEnc, oFlow, shoreS, shoreQ, oShore, oStroke,
     nStroke, oDye, oSurf) = struct.unpack_from('<QIIQIIQIIQIIQQIIQ', h, 0xA0)
    oSky = struct.unpack_from('<Q', h, 0x100)[0] if ver >= 4 else 0

    def plane(off):
        tX, tY, te, b = struct.unpack_from('<IIII', data, off)
        dOff, _ = struct.unpack_from('<QQ', data, off + 16)
        assert tX == cx and tY == cy
        dt = {1: np.uint8, 2: np.dtype('<u2'), 4: np.dtype('<u4')}[b]
        out = np.zeros((tY * te, tX * te), dtype=dt)
        for j in range(tY):
            for i in range(tX):
                o, cs, us = struct.unpack_from('<QII', data, dOff + (j * tX + i) * 16)
                if cs == 0:
                    out[j * te:(j + 1) * te, i * te:(i + 1) * te] = us
                else:
                    out[j * te:(j + 1) * te, i * te:(i + 1) * te] = np.frombuffer(
                        zlib.decompress(data[o:o + cs]), dtype=dt).reshape(te, te)
        return out
    offs = [o for o in (oBody, oName, oBodyP, oFlow, oShore, oStroke, oDye, oSurf, oSky) if o]
    return dict(data=data, ver=ver, nBody=nBody, first=min(offs), ids=plane(oBodyP), flow=plane(oFlow),
                shore=plane(oShore) if oShore else None, surf=plane(oSurf) if oSurf else None,
                sky=plane(oSky) if oSky else None)


def main():
    N, O = read(sys.argv[1]), read(sys.argv[2])
    ok = True
    print('new %s: %d bytes v%d, %d bodies, water from %d' % (sys.argv[1], len(N['data']), N['ver'], N['nBody'], N['first']))
    print('old %s: %d bytes v%d, %d bodies, water from %d' % (sys.argv[2], len(O['data']), O['ver'], O['nBody'], O['first']))
    t1 = N['data'][:0xA0] == O['data'][:0xA0]
    t2 = N['first'] == O['first'] and N['data'][0x110:N['first']] == O['data'][0x110:O['first']]
    hw = [hex(o) for o in range(0xA0, 0x110, 4) if N['data'][o:o + 4] != O['data'][o:o + 4]]
    print('T  header 0..0xA0 equal %s; bytes 0x110..first water section equal %s (%d bytes); water header words that '
          'differ: %s' % (t1, t2, N['first'] - 0x110, ' '.join(hw) or 'none'))
    if not t2 and N['first'] == O['first']:
        a = np.frombuffer(N['data'][0x110:N['first']], np.uint8)
        b = np.frombuffer(O['data'][0x110:O['first']], np.uint8)
        d = np.flatnonzero(a != b)
        print('   %d differing bytes in the terrain region, first at %d' % (d.size, 0x110 + (d[0] if d.size else 0)))
    ok = ok and t1 and t2
    for k in ('shore', 'surf', 'sky'):
        if O[k] is None and N[k] is None:
            continue
        e = O[k] is not None and N[k] is not None and np.array_equal(N[k], O[k])
        print('P  %-5s plane equal: %s' % (k, e))
        ok = ok and e
    ch = N['ids'] != O['ids']
    oldsea = O['ids'][ch]
    newid = N['ids'][ch]
    split_ok = bool(np.all(newid > O['nBody'])) and bool(np.all(np.isin(oldsea, [1])))
    print('P  body-ID plane: %d texels differ; every one an old sea texel now a new body (> %d): %s; new ids %s'
          % (int(ch.sum()), O['nBody'], split_ok, sorted(set(np.unique(newid).tolist()))[:30]))
    ok = ok and split_ok
    fc = N['flow'] != O['flow']
    print('F  flow plane: %d texels differ (%d inside the split channels, %d elsewhere)' % (
        int(fc.sum()), int((fc & ch).sum()), int((fc & ~ch).sum())))
    print('RESULT', 'WATER-ONLY' if ok else 'NOT WATER-ONLY')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
