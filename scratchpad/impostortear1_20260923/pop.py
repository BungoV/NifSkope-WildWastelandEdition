"""IMPOSTORTEAR1 -- the popping bar and the tear numbers, from the orbit grabs.

    python pop.py <popdir> [runA runB ...]

<popdir>/<run>/<subject>/<az|el>/v_az%03d_el%02d_{card,mesh}.png, written by
sweep.sh (card = debug channel 2, mesh = LOD channel 8, clear colour exact).
A pixel is COVERED when it differs from the clear colour at (0,0).

POPPING (pre-registered in progress.md before any number was read):
  step = XOR count of covered card pixels between consecutive views
         (az wraps 359 -> 0; el does not);
  the mesh's own step printed beside it (the floor no card beats);
  bar per subject x sweep: worst step of runB <= worst step of runA x 1.5.
TEAR (az sweep, el 20, the torn azimuth of each subject):
  IoU card vs mesh; with <popdir>/near/<subject>/list (WW_IMPOSTOR_BLEND=0)
  torn share = |mesh & nearest & ~card| / |mesh & nearest|.
Also the mean IoU over the whole az sweep, so a win at one view is not a loss
everywhere else.
"""
import sys, os
import numpy as np
from PIL import Image

pop = sys.argv[1]
runs = sys.argv[2:] or ['run_ship', 'run_fix']
SUBJ = {'blast_n4': 30, 'maple_n4': 210, 'rock_n4': 30}


def mask(p):
    a = np.asarray(Image.open(p).convert('RGB'))
    return (a != a[0, 0]).any(-1)


def views(d, sw, torn):
    return [(a, 20) for a in range(360)] if sw == 'az' else [(torn, e) for e in range(90)]


def name(d, a, e, k):
    return os.path.join(d, 'v_az%03d_el%02d_%s.png' % (a, e, k))


res = {}
for r in runs:
    for s, torn in SUBJ.items():
        for sw in ('az', 'el'):
            d = os.path.join(pop, r, s, sw)
            if not os.path.isdir(d):
                continue
            vs = views(d, sw, torn)
            C = [mask(name(d, a, e, 'card')) for a, e in vs]
            M = [mask(name(d, a, e, 'mesh')) for a, e in vs]
            n = len(vs)
            pairs = [(i, (i + 1) % n) for i in range(n if sw == 'az' else n - 1)]
            cs = np.array([(C[i] ^ C[j]).sum() for i, j in pairs])
            ms = np.array([(M[i] ^ M[j]).sum() for i, j in pairs])
            iou = np.array([(C[i] & M[i]).sum() / max(1, (C[i] | M[i]).sum()) for i in range(n)])
            area = np.mean([c.sum() for c in C])
            w = int(cs.argmax())
            tv = vs.index((torn, 20)) if (torn, 20) in vs else None
            res[(r, s, sw)] = dict(worst=int(cs.max()), mean=float(cs.mean()), at=vs[pairs[w][0]],
                                   mworst=int(ms.max()), mmean=float(ms.mean()), area=area,
                                   iou=iou, vs=vs, tC=C[tv] if tv is not None else None,
                                   tM=M[tv] if tv is not None else None)
            del C, M
            print('%-8s %-9s %s: card step worst %6d at az%03d el%02d, mean %7.1f | mesh step worst %6d mean %7.1f | card area %7.0f | IoU mean %.4f min %.4f'
                  % (r, s, sw, cs.max(), vs[pairs[w][0]][0], vs[pairs[w][0]][1], cs.mean(), ms.max(), ms.mean(), area, iou.mean(), iou.min()),
                  flush=True)

if len(runs) >= 2:
    A, B = runs[0], runs[1]
    print('\nPOPPING BAR (%s worst <= %s worst x 1.5):' % (B, A))
    allok = True
    for s in SUBJ:
        for sw in ('az', 'el'):
            if (A, s, sw) in res and (B, s, sw) in res:
                a, b = res[(A, s, sw)]['worst'], res[(B, s, sw)]['worst']
                ok = b <= 1.5 * a
                allok &= ok
                print('  %-9s %s: %6d vs %6d x1.5 = %8.1f  ratio %.2f  %s' % (s, sw, b, a, 1.5 * a, b / max(1, a), 'PASS' if ok else 'FAIL'))
    print('  VERDICT', 'PASS' if allok else 'FAIL')

print('\nTEAR at the torn views (el 20):')
for s, torn in SUBJ.items():
    nd = os.path.join(pop, 'near', s, 'list')
    Nm = mask(name(nd, torn, 20, 'card')) if os.path.isfile(name(nd, torn, 20, 'card')) else None
    for r in runs:
        k = (r, s, 'az')
        if k not in res:
            continue
        i = torn
        C, M = res[k]['tC'], res[k]['tM']
        line = '  %-9s az%03d %-8s IoU %.4f  card %6d mesh %6d' % (s, torn, r, res[k]['iou'][i], C.sum(), M.sum())
        if Nm is not None:
            base = M & Nm
            line += '  torn share %.1f%% (%d of %d mesh&nearest px)' % (100.0 * (base & ~C).sum() / max(1, base.sum()), (base & ~C).sum(), base.sum())
        print(line)
