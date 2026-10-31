"""Lane FARWATER4: judges of lodgen's per-texel river bake (tests/farwater4/run.sh). Exit 0 = PASS, 1 = FAIL.
  f5 <file>                 L5: on every writer-solved river of >= 1000 texels, F5's seam share (> 10 deg) < 5 percent
                            and p99 < 20 deg (pre-registered from the raw bake: seams 5.6-30.7 %, p99 28-69); the
                            written bake holds it on 12 of 14 (bodies 19, 20: seams 7.3 / 6.2 %), a registered FAIL
  better <new> <red>        L5: every such river has fewer seams and no higher p99 than in <red>, median p99 halved
  dironly <new> <dirred>    L5 touches the DIRECTION only: ids equal, flow words equal outside bits 0..7
  floor <new> <floorred>    L4: the words differ only where the red wrote speed 0 and the new file 1 (a solved
                            texel), every other bit equal; and the new file has fewer solved texels at speed 0
  same <a> <b>              the id and flow planes are equal"""
import os
import sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lodlflow


def f5(path):
    L = lodlflow.Lodl(path)
    bad = 0
    rs = lodlflow.rivers(L)
    p99s = []
    for b, y0, y1, x0, x1, m in rs:
        d = L.flow[y0:y1, x0:x1] & 0xFF
        p99, seam, n = lodlflow.f5(m, d)
        ok = seam < 0.05 and p99 < 20.0
        bad += not ok
        p99s.append(p99)
        print('  body %4d: %6d solved texels, F5 p99 %6.2f seam %6.3f%% over %d pairs  %s' % (
            b, m.sum(), p99, 100 * seam, n, 'ok' if ok else 'FAIL'))
    print('  %d rivers of >= 1000 texels, %d fail; p99 median %.2f max %.2f' % (
        len(rs), bad, float(np.median(p99s)) if p99s else 0, max(p99s) if p99s else 0))
    return len(rs) > 0 and bad == 0


def better(a, b):
    """every river of >= 1000 texels of `a` has a lower seam share and no higher p99 than in `b`, and the median
    p99 over them is at most half of b's"""
    A, B = lodlflow.Lodl(a), lodlflow.Lodl(b)
    pa, pb, worse = [], [], 0
    for bid, y0, y1, x0, x1, m in lodlflow.rivers(A):
        r1 = lodlflow.f5(m, A.flow[y0:y1, x0:x1] & 0xFF)
        r0 = lodlflow.f5(m, B.flow[y0:y1, x0:x1] & 0xFF)
        pa.append(r1[0])
        pb.append(r0[0])
        if not (r1[1] < r0[1] and r1[0] <= r0[0]):
            worse += 1
            print('  body %d: p99 %.2f vs %.2f, seam %.3f%% vs %.3f%%: not better' % (bid, r1[0], r0[0], 100 * r1[1], 100 * r0[1]))
    ma, mb = float(np.median(pa)) if pa else 0, float(np.median(pb)) if pb else 0
    print('  %d rivers: %d not better; p99 median %.2f against %.2f' % (len(pa), worse, ma, mb))
    return len(pa) > 0 and worse == 0 and ma <= 0.5 * mb


def dironly(a, b):
    A, B = lodlflow.Lodl(a), lodlflow.Lodl(b)
    ids = np.array_equal(A.ids, B.ids)
    hi = np.array_equal(A.flow & 0xFF00, B.flow & 0xFF00)
    nd = int(((A.flow & 0xFF) != (B.flow & 0xFF)).sum())
    print('  id planes equal %s; speed + confidence equal %s; %d direction bytes differ' % (ids, hi, nd))
    return ids and hi and nd > 0


def floor(a, b):
    A, B = lodlflow.Lodl(a), lodlflow.Lodl(b)
    ids = np.array_equal(A.ids, B.ids)
    diff = A.flow != B.flow
    sa, sb = (A.flow >> 8) & 0xF, (B.flow >> 8) & 0xF
    lifted = diff & (sb == 0) & (sa == 1) & (((A.flow >> 12) & 0xF) == 8) & ((A.flow & 0xF0FF) == (B.flow & 0xF0FF))
    other = int((diff & ~lifted).sum())
    solvedA = ((A.flow >> 12) & 0xF) == 8
    solvedB = ((B.flow >> 12) & 0xF) == 8
    za = int((solvedA & (sa == 0) & (A.ids > 0)).sum())
    zb = int((solvedB & (sb == 0) & (B.ids > 0)).sum())
    print('  id planes equal %s; %d words lifted 0 -> 1, %d other differences; solved texels at speed 0: %d (red %d of %d)' % (
        ids, int(lifted.sum()), other, za, zb, int((solvedB & (B.ids > 0)).sum())))
    return ids and other == 0 and int(lifted.sum()) > 0 and za < zb


def same(a, b):
    A, B = lodlflow.Lodl(a), lodlflow.Lodl(b)
    e1, e2 = np.array_equal(A.ids, B.ids), np.array_equal(A.flow, B.flow)
    print('  id planes equal %s, flow planes equal %s (%d words differ)' % (e1, e2, int((A.flow != B.flow).sum())))
    return e1 and e2


if __name__ == '__main__':
    fn = {'f5': f5, 'better': better, 'dironly': dironly, 'floor': floor, 'same': same}[sys.argv[1]]
    sys.exit(0 if fn(*sys.argv[2:]) else 1)
