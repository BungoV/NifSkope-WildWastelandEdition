"""impostor_draw.sh rows 17 and 18 (lane IMPOSTORTEAR1, 2026-09-23).

    python impostor_tear_pop.py tear <blend dir> <nearest dir> <views>
    python impostor_tear_pop.py pop  <views> <dir> [<dir> ...]

Every dir holds the orbit harness's grabs, v_az%03d_el%02d_{card,mesh}.png,
card through debug channel 2 (the coverage the cut read, so a discarded pixel
is the clear colour) and mesh through LOD channel 8 (view-space normal, so no
mesh pixel can match the clear colour). A pixel is COVERED when it differs
from the clear colour at (0,0).

TEAR: the share of the pixels where the MESH is covered AND the nearest frame
alone (WW_IMPOSTOR_BLEND=0) is covered that the BLENDED card leaves open:
    torn = |mesh & nearest & ~card| / |mesh & nearest|, summed over the views.
Those are pixels the card itself says are solid, that the object really fills,
and that the drawer tore out. It is the number behind bungo's "like somebody
ripped out a piece of paper".

POP: per dir, the per-step change in covered card pixels over consecutive
views (XOR count; the list wraps only when it is a full 360-degree ring), its
worst and mean, and the MESH's own worst step beside it (the floor no card
beats). One line per dir, in the order given.
"""
import sys, os
import numpy as np
from PIL import Image


def mask(p):
    a = np.asarray(Image.open(p).convert('RGB'))
    return (a != a[0, 0]).any(-1)


def views(s):
    return [tuple(int(x) for x in v.split(':')) for v in s.split(',') if ':' in v]


def name(d, a, e, k):
    return os.path.join(d, 'v_az%03d_el%02d_%s.png' % (a, e, k))


def need(p):
    if not os.path.isfile(p):
        print('REFUSED: missing grab %s' % p)
        sys.exit(2)
    return p


cmd = sys.argv[1]
if cmd == 'tear':
    bd, nd, vs = sys.argv[2], sys.argv[3], views(sys.argv[4])
    torn = base = 0
    worst = (0.0, None)
    for a, e in vs:
        C = mask(need(name(bd, a, e, 'card')))
        M = mask(need(name(bd, a, e, 'mesh')))
        N = mask(need(name(nd, a, e, 'card')))
        b = M & N
        t = b & ~C
        torn += int(t.sum()); base += int(b.sum())
        sh = t.sum() / max(1, b.sum())
        if sh > worst[0]:
            worst = (sh, (a, e))
    share = torn / max(1, base)
    print('views %d  mesh&nearest px %d  torn px %d' % (len(vs), base, torn))
    print('worst view az%03d el%02d torn %.4f' % (worst[1][0], worst[1][1], worst[0]) if worst[1] else 'worst view none')
    print('torn share %.4f' % share)
elif cmd == 'pop':
    vs = views(sys.argv[2])
    ring = len(vs) == 360 and len({e for _, e in vs}) == 1
    for d in sys.argv[3:]:
        prevC = prevM = firstC = firstM = None
        cs, ms = [], []
        for a, e in vs:
            C = mask(need(name(d, a, e, 'card')))
            M = mask(need(name(d, a, e, 'mesh')))
            if prevC is None:
                firstC, firstM = C, M
            else:
                cs.append(int((C ^ prevC).sum())); ms.append(int((M ^ prevM).sum()))
            prevC, prevM = C, M
        if ring:
            cs.append(int((firstC ^ prevC).sum())); ms.append(int((firstM ^ prevM).sum()))
        w = int(np.argmax(cs))
        print('pop %s  card worst %d at step %d  mean %.1f  mesh worst %d mean %.1f  steps %d'
              % (os.path.basename(d.rstrip('/\\')), max(cs), w, float(np.mean(cs)), max(ms), float(np.mean(ms)), len(cs)))
else:
    sys.exit('usage: tear|pop')
