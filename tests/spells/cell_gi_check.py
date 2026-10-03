#!/usr/bin/env python3
"""The GI check (lane PRTPGI, 2026-10-01; tests/spells/cell_gi.sh, src/probegi.h).

  cell_gi_check.py <Fallout4.esm> <interior EDID> <run dir>

The run dir holds what one cell_gi.sh pass wrote: bake/ (the .tbk files), soup.psp (the bake's
triangles), dump/ (gi_surfels.bin, gi_probes.bin, gi_grid.bin, gi_meta.txt) and, for stage D, the
pictures probe2..5.png + lit.notes. Every stage is rebuilt here from the files and the plugin, with
its own reader, tracer and light walk (cell_lit_check.lights_of), nothing taken from NifSkope's code:

  A  surfel light   the dumped surfels are exactly the bake's (position, normal, albedo); 400 of them
                    relit: each light within its radius and shape, facing, PRTP2 curve and cone, behind a shadow
                    segment from 2 units off the surface to `fixtureClear` short of the light
  B  probe gather   every probe's six-axis cube from its links (octahedral direction, weight x scale x
                    4 pi, max(axis . dir, 0)), the unlinked share renormalized over the linked
  C  voxel grid     250 voxels next to the surfaces: the probes within the radius the voxel can see
                    (an unblocked segment), weight (1 - d^2 / r^2)^2; an empty voxel must see none
  D  the picture    probe 5 at every clean pixel against this file's trilinear sample of the grid at
                    the pixel's position (probes 2 + 3) + half a voxel along its normal (probe 4)
  F  the passes     (lane BOUNCE2) each surfel's probes (own rays + room boxes), then B_k = B_1 + albedo x E/pi
                    re-gathered pass by pass until the bar; settles geometrically, gain under 1/(1-albedo)
  P  the pairs      (lane BOUNCE2) pairs/<name>/pass_one|pass_set.png: the settled GI never darker, brighter
GI_CHECK_DUMP names another dump folder in the run dir (the one-pass pin's); GI_CHECK_RED=onepass stops the twin.
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
from cell_lit_check import lights_of, inside  # noqa: E402
from probe_bake import read_tbk, unpack_dir, floordiv  # noqa: E402
from cell_rooms_check import read_rooms, room_at, surface_room, gi_sample  # noqa: E402

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
        # lane ALPHATEST1: the soup's alpha-test masks (AMK1 tail): a hit on a texel under the threshold passes on
        from alphatest_check import read_soup
        _n, _t, am = read_soup(path)
        self.am = None
        if am is not None and len(am['rec']):
            self.am = am
            self.amOf = np.full(ntri, -1, np.int64)
            self.amOf[am['rec']['tri'].astype(np.int64)] = np.arange(len(am['rec']))

    def holes(self, tri, b1, b2):
        """True where the hit at barycentrics (b1, b2) of soup triangle tri lands on an alpha-test hole"""
        out = np.zeros(len(tri), bool)
        if self.am is None:
            return out
        r = self.amOf[tri]
        for i in np.nonzero(r >= 0)[0]:
            rec = self.am['rec'][r[i]]
            uv = rec['uv'].astype(np.float64)
            b0 = 1.0 - b1[i] - b2[i]
            u = b0 * uv[0] + b1[i] * uv[2] + b2[i] * uv[4]
            v = b0 * uv[1] + b1[i] * uv[3] + b2[i] * uv[5]
            m = self.am['maps'][int(rec['map'])]
            h, w = m.shape
            u -= math.floor(u)
            v -= math.floor(v)
            x = min(max(int(u * w), 0), w - 1)
            y = min(max(int(v * h), 0), h - 1)
            out[i] = int(m[y, x]) < int(rec['thr'])
        return out

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
        ti = np.unique(np.concatenate(idx))
        T = self.t[ti]
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
        hit = ok & (u >= 0) & (u <= 1) & (v >= 0) & (u + v <= 1) & (t > 1e-4) & (t <= tmax)
        if self.am is not None and hit.any():
            hi_ = np.nonzero(hit)[0]
            hit[hi_[self.holes(ti[hi_], u[hi_], v[hi_])]] = False
        return bool(np.any(hit))


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
    out = dict(origin=np.array([ox, oy, oz], float), voxel=float(vox), radius=float(rad), dims=dims, grid=G)
    # lane ROOMCLAMP1: the rooms (gi_rooms.bin), each voxel's two slots and slot 1's grid (gi_slots.bin), the probes'
    # rooms (gi_proberooms.bin); absent with the red noclamp (one value a voxel)
    if all(os.path.exists(os.path.join(d, f)) for f in ('gi_rooms.bin', 'gi_slots.bin', 'gi_proberooms.bin')):
        nv = dims[0] * dims[1] * dims[2]
        s = open(os.path.join(d, 'gi_slots.bin'), 'rb').read()
        out['R'] = read_rooms(os.path.join(d, 'gi_rooms.bin'))
        out['slots'] = np.frombuffer(s, '<i4', 2 * nv, 32).reshape(dims[2], dims[1], dims[0], 2)
        out['grid2'] = np.frombuffer(s, '<f4', 6 * nv * 4, 32 + 8 * nv).reshape(6, dims[2], dims[1], dims[0], 4)
        b = open(os.path.join(d, 'gi_proberooms.bin'), 'rb').read()
        out['prooms'] = np.frombuffer(b, '<i4', 2 * struct.unpack_from('<i', b)[0], 4).reshape(-1, 2)
    return S, P, out, clear


def surfel_light(lights, p, n, soup, clear, shapes=True):
    E = np.zeros(3)
    o = p + n * SURF_OFF
    blocked = 0
    for L in lights:
        v = L['pos'] - p
        d = float(np.linalg.norm(v))
        if d >= L['r']:
            continue
        if shapes and not inside(L, p[None, :])[0]:   # lane HEMI1: a hemisphere or box light lights only its volume
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


def stage_a(esm, cell, tbks, S, soup, clear, light=True):
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
    # lane EMISSIVEGI1: a glowing surfel's B carries its own Le (the .tbk emissive tail), added once
    Le = np.zeros((len(S), 3))
    nglow = 0
    for _, t in tbks:
        for e in t.get('emits', ()):
            j = int(e['surfel'])
            s = (t['back'] if j & 0x80000000 else t['surfels'])[j & 0x7fffffff]
            k = (np.float32(s['pos']).tobytes(), tuple(int(c) for c in s['nrm']))
            if k in row_of:
                Le[row_of[k]] = np.array(e['le'], float)
                nglow += 1
    if not light:   # lane BOUNCE2: an exterior (its light is cell_sky's to check): the surfels only
        return 'A PASS surfels: the dump\'s %d are the bake\'s (light not checked here)' % len(S), row_of
    lights = lights_of(esm, cell)
    rng = np.random.default_rng(7)
    pick = rng.choice(len(S), size=min(400, len(S)), replace=False)
    good, lit, shadowed, shaped = 0, 0, 0, 0
    for i in pick:
        E, nb = surfel_light(lights, S[i, 0:3], S[i, 3:6], soup, clear)
        B = S[i, 6:9] * E + Le[i]
        # lane HEMI1: the surfels a light's shape decides (the same sum with every light an omni differs)
        shaped += int(not close(B, S[i, 6:9] * surfel_light(lights, S[i, 0:3], S[i, 3:6], soup, clear, False)[0] + Le[i]))
        shadowed += nb
        lit += int(B.max() > 1e-3)
        good += int(close(S[i, 9:12], B))
    share = good / len(pick)
    ok = share >= 0.97 and shadowed >= 20 and lit >= 50
    return ('A %s surfel light: %d lights; %d surfels relit, %d lit, %d shadow segments blocked, %d decided by a '
            'light\'s shape; agree %.1f%%'
            % ('PASS' if ok else 'FAIL', len(lights), len(pick), lit, shadowed, shaped, 100 * share)), row_of


def stage_b(tbks, S, P, row_of, Bsrc=None, sky=None):
    # lane BOUNCE2: Bsrc = the last pass's B (gi_bounce.bin), sky = an exterior's sky part (gi_sky.bin)
    if Bsrc is None:
        Bsrc = S[:, 9:12]
    cubes, k = [], 0
    unresolved = 0
    for _, t in tbks:
        cs = float(t['cell'])
        keys = tuple({} for _ in t['bysides'])   # lane BAKE4 front|back; lane SIDES6 v5: six
        for side, arr in enumerate(t['bysides']):
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
                s = t['bysides'][side][j]
                # the glass on the way tints what the link carries
                B = Bsrc[row_of[(np.float32(s['pos']).tobytes(), tuple(int(c) for c in s['nrm']))]] \
                    * (x['tint'].astype(np.float64) / 255.0)
                d = unpack_dir(lk['dir'])
                w = float(lk['w']) * float(pr['scale'])
                linked += w
                E += np.maximum(AXES @ d, 0.0)[:, None] * B[None, :] * (w * 4 * math.pi)
            if linked > 0 and pr['unl'] > 0:
                E *= (linked + float(pr['unl'])) / linked
            if sky is not None:
                E = E + sky[k]
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


def stage_c(S, P, G, soup, eye_rule=True):
    """lane ROOMCLAMP1: a voxel no probe is seen from at its centre gathers from its EYE (the sample point
    surface + half a voxel along the normal nearest its centre) within the radius, then twice it; still none:
    the mean of its valid neighbours (two rings). eye_rule False (the run's red noeye): the old centre-only grid."""
    v, o, dims, rad, grid = G['voxel'], G['origin'], G['dims'], G['radius'], G['grid']
    sp = S[:, 0:3] + S[:, 3:6] * v * 0.5
    g = np.floor((sp - o) / v).astype(int)
    if 'R' in G:
        return stage_c_rooms(S, P, G, soup, sp, g, eye_rule)
    near = np.zeros((dims[2], dims[1], dims[0]), bool)
    eyeD = np.full((dims[2], dims[1], dims[0]), np.inf)
    eye = np.zeros((dims[2], dims[1], dims[0], 3))
    for dz in (-1, 0, 1):
        for dy in (-1, 0, 1):
            for dx in (-1, 0, 1):
                q = g + np.array([dx, dy, dz])
                m = np.all((q >= 0) & (q < np.array(dims)), 1)
                near[q[m, 2], q[m, 1], q[m, 0]] = True
                qm, sm = q[m], sp[m]
                d2 = np.sum((sm - (o + (qm + 0.5) * v)) ** 2, 1)
                order = np.argsort(-d2, kind='stable')      # the nearest written last wins
                qo, so, do = qm[order], sm[order], d2[order]
                cur = eyeD[qo[:, 2], qo[:, 1], qo[:, 0]]
                take = do < cur
                eyeD[qo[take, 2], qo[take, 1], qo[take, 0]] = do[take]
                eye[qo[take, 2], qo[take, 1], qo[take, 0]] = so[take]
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
    ways = {'centre': 0, 'eye': 0, 'far': 0, 'grown': 0, 'empty': 0}

    def gather(c, r):
        nonlocal rays, blocked
        d2 = np.sum((pp - c) ** 2, 1)
        acc, ws = np.zeros((6, 3)), 0.0
        for j in np.nonzero(d2 < r * r)[0]:
            rays += 1
            if soup.blocked(c, pp[j], 0.0):
                blocked += 1
                continue
            w = (1 - d2[j] / (r * r)) ** 2
            ws += w
            acc += w * cube[j]
        return acc, ws
    for (z, y, x), isvalid in pick:
        c = o + (np.array([x, y, z], float) + 0.5) * v
        acc, ws = gather(c, rad)
        way = 'centre'
        if ws == 0.0 and eye_rule:
            e = eye[z, y, x]
            acc, ws = gather(e, rad)
            way = 'eye'
            if ws == 0.0:
                acc, ws = gather(e, 2 * rad)
                way = 'far'
        got = np.stack([grid[a, z, y, x, 0:3] for a in range(6)])
        if ws == 0.0:
            # grown: the mean of valid neighbours lies inside their range, component by component
            nb = [(z + a, y + b, x + c2) for a in (-1, 0, 1) for b in (-1, 0, 1) for c2 in (-1, 0, 1)
                  if (a, b, c2) != (0, 0, 0) and 0 <= z + a < dims[2] and 0 <= y + b < dims[1] and 0 <= x + c2 < dims[0]]
            vals = [np.stack([grid[s, k[0], k[1], k[2], 0:3] for s in range(6)]) for k in nb if valid[k]]
            if not isvalid:
                ways['empty'] += 1
                good += 1
            elif eye_rule and vals:
                ways['grown'] += 1
                lo, hi = np.min(vals, 0), np.max(vals, 0)
                good += int(np.all(got >= lo - 1e-4 * (1 + np.abs(lo))) and np.all(got <= hi + 1e-4 * (1 + np.abs(hi))))
            continue
        ways[way] += 1
        if isvalid:
            good += int(close(got, acc / ws))
    share = good / len(pick)
    ok = share >= 0.97 and blocked >= 20
    return ('C %s voxel grid: %d voxels (%d lit, %d empty), %d probe segments, %d blocked; agree %.1f%% (from the centre '
            '%d, the eye %d, twice the radius %d, neighbours %d, empty %d)'
            % ('PASS' if ok else 'FAIL', len(pick), sum(1 for _, a in pick if a), sum(1 for _, a in pick if not a),
               rays, blocked, 100 * share, ways['centre'], ways['eye'], ways['far'], ways['grown'], ways['empty']))


def surfel_rooms(R, S):
    """Every surfel's room by the surface rule (cell_rooms_check.surface_room): the two direct reads in bulk, the
    rest one by one."""
    p, n = S[:, 0:3], S[:, 3:6]
    out = np.full((len(S), 2), -1, int)
    dims = np.array(R['dims'])
    for k in (0.75, 1.75):
        c = np.floor((p + n * k * R['cell'] - R['origin']) / R['cell']).astype(int)
        ok = np.all((c >= 0) & (c < dims), 1) & (out[:, 0] < 0)
        ab = np.full((len(S), 2), -1, int)
        ab[ok] = R['ab'][c[ok, 2], c[ok, 1], c[ok, 0]]
        take = ok & (ab[:, 0] >= 0)
        out[take] = ab[take]
    for i in np.nonzero(out[:, 0] < 0)[0]:
        L = surface_room(R, p[i], n[i])
        if L is not None:
            out[i] = L
    return out


def stage_c_rooms(S, P, G, soup, sp, g, eye_rule, vals=None, grids=None, kset=(0, 1), tag='C', what='voxel grid'):
    """lane ROOMCLAMP1: with rooms each voxel keeps two slots, the two rooms its surfaces read most. A slot gathers
    only the probes of its room (gi_proberooms.bin, room or second room), from the voxel's centre when the centre
    stands in that room, else from the slot's eye (the nearest sample point of a surface of that room); then from
    the eye, then twice the radius; still none: the mean of the same room's valid slots among the neighbours.
    vals / grids / kset / tag (cell_pass_check's stage T): the probes' own values (n x 6 x 3) and the grids they
    fill (the sky grid and its slot 1, gi_slots.bin's tail)."""
    v, o, dims, rad = G['voxel'], G['origin'], G['dims'], G['radius']
    R, slots, prooms = G['R'], G['slots'], G['prooms']
    grids = grids if grids is not None else (G['grid'], G['grid2'])
    SR = surfel_rooms(R, S)
    near = np.zeros((dims[2], dims[1], dims[0]), bool)
    for dz in (-1, 0, 1):
        for dy in (-1, 0, 1):
            for dx in (-1, 0, 1):
                q = g + np.array([dx, dy, dz])
                m = np.all((q >= 0) & (q < np.array(dims)), 1)
                near[q[m, 2], q[m, 1], q[m, 0]] = True
    fails = []
    for k in kset:
        stray = (grids[k][0, :, :, :, 3] > 0.5) & ~near
        if np.any(stray):
            fails.append('%d slot-%d voxels away from every surface hold light' % (int(stray.sum()), k))
        nolab = (grids[k][0, :, :, :, 3] > 0.5) & (slots[:, :, :, k] < 0) & (k == 1)
        if np.any(nolab):
            fails.append('%d slot-1 voxels hold light with no room' % int(nolab.sum()))
    rng = np.random.default_rng(11)
    pick = []
    for k in kset:
        valid = grids[k][0, :, :, :, 3] > 0.5
        vz = np.argwhere(valid)
        ez = np.argwhere(near & ~valid & (slots[:, :, :, k] >= -1 if k == 0 else slots[:, :, :, k] >= 0))
        nv, ne = (150, 100) if k == 0 else (100, 50)
        pick += [(tuple(c), True, k) for c in vz[rng.choice(len(vz), size=min(nv, len(vz)), replace=False)]] if len(vz) else []
        pick += [(tuple(c), False, k) for c in ez[rng.choice(len(ez), size=min(ne, len(ez)), replace=False)]] if len(ez) else []
    pp, cube = P[:, 0:3], (P[:, 3:21].reshape(-1, 6, 3) if vals is None else vals)
    good, rays, blocked, leak = 0, 0, 0, 0
    ways = {'centre': 0, 'elsewhere': 0, 'eye': 0, 'far': 0, 'grown': 0, 'empty': 0}

    def gather(c, r, lab):
        nonlocal rays, blocked
        d2 = np.sum((pp - c) ** 2, 1)
        acc, ws = np.zeros((6, 3)), 0.0
        inroom = np.ones(len(pp), bool) if lab < 0 else (prooms[:, 0] == lab) | (prooms[:, 1] == lab)
        for j in np.nonzero((d2 < r * r) & inroom)[0]:
            rays += 1
            if soup.blocked(c, pp[j], 0.0):
                blocked += 1
                continue
            w = (1 - d2[j] / (r * r)) ** 2
            ws += w
            acc += w * cube[j]
        return acc, ws
    for (z, y, x), isvalid, k in pick:
        lab = int(slots[z, y, x, k])
        c = o + (np.array([x, y, z], float) + 0.5) * v
        # the slot's eye: the nearest sample point (in surfel order on a tie) of a surface of its room reading the voxel
        m = np.all(np.abs(g - np.array([x, y, z])) <= 1, 1) & ((SR[:, 0] == lab) | (SR[:, 1] == lab))
        idx = np.nonzero(m)[0]
        e = sp[idx[np.argmin(np.sum((sp[idx] - c) ** 2, 1))]] if len(idx) else c
        cr = room_at(R, c)
        centre = lab < 0 or lab in cr
        acc, ws = gather(c if centre else e, rad, lab)
        way = 'centre' if centre else 'elsewhere'
        if ws == 0.0 and eye_rule:
            if centre:
                acc, ws = gather(e, rad, lab)
                way = 'eye'
            if ws == 0.0:
                acc, ws = gather(e, 2 * rad, lab)
                way = 'far'
        got = np.stack([grids[k][a, z, y, x, 0:3] for a in range(6)])
        if ws == 0.0:
            nb = []
            for a in (-1, 0, 1):
                for b in (-1, 0, 1):
                    for c2 in (-1, 0, 1):
                        zz, yy, xx = z + a, y + b, x + c2
                        if (a, b, c2) == (0, 0, 0) or not (0 <= zz < dims[2] and 0 <= yy < dims[1] and 0 <= xx < dims[0]):
                            continue
                        for sj in kset:
                            if slots[zz, yy, xx, sj] == lab and grids[sj][0, zz, yy, xx, 3] > 0.5:
                                nb.append(np.stack([grids[sj][s, zz, yy, xx, 0:3] for s in range(6)]))
            if not isvalid:
                ways['empty'] += 1
                good += 1
            elif eye_rule and nb:
                ways['grown'] += 1
                lo, hi = np.min(nb, 0), np.max(nb, 0)
                good += int(np.all(got >= lo - 1e-4 * (1 + np.abs(lo))) and np.all(got <= hi + 1e-4 * (1 + np.abs(hi))))
            continue
        ways[way] += 1
        if isvalid:
            ok = close(got, acc / ws)
            good += int(ok)
    share = good / max(len(pick), 1)
    ok = share >= 0.97 and blocked >= 20 and not fails
    return ('%s %s %s (rooms %d): %d slots (%d lit, %d empty; %d of them slot 1), %d probe segments, %d blocked; '
            'agree %.1f%% (from the centre %d, the eye with the centre in another room %d, the eye %d, twice the radius '
            '%d, neighbours %d, empty %d)%s'
            % (tag, 'PASS' if ok else 'FAIL', what, R['rooms'], len(pick), sum(1 for _, a, _k in pick if a),
               sum(1 for _, a, _k in pick if not a), sum(1 for _, _a, k in pick if k == 1), rays, blocked, 100 * share,
               ways['centre'], ways['elsewhere'], ways['eye'], ways['far'], ways['grown'], ways['empty'],
               ('; ' + '; '.join(fails)) if fails else ''))


# ---------------------------------------------------------------- F: more than one bounce (lane BOUNCE2)
SETTLE, CAP = 1e-3, 64          # the bar: the largest change under a thousandth of the brightest surfel; at most 64
ROOM_OUT, ROOM_REACH = 17.5, 35.0   # a surfel's air: this far out along its normal; a room box within this reach
ROOM_NONE = 0xFFFFFFFF


def read_bounce(d):
    p = os.path.join(d, 'gi_bounce.bin')
    if not os.path.exists(p):
        return None
    b = open(p, 'rb').read()
    n, passes = struct.unpack_from('<2i', b)
    Bf = np.frombuffer(b, '<f4', n * 3, 8).reshape(n, 3).astype(np.float64)
    b = open(os.path.join(d, 'gi_feed.bin'), 'rb').read()
    n2 = struct.unpack_from('<i', b)[0]
    start = np.frombuffer(b, '<i4', n2 + 1, 4).astype(np.int64)
    m, o = int(start[-1]), 4 + 4 * (n2 + 1)
    ent = np.frombuffer(b, np.dtype([('j', '<i4'), ('w', '<f4')]), m, o)
    room = np.frombuffer(b, '<i4', n2, o + 8 * m).astype(np.int64)
    head, log = {}, []
    for line in open(os.path.join(d, 'gi_passes.txt'), encoding='utf-8'):
        f = line.split()
        if f and f[0] == 'passes':
            head = dict(passes_txt=int(f[1]), settled=int(f[3]), red=f[5])
        elif f and f[0] == 'pass':
            log.append([float(x) for x in f[2:5]])
    return dict(B=Bf, passes=passes, start=start, j=ent['j'].astype(np.int64), w=ent['w'].astype(np.float64),
                room=room, log=np.array(log), **head)


def own_rooms(tbks, Pt, Nt):
    """each surfel's room: the nearest room box to its air (ROOM_OUT along the normal), within ROOM_REACH; -1 none"""
    bx = np.concatenate([t['boxes'] for _, t in tbks]) if tbks else np.zeros(0)
    out = np.full(len(Pt), -1, np.int64)
    if not len(bx):
        return out
    lo, hi, rid = bx['lo'].astype(np.float64), bx['hi'].astype(np.float64), bx['room'].astype(np.int64)
    qa = Pt + Nt * ROOM_OUT
    for a in range(0, len(qa), 1000):
        q = qa[a:a + 1000]
        e = np.maximum(np.maximum(lo[None] - q[:, None], 0.0), q[:, None] - hi[None])
        d2 = np.einsum('sbk,sbk->sb', e, e)
        last = d2.shape[1] - 1 - np.argmin(d2[:, ::-1], 1)        # ties: the later box (the relight's <=)
        ok = d2[np.arange(len(q)), last] <= ROOM_REACH ** 2
        out[a:a + 1000] = np.where(ok, rid[last], -1)
    return out


def own_feed(i, Pt, Nt, room, pp, prooms, rad, soup):
    o = Pt[i] + Nt[i] * SURF_OFF
    d2 = np.sum((pp - o) ** 2, 1)
    c = np.nonzero(d2 < 4 * rad * rad)[0]
    c = c[np.lexsort((c, d2[c]))]
    got = []
    for j in c:
        inr = d2[j] < rad * rad
        if not inr and got:
            break
        if room[i] >= 0 and room[i] != prooms[j, 0] and room[i] != prooms[j, 1]:
            continue
        if soup.blocked(o, pp[j], 0.0):
            continue
        if inr:
            got.append((int(j), (1 - d2[j] / (rad * rad)) ** 2))
        else:
            got.append((int(j), 1.0))
            break
    return got


def stage_f(tbks, S, row_of, soup, G, bn, sky):
    """the passes, repeated here: each surfel's probes (its own rays and room test on a sample; every dumped entry
    against its room), then B_k = B_1 + albedo x E_{k-1} / pi with this file's own gather, until the bar"""
    red = os.environ.get('GI_CHECK_RED', '')
    pin = int(os.environ.get('WW_CELL_GI_PASSES', '0') or 0)
    n = len(S)
    # the surfels' points and normals from the bake (the relight's own doubles), the probes', their rooms
    Pt, Nt = S[:, 0:3].copy(), np.zeros((n, 3))
    for _, t in tbks:
        for s in list(t['surfels']) + list(t['back']):
            r = row_of[(np.float32(s['pos']).tobytes(), tuple(int(c) for c in s['nrm']))]
            v = np.array(s['nrm'], np.float64) / 32767.0
            Nt[r] = v / math.sqrt(max(float(v @ v), 1e-12))
    pp = np.concatenate([np.asarray(t['probes']['pos'], np.float64) for _, t in tbks])
    prooms = np.concatenate([t['pext']['room'].astype(np.int64) for _, t in tbks])
    rad = G['radius']
    room = own_rooms(tbks, Pt, Nt)
    # every dumped entry against the surfel's room; a sample's lists rebuilt with this file's rays
    st, ej = bn['start'], bn['j']
    owner = np.repeat(np.arange(n), np.diff(st))
    known = room[owner] >= 0
    leak_room = int(np.sum(known & (room[owner] != prooms[ej, 0]) & (room[owner] != prooms[ej, 1])))
    rng = np.random.default_rng(5)
    pick = rng.choice(n, size=min(400, n), replace=False)
    agree, leak_wall, fed = 0, 0, 0
    for i in pick:
        mine = own_feed(i, Pt, Nt, room, pp, prooms, rad, soup)
        theirs = list(zip(ej[st[i]:st[i + 1]].tolist(), bn['w'][st[i]:st[i + 1]].tolist()))
        fed += int(bool(theirs))
        o = Pt[i] + Nt[i] * SURF_OFF
        leak_wall += sum(1 for j, _ in theirs if soup.blocked(o, pp[j], 0.0))
        agree += int(len(mine) == len(theirs) and all(a[0] == b[0] and abs(a[1] - b[1]) <= 1e-5 for a, b in zip(mine, theirs)))
    # the passes, this file's own gather (stage B's arithmetic, vectorized) and feed
    lp, lr, lf, lt = [], [], [], []
    k = 0
    for _, t in tbks:
        cs = float(t['cell'])
        keys = tuple({} for _ in t['bysides'])
        for side, arr in enumerate(t['bysides']):
            for j, s in enumerate(arr):
                keys[side].setdefault(tuple(floordiv(s['pos'][a], cs) for a in range(3)), j)
        for pr in t['probes']:
            pk = [floordiv(pr['pos'][a], cs) for a in range(3)]
            rows, linked = [], 0.0
            for li, lk in enumerate(t['links'][pr['off']:pr['off'] + pr['cnt']]):
                x = t['lext'][int(pr['off']) + li]
                j = keys[int(x['side'])].get(tuple(int(pk[a]) + int(lk['delta'][a]) for a in range(3)))
                if j is None:
                    continue
                s = t['bysides'][int(x['side'])][j]
                w = float(lk['w']) * float(pr['scale'])
                linked += w
                rows.append((row_of[(np.float32(s['pos']).tobytes(), tuple(int(c) for c in s['nrm']))],
                             np.maximum(AXES @ unpack_dir(lk['dir']), 0.0) * (w * 4 * math.pi), x['tint'] / 255.0))
            kk = (linked + float(pr['unl'])) / linked if linked > 0 and pr['unl'] > 0 else 1.0
            for r, f, ti in rows:
                lp.append(k)
                lr.append(r)
                lf.append(f * kk)
                lt.append(ti)
            k += 1
    lp, lr, lf, lt = np.array(lp), np.array(lr), np.array(lf), np.array(lt, np.float64)
    skyE = sky if sky is not None else np.zeros((k, 6, 3))

    def gather(B):
        E = np.zeros((k, 6, 3))
        np.add.at(E, lp, lf[:, :, None] * (lt * B[lr])[:, None, :])
        return E + skyE
    n2 = Nt * Nt
    face = np.stack([np.where(Nt[:, a] >= 0, 2 * a, 2 * a + 1) for a in range(3)], 1)
    ws = np.bincount(owner, weights=bn['w'], minlength=n)

    def feed(E):
        e = sum(n2[owner, a:a + 1] * E[ej, face[owner, a]] for a in range(3))
        out = np.stack([np.bincount(owner, weights=bn['w'] * e[:, c], minlength=n) for c in range(3)], 1)
        return np.where(ws[:, None] > 0, out / np.maximum(ws, 1e-300)[:, None], 0.0)
    alb, B1 = S[:, 6:9], S[:, 9:12]
    B, E, log = B1.copy(), gather(B1), [[0.0, B1.sum(), B1.max()]]
    cap = 1 if red == 'onepass' else (pin if pin > 0 else CAP)
    for _ in range(2, cap + 1):
        Bn = B1 + alb * feed(E) / math.pi
        ch = float(np.abs(Bn - B).max())
        B, E = Bn, gather(Bn)
        log.append([ch, B.sum(), B.max()])
        if pin <= 0 and ch <= SETTLE * B.max():
            break
    log = np.array(log)
    passes = len(log)
    good_B = float(np.mean(np.all(np.abs(bn['B'] - B) <= 1e-3 + 0.02 * np.maximum(np.abs(B), np.abs(bn['B'])), 1)))
    dl = bn['log']
    log_ok = len(dl) == passes and bool(np.all(np.abs(dl[:, 1] - log[:, 1]) <= 0.01 * np.abs(log[:, 1]) + 1e-6))
    gain = float(log[-1, 1] / max(log[0, 1], 1e-30))
    aw = float(np.max((alb * B).sum(0) / np.maximum(B.sum(0), 1e-30)))
    bound = 1.0 / max(1.0 - aw, 1e-9)
    # the series' source: the direct light plus, outdoors, the sky once off the surfaces (new light, not a bounce)
    src = float((B1 + alb * feed(skyE) / math.pi).sum()) if sky is not None else float(B1.sum())
    sgain = float(B.sum() / max(src, 1e-30))
    ch = log[1:, 0]
    falls = bool(np.all(ch[1:] < ch[:-1])) if len(ch) > 1 else True
    rate = float((ch[-1] / ch[0]) ** (1.0 / (len(ch) - 1))) if len(ch) > 1 and ch[0] > 0 else 0.0
    # lane ROOMCLAMP1: a red prelane dump (the WW_CELL_ROOMCLAMP_PIN=off twin) is the exe before the rooms clamp;
    # its probes may read through walls, so the wall and room rule is printed but not judged
    prelane = bn.get('red') == 'prelane'
    ok = (bn['passes'] == passes and log_ok and good_B >= 0.97 and agree >= 0.97 * len(pick)
          and (prelane or (leak_room == 0 and leak_wall == 0)))
    if pin <= 0:
        ok = ok and bn.get('settled') == 1 and passes < CAP and falls and sgain <= bound
    return ('F %s bounce: %d passes (this file %d%s), largest change per pass %s, rate %.3f, gain %.4f (over the '
            'source %.4f, the albedo series bound %.4f, albedo %.3f); last B agree %.1f%%; probes read: %d of %d sampled lists agree, %d of '
            '%d surfels fed, %d in a known room; reads from another room %d, through a wall %d (sampled)%s'
            % ('PASS' if ok else 'FAIL', bn['passes'], passes, ' pinned' if pin > 0 else '',
               ' '.join('%.3g' % c for c in ch[:6]) + (' ...' if len(ch) > 6 else ''), rate, gain, sgain, bound, aw,
               100 * good_B, agree, len(pick), int(np.sum(np.diff(st) > 0)), n, int(np.sum(room >= 0)),
               leak_room, leak_wall, ', not judged (red prelane)' if prelane else ''))


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
    if 'R' in G:     # lane ROOMCLAMP1: cellGiRoomSample (cell_rooms_check.gi_sample) where the surface finds a room
        GG = {'origin': G['origin'], 'voxel': G['voxel'], 'dims': G['dims'], 'g': G['grid'], 'g2': G['grid2'], 'slots': G['slots']}
        Nn = N[ys, xs] / nlen[ys, xs][:, None]
        for t, (pw, nw) in enumerate(zip(P[ys, xs], Nn)):
            if surface_room(G['R'], pw, nw) is None:
                continue
            s = gi_sample(GG, G['R'], pw, nw)
            exp[t] = np.clip((np.maximum(s[0:3] / s[3], 0) if s[3] > 0.01 else 0.0) / math.pi, 0, 1)
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


def main(esm, cell, run, stages='ABCDEFP'):
    bake = os.path.join(run, 'bake')
    tbks = [(f, read_tbk(os.path.join(bake, f))) for f in sorted(os.listdir(bake)) if f.endswith('.tbk')]
    dump = os.path.join(run, os.environ.get('GI_CHECK_DUMP', 'dump'))   # lane BOUNCE2: the pin's own dump
    S, P, G, clear = read_dump(dump)
    soup = Soup(os.path.join(run, 'soup.psp'))
    lines = []
    a, row_of = stage_a(esm, cell, tbks, S, soup, clear, 'A' in stages)
    lines.append(a)
    # lane BOUNCE2: the probes gathered the last pass's B; an exterior's probes add their sky (gi_sky.bin)
    bn = read_bounce(dump)
    sky = None
    sp = os.path.join(dump, 'gi_sky.bin')
    if os.path.exists(sp):
        b = open(sp, 'rb').read()
        sky = np.frombuffer(b, '<f4', struct.unpack_from('<i', b)[0] * 18, 4).reshape(-1, 6, 3).astype(np.float64)
    if row_of is not None:
        lines.append(stage_b(tbks, S, P, row_of, bn['B'] if bn else None, sky))
        if 'F' in stages:
            lines.append(stage_f(tbks, S, row_of, soup, G, bn, sky) if bn else 'F FAIL bounce: no gi_bounce.bin in the dump')
    # lane ROOMCLAMP1: the pin (WW_CELL_ROOMCLAMP_PIN=off, red prelane) is the grid from before the lane: no eye rule
    lines.append(stage_c(S, P, G, soup, eye_rule=not (bn and bn.get('red') == 'prelane')))
    if 'D' in stages and os.path.exists(os.path.join(run, 'probe5.png')):
        lines.append(stage_d(run, cell, G))
    if 'E' in stages and os.path.exists(os.path.join(run, 'pbr', 'probe5.png')):
        lines.append(stage_e(run, cell, G))
    if 'P' in stages and os.path.isdir(os.path.join(run, 'pairs')):
        lines.append(stage_p(run))
    return lines


def stage_p(run):
    """lane BOUNCE2: each pair of GI pictures (the Pass view's GI, one pass against settled, the same camera):
    every later pass only adds light, so no pixel may darken (1% grace for edges) and the mean must rise"""
    out, ok = [], True
    for name in sorted(os.listdir(os.path.join(run, 'pairs'))):
        d = os.path.join(run, 'pairs', name)
        try:
            a = np.asarray(Image.open(os.path.join(d, 'pass_one.png')).convert('RGB'), float).sum(2)
            b = np.asarray(Image.open(os.path.join(d, 'pass_set.png')).convert('RGB'), float).sum(2)
        except OSError:
            out.append('%s: missing' % name)
            ok = False
            continue
        darker = float((b < a - 6).mean())
        good = darker <= 0.01 and b.mean() > a.mean() + 0.05
        ok = ok and good
        out.append('%s mean %.2f -> %.2f, darker %.2f%%%s' % (name, a.mean() / 3, b.mean() / 3, 100 * darker,
                                                            '' if good else ' (BAD)'))
    return 'P %s pairs: %s' % ('PASS' if ok and out else 'FAIL', '; '.join(out) or 'none')


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
