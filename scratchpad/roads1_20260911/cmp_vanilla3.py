"""ROADS1 section 1, part 4: the two decisive discriminators, with the AUC
arithmetic done in float (the first pass overflowed int32 and printed AUCs
above 1, which is how the bug announced itself).

  D2  grey/brightness AUC of the projected road footprint against vanilla's own
      colour sheet, with shifted controls and the ceiling.
  D2b The road MATERIAL's own average diffuse colour beside the colour vanilla's
      sheet carries inside the footprint, and beside what it carries outside.
  D5  Vanilla's `_msn` against a normal computed from the LAND heightmap ALONE
      (VHGT parsed here, independently).  If the road is in the normal sheet the
      footprint must disagree with the heightmap far more than the background
      does.

  python cmp_vanilla3.py <masks.npz> <esm> <vanillaDir> <dataRoot> <cx> <cy>
"""

import os
import struct
import sys
import zlib

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, '..', '..', 'tests', 'spells'))
from lodgen_terrain_model import Dds                          # noqa: E402
from lodgen_cover_model import read_fields, record_data, GRUP, REC_HDR  # noqa: E402

FAMS = ('road', 'trees', 'rocks', 'buildings', 'setdressing')
DIM = 4
CELL = 4096.0
SEP = chr(92)


def sheet(path):
    t = Dds(path)
    px, w, h = t._level(0)
    return np.array(px, dtype=np.float32).reshape(h, w, 4)


def tex_mean(dataRoot, rel):
    p = rel.replace(SEP, '/').lstrip('/')
    if not p.lower().startswith('textures/'):
        p = 'textures/' + p
    full = os.path.join(dataRoot, p.replace('/', os.sep))
    if not os.path.isfile(full):
        cur = dataRoot
        for part in p.split('/'):
            try:
                names = os.listdir(cur)
            except Exception:
                return None
            hit = next((n for n in names if n.lower() == part.lower()), None)
            if hit is None:
                return None
            cur = os.path.join(cur, hit)
        full = cur
    try:
        t = Dds(full)
    except Exception:
        return None
    m = min(t.maxMip, 6)
    px, w, h = t._level(m)
    a = np.array(px, dtype=np.float32).reshape(h, w, 4)
    return a[:, :, :3].reshape(-1, 3).mean(0) * 255.0, (w, h), t.fourcc.decode('latin-1')


def auc(score, mask):
    s = score.ravel().astype(np.float64)
    y = mask.ravel()
    order = np.argsort(s)
    ranks = np.empty(len(s), dtype=np.float64)
    ranks[order] = np.arange(1, len(s) + 1, dtype=np.float64)
    n1 = float(y.sum())
    n0 = float(len(y) - n1)
    if n1 == 0 or n0 == 0:
        return float('nan')
    return (ranks[y].sum() - n1 * (n1 + 1) / 2.0) / (n1 * n0)


def shift(m, dx, dy):
    return np.roll(np.roll(m, dy, axis=0), dx, axis=1)


def cell_heights(esmPath, cells):
    """VHGT for the named cells, parsed here: float offset then a 33x33 grid of
    signed byte gradients, row 0 SOUTH, accumulated x8 down the first column and
    then along each row (the format's own rule, re-typed, not imported)."""
    with open(esmPath, 'rb') as f:
        buf = f.read()
    want = set(cells)
    out = {}

    def walkRange(start, end, world, coord):
        i = start
        while i + REC_HDR <= end:
            t = buf[i:i + 4]
            size = struct.unpack_from('<I', buf, i + 4)[0]
            if t == GRUP:
                label = struct.unpack_from('<I', buf, i + 8)[0]
                gtype = struct.unpack_from('<I', buf, i + 12)[0]
                coord = walkRange(i + REC_HDR, i + size,
                                  label if gtype == 1 else world, coord)
                i += size
                continue
            flags = struct.unpack_from('<I', buf, i + 8)[0]
            body = record_data(buf, i + REC_HDR, size, flags)
            if t == b'CELL':
                coord = None
                for ft, fd in read_fields(body):
                    if ft == b'XCLC' and len(fd) >= 8:
                        coord = struct.unpack_from('<ii', fd, 0)
            elif t == b'LAND' and world == 0x3C and coord in want:
                for ft, fd in read_fields(body):
                    if ft == b'VHGT' and len(fd) >= 4 + 33 * 33:
                        off = struct.unpack_from('<f', fd, 0)[0]
                        g = np.frombuffer(fd[4:4 + 33 * 33], dtype=np.int8) \
                            .reshape(33, 33).astype(np.float64)
                        h = np.zeros((33, 33))
                        col = np.cumsum(g[:, 0])
                        for r in range(33):
                            h[r, :] = col[r] + np.cumsum(g[r, :]) - g[r, 0]
                        out[coord] = (h * 8.0 + off * 8.0)
            i += REC_HDR + size
        return coord

    size = struct.unpack_from('<I', buf, 4)[0]
    walkRange(REC_HDR + size, len(buf), 0, None)
    return out


def main(argv):
    npz, esmPath, vanDir, dataRoot, cx, cy = (argv[0], argv[1], argv[2], argv[3],
                                              int(argv[4]), int(argv[5]))
    d = np.load(npz)
    road = d['road']
    col = sheet(os.path.join(vanDir, 'Commonwealth.4.%d.%d.DDS' % (cx, cy)))
    msn = sheet(os.path.join(vanDir, 'Commonwealth.4.%d.%d_msn.DDS' % (cx, cy)))
    N = col.shape[0]

    sat = col[:, :, :3].max(2) - col[:, :, :3].min(2)
    lum = col[:, :, 0] * .2126 + col[:, :, 1] * .7152 + col[:, :, 2] * .0722
    print('=== D2  AUC, float ===')
    for f in FAMS:
        print('   %-12s AUC(grey=-sat) %.3f   AUC(bright) %.3f'
              % (f, auc(-sat, d[f]), auc(lum, d[f])))
    for dx, dy in ((64, 64), (-96, 48), (128, -128), (0, 200), (200, 0)):
        s = shift(road, dx, dy)
        print('   control shifted %4d,%4d  AUC(grey) %.3f  AUC(bright) %.3f  '
              'overlap %.3f' % (dx, dy, auc(-sat, s), auc(lum, s),
                                float((s & road).sum()) / max(1, road.sum())))
    print('   ceiling (mask as its own score) %.3f' % auc(road.astype(float), road))

    print('=== D2b  the road material vs the sheet ===')
    for rel in ('Landscape/Roads/Sanctuary/SancRoad01_d.dds',
                'Landscape/Roads/Sanctuary/SancSW01_d.dds',
                'Landscape/Ground/DriedGrass01_D.dds',
                'Landscape/Ground/DirtGravel01_d.dds'):
        r = tex_mean(dataRoot, rel)
        if r is None:
            print('   %-48s (not found)' % rel)
        else:
            print('   %-48s mean rgb %5.1f %5.1f %5.1f  %s %s'
                  % (rel, r[0][0], r[0][1], r[0][2], r[1], r[2]))
    ins = col[road][:, :3].mean(0) * 255
    bgm = ~(road | d['trees'] | d['rocks'] | d['buildings'] | d['setdressing'])
    outs = col[bgm][:, :3].mean(0) * 255
    print('   vanilla sheet INSIDE the road footprint  rgb %5.1f %5.1f %5.1f'
          % tuple(ins))
    print('   vanilla sheet on the plain background    rgb %5.1f %5.1f %5.1f'
          % tuple(outs))

    print('=== D5  vanilla _msn vs the LAND heightmap alone ===')
    cells = [(gx, gy) for gx in range(cx, cx + DIM) for gy in range(cy, cy + DIM)]
    H = cell_heights(esmPath, cells)
    print('   cells with a LAND record: %d of %d' % (len(H), len(cells)))
    # one (4*32+1)^2 height grid for the chunk, row 0 SOUTH
    n = DIM * 32 + 1
    grid = np.full((n, n), np.nan)
    for (gx, gy), h in H.items():
        ox, oy = (gx - cx) * 32, (gy - cy) * 32
        grid[oy:oy + 33, ox:ox + 33] = np.where(np.isnan(grid[oy:oy + 33, ox:ox + 33]),
                                                h, np.maximum(grid[oy:oy + 33, ox:ox + 33], h))
    step = DIM * CELL / (n - 1)
    gy_, gx_ = np.gradient(grid, step)
    nz = 1.0 / np.sqrt(1.0 + gx_ ** 2 + gy_ ** 2)
    nxh, nyh = -gx_ * nz, -gy_ * nz
    # sample to the texel grid (row 0 NORTH on the sheet, SOUTH on the grid)
    ii = (np.arange(N) + 0.5) * (n - 1) / float(N)
    jj = (np.arange(N) + 0.5) * (n - 1) / float(N)
    J, I = np.meshgrid(jj, ii, indexing='ij')
    J = (n - 1) - J
    i0, j0 = np.clip(I.astype(int), 0, n - 2), np.clip(J.astype(int), 0, n - 2)

    def samp(a):
        return a[j0, i0]

    hx, hy, hz = samp(nxh), samp(nyh), samp(nz)
    vx = msn[:, :, 0] * 2 - 1
    vz = msn[:, :, 1] * 2 - 1          # G is the near-1 channel: UP
    vy = msn[:, :, 2] * 2 - 1
    ln = np.sqrt(vx ** 2 + vy ** 2 + vz ** 2) + 1e-9
    dot = np.clip((vx / ln) * hx + (vy / ln) * hy + (vz / ln) * hz, -1, 1)
    ang = np.degrees(np.arccos(dot))
    for f in FAMS:
        m = d[f]
        if m.sum():
            print('   %-12s |angle(msn, heightmap normal)| mean %6.2f  median %6.2f'
                  % (f, ang[m].mean(), np.median(ang[m])))
    print('   %-12s |angle| mean %6.2f  median %6.2f'
          % ('background', ang[bgm].mean(), np.median(ang[bgm])))
    s = shift(road, 64, 64)
    print('   %-12s |angle| mean %6.2f  median %6.2f'
          % ('road shifted', ang[s].mean(), np.median(ang[s])))
    print('   sign check: the SAME statistic with the heightmap normal replaced '
          'by straight up = %.2f (road) %.2f (background)'
          % (np.degrees(np.arccos(np.clip(vz[road] / ln[road], -1, 1))).mean(),
             np.degrees(np.arccos(np.clip(vz[bgm] / ln[bgm], -1, 1))).mean()))


if __name__ == '__main__':
    main(sys.argv[1:])
