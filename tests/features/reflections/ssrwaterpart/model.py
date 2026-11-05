"""lane PERFFIX1: the SSR water ray pass with a particle system in the sorted list, as a GL mask machine (after
notes/refldraws1/model/maskmodel.py). Old = Scene::drawDeferredShapes( the whole sorted list ); new = cellssr.cpp's
lean walk: the water in its order, then the other sorted draws until one has shut the masks, the particles only when
none did. Pass = on every random sequence both write the same colour events (index, kind) into the ray target.

Items (the sorted list, any order before drawDeferredShapes groups it):
  D = a decal Shape that reaches its setup (wwCellSsrDraw: both masks shut)     d = a decal that returns before it
  W = a water Shape (colour mask open, depth mask off, writes a ray)
  S = another Shape that reaches its setup with a non-water program (masks shut)
  O = another Shape that reaches its setup with a WATER program (opens the colour mask, writes). It cannot occur:
      renderer.cpp setupProgram picks a water program only under wwCellWaterWanted( shape ), the predicate of
      Shape::wwWaterDraws, so every such Shape is in the water group. `--with-o` adds it: the lean walk then FAILS.
  E = another Shape that returns before its setup (no state change, no write)
  P = a particle system (no setup: writes colour if the colour mask is open; depth mask on after its draw)
Start state: colour shut, depth mask off (cellssr.cpp: glColorMask false, glDepthMask false before the list).

    python -I tests/features/reflections/ssrwaterpart/model.py [green|anyshape|wateronly]
green passes; the reds must FAIL: anyshape = the first lean rule (any non-water Shape in the list means the particles
write nothing: wrong when every such Shape returns before its setup), wateronly = the water alone, particles dropped."""
import random, sys

def step(k, cm, dm, i, ev):
    if k in 'DS':
        return False, False
    if k in 'WO':
        ev.append((i, 'c'))
        return True, False
    if k == 'P':
        if cm:
            ev.append((i, 'c'))
        return cm, True
    return cm, dm	# d, E

def old(lst):
    """drawDeferredShapes: decals, water, the rest (not particles), particles"""
    ev, cm, dm = [], False, False
    order = [x for x in lst if x[1] in 'Dd'] + [x for x in lst if x[1] == 'W'] + \
        [x for x in lst if x[1] in 'SOE'] + [x for x in lst if x[1] == 'P']
    for i, k in order:
        cm, dm = step(k, cm, dm, i, ev)
    return ev

def new(lst, mode):
    ev, cm, dm = [], False, False
    for i, k in [x for x in lst if x[1] == 'W']:
        cm, dm = step(k, cm, dm, i, ev)
    if mode == 'wateronly':
        return ev
    if mode == 'anyshape':
        if any(k in 'SOE' for _, k in lst):
            return ev
        for i, k in [x for x in lst if x[1] == 'P']:
            cm, dm = step(k, cm, dm, i, ev)
        return ev
    shut = False
    for i, k in [x for x in lst if x[1] in 'SOE']:
        if shut:
            break
        cm, dm = step(k, cm, dm, i, ev)
        if k == 'S':
            shut = True	# wwCellSsrDraw: s.waterRayShut = !water
        elif k == 'O':
            shut = False
    if not shut:
        for i, k in [x for x in lst if x[1] == 'P']:
            cm, dm = step(k, cm, dm, i, ev)
    return ev

def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else 'green'
    rng = random.Random(1101)
    kinds, w = 'DdWSOEP', [3, 1, 3, 6, 1 if '--with-o' in sys.argv else 0, 2, 3]
    N = 200000
    bad = lean = 0
    first = None
    for _ in range(N):
        lst = [(rng.randrange(1 << 30), rng.choices(kinds, w)[0]) for _ in range(rng.randrange(1, 14))]
        if not any(k == 'P' for _, k in lst):
            lst.append((rng.randrange(1 << 30), 'P'))	# the pass reaches this walk only with a particle in the list
        a, b = old(lst), new(lst, mode)
        if sorted(a) != sorted(b) or a != b:
            bad += 1
            first = first or ''.join(k for _, k in lst)
        lean += 1 if any(k == 'S' for _, k in lst) else 0
    print('%s: %d sequences, %d with a shut-masks draw (the lean case), %d differ%s' % (
        mode, N, lean, bad, ' (first: %s)' % first if first else ''))
    ok = bad == 0
    print('SSRWATERPART MODEL %s %s' % ('PASS' if ok else 'FAIL', mode))
    return 0 if ok else 1

if __name__ == '__main__':
    sys.exit(main())
