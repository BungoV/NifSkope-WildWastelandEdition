#!/usr/bin/env python3
"""The GI check (lane PRTPGI, 2026-10-01; tests/spells/cell_gi.sh, src/probegi.h).

  cell_gi_check.py <Fallout4.esm> <interior EDID> <run dir>

The run dir holds what one cell_gi.sh pass wrote: bake/ (the .tbk files), soup.psp (the bake's
triangles), dump/ (gi_surfels.bin, gi_probes.bin, gi_grid.bin, gi_meta.txt) and, for stage D, the
pictures probe2..5.png + lit.notes. Every stage is rebuilt here from the files and the plugin, with
its own reader, tracer and light walk (cell_lit_check.lights_of), nothing taken from NifSkope's code:

  A  surfel light   the dumped surfels are exactly the bake's (position, normal, albedo); 400 of them
                    relit: each light within its radius, facing, PRTP2 curve and cone, behind a shadow
                    segment from 2 units off the surface to `fixtureClear` short of the light
  B  probe gather   every probe's six-axis cube from its links (octahedral direction, weight x scale x
                    4 pi, max(axis . dir, 0)), the unlinked share renormalized over the linked
  C  voxel grid     250 voxels next to the surfaces: the probes within the radius the voxel can see
                    (an unblocked segment), weight (1 - d^2 / r^2)^2; an empty voxel must see none
  D  the picture    probe 5 at every clean pixel against this file's trilinear sample of the grid at
                    the pixel's position (probes 2 + 3) + half a voxel along its normal (probe 4)
One line a stage ("A PASS ..."), then the verdict line; exit 0 when every stage passes.
"""
import math
import os
import re
import struct
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from cell_lit_check import lights_of  # noqa: E402
from probe_bake import read_tbk, unpack_dir, floordiv  # noqa: E402

SURF_OFF = 2.0      # the relight's shadow segment starts this far off the surface (probegi.cpp)
AXES = np.array([[1, 0, 0], [-1, 0, 0], [0, 1, 0], [0, -1, 0], [0, 0, 1], [0, 0, -1]], float)


# ---------------------------------------------------------------- the soup and a segment test
class Soup:
    CELL = 128.0

    def __init__(self, path):
        b = open(path, 'rb').read()
        magic, ntri, ndoor = struct.unpack_from('<3I', b, 0)
        assert magic == 0x31505350, 'not a PSP1 soup'
        self.t = np.frombuffer(b, '<f4', ntri * 9, 12).reshape(ntri, 3, 3).astype(np.float64)
        lo = np.floor(self.t.min(1) / self.CELL).astype(np.int64)
        hi = np.floor(self.t.max(1) / self.CELL).astype(np.int64)
        span = hi - lo + 1
        cnt = span.prod(1)
        tri = np.repeat(np.arange(ntri), cnt)
        start = np.repeat(np.cumsum(cnt) - cnt, cnt)
        k = np.arange(cnt.sum()) - start
        sp = span[tri]
        ix = lo[tri, 0] + k % sp[:, 0]
        iy = lo[tri, 1] + (k // sp[:, 0]) % sp[:, 1]
        iz = lo[tri, 2] + k // (sp[:, 0] * sp[:, 1])
        key = self.key(ix, iy, iz)
        order = np.argsort(key, kind='stable')
        self.keys, first = np.unique(key[order], return_index=True)
        self.first = np.append(first, len(order))
        self.tris = tri[order]

    @staticmethod
    def key(ix, iy, iz):
        return ((ix + (1 << 20)) << 42) | ((iy + (1 << 20)) << 21) | (iz + (1 << 20))

    def blocked(self, o, q, clear_end):
        """A hit in (1e-4, |q - o| - clear_end] along o -> q (double-sided)."""
        d = q - o
        L = float(np.linalg.norm(d))
        tmax = L - clear_end
        if tmax <= 1e-3:
            return False
        d = d / L
        lo = np.floor(np.minimum(o, o + d * tmax) / self.CELL).astype(np.int64)
        hi = np.floor(np.maximum(o, o + d * tmax) / self.CELL).astype(np.int64)
        g = np.stack(np.meshgrid(*[np.arange(lo[a], hi[a] + 1) for a in range(3)], indexing='ij'), -1).reshape(-1, 3)
        kk = self.key(g[:, 0], g[:, 1], g[:, 2])
        pos = np.searchsorted(self.keys, kk)
        inside = pos < len(self.keys)
        pos, kk = pos[inside], kk[inside]
        pos = pos[self.keys[pos] == kk]
        idx = [self.tris[self.first[p]:self.first[p + 1]] for p in pos]
        if not idx:
            return False
        T = self.t[np.unique(np.concatenate(idx))]
        p0, e1, e2 = T[:, 0], T[:, 1] - T[:, 0], T[:, 2] - T[:, 0]
        pv = np.cross(d[None, :], e2)
        det = np.einsum('tk,tk->t', e1, pv)
        ok = np.abs(det) >= 1e-12
        idt = np.where(ok, 1.0 / np.where(ok, det, 1.0), 0.0)
        tv = o[None, :] - p0
        u = np.einsum('tk,tk->t', tv, pv) * idt
        qv = np.cross(tv, e1)
        v = (qv @ d) * idt
        t = np.einsum('tk,tk->t', e2, qv) * idt
        return bool(np.any(ok & (u >= 0) & (u <= 1) & (v >= 0) & (u + v <= 1) & (t > 1e-4) & (t <= tmax)))


# ---------------------------------------------------------------- the dump
def read_dump(d):
    b = open(os.path.join(d, 'gi_surfels.bin'), 'rb').read()
    n = struct.unpack_from('<i', b)[0]
    S = np.frombuffer(b, '<f4', n * 12, 4).reshape(n, 12).astype(np.float64)
    b = open(os.path.join(d, 'gi_probes.bin'), 'rb').read()
    n = struct.unpack_from('<i', b)[0]
    P = np.frombuffer(b, '<f4', n * 21, 4).reshape(n, 21).astype(np.float64)
    b = open(os.path.join(d, 'gi_grid.bin'), 'rb').read()
    ox, oy, oz, vox, rad = struct.unpack_from('<5f', b, 0)
    dims = struct.unpack_from('<3i', b, 20)
    G = np.frombuffer(b, '<f4', 6 * dims[0] * dims[1] * dims[2] * 4, 32).reshape(6, dims[2], dims[1], dims[0], 4)
    meta = open(os.path.join(d, 'gi_meta.txt'), encoding='utf-8').read()
    clear = float(re.search(r'fixtureClear (\S+)', meta).group(1))
    return S, P, dict(origin=np.array([ox, oy, oz], float), voxel=float(vox), radius=float(rad), dims=dims, grid=G), clear


def surfel_light(lights, p, n, soup, clear):
    E = np.zeros(3)
    o = p + n * SURF_OFF
    blocked = 0
    for L in lights:
        v = L['pos'] - p
        d = float(np.linalg.norm(v))
        if d >= L['r']:
            continue
        Ld = v / max(d, 1e-3)
        nl = float(n @ Ld)
        if nl <= 0:
            continue
        x = min(max(d / L['r'], 0.0), 1.0)
        bias, scale, ex = L['bse']
        a = (1 - min(max(scale * (x ** ex if ex > 0 else 1.0) + bias, 0.0), 1.0)) ** 2.2
        if L['spot']:
            base = min(max(1 - (1 - float(-Ld @ L['aim'])) / max(1 - L['cos'], 1e-4), 0.0), 1.0)
            a *= min(base ** max(L['cone'], 1e-3), 1.0)
        if a * nl <= 0:
            continue
        if soup.blocked(o, L['pos'], clear):
            blocked += 1
            continue
        E += L['c'] * a * nl
    return E, blocked


def close(a, b, rel=0.02, ab=1e-3):
    return bool(np.all(np.abs(a - b) <= ab + rel * np.maximum(np.abs(a), np.abs(b))))


def stage_a(esm, cell, tbks, S, soup, clear):
    # the dump's surfels are the bake's, one per (position, normal)
    want = {}
    for _, t in tbks:
        for s in list(t['surfels']) + list(t['back']):   # lane BAKE4: v4's back sides are surfels too
            k = tuple(np.float32(s['pos']).tobytes() for _ in (0,)) + (tuple(int(c) for c in s['nrm']),)
            want[k] = (np.array(s['nrm'], float), np.array(s['alb'], float) / 255.0)
    got = {}
    for i, r in enumerate(S):
        got.setdefault(np.float32(r[0:3]).astype('<f4').tobytes(), []).append(i)
    if len(want) != len(S):
        return 'A FAIL surfels: the dump holds %d, the bake %d unique' % (len(S), len(want)), None
    bad = 0
    row_of = {}
    for (pb, nk), (nv, alb) in want.items():
        hit = None
        for i in got.get(pb, []):
            nn = nv / np.linalg.norm(nv)
            if np.allclose(S[i, 3:6], nn, atol=1e-5) and np.allclose(S[i, 6:9], alb, atol=1e-6):
                hit = i
        if hit is None:
            bad += 1
        else:
            row_of[(pb, nk)] = hit
    if bad:
        return 'A FAIL surfels: %d of the bake\'s surfels are not in the dump' % bad, None
    lights = lights_of(esm, cell)
    rng = np.random.default_rng(7)
    pick = rng.choice(len(S), size=min(400, len(S)), replace=False)
    good, lit, shadowed = 0, 0, 0
    for i in pick:
        E, nb = surfel_light(lights, S[i, 0:3], S[i, 3:6], soup, clear)
        B = S[i, 6:9] * E
        shadowed += nb
        lit += int(B.max() > 1e-3)
        good += int(close(S[i, 9:12], B))
    share = good / len(pick)
    ok = share >= 0.97 and shadowed >= 20 and lit >= 50
    return ('A %s surfel light: %d lights; %d surfels relit, %d lit, %d shadow segments blocked; agree %.1f%%'
            % ('PASS' if ok else 'FAIL', len(lights), len(pick), lit, shadowed, 100 * share)), row_of


def stage_b(tbks, S, P, row_of):
    cubes, k = [], 0
    unresolved = 0
    for _, t in tbks:
        cs = float(t['cell'])
        keys = ({}, {})   # lane BAKE4: the front sides, the v4 back sides
        for side, arr in enumerate((t['surfels'], t['back'])):
            for j, s in enumerate(arr):
                keys[side].setdefault(tuple(floordiv(s['pos'][a], cs) for a in range(3)), j)
        for pr in t['probes']:
            pk = [floordiv(pr['pos'][a], cs) for a in range(3)]
            E = np.zeros((6, 3))
            linked = 0.0
            a0 = int(pr['off'])
            for li, lk in enumerate(t['links'][pr['off']:pr['off'] + pr['cnt']]):
                x = t['lext'][a0 + li]
                side = int(x['side'])
                j = keys[side].get(tuple(int(pk[a]) + int(lk['delta'][a]) for a in range(3)))
                if j is None:
                    unresolved += 1
                    continue
                s = (t['surfels'], t['back'])[side][j]
                # the glass on the way tints what the link carries
                B = S[row_of[(np.float32(s['pos']).tobytes(), tuple(int(c) for c in s['nrm']))], 9:12] \
                    * (x['tint'].astype(np.float64) / 255.0)
                d = unpack_dir(lk['dir'])
                w = float(lk['w']) * float(pr['scale'])
                linked += w
                E += np.maximum(AXES @ d, 0.0)[:, None] * B[None, :] * (w * 4 * math.pi)
            if linked > 0 and pr['unl'] > 0:
                E *= (linked + float(pr['unl'])) / linked
            cubes.append((np.array(pr['pos'], float), E))
            k += 1
    if k != len(P):
        return 'B FAIL probe gather: the dump holds %d probes, the bake %d' % (len(P), k)
    good = sum(1 for i, (pos, E) in enumerate(cubes)
               if np.allclose(P[i, 0:3], pos, atol=1e-3) and close(P[i, 3:21].reshape(6, 3), E))
    lit = sum(1 for _, E in cubes if E.max() > 1e-3)
    share = good / len(cubes)
    ok = share >= 0.99 and lit >= min(20, len(cubes) // 3) and unresolved == 0
    return ('B %s probe gather: %d probes (%d see bounce light), %d links unresolved; agree %.1f%%'
            % ('PASS' if ok else 'FAIL', len(cubes), lit, unresolved, 100 * share))


def stage_c(S, P, G, soup):
    v, o, dims, rad, grid = G['voxel'], G['origin'], G['dims'], G['radius'], G['grid']
    g = np.floor((S[:, 0:3] + S[:, 3:6] * v * 0.5 - o) / v).astype(int)
    near = np.zeros((dims[2], dims[1], dims[0]), bool)
    for dz in (-1, 0, 1):
        for dy in (-1, 0, 1):
            for dx in (-1, 0, 1):
                q = g + np.array([dx, dy, dz])
                m = np.all((q >= 0) & (q < np.array(dims)), 1)
                near[q[m, 2], q[m, 1], q[m, 0]] = True
    valid = grid[0, :, :, :, 3] > 0.5
    if np.any(valid & ~near):
        return 'C FAIL voxel grid: %d voxels away from every surface hold light' % int(np.sum(valid & ~near))
    rng = np.random.default_rng(11)
    vz = np.argwhere(valid)
    ez = np.argwhere(near & ~valid)
    pick = [(tuple(c), True) for c in vz[rng.choice(len(vz), size=min(150, len(vz)), replace=False)]] + \
           [(tuple(c), False) for c in ez[rng.choice(len(ez), size=min(100, len(ez)), replace=False)]]
    pp, cube = P[:, 0:3], P[:, 3:21].reshape(-1, 6, 3)
    good, rays, blocked = 0, 0, 0
    for (z, y, x), isvalid in pick:
        c = o + (np.array([x, y, z], float) + 0.5) * v
        d2 = np.sum((pp - c) ** 2, 1)
        acc, ws = np.zeros((6, 3)), 0.0
        for j in np.nonzero(d2 < rad * rad)[0]:
            rays += 1
            if soup.blocked(c, pp[j], 0.0):
                blocked += 1
                continue
            w = (1 - d2[j] / (rad * rad)) ** 2
            ws += w
            acc += w * cube[j]
        if not isvalid:
            good += int(ws == 0.0)
        elif ws > 0:
            got = np.stack([grid[a, z, y, x, 0:3] for a in range(6)])
            good += int(close(got, acc / ws))
    share = good / len(pick)
    ok = share >= 0.97 and blocked >= 20
    return ('C %s voxel grid: %d voxels (%d lit, %d empty), %d probe segments, %d blocked; agree %.1f%%'
            % ('PASS' if ok else 'FAIL', len(pick), sum(1 for _, a in pick if a), sum(1 for _, a in pick if not a),
               rays, blocked, 100 * share))


def sample_grid(G, Pw, Nw):
    v, o, dims, grid = G['voxel'], G['origin'], np.array(G['dims'], float), G['grid']
    g = (Pw + Nw * (0.5 * v) - o) / v
    z = np.clip(g[:, 2], 0.5, dims[2] - 0.5)

    def tex(slab):
        # GL_LINEAR, clamp to edge: texel centres at i + 0.5
        cx, cy, cz = g[:, 0] - 0.5, g[:, 1] - 0.5, z + slab * dims[2] - 0.5
        out = np.zeros((len(g), 4))
        for ox in (0, 1):
            for oy in (0, 1):
                for oz in (0, 1):
                    ix = np.clip(np.floor(cx).astype(int) + ox, 0, int(dims[0]) - 1)
                    iy = np.clip(np.floor(cy).astype(int) + oy, 0, int(dims[1]) - 1)
                    iz = np.clip(np.floor(cz).astype(int) + oz, 0, 6 * int(dims[2]) - 1)
                    fx, fy, fz = cx - np.floor(cx), cy - np.floor(cy), cz - np.floor(cz)
                    w = (fx if ox else 1 - fx) * (fy if oy else 1 - fy) * (fz if oz else 1 - fz)
                    flat = grid.reshape(6 * int(dims[2]), int(dims[1]), int(dims[0]), 4)
                    out += w[:, None] * flat[iz, iy, ix]
        return out
    n2 = Nw * Nw
    s = (n2[:, 0:1] * np.where(Nw[:, 0:1] >= 0, tex(0), tex(1)) + n2[:, 1:2] * np.where(Nw[:, 1:2] >= 0, tex(2), tex(3))
         + n2[:, 2:3] * np.where(Nw[:, 2:3] >= 0, tex(4), tex(5)))
    E = np.where(s[:, 3:4] > 0.01, np.maximum(s[:, 0:3] / np.maximum(s[:, 3:4], 1e-9), 0), 0.0)
    return np.clip(E / math.pi, 0, 1)


def stage_d(run, cell, G, sub='', label='D'):
    img = {p: np.asarray(Image.open(os.path.join(run, sub, 'probe%d.png' % p)).convert('RGB'), float) for p in (2, 3, 4, 5)}
    notes = open(os.path.join(run, 'lit.notes'), encoding='utf-8', errors='replace').read()
    m = re.search(r'cell lighting: .*center=(-?[\d.]+),(-?[\d.]+),(-?[\d.]+)', notes)
    if not m:
        return label + ' FAIL picture: no "center=" in the notes'
    center = np.array([float(x) for x in m.groups()])
    P = np.round(img[2]) * 256 + np.round(img[3]) + 0.5 - 32768.0 + center
    N = img[4] / 255.0 * 2 - 1
    nlen = np.linalg.norm(N, axis=2)
    ok = np.abs(nlen - 1) < 0.06
    for k in (2, 3, 4):
        ok &= np.any(img[k] != img[k][0, 0], axis=2)
    ok &= np.any(img[2] != img[3], axis=2) | np.any(img[3] != img[4], axis=2)
    for dy, dx in ((0, 1), (1, 0), (0, -1), (-1, 0)):
        ok &= np.linalg.norm(P - np.roll(P, (dy, dx), (0, 1)), axis=2) < 40
    ok[0, :] = ok[-1, :] = ok[:, 0] = ok[:, -1] = False
    ys, xs = np.nonzero(ok)
    if len(ys) < 2000:
        return label + ' FAIL picture: %d clean pixels, under 2000' % len(ys)
    rng = np.random.default_rng(3)
    pick = rng.choice(len(ys), size=min(20000, len(ys)), replace=False)
    ys, xs = ys[pick], xs[pick]
    exp = sample_grid(G, P[ys, xs], N[ys, xs] / nlen[ys, xs][:, None])
    got = img[5][ys, xs] / 255.0
    good = np.all(np.abs(got - exp) <= 3.0 / 255 + 0.05 * exp, axis=1)
    lit = exp.max(1) > 0.02
    share, lit_share = good.mean(), (good[lit].mean() if lit.any() else 0.0)
    if lit.sum() < 500:
        # a dim cell: too little bounce in frame to test it (the red "off" cannot fail here)
        ok = share >= 0.97
        return (label + ' %s picture: %d clean pixels, no bounce above 0.02 in frame (dim cell); agree %.1f%%, mean |err| %.4f'
                % ('PASS' if ok else 'FAIL', len(ys), 100 * share, np.abs(got - exp).mean()))
    ok = share >= 0.95 and lit_share >= 0.93
    return (label + ' %s picture: %d clean pixels, %d with bounce; agree %.1f%% (with bounce %.1f%%), mean |err| %.4f'
            % ('PASS' if ok else 'FAIL', len(ys), lit.sum(), 100 * share, 100 * lit_share, np.abs(got - exp).mean()))


def stage_e(run, cell, G):
    # the PBR program (pbrm_cell.prog) shows the same grid: its probes 2-5 judged as D's, and the census says
    # every material shape it drew went through pbrm_cell, none through the plain pbrm_default
    cen = open(os.path.join(run, 'pbr', 'probe5.prog.txt'), encoding='utf-8', errors='replace').read()
    cell_n, plain_n = cen.count('prog=pbrm_cell.prog'), cen.count('prog=pbrm_default.prog')
    line = stage_d(run, cell, G, 'pbr', 'E')
    if cell_n < 10 or plain_n:
        line = line.replace('E PASS', 'E FAIL', 1)
    return line + '; census pbrm_cell %d, pbrm_default %d' % (cell_n, plain_n)


def main(esm, cell, run, stages='ABCDE'):
    bake = os.path.join(run, 'bake')
    tbks = [(f, read_tbk(os.path.join(bake, f))) for f in sorted(os.listdir(bake)) if f.endswith('.tbk')]
    S, P, G, clear = read_dump(os.path.join(run, 'dump'))
    soup = Soup(os.path.join(run, 'soup.psp'))
    lines = []
    a, row_of = stage_a(esm, cell, tbks, S, soup, clear)
    lines.append(a)
    if row_of is not None:
        lines.append(stage_b(tbks, S, P, row_of))
    lines.append(stage_c(S, P, G, soup))
    if 'D' in stages and os.path.exists(os.path.join(run, 'probe5.png')):
        lines.append(stage_d(run, cell, G))
    if 'E' in stages and os.path.exists(os.path.join(run, 'pbr', 'probe5.png')):
        lines.append(stage_e(run, cell, G))
    return lines


if __name__ == '__main__':
    if len(sys.argv) < 4:
        print(__doc__)
        sys.exit(2)
    lines = main(*sys.argv[1:5])
    for line in lines:
        print(line)
    ok = all(' PASS ' in line for line in lines) and len(lines) >= 3
    print('gi %s %s' % ('PASS' if ok else 'FAIL', sys.argv[2]))
    sys.exit(0 if ok else 1)
