# VT1 item 4: V9c, measured rather than argued.
#
# This is check V9c of tests/spells/lodgen_terrain_vt.sh, lifted unchanged (the
# same decode, the same columns, the same bars) so it can be run against several
# DIRECT bakes and the numbers compared side by side. V9c reads the _msn sheets
# of the DIRECT bake only, so it cannot see the pyramid path at all.
#
# usage: v9c.py <tex-dir> [label] [<tex-dir> [label] ...]
import struct, sys

RATIO_EW = 3.20
RATIO_NS = 3.30
EDGE_MAX = 2.60
CTL_LO, CTL_HI = 1.20, 2.20


def c565(c):
    return ((((c >> 11) & 31) * 255 + 15) // 31,
            (((c >> 5) & 63) * 255 + 31) // 63,
            ((c & 31) * 255 + 15) // 31)


def bc1(path):
    b = open(path, 'rb').read()
    assert b[:4] == b'DDS ', path
    h, w = struct.unpack_from('<II', b, 12)
    off = 148 if b[84:88] == b'DX10' else 128
    px = [(0, 0, 0)] * (w * h)
    p = off
    for by in range((h + 3) // 4):
        for bx in range((w + 3) // 4):
            c0, c1 = struct.unpack_from('<HH', b, p)
            idx = struct.unpack_from('<I', b, p + 4)[0]
            p += 8
            p0, p1 = c565(c0), c565(c1)
            if c0 > c1:
                pal = [p0, p1,
                       tuple((2 * p0[k] + p1[k]) // 3 for k in range(3)),
                       tuple((p0[k] + 2 * p1[k]) // 3 for k in range(3))]
            else:
                pal = [p0, p1, tuple((p0[k] + p1[k]) // 2 for k in range(3)), (0, 0, 0)]
            for j in range(4):
                for i in range(4):
                    x, y = bx * 4 + i, by * 4 + j
                    if x < w and y < h:
                        px[y * w + x] = pal[(idx >> (2 * (j * 4 + i))) & 3]
    return px, w, h


def mad(a, b):
    return sum(max(abs(p[k] - q[k]) for k in range(3)) for p, q in zip(a, b)) / float(len(a))


def col(t, x):
    px, w, h = t
    return [px[y * w + x] for y in range(h)]


def row(t, y):
    px, w, h = t
    return [px[y * w + x] for x in range(w)]


def measure(d):
    W = bc1(d + '/Commonwealth.4.-24.24_msn.DDS')
    E = bc1(d + '/Commonwealth.4.-20.24_msn.DDS')
    S = W
    N = bc1(d + '/Commonwealth.4.-24.28_msn.DDS')
    w = W[1]
    seam_x = mad(col(W, w - 1), col(E, 0))
    ctl_x = sum(mad(col(t, x), col(t, x + 1)) for t in (W, E) for x in (100, 200, 300, 400)) / 8.0
    edge_x = (mad(col(W, w - 2), col(W, w - 1)) + mad(col(E, 0), col(E, 1))) / 2.0
    seam_y = mad(row(S, 0), row(N, w - 1))
    ctl_y = sum(mad(row(t, y), row(t, y + 1)) for t in (S, N) for y in (100, 200, 300, 400)) / 8.0
    edge_y = (mad(row(S, 0), row(S, 1)) + mad(row(N, w - 2), row(N, w - 1))) / 2.0
    # the same statistic one column FURTHER IN, to tell a seam defect from a
    # sheet that is simply noisier everywhere
    far_x = mad(col(W, w - 3), col(W, w - 2))
    return dict(seam_x=seam_x, ctl_x=ctl_x, edge_x=edge_x, far_x=far_x,
                seam_y=seam_y, ctl_y=ctl_y, edge_y=edge_y)


def verdict(m):
    f = []
    if not (CTL_LO <= m['ctl_x'] <= CTL_HI) or not (CTL_LO <= m['ctl_y'] <= CTL_HI):
        f.append('control')
    if m['ctl_x'] and m['seam_x'] / m['ctl_x'] > RATIO_EW:
        f.append('E/W ratio')
    if m['ctl_y'] and m['seam_y'] / m['ctl_y'] > RATIO_NS:
        f.append('N/S ratio')
    if max(m['edge_x'], m['edge_y']) > EDGE_MAX:
        f.append('edge step')
    return f


def main(argv):
    jobs = []
    i = 1
    while i < len(argv):
        d = argv[i]
        lab = argv[i + 1] if i + 1 < len(argv) else d
        jobs.append((d, lab))
        i += 2
    print('%-34s %8s %8s %6s | %8s %8s %6s | %8s %8s' %
          ('direct bake', 'E/W seam', 'interior', 'ratio', 'N/S seam', 'interior',
           'ratio', 'edge E/W', 'edge N/S'))
    for d, lab in jobs:
        m = measure(d)
        print('%-34s %8.3f %8.3f %6.2f | %8.3f %8.3f %6.2f | %8.3f %8.3f   %s' %
              (lab, m['seam_x'], m['ctl_x'], m['seam_x'] / m['ctl_x'],
               m['seam_y'], m['ctl_y'], m['seam_y'] / m['ctl_y'],
               m['edge_x'], m['edge_y'],
               'FAIL ' + ', '.join(verdict(m)) if verdict(m) else 'pass'))
        print('%-34s   the step one column further in (w-3 -> w-2) is %.3f, against the '
              'edge step %.3f' % ('', m['far_x'], m['edge_x']))
    print()
    print('bars: interior control must be %.2f..%.2f, E/W ratio <= %.2f, N/S ratio <= %.2f, '
          'edge step <= %.2f' % (CTL_LO, CTL_HI, RATIO_EW, RATIO_NS, EDGE_MAX))
    print('pinned 2026-09-09 on the old look: interior 1.803/1.797 E/W, 1.603/1.606 N/S; '
          'E/W ratio 2.75 ringed vs 4.07 clamped; edge step 1.961 vs 3.310')
    return 0


sys.exit(main(sys.argv))
