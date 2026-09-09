#!/usr/bin/env python
"""Diff a .lodt's full-rate height plane against the shadow HeightMap DDS of
the same worldspace, offline, sharing no code with either writer.

Both files claim to hold the SAME surface at the SAME rate:
  .lodt  stored word = height/quantum + 32767, quantum 8 for a FO4 source,
         global sample grid, gy 0 = SOUTH, progressive pyramid.
  DDS    R16_UNORM, pixel = height/8 + 32767, texel row 0 = NORTH,
         cellsX*32 by cellsY*32 at native size.

usage: lodt_vs_heightmap.py <file.lodt> <heightmap.dds> [--dump N]
"""
import struct, sys, zlib
import numpy as np


def read_lodt(path):
    b = open(path, 'rb').read()
    assert b[0:4] == b'LODT', 'not a .lodt'
    ver = struct.unpack_from('<I', b, 4)[0]
    minX, minY, maxX, maxY = struct.unpack_from('<iiii', b, 8)
    spc, be, levels = struct.unpack_from('<III', b, 0x18)
    quant = struct.unpack_from('<f', b, 0x2C)[0]
    (ltexN, watrN, gcvrN, aoS, ovS, flags) = struct.unpack_from('<IIIIII', b, 0x30)
    off = struct.unpack_from('<QQQQQQQQQQ', b, 0x48)
    return dict(buf=b, ver=ver, minX=minX, minY=minY, maxX=maxX, maxY=maxY,
                spc=spc, be=be, levels=levels, quant=quant, flags=flags,
                watrN=watrN, oWatr=off[1], oCell=off[4], oDir=off[7], oData=off[8])


def height_grid(f):
    """Full-rate stored words, [gy][gx], gy 0 = south. Independent of lodtfile.cpp."""
    b, be, levels = f['buf'], f['be'], f['levels']
    cellsX, cellsY = f['maxX'] - f['minX'] + 1, f['maxY'] - f['minY'] + 1
    gw, gh = cellsX * f['spc'], cellsY * f['spc']
    H = np.zeros((gh, gw), np.uint16)
    coarsest = levels - 1
    idx = 0
    for L in range(coarsest, -1, -1):
        s = 1 << L
        bx = (cellsX + (1 << L) - 1) >> L
        by = (cellsY + (1 << L) - 1) >> L
        for j in range(by):
            for i in range(bx):
                o, csz, usz = struct.unpack_from('<QII', b, f['oDir'] + idx * 16)
                idx += 1
                n = be * be if L == coarsest else (be * be * 3) // 4
                raw = zlib.decompress(b[o:o + csz])
                assert len(raw) == usz, 'block %d inflated to %d, header says %d' % (idx, len(raw), usz)
                v = np.frombuffer(raw[:n * 2], np.uint16)
                ox, oy = i * be, j * be
                if L == coarsest:
                    v = v.reshape(be, be)
                    ys = (oy + np.arange(be)) * s
                    xs = (ox + np.arange(be)) * s
                    my, mx = ys < gh, xs < gw
                    H[np.ix_(ys[my], xs[mx])] = v[np.ix_(my, mx)]
                else:
                    v = v.reshape(be // 2, be // 2, 3)
                    ye = (oy + np.arange(0, be, 2)) * s      # parent rows
                    yo = (oy + np.arange(1, be, 2)) * s      # new rows
                    xe = (ox + np.arange(0, be, 2)) * s
                    xo = (ox + np.arange(1, be, 2)) * s
                    for (ys, xs, k) in ((ye, xo, 0), (yo, xe, 1), (yo, xo, 2)):
                        my, mx = ys < gh, xs < gw
                        H[np.ix_(ys[my], xs[mx])] = v[:, :, k][np.ix_(my, mx)]
    assert idx == (f['oData'] - f['oDir']) // 16, 'block count'
    return H


def cell_table(f):
    cellsX, cellsY = f['maxX'] - f['minX'] + 1, f['maxY'] - f['minY'] + 1
    n = cellsX * cellsY
    a = np.frombuffer(f['buf'], np.dtype([('lo', '<f4'), ('hi', '<f4'),
                                          ('wh', '<f4'), ('wt', '<u2'), ('fl', '<u2')]),
                      count=n, offset=f['oCell'])
    return a.reshape(cellsY, cellsX)


def read_dds(path):
    b = open(path, 'rb').read()
    assert b[0:4] == b'DDS ', 'not a DDS'
    h, w = struct.unpack_from('<ii', b, 12)
    fourcc = b[84:88]
    off = 128 + (20 if fourcc == b'DX10' else 0)
    a = np.frombuffer(b, np.uint16, count=w * h, offset=off).reshape(h, w)
    return a, w, h, fourcc


def main():
    lodt, dds = sys.argv[1], sys.argv[2]
    dump = int(sys.argv[sys.argv.index('--dump') + 1]) if '--dump' in sys.argv else 40
    f = read_lodt(lodt)
    cellsX, cellsY = f['maxX'] - f['minX'] + 1, f['maxY'] - f['minY'] + 1
    print('lodt   v%d cells x %d..%d (%d) y %d..%d (%d) spc %d be %d levels %d quantum %g'
          % (f['ver'], f['minX'], f['maxX'], cellsX, f['minY'], f['maxY'], cellsY,
             f['spc'], f['be'], f['levels'], f['quant']))
    D, w, h, cc = read_dds(dds)
    print('dds    %dx%d  %s  %d texels' % (w, h, cc.decode('latin1'), w * h))
    H = height_grid(f)
    print('lodt   grid %dx%d  %d samples' % (H.shape[1], H.shape[0], H.size))
    assert (h, w) == H.shape, 'shape mismatch'

    # CONTROL: the row flip is not assumed. The wrong one must be catastrophic.
    for name, G in (('north-up (row = RY-1-gy)', D[::-1, :]), ('south-up', D)):
        d = int(np.count_nonzero(G != H))
        print('  orientation %-24s  %d of %d texels differ' % (name, d, H.size))
    G = D[::-1, :]
    diff = G != H
    n = int(np.count_nonzero(diff))
    print('DIFFERING TEXELS: %d of %d (%.3g%%)' % (n, H.size, 100.0 * n / H.size))
    if not n:
        return
    ys, xs = np.nonzero(diff)
    ct = cell_table(f)
    HAS_LAND = 2
    cells = {}
    for y, x in zip(ys, xs):
        cx, cy = f['minX'] + x // 32, f['minY'] + y // 32
        cells.setdefault((cx, cy), []).append((int(x % 32), int(y % 32),
                                               int(H[y, x]), int(G[y, x])))
    print('cells touched: %d' % len(cells))
    for (cx, cy), pts in sorted(cells.items()):
        fl = int(ct[cy - f['minY']][cx - f['minX']]['fl'])
        cols = sorted(set(p[0] for p in pts))
        rows = sorted(set(p[1] for p in pts))
        print('  cell (%d,%d) flags 0x%04x hasLand=%d  %d texels  cols %s  rows %s'
              % (cx, cy, fl, bool(fl & HAS_LAND), len(pts),
                 cols if len(cols) < 12 else '%d..%d' % (cols[0], cols[-1]),
                 rows if len(rows) < 12 else '%d..%d' % (rows[0], rows[-1])))
        for p in pts[:dump]:
            print('      col %2d row %2d   lodt %5d (%.0f)   dds %5d (%.0f)'
                  % (p[0], p[1], p[2], (p[2] - 32767) * f['quant'],
                     p[3], (p[3] - 32767) * f['quant']))
    # neighbourhood of the touched cells: who has land?
    print('land flags around the touched cells:')
    for (cx, cy) in sorted(cells):
        for dy in (1, 0, -1):
            row = []
            for dx in (-1, 0, 1):
                x, y = cx + dx, cy + dy
                if f['minX'] <= x <= f['maxX'] and f['minY'] <= y <= f['maxY']:
                    fl = int(ct[y - f['minY']][x - f['minX']]['fl'])
                    row.append('L' if fl & HAS_LAND else '.')
                else:
                    row.append('?')
            print('    %s' % ' '.join(row))
        print('    (centre = cell %d,%d)' % (cx, cy))


if __name__ == '__main__':
    main()
