#!/usr/bin/env python3
"""Judge the Volumetric Air's optimisation (lane VOLAIR1, coordinator 10-14; src/gl/cellvolair.cpp, cell_volair.frag):
the clustered light lists and the conservative depth skip against the unoptimised path.

  python tests/spells/volair1_opt_check.py <dump dir> <prefix>...

Per prefix (a place: the shots <prefix>_full, _full2, _cl, _both, _dropcl, _nearskip; each a .png, its .bin air dump
with .bin.txt, and the launch's .notes), all from one exe, one camera, one weather:
  full   WW_CELL_AIR_OPT=0 (every light, every froxel)   full2  the same again (the launches' own flake)
  cl     OPT=1 (clustered lights alone)                  both   OPT=3 (clusters + depth skip, the shipped default)
  dropcl OPT=3 + WW_CELL_AIR_RED=dropcluster (red)       nearskip OPT=3 + WW_CELL_AIR_RED=nearskip (red)
  zcolour OPT=3 + WW_CELL_AIR_RED=zcolour (red, 10-16: the skip's columns from the obscurance pass's colour, as before)
  nofarclamp OPT=3 + WW_CELL_AIR_RED=nofarclamp (red, 10-16: the surfaces' lookup not clamped to the column's farthest)

Stages (bars fixed before the first run):
  Q  clusters   cl's injected and integrated volumes equal full's bit for bit, and cl.png == full.png byte for byte
                (or within full vs full2). The clusters must have been built (opt line: clusters > 0).
  Z  depth      both's injected volume equals cl's on every froxel it did not skip; a skipped froxel holds exactly
                (0, 0, 0, 1); both.png == cl.png byte for byte (or within full vs full2). The skip must have been on
                (depthskip=1) and have skipped >= 1% of the froxels for the stage to have teeth.
  C  cost       the pass's ms (glFinish to glFinish, the median over the launch's frames) full vs both: reported.
Reds (each must FAIL its stage): dropcl -> Q (the volume differs from full's), nearskip -> Z (the picture differs),
zcolour -> Z (the picture differs: a fragment behind a draw that writes no depth reads a skipped froxel),
nofarclamp -> Z (a sliver's fragment shaded past its own depth reads a skipped froxel).
A missing shot fails its stage (X). Exit 0 when the greens pass and every red fails.
"""
import os
import re
import sys

import numpy as np

try:
    from PIL import Image
except ImportError:
    Image = None


def load_bin(p):
    b = np.fromfile(p, dtype=np.uint8)
    c, r, s = np.frombuffer(b[:12].tobytes(), dtype=np.int32)
    v = np.frombuffer(b[12:].tobytes(), dtype=np.float32)
    n = int(c) * int(r) * int(s) * 4
    if v.size < 2 * n:
        raise ValueError('short dump %s' % p)
    inj = v[:n].reshape(int(s), int(r), int(c), 4)
    integ = v[n:2 * n].reshape(int(s), int(r), int(c), 4)
    return inj, integ


def load_txt(p):
    out = {}
    try:
        for line in open(p, encoding='utf-8', errors='replace'):
            k, _, rest = line.strip().partition(' ')
            if k and k not in out:
                out[k] = rest
    except OSError:
        pass
    return out


def opt_fields(t):
    return dict(re.findall(r'(\w+)=([-0-9.]+)', t.get('opt', '')))


def ms_of(notes, txt):
    vals = []
    try:
        for line in open(notes, encoding='utf-8', errors='replace'):
            if 'volair=on' in line:
                vals += [float(x) for x in re.findall(r' ms=([0-9.]+)', line)]
    except OSError:
        pass
    vals = [v for v in vals if v > 0]
    if vals:
        return float(np.median(vals)), len(vals)
    try:
        return float(txt.get('ms', '-1').split()[0]), 1
    except ValueError:
        return -1.0, 0


def png(p):
    if Image is None or not os.path.exists(p):
        return None
    return np.asarray(Image.open(p).convert('RGB')).astype(np.int16)


def pic_diff(a, b):
    if a is None or b is None or a.shape != b.shape:
        return None
    d = np.abs(a - b).max(axis=-1)
    return int(d.max()), int((d > 0).sum())


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    root = sys.argv[1]
    ok_all = True
    for pre in sys.argv[2:]:
        f = lambda tag, ext: os.path.join(root, '%s_%s%s' % (pre, tag, ext))
        print('== %s' % pre)
        shots = {}
        for tag in ('full', 'full2', 'cl', 'both', 'dropcl', 'nearskip', 'zcolour', 'nofarclamp'):
            if os.path.exists(f(tag, '.bin')) and os.path.exists(f(tag, '.png')):
                try:
                    shots[tag] = load_bin(f(tag, '.bin'))
                except ValueError as e:
                    print('  X  %s: %s' % (tag, e))
        need = ('full', 'cl', 'both')
        miss = [t for t in need if t not in shots]
        if miss:
            print('  X  FAIL missing %s' % ' '.join(miss))
            ok_all = False
            continue
        P = {t: png(f(t, '.png')) for t in ('full', 'full2', 'cl', 'both', 'dropcl', 'nearskip', 'zcolour', 'nofarclamp')}
        T = {t: load_txt(f(t, '.bin.txt')) for t in shots}
        flake = pic_diff(P['full'], P['full2'])
        print('  launch flake full vs full2: %s' % ('(no full2)' if flake is None else 'max %d, %d px' % flake))
        floor = 0 if flake is None else flake[0]

        def q_pass(tag, ref='full'):
            a, b = shots[tag], shots[ref]
            if a[0].shape != b[0].shape:
                return False, 'dims differ'
            di = float(np.abs(a[0] - b[0]).max())
            dg = float(np.abs(a[1] - b[1]).max())
            nd = int((np.abs(a[0] - b[0]).max(axis=-1) > 0).sum())
            pd = pic_diff(P[tag], P[ref])
            okv = di == 0.0 and dg == 0.0
            okp = pd is not None and pd[0] <= floor
            return okv and okp, 'inject max|d| %.3g (%d froxels) integ max|d| %.3g picture %s' % (
                di, nd, dg, 'n/a' if pd is None else 'max %d, %d px' % pd)

        of = opt_fields(T['cl'])
        built = int(float(of.get('clusters', '0'))) > 0
        ok, why = q_pass('cl')
        ok = ok and built
        print('  Q  %s  clusters %s non-empty %s mean list %s build %s ms; %s' % (
            'PASS' if ok else 'FAIL', of.get('clusters', '?'), of.get('nonempty', '?'), of.get('meanlist', '?'),
            of.get('buildms', '?'), why))
        ok_all &= ok
        if 'dropcl' in shots:
            rok, rwhy = q_pass('dropcl')
            print('  red dropcl -> Q %s (must FAIL)  %s' % ('PASS' if rok else 'FAIL', rwhy))
            ok_all &= not rok

        def z_pass(tag, ref='cl'):
            a, b = shots[tag][0], shots[ref][0]
            if a.shape != b.shape:
                return False, 0.0, 'dims differ'
            same = np.all(a == b, axis=-1)
            skipped = np.all(a == np.array([0, 0, 0, 1], np.float32), axis=-1) & ~same
            bad = int((~same & ~skipped).sum())
            frac = float(skipped.sum()) / float(same.size)
            pd = pic_diff(P[tag], P[ref])
            okp = pd is not None and pd[0] <= floor
            return bad == 0 and okp, frac, 'skipped %.1f%% of froxels, %d changed but not skipped; picture %s' % (
                100 * frac, bad, 'n/a' if pd is None else 'max %d, %d px' % pd)

        ob = opt_fields(T['both'])
        on = ob.get('depthskip', '0') == '1'
        ok, frac, why = z_pass('both')
        teeth = on and frac >= 0.01
        print('  Z  %s  depthskip=%s; %s%s' % ('PASS' if ok and teeth else 'FAIL', ob.get('depthskip', '?'), why,
                                              '' if teeth else '  (no teeth: skip off or under 1%)'))
        ok_all &= ok and teeth
        if 'nearskip' in shots:
            rok, rfrac, rwhy = z_pass('nearskip')
            # the red's picture must differ (its skipped froxels are seen); the volume rule alone cannot tell
            pd = pic_diff(P['nearskip'], P['cl'])
            fails = pd is None or pd[0] > floor
            print('  red nearskip -> Z %s (must FAIL)  %s' % ('FAIL' if fails else 'PASS', rwhy))
            ok_all &= fails
        if 'zcolour' in shots:
            rok, rfrac, rwhy = z_pass('zcolour')
            print('  red zcolour -> Z %s (must FAIL)  %s' % ('PASS' if rok else 'FAIL', rwhy))
            ok_all &= not rok
        if 'nofarclamp' in shots:
            rok, rfrac, rwhy = z_pass('nofarclamp')
            print('  red nofarclamp -> Z %s (must FAIL)  %s' % ('PASS' if rok else 'FAIL', rwhy))
            ok_all &= not rok
        mf, nf = ms_of(f('full', '.notes'), T['full'])
        mb, nb = ms_of(f('both', '.notes'), T['both'])
        mc, nc = ms_of(f('cl', '.notes'), T['cl'])
        print('  C  cost (median ms over frames): full %.3f (%d)  clusters %.3f (%d)  both %.3f (%d)  -> %.2fx; lights %s' % (
            mf, nf, mc, nc, mb, nb, mf / mb if mb > 0 else float('nan'), T['full'].get('cellLightCount', '?')))
    print('VERDICT %s' % ('PASS' if ok_all else 'FAIL'))
    return 0 if ok_all else 1


if __name__ == '__main__':
    sys.exit(main())
