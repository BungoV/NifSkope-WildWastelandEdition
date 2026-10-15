#!/usr/bin/env python3
"""THE SKY VISIBILITY STRENGTH, JUDGED WITHOUT NIFSKOPE (lane SKYSTR1, 2026-10-13).

  python tests/spells/skystr1_check.py <Fallout4.esm> <run dir> [--weather W] [--hour H] [--red ignore]
  python tests/spells/skystr1_check.py --live <live dir>

The slider (Show > Lighting > Sky visibility, setting WW/CellSkyVis, pin WW_CELL_SKYSTR) blends each probe
octant's stored sky visibility v toward an unoccluded sky, as FO4CS's Skylighting fStrength does:
v' = lerp(1, v, s). At s = 100 % nothing changes; at 0 % every octant sees the whole sky (what the existing
refuter WW_CELL_SKY_RED=novis does). The run dir is what tests/spells/skystr1_cell.sh writes: bake/*.tbk, and
per camera <cam>/<shot>.png, plus dump_s<pct>/ (the relight's numbers, WW_CELL_GI_DUMP) per strength.

Stages, each from the files on disk (with --red ignore the subject is the red's dumps and pictures, which must
FAIL K, Z and H):
  K  the sky at every probe, per strength: this file's own pi x ambient x mean over the side's four octants of
     (1 + s (v - 1)) x the glass tint, from the .tbk and its own read of the weather, against gi_sky.bin
  E  at 100 % the pictures are the exe from before the lane's, byte for byte (the default and the pinned 100)
  Z  at 0 % the pictures equal the sky-visibility-off red's (WW_CELL_SKY_RED=novis) within the frame noise
  H  at 50 % every changed pixel lies between its 0 % and 100 % values, and off both ends
Only this file's own .tbk reader (probe_bake.read_tbk) and weather walk (cell_sky_check.weather_light) are
shared with other checkers; neither calls NifSkope.
"""
import math
import os
import struct
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from probe_bake import read_tbk  # noqa: E402   (the checker's own .tbk reader)
from cell_sky_check import weather_light  # noqa: E402   (the checker's own walk of WTHR / CLMT)

PCTS = (0, 50, 100)
NOISE_LVL = 2          # Z: a pixel "equal" when every channel is within this many 8-bit levels
Z_SHARE = 0.999        # Z: this share of the pixels equal
CHANGE_LVL = 6         # H: a pixel "changed" by the slider when 0 % and 100 % differ by more than this
MIN_CHANGED = 2000     # H, Z: the slider must move at least this many pixels in a picture for it to judge
H_TOL = 2              # H: in-between allows this many levels of rounding at each end
H_INNER = 0.95         # H: this share of the changed pixels sits strictly inside (t in 0.05 .. 0.95)


def own_sky(tbks, amb, s):
    out = []
    for t in tbks:
        v = np.clip(t['probes']['sky'].astype(np.float64), 0, 1)              # (n, 8)
        tn = t['pext']['skytint'].astype(np.float64) / 255.0                   # (n, 8, 3)
        v2 = 1.0 + s * (v - 1.0)                                              # FO4CS lerp(1, v, s)
        E = np.zeros((len(v), 6, 3))
        for a in range(6):
            ax, neg = a >> 1, a & 1
            octs = [o for o in range(8) if ((o >> ax) & 1) == neg]             # bit 0 x<0, bit 1 y<0, bit 2 z<0
            E[:, a, :] = math.pi * amb[a][None, :] * (v2[:, octs, None] * tn[:, octs, :]).mean(1)
        out.append(E)
    return np.concatenate(out)


def read_sky_bin(path):
    b = open(path, 'rb').read()
    n = struct.unpack_from('<i', b)[0]
    return np.frombuffer(b, '<f4', n * 18, 4).reshape(n, 6, 3).astype(np.float64)


def stage_k(tbks, W, run, pfx):
    lines, ok = [], True
    for pct in PCTS:
        p = os.path.join(run, '%sdump_s%d' % (pfx, pct), 'gi_sky.bin')
        if not os.path.exists(p):
            if pfx and pct == 100:
                continue
            lines.append('K FAIL s %d%%: no gi_sky.bin at %s' % (pct, p))
            ok = False
            continue
        V = read_sky_bin(p)
        own = own_sky(tbks, W['amb'], pct / 100.0)
        if len(V) != len(own):
            lines.append('K FAIL s %d%%: the bake holds %d probes, gi_sky.bin %d' % (pct, len(own), len(V)))
            ok = False
            continue
        a2, b2 = V.reshape(len(V), -1), own.reshape(len(own), -1)
        good = np.all(np.abs(a2 - b2) <= 1e-4 + 0.01 * np.maximum(np.abs(a2), np.abs(b2)), axis=1)
        ratio = float(V.sum() / max(own.sum(), 1e-9))
        full = own_sky(tbks, W['amb'], 1.0)
        moved = int((np.abs(full - own_sky(tbks, W['amb'], 0.0)).max((1, 2)) > 1e-3).sum())
        k = good.mean() >= 0.99 and 0.99 <= ratio <= 1.01 and moved >= 20
        ok &= k
        lines.append('K %s s %d%%: %d probes, %d whose sky the slider moves; agree %.2f%%, the viewer\'s total over '
                     'this file\'s %.4f (100 %%: %.4f, 0 %%: %.4f of it)'
                     % ('PASS' if k else 'FAIL', pct, len(own), moved, 100 * good.mean(), ratio,
                        full.sum() / max(own.sum(), 1e-9), own_sky(tbks, W['amb'], 0.0).sum() / max(own.sum(), 1e-9)))
    return lines, ok


def load(p):
    return np.asarray(Image.open(p).convert('RGB')).astype(np.int16)


def stage_e(cam, d):
    lines, ok = [], True
    for name in ('s100', 's100pin', 'probe5_s100'):
        a, b = os.path.join(d, 'before' if name != 'probe5_s100' else 'probe5_before'), os.path.join(d, name)
        a, b = a + '.png', b + '.png'
        if name == 'probe5_s100' and not os.path.exists(a) and not os.path.exists(b):
            continue   # the probe-5 pair is shot on the first camera only
        if not (os.path.exists(a) and os.path.exists(b)):
            lines.append('E FAIL %s %s: missing %s' % (cam, name, a if not os.path.exists(a) else b))
            ok = False
            continue
        same = open(a, 'rb').read() == open(b, 'rb').read()
        px = int((load(a) != load(b)).any(2).sum())
        ok &= same
        lines.append('E %s %s %s: byte for byte the exe before the lane: %s (%d px differ)'
                     % ('PASS' if same else 'FAIL', cam, name, same, px))
    return lines, ok


def stage_z(cam, d, pfx, base=''):
    a = load(os.path.join(d, '%s%ss0.png' % (pfx, base)))
    n = load(os.path.join(d, '%sn0.png' % base))
    one = load(os.path.join(d, '%ss100.png' % base))
    eq = (np.abs(a - n).max(2) <= NOISE_LVL)
    share = float(eq.mean())
    slid = int((np.abs(one - n).max(2) > CHANGE_LVL).sum())
    worst = int(np.abs(a - n).max())
    ok = share >= Z_SHARE and slid >= MIN_CHANGED
    return ('Z %s %s%s 0%%: equal to the sky-visibility-off picture on %.4f%% of the pixels (bar %.1f%%, within %d '
            'levels; worst %d); the off picture differs from 100 %% on %d px (bar %d)'
            % ('PASS' if ok else 'FAIL', cam, ' ' + base.rstrip('_') if base else '', 100 * share, 100 * Z_SHARE,
               NOISE_LVL, worst, slid, MIN_CHANGED)), ok


def stage_h(cam, d, pfx, base=''):
    z = load(os.path.join(d, '%ss0.png' % base))     # the green 0 % (a red judges its own 50 % against it)
    one = load(os.path.join(d, '%ss100.png' % base))
    h = load(os.path.join(d, '%s%ss50.png' % (pfx, base)))
    diff = one - z
    ch = np.abs(diff).argmax(2)
    take = lambda x: np.take_along_axis(x, ch[..., None], 2)[..., 0]  # noqa: E731   (the channel that moves most)
    dz, dh = take(diff).astype(float), take(h - z).astype(float)
    changed = np.abs(dz) > CHANGE_LVL
    lo, hi = np.minimum(z, one) - H_TOL, np.maximum(z, one) + H_TOL
    inside_all = ((h >= lo) & (h <= hi)).all(2)
    t = dh[changed] / dz[changed]
    n = int(changed.sum())
    between = float(inside_all[changed].mean()) if n else 0.0
    inner = float(((t > 0.05) & (t < 0.95)).mean()) if n else 0.0
    ok = n >= MIN_CHANGED and between >= 0.999 and inner >= H_INNER
    return ('H %s %s%s 50%%: %d px the slider moves (bar %d); %.3f%% of them between 0 %% and 100 %% (bar 99.9%%), '
            '%.2f%% strictly inside (bar %.0f%%); t (0 = the 0 %% value, 1 = the 100 %%) median %.3f, 5..95%% %.3f..%.3f'
            % ('PASS' if ok else 'FAIL', cam, ' ' + base.rstrip('_') if base else '', n, MIN_CHANGED, 100 * between,
               100 * inner, 100 * H_INNER, float(np.median(t)) if n else float('nan'),
               float(np.percentile(t, 5)) if n else float('nan'), float(np.percentile(t, 95)) if n else float('nan'))), ok


def live(d):
    """the slider in the running window: shots after set 0 / 100 / 50 against the headless pictures' rules"""
    lines, ok = [], True
    a, z, b, h = (load(os.path.join(d, n + '.png')) for n in ('l100', 'l0', 'l100b', 'l50'))
    same = int((a != b).any(2).sum())
    moved = int((np.abs(a - z).max(2) > CHANGE_LVL).sum())
    good = same == 0 and moved >= MIN_CHANGED
    ok &= good
    lines.append('L %s live: set 0 moved %d px (bar %d); set back to 100 gives the first picture again: %d px differ'
                 % ('PASS' if good else 'FAIL', moved, MIN_CHANGED, same))
    diff = a - z
    ch = np.abs(diff).argmax(2)
    take = lambda x: np.take_along_axis(x, ch[..., None], 2)[..., 0]  # noqa: E731
    dz, dh = take(diff).astype(float), take(h - z).astype(float)
    c = np.abs(dz) > CHANGE_LVL
    t = dh[c] / dz[c] if c.any() else np.array([0.0])
    inner = float(((t > 0.05) & (t < 0.95)).mean())
    g2 = inner >= H_INNER
    ok &= g2
    lines.append('L %s live 50: %.2f%% of the moved pixels strictly between (bar %.0f%%), t median %.3f'
                 % ('PASS' if g2 else 'FAIL', 100 * inner, 100 * H_INNER, float(np.median(t))))
    return lines, ok


def main(argv):
    if argv[0] == '--live':
        lines, ok = live(argv[1])
        for ln in lines:
            print(ln)
        print('skystr live %s' % ('PASS' if ok else 'FAIL'))
        return 0 if ok else 1
    esm, run = argv[0], argv[1]
    weather, hour, red = 'CommonwealthClear', 12.0, ''
    i = 2
    while i < len(argv):
        if argv[i] == '--weather':
            weather = argv[i + 1]
        elif argv[i] == '--hour':
            hour = float(argv[i + 1])
        elif argv[i] == '--red':
            red = argv[i + 1]
        else:
            raise SystemExit('unknown argument ' + argv[i])
        i += 2
    pfx = 'red_' + red + '_' if red else ''
    bake = os.path.join(run, 'bake')
    tbks = [read_tbk(os.path.join(bake, f)) for f in sorted(os.listdir(bake)) if f.endswith('.tbk')]
    W = weather_light(esm, weather, hour)
    lines, okK = stage_k(tbks, W, run, pfx)
    oks = {'K': okK}
    cams = sorted(c for c in os.listdir(run) if os.path.isfile(os.path.join(run, c, 'n0.png')))
    if not red:
        oks['E'] = True
        for cam in cams:
            ln, ok = stage_e(cam, os.path.join(run, cam))
            lines += ln
            oks['E'] &= ok
    oks['Z'], oks['H'] = True, True
    for cam in cams:
        d = os.path.join(run, cam)
        for base in ('', 'probe5_'):
            if not os.path.exists(os.path.join(d, '%s%ss0.png' % (pfx, base))):
                continue
            ln, ok = stage_z(cam, d, pfx, base)
            lines.append(ln)
            oks['Z'] &= ok
            ln, ok = stage_h(cam, d, pfx, base)
            lines.append(ln)
            oks['H'] &= ok
    if not cams:
        lines.append('Z FAIL no camera directory with pictures')
        oks['Z'] = oks['H'] = False
    for ln in lines:
        print(ln)
    for k, v in oks.items():
        print('%s %s' % (k, 'PASS' if v else 'FAIL'))
    ok = all(oks.values())
    print('skystr %s%s' % ('red ' + red + ' ' if red else '', 'PASS' if ok else 'FAIL'))
    return 0 if ok else 1


if __name__ == '__main__':
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(2)
    sys.exit(main(sys.argv[1:]))
