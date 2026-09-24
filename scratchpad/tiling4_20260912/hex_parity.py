"""TILING4 -- the prototype half of the hex-tiling parity check.

It evaluates h_cand.py's OWN `tri_grid` and `hash01` (imported, not re-typed --
if the prototype is edited this check follows it) plus the same blend formula,
at the same fifteen world positions x five settings as hex_parity.cpp, and diffs
the two.

FLOORS, asserted rather than eyeballed:
  * the OFF block must equal the plain wrapped tap exactly -- "off is the rung's
    bytes" is this switch's whole safety argument;
  * every ON block must DIFFER from OFF at a majority of the fifteen positions,
    or the comparison is passing on a sampler that does nothing;
  * a deliberately WRONG C++ (the skew's sign flipped) must fail this check --
    run with --sabotage to see it fail, which is the only proof the check can
    fail at all.

    python hex_parity.py            ->  logs/hex_parity.txt
    python hex_parity.py --sabotage ->  the same check against a wrong prototype
"""
import os
import subprocess
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(os.path.dirname(HERE), 'tiling3_20260911'))
sys.path.insert(0, os.path.join(os.path.dirname(HERE), 'tiling2_20260911'))
sys.path.insert(0, os.path.join(os.path.dirname(HERE), 'splat1_20260911'))
import h_cand as H                                            # noqa: E402

TILE = 341.3333
POS = [(0.0, 0.0), (1.0, -1.0), (341.3333, 341.3333),
       (-81920.0, 98304.0), (-81920.0, 81920.0), (-147456.0, -81920.0),
       (-16384.0, -81920.0), (114688.0, -81920.0), (-16384.0, 65536.0),
       (98304.0, 65536.0), (1999999.0, -1999999.0), (12345.678, -98765.432),
       (-0.5, -0.5), (-1024.0, -1024.0), (1023.9999, 1023.9999)]
SIZES = [np.float32(256.0), np.float32(341.3333), np.float32(512.0),
         np.float32(682.6667)]
MEAN = np.array([127.5, 127.5, 127.5])


def fake_tap(u, v):
    return np.array([255.0 * u, 255.0 * v, 255.0 * u * v])


def wrap(x, tile=TILE):
    u = np.fmod(x / tile, 1.0)
    return u + 1.0 if u < 0.0 else u


def py_off(wx, wy):
    return fake_tap(wrap(wx), wrap(wy))


def py_hex(wx, wy, size, sabotage=False):
    """The prototype's own tri_grid and hash01, on one scalar position."""
    px = np.array([wx], np.float64) / np.float64(size)
    py = np.array([wy], np.float64) / np.float64(size)
    if sabotage:
        # the deliberate defect: the skew applied with the wrong sign
        sx = px + 0.57735026918962576 * py
        sy = 1.15470053837925152 * py
        bi = np.floor(sx)
        bj = np.floor(sy)
        tx, ty = sx - bi, sy - bj
        tz = 1.0 - tx - ty
        up = tz > 0.0
        w = (np.where(up, tz, -tz), np.where(up, ty, 1.0 - ty),
             np.where(up, tx, 1.0 - tx))
        o = np.where(up, 0.0, 1.0)
        verts = (((bi + o).astype(np.int64), (bj + o).astype(np.int64)),
                 ((bi + o).astype(np.int64), (bj + 1.0 - o).astype(np.int64)),
                 ((bi + 1.0 - o).astype(np.int64), (bj + o).astype(np.int64)))
    else:
        w, verts = H.tri_grid(px, py)
    acc = np.zeros(3)
    wsq = 0.0
    cells = []
    for k, (i, j) in enumerate(verts):
        ox = H.hash01(i, j, 0)[0] * TILE
        oy = H.hash01(i, j, 1)[0] * TILE
        s = fake_tap(wrap(wx + ox), wrap(wy + oy))
        acc += (s - MEAN) * float(w[k][0])
        wsq += float(w[k][0]) ** 2
        cells.append((int(i[0]), int(j[0])))
    if wsq > 1e-12:
        acc = acc / np.sqrt(wsq)
    return cells, [float(x[0]) for x in w], MEAN + acc


def main():
    sab = '--sabotage' in sys.argv
    cpp = os.path.join(HERE, 'logs', 'hex_parity_cpp.txt')
    if not os.path.exists(cpp):
        print('REFUSED: %s is missing -- compile and run hex_parity.cpp first'
              % cpp)
        return 1
    lines = [l.strip() for l in open(cpp) if l.strip()]
    L = ['TILING4 -- hex tiling: src/lodgen.cpp vs the prototype', '',
         'fifteen world positions x five settings; the C++ side is',
         'hex_parity.cpp (lodgenLandHexCell and lodgenLandHexOffset copied',
         'verbatim), the prototype side is h_cand.py`s own tri_grid and hash01.',
         'Both run in double: this checks the geometry, the hash, the vertex',
         'triple and the blend formula, not the product`s last float bit.', '']
    if sab:
        L.append('--sabotage: the prototype side has the SKEW SIGN FLIPPED, so')
        L.append('this run MUST fail -- it is the proof the check can fail.')
        L.append('')

    it = iter(lines)
    head = next(it)
    assert head == 'OFF', head
    bad = 0
    off = []
    for n, (wx, wy) in enumerate(POS):
        got = [float(x) for x in next(it).split()]
        want = py_off(wx, wy)
        off.append(got)
        d = max(abs(g - w) for g, w in zip(got, want))
        if d > 1e-6:
            bad += 1
            L.append('   OFF (%g,%g): C++ %s vs prototype %s' % (wx, wy, got, want))
    L.append('OFF: %d of 15 positions disagree (floor 0).' % bad)

    nmoved = []
    for s, size in enumerate(SIZES):
        head = next(it)
        assert head.startswith('SIZE='), head
        b = 0
        moved = 0
        for n, (wx, wy) in enumerate(POS):
            f = next(it).split()
            cvi = [(int(f[0]), int(f[1])), (int(f[2]), int(f[3])),
                   (int(f[4]), int(f[5]))]
            cw = [float(x) for x in f[6:9]]
            co = [float(x) for x in f[9:12]]
            pcells, pw, po = py_hex(wx, wy, size, sab)
            dcell = 0 if cvi == pcells else 1
            dw = max(abs(a - b2) for a, b2 in zip(cw, pw))
            do = max(abs(a - b2) for a, b2 in zip(co, po))
            if dcell or dw > 1e-9 or do > 1e-6:
                b += 1
                L.append('   size %g (%g,%g): cells %s vs %s, dw %.3g, dcol %.3g'
                         % (float(size), wx, wy, cvi, pcells, dw, do))
            if max(abs(a - b2) for a, b2 in zip(co, off[n])) > 1e-6:
                moved += 1
        nmoved.append(moved)
        L.append('size %-9g %d of 15 positions disagree (floor 0); the sampler '
                 'moved %d of 15 off the plain tap (floor 8)'
                 % (float(size), b, moved))
        bad += b

    L.append('')
    ok = bad == 0 and all(m >= 8 for m in nmoved)
    L.append('PARITY: %s -- %d disagreements over 75 comparisons, and every '
             'setting moves the sample.' % ('PASS' if ok else 'FAIL', bad))
    if sab:
        L.append('(and with the sabotage in place a PASS here would mean the '
                 'check cannot fail)')
    txt = '\n'.join(L) + '\n'
    print(txt)
    name = 'hex_parity_sabotage.txt' if sab else 'hex_parity.txt'
    with open(os.path.join(HERE, 'logs', name), 'w', newline='\n') as f:
        f.write(txt)
    return 0 if ok else 2


if __name__ == '__main__':
    sys.exit(main())
