#!/usr/bin/env python3
"""The interior fog check (lane FOG2, 2026-10-01; tests/spells/cell_fog.sh, docs/PRTP_PLAN.md 2l).

  cell_fog_check.py <Fallout4.esm> <interior EDID> <shot dir>      the verdict
  cell_fog_check.py <Fallout4.esm> --dump [EDID ...]                the fog fields of interiors (all when none named)

The fog from its OWN walk of the plugin (nothing shared with src/cellview.cpp or src/esmweather.cpp):
the cell's XCLL and its lighting template (LTMP -> LGTM DATA, the same layout), each field from the template
when the cell has no XCLL or its Inherits flag (XCLL 88) names it -- 0x4 the four colours, their scales, the
two height bands and the high density scale; 0x8 near; 0x10 far; 0x100 power; 0x200 max. The game's clamps
(far <= 0 or > 163840 -> 163840; near <= 0 or > far -> 0.17 far). Colours byte / 255 x scale, ^2.2.
Then the fog formula (the engine composite, lookdev_fog.glsl's port written out again here) at every clean
sampled pixel: position from probes 2 + 3, distance from the camera the .cam dumps give (WW_CELL_CAM_DUMP),
height = world z. Only pixels whose fog read that same surface (fog probe 8's distance and height) count.
Compared against fog probe 6 (alpha, height blend) and 7 (colour ^ 1/2.2). One verdict line; exit 0 on PASS.
"""
import os
import re
import struct
import sys
import zlib

import numpy as np
from PIL import Image

GRUP = b'GRUP'


def fields(buf):
    off, big = 0, 0
    while off + 6 <= len(buf):
        t = buf[off:off + 4]
        sz = struct.unpack_from('<H', buf, off + 4)[0]
        off += 6
        if t == b'XXXX':
            big = struct.unpack_from('<I', buf, off)[0]
            off += sz
            continue
        if big:
            sz, big = big, 0
        yield t, buf[off:off + sz]
        off += sz


def record(buf, off):
    size, flags = struct.unpack_from('<II', buf, off + 4)
    data = buf[off + 24:off + 24 + size]
    if flags & 0x00040000:
        data = zlib.decompress(data[4:])
    d = {}
    for t, p in fields(data):
        d.setdefault(t, p)
    return flags, d


def interiors(esm, want=None):
    """{EDID: (XCLL bytes, LGTM DATA bytes or b'')} for interior cells (top-level CELL group, type 0 children)."""
    buf = open(esm, 'rb').read()
    cells, lgtm = {}, {}
    off = 24 + struct.unpack_from('<I', buf, 4)[0]
    stack = []
    while off + 24 <= len(buf):
        while stack and off >= stack[-1]:
            stack.pop()
        t = buf[off:off + 4]
        size = struct.unpack_from('<I', buf, off + 4)[0]
        if t == GRUP:
            label = buf[off + 8:off + 12]
            gtype = struct.unpack_from('<i', buf, off + 12)[0]
            if gtype == 0 and label not in (b'CELL', b'LGTM'):
                off += size     # a top group we do not need
                continue
            stack.append(off + size)
            off += 24
            continue
        form = struct.unpack_from('<I', buf, off + 12)[0]
        if t == b'LGTM':
            _, f = record(buf, off)
            lgtm[form] = f.get(b'DATA', b'')
        elif t == b'CELL':
            _, f = record(buf, off)
            edid = f.get(b'EDID', b'').split(b'\0')[0].decode('cp1252', 'replace')
            data = f.get(b'DATA', b'\0')
            if data[0] & 1 and (want is None or edid in want):
                ltmp = struct.unpack_from('<I', f[b'LTMP'])[0] if b'LTMP' in f else 0
                cells[edid] = (f.get(b'XCLL', b''), ltmp)
        off += 24 + size
    return {e: (x, lgtm.get(lt, b'')) for e, (x, lt) in cells.items()}


def fog_of(xcll, tmpl, red=''):
    """the packed fog (engine rules above) -> dict of scalars and linear colours"""
    inh = struct.unpack_from('<I', xcll, 88)[0] if len(xcll) >= 92 else 0

    def src(flag):
        if not tmpl:
            return xcll
        if not xcll:
            return tmpl
        return tmpl if (inh & flag) and red != 'noinherit' else xcll

    def f(a, o, d):
        return struct.unpack_from('<f', a, o)[0] if len(a) >= o + 4 else d
    c, n, fa, p, m = src(0x4), src(0x8), src(0x10), src(0x100), src(0x200)
    if len(n) < 20 or len(fa) < 20:
        return None
    far, near = f(fa, 16, 0.0), f(n, 12, 0.0)
    if red != 'noclamp':
        if not far > 0 or far > 163840:
            far = 163840.0
        if not near > 0 or near > far:
            near = far * 0.17
    o = dict(near=near, far=far, power=f(p, 36, 1.0), max=f(m, 76, 1.0), nmid=f(c, 92, 0.0), nrange=f(c, 96, 10000.0),
             hds=f(c, 108, 1.0), fmid=f(c, 128, 0.0), frange=f(c, 132, 10000.0), inh=inh,
             tmpl=bool(tmpl), xlen=len(xcll))
    for k, (at, name) in enumerate(((8, 'nl'), (72, 'fl'), (100, 'nh'), (104, 'fh'))):
        sc = f(c, 112 + 4 * k, 1.0)
        rgb = np.array([c[at + i] if len(c) >= at + 3 else 0 for i in range(3)], float) / 255.0 * sc
        o[name] = np.maximum(rgb, 0) ** (1.0 if red == 'nogamma' else 2.2)
    return o


def fog_eval(F, d, z):
    """alpha, height blend, colour (before the sun term) at distance d and world height z (arrays)"""
    span = (F['far'] - F['near']) or 1.0
    ramp = (d - F['near']) / span
    fr = np.clip(ramp, 0, 1)
    nr, frr = F['nrange'] or 1.0, F['frange'] or 1.0
    hn = np.clip((z - (F['nmid'] - nr)) / (2 * nr), 0, 1)
    hf = np.clip((z - (F['fmid'] - frr)) / (2 * frr), 0, 1)
    hb = hn + (hf - hn) * fr
    mx = F['max']
    clamp_t = np.where(ramp > 0.75, np.minimum((fr - 0.75) * 4 * (1 - mx) + mx, 1.0), mx)
    esc = np.where(ramp < 0.015, fr * 66.666672, 1.0)
    inten = np.where(fr > 0, np.minimum(clamp_t, np.power(fr, F['power'])), 0.0)
    alpha = (hb * F['hds'] + (1 - hb)) * inten * esc
    lo = F['nl'] + (F['fl'] - F['nl']) * inten[..., None]
    hi = F['nh'] + (F['fh'] - F['nh']) * inten[..., None]
    return alpha, hb, lo + (hi - lo) * hb[..., None]


def describe(e, F):
    if F is None:
        return '%s: no fog (no XCLL, no template)' % e
    c = lambda v: '%.4f,%.4f,%.4f' % tuple(v)
    return ('%s: near %.1f far %.1f power %.3f max %.3f hds %.3f nmid %.0f nrange %.0f fmid %.0f frange %.0f '
            'inh 0x%x tmpl %d xcll %d | nl %s fl %s nh %s fh %s' % (
                e, F['near'], F['far'], F['power'], F['max'], F['hds'], F['nmid'], F['nrange'], F['fmid'], F['frange'],
                F['inh'], F['tmpl'], F['xlen'], c(F['nl']), c(F['fl']), c(F['nh']), c(F['fh'])))


def main(esm, cell, shots):
    img = {}
    tags = ('probe2', 'probe3', 'fog6', 'fog7', 'fog8')
    for tag in tags:
        img[tag] = np.asarray(Image.open(os.path.join(shots, '%s.%s.png' % (cell, tag))).convert('RGB'), float)
    notes = open(os.path.join(shots, cell + '.fog6.notes'), encoding='utf-8', errors='replace').read()
    m = re.search(r'cell lighting: .*center=(-?[\d.]+),(-?[\d.]+),(-?[\d.]+)', notes)
    cams = [open(os.path.join(shots, '%s.%s.cam' % (cell, t))).read() for t in tags
            if os.path.exists(os.path.join(shots, '%s.%s.cam' % (cell, t)))]
    mc = re.findall(r'cam=(-?[\d.]+),(-?[\d.]+),(-?[\d.]+)', cams[-1]) if len(cams) == len(tags) else []
    if not m or not mc:
        return 'fog FAIL %s: no "cell lighting ... center=" line in the notes, or a .cam dump missing' % cell
    # the four pictures must share one camera (positions from 2 + 3, the fog from 6 + 7)
    if len({c.split(' fogprobe')[0] for c in cams}) != 1:
        return 'fog FAIL %s: the passes did not share a camera: %s' % (cell, ' | '.join(c.strip() for c in cams))
    red = (re.search(r' red=(\w+)', notes.split('fog=', 1)[1]) or [None, ''])[1] if 'fog=' in notes else ''
    center = np.array([float(v) for v in m.groups()])
    cam = np.array([float(v) for v in mc[-1]])
    q = np.round(img['probe2']) * 256 + np.round(img['probe3'])
    P = q + 0.5 - 32768.0 + center
    ok = np.any(img['probe2'] != img['probe2'][0, 0], axis=2) | np.any(img['probe3'] != img['probe3'][0, 0], axis=2)
    ok &= np.any(img['probe2'] != img['probe3'], axis=2)
    for dy, dx in ((0, 1), (1, 0), (0, -1), (-1, 0)):
        ok &= np.linalg.norm(P - np.roll(P, (dy, dx), (0, 1)), axis=2) < 40
    ok[0, :] = ok[-1, :] = ok[:, 0] = ok[:, -1] = False
    # the same top surface in both: fog 8's distance / height (steps of 64 / 128 units) against the probes'
    d_all = np.linalg.norm(P - cam, axis=2)
    same = (np.abs(img['fog8'][..., 0] / 255.0 * 16384 - d_all) <= 100) & \
           (np.abs((img['fog8'][..., 1] / 255.0 - 0.5) * 32768 - P[..., 2]) <= 160)
    n_clean = int(ok.sum())
    same_share = (same & ok).sum() / max(n_clean, 1)
    if same_share < 0.6:
        return ('fog FAIL %s: only %.1f%% of %d clean pixels show the fog the same surface the position probes saw '
                '(the fog reads another distance / height)' % (cell, 100 * same_share, n_clean))
    ok &= same
    ys, xs = np.nonzero(ok)
    if len(ys) < 2000:
        return 'fog FAIL %s: %d clean cell-lit pixels, under 2000' % (cell, len(ys))
    rng = np.random.default_rng(1)
    pick = rng.choice(len(ys), size=min(20000, len(ys)), replace=False)
    ys, xs = ys[pick], xs[pick]
    Pp = P[ys, xs]
    F = fog_of(*interiors(esm, {cell})[cell])
    if F is None:
        return 'fog FAIL %s: the plugin gives this cell no fog' % cell
    d = np.linalg.norm(Pp - cam, axis=1)
    alpha, hb, col = fog_eval(F, d, Pp[:, 2])
    got_a = img['fog6'][ys, xs, 0] / 255.0
    got_hb = img['fog6'][ys, xs, 1] / 255.0
    got_c = img['fog7'][ys, xs] / 255.0
    exp_c = np.clip(col, 0, 1) ** (1 / 2.2)
    # tolerance: the position is quantised to 1 unit (+-0.5 per axis) and the probe to 8 bits
    tol_a = 2.5 / 255 + np.abs(fog_eval(F, d + 1.0, Pp[:, 2])[0] - alpha)
    good_a = np.abs(got_a - np.clip(alpha, 0, 1)) <= tol_a
    good_hb = np.abs(got_hb - hb) <= 2.5 / 255 + 0.01
    good_c = np.all(np.abs(got_c - exp_c) <= 3.0 / 255 + 0.01, axis=1)
    good = good_a & good_hb & good_c
    fogged = alpha > 0.02
    share, f_share = good.mean(), (good[fogged].mean() if fogged.any() else 0.0)
    verdict = share >= 0.97 and fogged.sum() >= 500 and f_share >= 0.95
    return ('fog %s %s: %d sampled (same surface %.1f%%), %d fogged (alpha > 0.02, max %.3f); agree %.1f%% (fogged %.1f%%; '
            'alpha %.1f%%, hb %.1f%%, colour %.1f%%); d %.0f..%.0f; near %.0f far %.0f power %.2f max %.2f%s' % (
                'PASS' if verdict else 'FAIL', cell, len(ys), 100 * same_share, fogged.sum(), alpha.max(), 100 * share,
                100 * f_share,
                100 * good_a.mean(), 100 * good_hb.mean(), 100 * good_c.mean(), d.min(), d.max(), F['near'], F['far'],
                F['power'], F['max'], ' (renderer red=%s)' % red if red else ''))


if __name__ == '__main__':
    if len(sys.argv) >= 3 and sys.argv[2] == '--dump':
        want = set(sys.argv[3:]) or None
        for e, (x, t) in sorted(interiors(sys.argv[1], want).items()):
            print(describe(e, fog_of(x, t)))
        sys.exit(0)
    if len(sys.argv) < 4:
        print(__doc__)
        sys.exit(2)
    line = main(*sys.argv[1:4])
    print(line)
    sys.exit(0 if ' PASS ' in line else 1)
