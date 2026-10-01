#!/usr/bin/env python3
"""The cell-lights check (lane PRTP3, 2026-10-01; tests/spells/cell_lit.sh).

  cell_lit_check.py <Fallout4.esm> <interior EDID> <shot dir>

Reads <cell>.probe1..4.png and <cell>.lit.notes from the shot dir, and the interior's lights from its
OWN walk of the plugin (nothing shared with src/esmdata.cpp or src/cellview.cpp):
  every REFR of the cell whose base is a LIGH, not deleted, not initially disabled, the base not
  "Off By Default"; radius = DATA radius + XRDS; colour = (byte / 255)^2.2 x (FNAM + XLIG fade delta);
  spots (0x400 / 0x4000) shine along the ref's local +X under the engine euler (-x, -y, -z), cone
  half-angle (FOV + XLIG FOV delta) / 2, edge exponent = DATA Falloff Exponent.
  lane HEMI1, the light shapes (the game's light builder and its stencil volumes): the hemisphere flag
  0x800 wins (never a spot) and lights only the half space in front of the ref's local +X; otherwise a
  non-spot light with an XLKR under keyword 00115705 (LightBoxLink) to a ref carrying XPRM lights only
  inside that ref's box: centre = its DATA position, half extents = |XPRM bounds| x the light's XSCL,
  frame = Rz(-z) Rx(-x) Ry(-y) of its DATA angles. Inside either volume the light is the omni curve.
Then, at every clean sampled pixel (position decoded from probes 2 + 3, normal from probe 4), the
docs/PRTP2_LIGHT_MODEL.md diffuse sum against probe 1 (irradiance / 4). The pixels the shapes decide
(where the shaped and the unclipped sums differ) must agree on their own too, as the spots do.
One verdict line; exit 0 on PASS.
"""
import math
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
    return flags, dict_multi(fields(data))


def dict_multi(fl):
    d = {}
    for t, p in fl:
        d.setdefault(t, p)
    return d


def walk(buf):
    """(type, form, offset, enclosing groups) for every record."""
    stack = []
    off = 24 + struct.unpack_from('<I', buf, 4)[0]
    while off + 24 <= len(buf):
        while stack and off >= stack[-1][0]:
            stack.pop()
        t = buf[off:off + 4]
        size = struct.unpack_from('<I', buf, off + 4)[0]
        if t == GRUP:
            stack.append((off + size, struct.unpack_from('<I', buf, off + 8)[0], struct.unpack_from('<i', buf, off + 12)[0]))
            off += 24
            continue
        yield t, struct.unpack_from('<I', buf, off + 12)[0], off, stack
        off += 24 + size


def euler(x, y, z):
    sx, cx, sy, cy, sz, cz = math.sin(x), math.cos(x), math.sin(y), math.cos(y), math.sin(z), math.cos(z)
    return np.array([[cy * cz, -cy * sz, sy],
                     [sx * sy * cz + sz * cx, cx * cz - sx * sy * sz, -sx * cy],
                     [sx * sz - cx * sy * cz, cx * sy * sz + sx * cz, cx * cy]])


def lights_of(esm, cell_edid):
    buf = open(esm, 'rb').read()
    cell_form, ligh, refs = None, {}, []
    for t, form, off, stack in walk(buf):
        if t == b'CELL' and cell_form is None and all(g[2] != 1 for g in stack):
            _, f = record(buf, off)
            if f.get(b'EDID', b'').split(b'\0')[0].decode('cp1252', 'replace') == cell_edid:
                cell_form = form
        elif t == b'LIGH':
            _, f = record(buf, off)
            if b'DATA' in f:
                ligh[form] = f
        elif t == b'REFR' and cell_form is not None and any(g[1] == cell_form and g[2] in (6, 8, 9) for g in stack):
            flags, f = record(buf, off)
            refs.append((form, flags, f, off))
    byform = {form: f for form, flags, f, off in refs if not flags & 0x20}
    out = []
    for form, flags, f, roff in refs:
        if flags & 0x20 or flags & 0x800 or b'NAME' not in f:
            continue
        b = ligh.get(struct.unpack_from('<I', f[b'NAME'])[0])
        if b is None:
            continue
        d = b[b'DATA']
        radius, = struct.unpack_from('<I', d, 4)
        col = np.array(list(d[8:11]), float)
        lf, = struct.unpack_from('<I', d, 12)
        if lf & 0x20 or lf & 0x100000:  # off; Ambient Only (no direct light in game, lane AMBO1)
            continue
        falloff, fov = struct.unpack_from('<2f', d, 16) if len(d) >= 28 else (1.0, 90.0)
        bse = struct.unpack_from('<3f', d, 40) if len(d) >= 52 else (0.0, 1.0, 2.0)
        fade = struct.unpack_from('<f', b[b'FNAM'])[0] if b'FNAM' in b else 1.0
        xlig = struct.unpack_from('<%df' % (len(f[b'XLIG']) // 4), f[b'XLIG']) if b'XLIG' in f else ()
        r = radius + (struct.unpack_from('<f', f[b'XRDS'])[0] if b'XRDS' in f else 0.0)
        if r <= 0:
            continue
        c = (col / 255.0) ** 2.2 * (fade + (xlig[1] if len(xlig) >= 2 else 0.0))
        if c.max() <= 0:
            continue
        pos = struct.unpack_from('<3f', f[b'DATA'], 0)
        rot = struct.unpack_from('<3f', f[b'DATA'], 12)
        hemi = bool(lf & 0x800)
        spot = bool(lf & 0x4400) and not hemi
        cos_outer = math.cos(math.radians(fov + (xlig[0] if xlig else 0.0)) / 2) if spot else -2.0
        aim = euler(-rot[0], -rot[1], -rot[2]) @ np.array([1.0, 0.0, 0.0])
        box = None
        if not hemi and not spot:
            box = light_box(buf, roff, byform, struct.unpack_from('<f', f[b'XSCL'])[0] if b'XSCL' in f else 1.0)
        # lane RIM1: No Rim Lighting (0x80000) drops the back-light; Ignore Roughness (0x40000) also turns the
        # diffuse to Lambert
        out.append(dict(pos=np.array(pos), r=r, c=c, spot=spot, cos=cos_outer, aim=aim, cone=falloff, bse=bse,
                        norim=bool(lf & 0x80000), rough=bool(lf & 0x40000), flags=lf, hemi=hemi, box=box))
    return out


def light_box(buf, off, byform, scale):
    """lane HEMI1: (centre, frame columns, half extents) of the ref's LightBoxLink primitive, or None."""
    size, flags = struct.unpack_from('<II', buf, off + 4)
    data = buf[off + 24:off + 24 + size]
    if flags & 0x00040000:
        data = zlib.decompress(data[4:])
    for t, p in fields(data):   # every XLKR, not only the first
        if t != b'XLKR' or len(p) < 8:
            continue
        kw, to = struct.unpack_from('<II', p)
        tf = byform.get(to)
        if kw != 0x00115705 or tf is None or b'XPRM' not in tf:
            continue
        d = struct.unpack_from('<6f', tf[b'DATA'])
        x, y, z = d[3:]
        rz = np.array([[math.cos(-z), -math.sin(-z), 0], [math.sin(-z), math.cos(-z), 0], [0, 0, 1]])
        rx = np.array([[1, 0, 0], [0, math.cos(-x), -math.sin(-x)], [0, math.sin(-x), math.cos(-x)]])
        ry = np.array([[math.cos(-y), 0, math.sin(-y)], [0, 1, 0], [-math.sin(-y), 0, math.cos(-y)]])
        half = np.abs(np.array(struct.unpack_from('<3f', tf[b'XPRM']))) * scale
        return np.array(d[:3]), rz @ rx @ ry, half
    return None


def inside(L, P):
    """lane HEMI1: the pixels inside light L's volume (all of them for an omni or spot light)."""
    if L['hemi']:
        return (P - L['pos']) @ L['aim'] >= 0
    if L['box'] is not None:
        c, m, h = L['box']
        return np.all(np.abs((P - c) @ m) <= h[None, :], axis=1)
    return np.ones(len(P), bool)


def irradiance(lights, P, N, shapes=True):
    E = np.zeros_like(P)
    for L in lights:
        v = L['pos'] - P
        d = np.linalg.norm(v, axis=1)
        Ld = v / np.maximum(d, 1e-3)[:, None]
        nl = np.einsum('ij,ij->i', N, Ld)
        x = np.clip(d / L['r'], 0, 1)
        bias, scale, ex = L['bse']
        xe = x ** ex if ex > 0 else np.ones_like(x)
        a = (1 - np.clip(scale * xe + bias, 0, 1)) ** 2.2
        if L['spot']:
            base = np.clip(1 - (1 - (-Ld @ L['aim'])) / max(1 - L['cos'], 1e-4), 0, 1)
            a = a * np.minimum(base ** max(L['cone'], 1e-3), 1)
        w = np.where((d < L['r']) & (nl > 0), a * nl, 0.0)
        if shapes:
            w = np.where(inside(L, P), w, 0.0)
        E += w[:, None] * L['c'][None, :]
    return E


def main(esm, cell, shots):
    img = {p: np.asarray(Image.open(os.path.join(shots, '%s.probe%d.png' % (cell, p))).convert('RGB'), float)
           for p in (1, 2, 3, 4)}
    notes = open(os.path.join(shots, cell + '.lit.notes'), encoding='utf-8', errors='replace').read()
    m = re.search(r'cell lighting: .*center=(-?[\d.]+),(-?[\d.]+),(-?[\d.]+)', notes)
    if not m:
        return 'lit FAIL %s: no "cell lighting ... center=" line in the notes (the cell view published nothing)' % cell
    center = np.array([float(v) for v in m.groups()])
    q = np.round(img[2]) * 256 + np.round(img[3])
    P = q + 0.5 - 32768.0 + center
    N = img[4] / 255.0 * 2 - 1
    nlen = np.linalg.norm(N, axis=2)
    ok = np.abs(nlen - 1) < 0.06
    # the clear colour (the corner pixel) is never a surface, in any probe
    for k in (2, 3, 4):
        ok &= np.any(img[k] != img[k][0, 0], axis=2)
    # a fragment another program drew (effects, glass, PBR) ignores the probe: the same in all three
    ok &= np.any(img[2] != img[3], axis=2) | np.any(img[3] != img[4], axis=2)
    # clean pixels: every 4-neighbour within 40 units (no silhouette, no blend with the background)
    for dy, dx in ((0, 1), (1, 0), (0, -1), (-1, 0)):
        ok &= np.linalg.norm(P - np.roll(P, (dy, dx), (0, 1)), axis=2) < 40
    ok[0, :] = ok[-1, :] = ok[:, 0] = ok[:, -1] = False
    ys, xs = np.nonzero(ok)
    if len(ys) < 2000:
        return 'lit FAIL %s: %d clean cell-lit pixels, under 2000 (no probe served, or the camera is too far)' % (cell, len(ys))
    rng = np.random.default_rng(1)
    pick = rng.choice(len(ys), size=min(20000, len(ys)), replace=False)
    ys, xs = ys[pick], xs[pick]
    Pp, Np = P[ys, xs], N[ys, xs] / nlen[ys, xs][:, None]
    lights = lights_of(esm, cell)
    exp = np.clip(irradiance(lights, Pp, Np) / 4.0, 0, 1)
    got = img[1][ys, xs] / 255.0
    err = np.abs(got - exp)
    good = np.all(err <= 3.0 / 255 + 0.05 * exp, axis=1)
    lit = exp.max(axis=1) > 0.02
    share = good.mean()
    lit_share = good[lit].mean() if lit.any() else 0.0
    verdict = share >= 0.97 and lit.sum() >= 500 and lit_share >= 0.95
    # the spots on their own: a few dozen among hundreds of omnis barely move the totals above
    spots = [L for L in lights if L['spot']]
    spot_lit = (np.clip(irradiance(spots, Pp, Np) / 4.0, 0, 1).max(axis=1) > 0.02) if spots else np.zeros(len(ys), bool)
    spot_txt = 'no spot-lit pixels in frame'
    if spot_lit.sum() >= 200:
        spot_share = good[spot_lit].mean()
        verdict = verdict and spot_share >= 0.95
        spot_txt = '%d spot-lit, agree %.1f%%' % (spot_lit.sum(), 100 * spot_share)
    # lane HEMI1: the pixels the shapes decide -- where the unclipped (all omni) sum differs from the shaped one
    omni = np.clip(irradiance(lights, Pp, Np, shapes=False) / 4.0, 0, 1)
    decided = np.abs(omni - exp).max(axis=1) > 3.0 / 255 + 0.05 * exp.max(axis=1)
    hemis = [L for L in lights if L['hemi']]
    hemi_dec = decided & (np.abs(np.clip(irradiance(hemis, Pp, Np, False) / 4.0, 0, 1)
                                 - np.clip(irradiance(hemis, Pp, Np) / 4.0, 0, 1)).max(axis=1) > 0.02) if hemis \
        else np.zeros(len(ys), bool)
    shape_txt = 'shapes: %d hemisphere, %d box; too few shape-decided pixels in frame (%d, under 200)' % (
        len(hemis), sum(L['box'] is not None for L in lights), decided.sum())
    if decided.sum() >= 200:
        shape_share = good[decided].mean()
        verdict = verdict and shape_share >= 0.95
        shape_txt = 'shapes: %d hemisphere, %d box; %d shape-decided (%d by a hemisphere), agree %.1f%%' % (
            len(hemis), sum(L['box'] is not None for L in lights), decided.sum(), hemi_dec.sum(), 100 * shape_share)
    return ('lit %s %s: %d lights; %d clean pixels sampled, %d lit by them; agree %.1f%% (lit %.1f%%); %s; %s; '
            'mean |err| %.4f, p99 %.4f' % ('PASS' if verdict else 'FAIL', cell, len(lights), len(ys), lit.sum(),
                                           100 * share, 100 * lit_share, spot_txt, shape_txt, err.mean(),
                                           np.percentile(err, 99)))


if __name__ == '__main__':
    if len(sys.argv) < 4:
        print(__doc__)
        sys.exit(2)
    line = main(*sys.argv[1:4])
    print(line)
    sys.exit(0 if ' PASS ' in line else 1)
