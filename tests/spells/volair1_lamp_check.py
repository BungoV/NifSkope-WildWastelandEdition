#!/usr/bin/env python3
"""VOLAIR1 lamp gate (LAMPAIR2, bungo 10-14): does every placed light in view light the air around it?

  volair1_lamp_check.py <Fallout4.esm or the load order, comma-separated> <dir> <tag> <nolights tag> [maxdist] [label]

<dir>/<tag>.bin(.txt) is a WW_CELL_AIR_DUMP of a frame; <nolights tag> the same frame with WW_CELL_AIR_RED=nolights.
For every light of the dump's 'light' lines whose sphere centre is in the view and within maxdist (default 4000 u):

  ours  = the in-scatter the froxels add inside the light's sphere: Monte Carlo over the ball (uniform samples), at
          each sample the froxel it falls in, sigma0 x rho(z) x (J_tag - J_nolights), J = the slice's light / (1 - its
          transmittance) in luminance (what the pass stored), times the ball's volume / samples
  ref   = the same integral of an independent single-scatter law at the sample itself: for every light of the
          plugin's own records (this script's walk of the ESM: DATA radius + XRDS, colour (byte / 255)^2.2 x (FNAM +
          XLIG fade), the bias / scale / exponent radial curve, the spot cone) sigma0 x rho(z) x colour x radial x
          cone x pi x phase(cos to the eye) x exp(-sigma0 rho d), unshadowed; rho is the dump's fitted law
          (airMed, airLayer, airGround), the phase the dump's record weights (phase0..2), colour x phase per
          channel, then luminance (the weights are coloured: a warm lamp scatters less than its luminance says)
  ratio = ours / ref. Only samples inside the frustum and the slices count (coverage = their share of the ball).

A light FAILS when ratio < 0.5 (bungo's bar) or > 2 (the gate is two-sided); SKIP when coverage < 0.5 or the
reference is under 1e-6 of the frame's largest. Verdict: PASS when no light in view fails and at least 3 are judged.
A second bar a wrong law cannot pass (10-15): the median ratio over the judged lights lies in 0.8..1.25. The per-light
band alone let the linear falloff (WW_CELL_LIT_RED=linear, about 1.8x the in-scatter) through on most lights.
The table: formid, radius (record / dump), colour lum, distance, shadow kind, coverage, ref, ours, ratio.
Reds (the dumps): WW_CELL_AIR_RED=lightmid (the light at each slice's middle only, the old law) must FAIL.
"""
import math
import os
import struct
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cell_lit_check as CL  # noqa: E402  (the plugin walk: fields, record, walk, euler; nothing from src/)

LUM = np.array([0.2126, 0.7152, 0.0722])


def read_txt(p):
    t = {}
    lights = []
    for line in open(p):
        f = line.split()
        if not f:
            continue
        if f[0] == 'light':
            v = [float(x) for x in f[2:15]]
            lights.append(dict(i=int(f[1]), pos=np.array(v[:3]), r=v[3], c=np.array(v[4:7]), cos=v[7],
                               shape=int(v[8]), shadow=int(v[9]), ref=int(f[15], 16)))
            continue
        try:
            t[f[0]] = np.array([float(x) for x in f[1:]])
        except ValueError:
            pass
    return t, lights


def read_inj(p):
    b = np.fromfile(p, np.uint8)
    c, r, s = np.frombuffer(b[:12].tobytes(), np.int32)
    v = np.frombuffer(b[12:].tobytes(), np.float32).reshape(2, s, r, c, 4).astype(float)
    return v[0]


def J_of(inj):
    a = np.clip(1.0 - inj[..., 3], 1e-9, None)
    return (inj[..., :3] @ LUM) / a


def rho_law(t, z):
    """the fitted density's shape (sigma0 left out), z in the fog's height frame"""
    s0, zb, invh, top = t['airMed']
    w, zn, invhn, s = t['airLayer'] if 'airLayer' in t else (0.0, 0.0, 0.0, 0.0)
    zg, invhg = tuple(t['airGround'][:2]) if 'airGround' in t else (0.0, 0.0)   # the uniform's vec4: (zg, 1/Hg, 0, 0)

    def B(z0, ih):
        return np.minimum(1.0, np.exp(-(z - z0) * ih))
    r = (1.0 - w) * B(zb, invh) + (w * B(zn, invhn) if w > 0 else 0.0) + (s * B(zg, invhg) if s > 0 else 0.0)
    return np.where(z <= top, r, 0.0)


def phase_pi(t, c):
    """the phase x pi per colour channel, shape c.shape + (3,): the record's weights are coloured (the air term is
    Rayleigh-blue), so a light's luminance is taken AFTER the product. 10-15: lum(colour) x lum(weights) read a warm
    lamp ~15% high, which was the whole 'lamp shortfall' (median 0.848)."""
    def hg(g):
        d = 1.0 + g * g - 2.0 * g * c
        return (1.0 - g * g) / (4.0 * math.pi * d * np.sqrt(d))
    p0, p1, p2 = t['phase0'], t['phase1'], t['phase2']
    return math.pi * (np.multiply.outer(3.0 / (16.0 * math.pi) * (1.0 + c * c), p0[:3])
                      + np.multiply.outer(hg(p1[3]), p1[:3]) + np.multiply.outer(hg(p2[3]), p2[:3]))


def plugin_lights(esm, forms):
    """the records of the given REFR forms, read from the plugins (cell_lit_check.lights_of's rules, any cell).
    esm: one plugin, or the shot's whole load order comma-separated (a later plugin's record wins, as the cell
    loader's). 10-15: a ref read from Fallout4.esm alone failed 0006a559 at ratio 0.444; UltraExteriorLighting.esp
    overrides its XRDS (-103.6 -> -177.0: radius 152.4 -> 79.0, which is what the dump drew)."""
    ligh, refs = {}, {}
    order = [e.strip() for e in esm.split(',') if e.strip()]
    names = [os.path.basename(e).lower() for e in order]
    for li, one in enumerate(order):
        buf = open(one, 'rb').read()
        # a plugin's form ids name their file by its OWN master list (top byte = master index, its own records =
        # the master count); the dump's refs are load-order ids (top byte = load index), as the cell loader numbers
        # them. 10-15: 2222a12e (a light UltraExteriorLighting.esp adds, load index 0x22) had no record before.
        hsize = struct.unpack_from('<I', buf, 4)[0]
        hdata = buf[24:24 + hsize]
        masters = [bytes(p_).rstrip(b'\0').decode('utf-8', 'replace').lower() for t_, p_ in CL.fields(hdata) if t_ == b'MAST']
        remap = [names.index(m) if m in names else 0xFF for m in masters] + [li]

        def glob(fid, remap=remap):
            hi = fid >> 24
            return ((remap[hi] if hi < len(remap) else 0xFF) << 24) | (fid & 0xFFFFFF)
        for t, form, off, stack in CL.walk(buf):
            if t == b'LIGH':
                _, f = CL.record(buf, off)
                if b'DATA' in f:
                    ligh[glob(form)] = f
            elif t == b'REFR' and glob(form) in forms:
                fl, f = CL.record(buf, off)
                if b'NAME' in f:
                    f = dict(f)
                    f[b'NAME'] = struct.pack('<I', glob(struct.unpack_from('<I', f[b'NAME'])[0]))
                refs[glob(form)] = (fl, f)
    out = {}
    for form, (flags, f) in refs.items():
        if b'NAME' not in f:
            continue
        b = ligh.get(struct.unpack_from('<I', f[b'NAME'])[0])
        if b is None:
            continue
        d = b[b'DATA']
        radius, = struct.unpack_from('<I', d, 4)
        col = np.array(list(d[8:11]), float)
        lf, = struct.unpack_from('<I', d, 12)
        falloff, fov = struct.unpack_from('<2f', d, 16) if len(d) >= 28 else (1.0, 90.0)
        bse = struct.unpack_from('<3f', d, 40) if len(d) >= 52 else (0.0, 1.0, 2.0)
        fade = struct.unpack_from('<f', b[b'FNAM'])[0] if b'FNAM' in b else 1.0
        xlig = struct.unpack_from('<%df' % (len(f[b'XLIG']) // 4), f[b'XLIG']) if b'XLIG' in f else ()
        r = radius + (struct.unpack_from('<f', f[b'XRDS'])[0] if b'XRDS' in f else 0.0)
        c = (col / 255.0) ** 2.2 * (fade + (xlig[1] if len(xlig) >= 2 else 0.0))
        pos = struct.unpack_from('<3f', f[b'DATA'], 0)
        rot = struct.unpack_from('<3f', f[b'DATA'], 12)
        hemi = bool(lf & 0x800)
        spot = bool(lf & 0x4400) and not hemi
        cos_outer = math.cos(math.radians(fov + (xlig[0] if xlig else 0.0)) / 2) if spot else -2.0
        aim = CL.euler(-rot[0], -rot[1], -rot[2]) @ np.array([1.0, 0.0, 0.0])
        out[form] = dict(pos=np.array(pos), r=r, c=c, lum=float(c @ LUM), spot=spot, hemi=hemi, cos=cos_outer,
                         aim=aim, cone=falloff, bse=bse, off=bool(lf & 0x20) or bool(lf & 0x100000) or bool(flags & 0x800))
    return out


def ref_J(L, W, eye_dir, t, s0, zoff):
    """one light's single-scatter source at world points W (luminance, unshadowed), without sigma"""
    v = L['pos'] - W
    d = np.linalg.norm(v, axis=1)
    Ld = v / np.maximum(d, 1e-3)[:, None]
    x = np.clip(d / L['r'], 0, 1)
    bias, scale, ex = L['bse']
    xe = x ** ex if ex > 0 else np.ones_like(x)
    a = (1 - np.clip(scale * xe + bias, 0, 1)) ** 2.2
    if L['spot']:
        base = np.clip(1 - (1 - (-Ld @ L['aim'])) / max(1 - L['cos'], 1e-4), 0, 1)
        a = a * np.minimum(base ** max(L['cone'], 1e-3), 1)
    if L['hemi']:
        a = np.where((W - L['pos']) @ L['aim'] >= 0, a, 0.0)
    a = np.where(d < L['r'], a, 0.0)
    za, zl = W[:, 2] + zoff, L['pos'][2] + zoff
    od = s0 * 0.5 * (rho_law(t, za) + rho_law(t, np.full_like(za, zl))) * d
    c = np.einsum('ij,ij->i', Ld, eye_dir)	# the light's travel is -Ld; the scattered ray goes to the eye (-eye_dir)
    return a * np.exp(-od) * (phase_pi(t, c) @ (L['c'] * LUM))


def main(esm, d, tag, red, maxd=4000.0, label=''):
    t, lights = read_txt(os.path.join(d, tag + '.bin.txt'))
    JA = J_of(read_inj(os.path.join(d, tag + '.bin')))
    JR = J_of(read_inj(os.path.join(d, red + '.bin')))
    cols, rows, N = (int(x) for x in t['dims'])
    near, logR = t['slices']
    p00, p11 = t['proj']
    far = near * math.exp(logR)
    s0 = float(t['airMed'][0])
    ds = float(t['fogDistScale'][0]) if 'fogDistScale' in t else 1.0
    R = np.stack([t['cellRow%d' % i] for i in range(3)])
    M, cam = R[:, :3], R[:, 3]
    Minv = np.linalg.inv(M)
    zoff = float(t['fogView'][3]) - float(t['camZw'][0]) if 'camZw' in t else float(t['fogView'][3]) - cam[2]
    PL = plugin_lights(esm, {L['ref'] for L in lights if L['ref']})
    rng = np.random.default_rng(7)
    n = 6000
    u = rng.normal(size=(n, 3))
    u /= np.linalg.norm(u, axis=1)[:, None]
    u *= rng.random(n)[:, None] ** (1.0 / 3.0)
    rows_out, judged, fails = [], 0, 0
    cand = []
    for L in lights:
        v = Minv @ (L['pos'] - cam)
        dist = np.linalg.norm(v) * ds
        if v[2] >= 0 or dist > maxd:
            continue
        nx, ny = v[0] / -v[2] * p00, v[1] / -v[2] * p11
        if abs(nx) > 1 or abs(ny) > 1:
            continue
        cand.append((L, dist))
    refs_all = [P for P in PL.values() if not P['off'] and P['r'] > 0 and P['lum'] > 0]
    for L, dist in sorted(cand, key=lambda q: q[1]):
        P = PL.get(L['ref'])
        r = L['r']
        W = L['pos'][None, :] + u * r
        V = (W - cam) @ Minv.T
        dd = np.linalg.norm(V, axis=1) * ds
        ok = V[:, 2] < 0
        nx = np.where(ok, V[:, 0] / np.where(ok, -V[:, 2], 1) * p00, 9)
        ny = np.where(ok, V[:, 1] / np.where(ok, -V[:, 2], 1) * p11, 9)
        ok &= (np.abs(nx) < 1) & (np.abs(ny) < 1) & (dd > near) & (dd < far)
        cov = ok.mean()
        ci = np.clip(((nx + 1) * 0.5 * cols).astype(int), 0, cols - 1)
        ri = np.clip(((ny + 1) * 0.5 * rows).astype(int), 0, rows - 1)
        ki = np.clip((N * np.log(np.maximum(dd, near) / near) / logR).astype(int), 0, N - 1)
        sig = s0 * rho_law(t, W[:, 2] + zoff)
        dJ = np.where(ok, JA[ki, ri, ci] - JR[ki, ri, ci], 0.0)
        eye = (W - cam)
        eye /= np.maximum(np.linalg.norm(eye, axis=1), 1e-6)[:, None]
        Jr = np.zeros(n)
        for Q in refs_all:	# every plugin light that reaches the ball (the stored J holds them all)
            if np.linalg.norm(Q['pos'] - L['pos']) < Q['r'] + r:
                Jr += ref_J(Q, W, eye, t, s0, zoff)
        vol = 4.0 / 3.0 * math.pi * r ** 3
        ours = float((sig * dJ)[ok].sum() / n * vol)
        ref = float((sig * Jr)[ok].sum() / n * vol)
        rows_out.append([L, P, dist, cov, ref, ours])
    top = max([q[4] for q in rows_out] + [1e-30])
    lines = []
    for L, P, dist, cov, ref, ours in rows_out:
        ratio = ours / ref if ref > 0 else float('nan')
        if P is None:
            st = 'SKIP(no record)'
        elif cov < 0.5:
            st = 'SKIP(coverage)'
        elif ref < 1e-6 * top:
            st = 'SKIP(dim)'
        else:
            judged += 1
            st = 'ok' if 0.5 <= ratio <= 2.0 else 'FAIL'
            fails += st == 'FAIL'
        lines.append('  %08x r %7.1f/%7.1f lum %8.4f d %6.0f shadow %d cov %.2f ref %.4e ours %.4e ratio %6.3f %s' % (
            L['ref'], P['r'] if P else float('nan'), L['r'], P['lum'] if P else float('nan'), dist, L['shadow'], cov,
            ref, ours, ratio, st))
    rs = [q[5] / q[4] for q in rows_out if q[4] > 0 and q[3] >= 0.5 and q[1] is not None and q[4] >= 1e-6 * top]
    med = float(np.median(rs)) if rs else float('nan')
    verdict = 'PASS' if judged >= 3 and fails == 0 and 0.8 <= med <= 1.25 else 'FAIL'
    head = 'VOLAIR1 LAMP %s %s: %d lights in view within %.0f u, %d judged, %d fail; ratio median %.3f (bar 0.8..1.25) min %.3f max %.3f (red %d)' % (
        label or tag, verdict, len(rows_out), maxd, judged, fails, float(np.median(rs)) if rs else float('nan'),
        min(rs) if rs else float('nan'), max(rs) if rs else float('nan'), int(t['red'][0]) if 'red' in t else -1)
    return head, lines, verdict


if __name__ == '__main__':
    a = sys.argv[1:]
    head, lines, v = main(a[0], a[1], a[2], a[3], float(a[4]) if len(a) > 4 else 4000.0, a[5] if len(a) > 5 else '')
    print(head)
    print('\n'.join(lines))
    print('VOLAIR1 LAMP VERDICT %s' % v)
    sys.exit(0 if v == 'PASS' else 1)
