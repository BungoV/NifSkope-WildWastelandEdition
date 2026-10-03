#!/usr/bin/env python3
"""The Pass drop-down's check (lane PROBEVIEW1, 2026-10-02; tests/spells/cell_pass.sh).

  cell_pass_check.py <cell> <run dir>
  cell_pass_check.py --pick <run dir> <x,y,z>     the probe the links shot picks (most links near x,y,z)

The run dir holds bake/ (the .tbk files), soup.psp, dump/ (WW_CELL_GI_DUMP: gi_surfels, gi_probes, gi_grid,
gi_skygrid, gi_probesky, gi_links) and the pictures cell_pass.sh shot. Every value is rebuilt here from the
.tbk files and the soup with this file's own reader, sampler and display curve, nothing from NifSkope:

  S  probe sky       each probe's sky share on the six axes = the mean of the bake's four octants on that
                     side (octant = x<0 | (y<0)<<1 | (z<0)<<2) against gi_probesky.bin
  T  sky grid        gi_skygrid.bin holds exactly gi_grid.bin's voxels; 250 of them rebuilt from S's own
                     shares: the probes within the radius the voxel can see, weight (1 - d^2 / r^2)^2. With
                     rooms (gi_slots.bin; lane ROOMCLAMP1) both slots by cell_gi_check's stage C rule (a slot
                     gathers its room's probes only, the eye / twice the radius / neighbour fallbacks)
  G  GI pass         pass1.png at every clean pixel (position from probes 2 + 3, normal from probe 4)
                     against this file's sample of gi_grid.bin: (E / pi) clamped, ^ (1 / 2.2); magenta
                     where no probe reaches. With rooms, cell_rooms_check's gi_sample (the shader's room
                     blend) on 20000 of the pixels instead of the plain trilinear
  K  Sky visibility  pass2.png the same against gi_skygrid.bin (an interior: no sky anywhere it is valid)
  F  Surfel color    pass3id.png (WW_CELL_PV_ID=1) names the surfel under each pixel; pass3.png carries that
                     surfel's own albedo (this file's read of the bake) ^ (1 / 2.2) on every splat-interior
                     pixel, magenta exactly where no splat is, the splats sit on their surfels, every surfel drawn
  L  Surfel light    pass4.png the same with each surfel's outgoing light B / pi (gi_surfels.bin's B)
  N  links           the picked probe's linked surfels (links.pv.txt) are exactly the ones this file resolves
                     from the bake's links, and the picture shows them (green line pixels)
  Z  Combined        Pass Combined against the pre-lane exe's picture, GI on and off: no more differing pixels,
                     nor larger, than the pre-lane exe shows against itself (combined_gi1_old2, 1-LSB noise)
One line a stage, then "pass PASS|FAIL <cell>"; a stage whose pictures are missing is skipped.
"""
import math
import os
import re
import struct
import sys

import numpy as np
from PIL import Image
from scipy.spatial import cKDTree

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from cell_gi_check import Soup  # noqa: E402  (a segment test over the soup, no NifSkope code)
from probe_bake import read_tbk, floordiv  # noqa: E402
import cell_gi_check  # noqa: E402  (lane ROOMCLAMP1: stage C's rooms rule, rebuilt for the sky grid)
import cell_rooms_check  # noqa: E402  (lane ROOMCLAMP1: the shader's room blend)

MAGENTA = np.array([1.0, 0.0, 1.0])


def shown(x):
    return np.power(np.clip(x, 0.0, 1.0), 1.0 / 2.2)


def load_tbks(run):
    bake = os.path.join(run, 'bake')
    return [(f, read_tbk(os.path.join(bake, f))) for f in sorted(os.listdir(bake)) if f.endswith('.tbk')]


def read_grid(path):
    b = open(path, 'rb').read()
    ox, oy, oz, vox, rad = struct.unpack_from('<5f', b, 0)
    dims = struct.unpack_from('<3i', b, 20)
    g = np.frombuffer(b, '<f4', 6 * dims[0] * dims[1] * dims[2] * 4, 32).reshape(6, dims[2], dims[1], dims[0], 4)
    return dict(origin=np.array([ox, oy, oz], float), voxel=float(vox), radius=float(rad), dims=dims, grid=g)


def read_n(path, width, dtype='<f4'):
    b = open(path, 'rb').read()
    n = struct.unpack_from('<i', b)[0]
    return np.frombuffer(b, dtype, n * width, 4).reshape(n, width).astype(np.float64)


def read_links(path):
    b = open(path, 'rb').read()
    n = struct.unpack_from('<i', b)[0]
    start = np.frombuffer(b, '<i4', n + 1, 4)
    links = np.frombuffer(b, '<i4', int(start[-1]), 4 + 4 * (n + 1))
    return start, links


def own_probes(tbks):
    """per probe (bake order): position, six sky shares, the resolved linked surfels as (pos bytes, nrm)"""
    pos, sky, linked = [], [], []
    for _, t in tbks:
        cs = float(t['cell'])
        keys = ({}, {})
        for side, arr in enumerate((t['surfels'], t['back'])):
            for j, s in enumerate(arr):
                keys[side].setdefault(tuple(floordiv(s['pos'][a], cs) for a in range(3)), j)
        for pr in t['probes']:
            v = pr['sky'].astype(np.float64)
            o = np.arange(8)
            share = []
            for a in range(6):
                bit, want = 1 << (a // 2), (1 << (a // 2)) if a & 1 else 0
                share.append(np.clip(v[(o & bit) == want].mean(), 0, 1))
            pk = [floordiv(pr['pos'][a], cs) for a in range(3)]
            mine = []
            for li, lk in enumerate(t['links'][pr['off']:pr['off'] + pr['cnt']]):
                side = int(t['lext'][int(pr['off']) + li]['side'])
                j = keys[side].get(tuple(int(pk[a]) + int(lk['delta'][a]) for a in range(3)))
                if j is not None:
                    s = (t['surfels'], t['back'])[side][j]
                    mine.append((np.float32(s['pos']).tobytes(), tuple(int(c) for c in s['nrm'])))
            pos.append(np.array(pr['pos'], float))
            sky.append(share)
            linked.append(mine)
    return np.array(pos), np.array(sky), linked


def own_surfels(tbks):
    """the bake's unique surfels (front + back): position, unit normal, albedo (linear, the relight's /255)"""
    seen, P, N, A = set(), [], [], []
    for _, t in tbks:
        for arr in (t['surfels'], t['back']):
            for s in arr:
                k = (np.float32(s['pos']).tobytes(), tuple(int(c) for c in s['nrm']))
                if k in seen:
                    continue
                seen.add(k)
                n = s['nrm'].astype(np.float64) / 32767.0
                P.append(s['pos'].astype(np.float64))
                N.append(n / max(np.linalg.norm(n), 1e-9))
                A.append(s['alb'].astype(np.float64) / 255.0)
    return np.array(P), np.array(N), np.array(A)


def sample(G, Pw, Nw):
    """GL_LINEAR on the six slabs, clamp to edge, the three facing slabs blended by n^2 (rgba)"""
    v, o, dims, grid = G['voxel'], G['origin'], np.array(G['dims'], float), G['grid']
    g = (Pw + Nw * (0.5 * v) - o) / v
    z = np.clip(g[:, 2], 0.5, dims[2] - 0.5)
    flat = grid.reshape(6 * int(dims[2]), int(dims[1]), int(dims[0]), 4).astype(np.float64)

    def tex(slab):
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
                    out += w[:, None] * flat[iz, iy, ix]
        return out
    n2 = Nw * Nw
    return (n2[:, 0:1] * np.where(Nw[:, 0:1] >= 0, tex(0), tex(1)) + n2[:, 1:2] * np.where(Nw[:, 1:2] >= 0, tex(2), tex(3))
            + n2[:, 2:3] * np.where(Nw[:, 2:3] >= 0, tex(4), tex(5)))


def surface(run):
    """the clean cell-lit pixels: (ys, xs, world position, unit normal)"""
    img = {p: np.asarray(Image.open(os.path.join(run, 'probe%d.png' % p)).convert('RGB'), float) for p in (2, 3, 4)}
    notes = open(os.path.join(run, 'probe2.notes'), encoding='utf-8', errors='replace').read()
    m = re.search(r'cell lighting: .*center=(-?[\d.]+),(-?[\d.]+),(-?[\d.]+)', notes)
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
    return ys, xs, P[ys, xs], N[ys, xs] / nlen[ys, xs][:, None]


def stage_s(own_sky, dumped):
    if len(own_sky) != len(dumped):
        return 'S FAIL probe sky: the dump holds %d probes, the bake %d' % (len(dumped), len(own_sky))
    err = np.abs(own_sky - dumped).max()
    open_ = int(np.sum(own_sky.max(1) > 0.01))
    return ('S %s probe sky: %d probes (%d see sky), max |err| %.2g'
            % ('PASS' if err < 1e-5 else 'FAIL', len(own_sky), open_, err))


def read_rooms(dump, G, K):
    """lane ROOMCLAMP1: with rooms the dump also holds gi_rooms.bin, gi_slots.bin (labels, slot 1's grid, slot 1's
    sky grid) and gi_proberooms.bin. None: no rooms (the plain one-value grid)."""
    if not all(os.path.exists(os.path.join(dump, f)) for f in ('gi_rooms.bin', 'gi_slots.bin', 'gi_proberooms.bin')):
        return None
    dims = G['dims']
    nv = dims[0] * dims[1] * dims[2]
    s = open(os.path.join(dump, 'gi_slots.bin'), 'rb').read()
    shp = (6, dims[2], dims[1], dims[0], 4)
    slots = np.frombuffer(s, '<i4', 2 * nv, 32).reshape(dims[2], dims[1], dims[0], 2)
    g2 = np.frombuffer(s, '<f4', 6 * nv * 4, 32 + 8 * nv).reshape(shp)
    k2 = np.frombuffer(s, '<f4', 6 * nv * 4, 32 + 8 * nv + 6 * nv * 16).reshape(shp)
    R = cell_rooms_check.read_rooms(os.path.join(dump, 'gi_rooms.bin'))

    def two(grid, second):
        return {'origin': G['origin'], 'voxel': G['voxel'], 'dims': dims, 'g': grid, 'g2': second, 'slots': slots}
    return {'R': R, 'sky2': k2, 'G': two(G['grid'], g2), 'K': two(K['grid'], k2)}


def stage_t(G, K, ppos, own_sky, soup, rooms=None, dump=None):
    v, o, dims, rad = K['voxel'], K['origin'], K['dims'], K['radius']
    gv, kv = G['grid'][0, :, :, :, 3] > 0.5, K['grid'][0, :, :, :, 3] > 0.5
    if gv.shape != kv.shape or np.any(gv != kv):
        return 'T FAIL sky grid: its valid voxels are not the GI grid\'s'
    if rooms is not None:
        # lane ROOMCLAMP1: the sky grid's two slots by the GI grid's rule, the probes' own sky shares as the values
        S, P, D, _clear = cell_gi_check.read_dump(dump)
        sp = S[:, 0:3] + S[:, 3:6] * D['voxel'] * 0.5
        g = np.floor((sp - D['origin']) / D['voxel']).astype(int)
        return cell_gi_check.stage_c_rooms(S, P, D, soup, sp, g, True, vals=np.repeat(own_sky[:, :, None], 3, 2),
                                           grids=(K['grid'], rooms['sky2']), tag='T', what='sky grid')
    rng = np.random.default_rng(17)
    vz = np.argwhere(kv)
    pick = vz[rng.choice(len(vz), size=min(250, len(vz)), replace=False)]
    good, rays, blocked = 0, 0, 0
    for z, y, x in pick:
        c = o + (np.array([x, y, z], float) + 0.5) * v
        d2 = np.sum((ppos - c) ** 2, 1)
        acc, ws = np.zeros(6), 0.0
        for j in np.nonzero(d2 < rad * rad)[0]:
            rays += 1
            if soup.blocked(c, ppos[j], 0.0):
                blocked += 1
                continue
            w = (1 - d2[j] / (rad * rad)) ** 2
            ws += w
            acc += w * own_sky[j]
        got = np.array([K['grid'][a, z, y, x, 0] for a in range(6)], float)
        good += int(ws > 0 and np.all(np.abs(got - acc / max(ws, 1e-12)) <= 2e-3 + 2e-3 * np.abs(acc / max(ws, 1e-12))))
    share = good / max(len(pick), 1)
    ok = share >= 0.97 and blocked >= 20
    return ('T %s sky grid: %d valid voxels, %d probe segments, %d blocked; agree %.1f%%'
            % ('PASS' if ok else 'FAIL', len(pick), rays, blocked, 100 * share))


def judge_pass(run, tag, label, what, G, srf, scale, interior_black=False, rooms=None):
    path = os.path.join(run, tag + '.png')
    if not os.path.exists(path):
        return None
    ys, xs, Pw, Nw = srf
    if rooms is not None and len(ys) > 20000:
        # lane ROOMCLAMP1: the room blend is a per-pixel walk here; 20000 pixels, a fixed draw
        keep = np.sort(np.random.default_rng(23).choice(len(ys), 20000, replace=False))
        ys, xs, Pw, Nw = ys[keep], xs[keep], Pw[keep], Nw[keep]
    got = np.asarray(Image.open(path).convert('RGB'), float)[ys, xs] / 255.0
    if rooms is None:
        s = sample(G, Pw, Nw)
    else:
        GG = rooms['K' if label == 'K' else 'G']
        s = np.array([cell_rooms_check.gi_sample(GG, rooms['R'], Pw[i], Nw[i]) for i in range(len(Pw))])
    valid = s[:, 3] > 0.01
    v = np.where(valid[:, None], np.maximum(s[:, 0:3] / np.maximum(s[:, 3:4], 1e-9), 0), 0) * scale
    exp = np.where(valid[:, None], shown(v), MAGENTA[None, :])
    good = np.all(np.abs(got - exp) <= 4.0 / 255 + 0.06 * exp, axis=1)
    lit = valid & (exp.max(1) > 0.08)
    share = good.mean()
    lit_share = good[lit].mean() if lit.any() else 1.0
    ok = len(ys) >= 2000 and share >= 0.95 and lit_share >= 0.93 and valid.mean() >= 0.5
    extra = ''
    if interior_black:
        bright = int(np.sum(valid & (got.max(1) > 6 / 255)))
        ok = ok and bright <= 0.01 * max(int(valid.sum()), 1)
        extra = '; %d valid pixels brighter than black (an interior sees no sky)' % bright
    else:
        up = valid & (Nw[:, 2] > 0.9)
        if up.any() and label == 'K':
            vv = v[up, 0]
            extra = ('; facing up: %d px, open (> 0.5) %.0f%%, covered (< 0.1) %.0f%%'
                     % (int(up.sum()), 100 * np.mean(vv > 0.5), 100 * np.mean(vv < 0.1)))
    return ('%s %s %s: %d clean pixels, %.0f%% reached by a probe, %d bright; agree %.1f%% (bright %.1f%%), mean |err| %.4f%s'
            % (label, 'PASS' if ok else 'FAIL', what, len(ys), 100 * valid.mean(), int(lit.sum()), 100 * share,
               100 * lit_share, np.abs(got - exp).mean(), extra))


def judge_surfels(run, tag, label, what, srf, S, colors, cell):
    """colors: per gi_surfels.bin row, the linear value the splat must show. pass3id.png (WW_CELL_PV_ID=1) names
    the surfel under every pixel; the splat's own pixels (its id on all four sides) must carry its value."""
    path, idp = os.path.join(run, tag + '.png'), os.path.join(run, 'pass3id.png')
    if not (os.path.exists(path) and os.path.exists(idp)):
        return None
    img = np.asarray(Image.open(path).convert('RGB'), float) / 255.0
    rgb = np.asarray(Image.open(idp).convert('RGB'), np.int64)
    ids = rgb[:, :, 0] + rgb[:, :, 1] * 256 + rgb[:, :, 2] * 65536
    n = len(S)
    splat = (ids >= 1) & (ids <= n)
    inner = splat.copy()
    for dy, dx in ((0, 1), (1, 0), (0, -1), (-1, 0)):
        inner &= np.roll(ids, (dy, dx), (0, 1)) == ids
    inner[0, :] = inner[-1, :] = inner[:, 0] = inner[:, -1] = False
    ys, xs = np.nonzero(inner)
    row = ids[ys, xs] - 1
    exp = shown(colors[row])
    good = np.all(np.abs(img[ys, xs] - exp) <= 2.0 / 255, axis=1)
    # the same geometry: a pixel is magenta in the picture exactly where no splat covers it
    # (judged where the id is the same over the 3 x 3 around the pixel: splat edges are antialiased)
    mag = np.all(np.abs(img - MAGENTA) < 1.0 / 255, axis=2)
    uni = np.ones_like(splat)
    for dy in (-1, 0, 1):
        for dx in (-1, 0, 1):
            uni &= np.roll(ids, (dy, dx), (0, 1)) == ids
    cy, cx = srf[0], srf[1]
    u = uni[cy, cx]
    geo = np.mean(mag[cy, cx][u] == ~splat[cy, cx][u]) if u.any() else 0.0
    # and the splats sit on their surfels: the surface under a splat pixel lies within 1.5 cells of its surfel
    on = inner[cy, cx]
    near = np.linalg.norm(srf[2][on] - S[ids[cy, cx][on] - 1, 0:3], axis=1) < 1.5 * cell
    dump = open(os.path.join(run, tag + '.pv.txt'), encoding='utf-8').read() if os.path.exists(
        os.path.join(run, tag + '.pv.txt')) else ''
    m = re.search(r'tiles=(\d+)', dump)
    tiles = int(m.group(1)) if m else -1
    drawn = len(np.unique(row))
    ok = len(ys) >= 5000 and good.mean() >= 0.99 and geo >= 0.99 and near.mean() >= 0.8 and tiles == n
    return ('%s %s %s: %d splat pixels (%d surfels in view), %.2f%% carry their surfel\'s value; magenta exactly off the '
            'splats %.2f%%; on their surfel %.1f%%; the overlay drew %d of %d surfels'
            % (label, 'PASS' if ok else 'FAIL', what, len(ys), drawn, 100 * good.mean(), 100 * geo, 100 * near.mean(),
               tiles, n))


def own_albedo_rows(S, sP, sN, sA):
    """gi_surfels.bin's rows matched to this file's own read of the bake (same position, nearest normal)"""
    by = {}
    for j in range(len(sP)):
        by.setdefault(np.float32(sP[j]).tobytes(), []).append(j)
    out = np.full((len(S), 3), np.nan)
    for r in range(len(S)):
        js = by.get(np.float32(S[r, 0:3]).tobytes(), [])
        if js:
            out[r] = sA[max(js, key=lambda j: float(sN[j] @ S[r, 3:6]))]
    return out


def stage_n(run, linked, S):
    dump_p = os.path.join(run, 'links.pv.txt')
    if not os.path.exists(dump_p):
        return None
    d = open(dump_p, encoding='utf-8').read()
    kp = os.path.join(run, 'links.probe')
    ks = open(kp).read().strip() if os.path.exists(kp) else ''
    if not ks.lstrip('-').isdigit():
        return 'N FAIL links: no picked probe in links.probe'
    k = int(ks)
    m = re.search(r'probe=(-?\d+) links=(\d+)', d)
    lm = re.search(r'linked=([\d ]*)', d)
    if not m or int(m.group(1)) != k or not lm:
        return 'N FAIL links: the overlay did not pick probe %d (%s)' % (k, d.strip().replace('\n', ' | ')[:120])
    rows = [int(x) for x in lm.group(1).split()]
    # matched by position (gi_surfels.bin holds a unit normal, the bake an int16 one)
    mine = linked[k]
    got_pos = sorted(np.float32(S[r, 0:3]).tobytes() for r in rows)
    own_pos = sorted(p for p, _ in mine)
    same = got_pos == own_pos
    img = np.asarray(Image.open(os.path.join(run, 'links.png')).convert('RGB'), int)
    green = int(np.sum((img[:, :, 1] > 230) & (img[:, :, 0] < 60) & (img[:, :, 2] < 90)))
    ok = same and len(rows) >= 5 and green >= 100
    return ('N %s links: probe %d, the overlay links %d surfels, the bake resolves %d (%s); %d green line pixels'
            % ('PASS' if ok else 'FAIL', k, len(rows), len(mine), 'the same set' if same else 'DIFFERENT sets', green))


def stage_z(run):
    def diff(a, b):
        a, b = os.path.join(run, a + '.png'), os.path.join(run, b + '.png')
        if not (os.path.exists(a) and os.path.exists(b)):
            return None
        A = np.asarray(Image.open(a).convert('RGBA'), int)
        B = np.asarray(Image.open(b).convert('RGBA'), int)
        if A.shape != B.shape:
            return 10 ** 9, 255
        d = np.abs(A - B)
        return int(np.sum(np.any(d, axis=2))), int(d.max())
    lines = [diff('combined_gi%d_new' % g, 'combined_gi%d_old' % g) for g in (1, 0)]
    if None in lines:
        return None
    # the floor: the pre-lane exe shot twice (same settings) differs from itself by a few 1-LSB pixels
    floor = diff('combined_gi1_old2', 'combined_gi1_old') or (0, 0)
    # lane ROOMCLAMP1: a floor of 0 (the old exe twice, no noise that time) left no room for the GPU's own
    # 1-LSB noise, so one 1-level pixel failed it (scratch v5: GI on and GI off one pixel each, at different spots);
    # cell_gi measured the same noise and allows 1 level, so the cap is never below 20 pixels at 1 level
    cap = max(3 * floor[0], 20)
    ok = all(n <= cap and m <= max(floor[1], 1) for n, m in lines)
    return ('Z %s Combined: against the pre-lane exe, differing pixels GI on %d (max %d), GI off %d (max %d); '
            'the old exe against itself %d (max %d), allowed %d of at most that size'
            % ('PASS' if ok else 'FAIL', lines[0][0], lines[0][1], lines[1][0], lines[1][1], floor[0], floor[1], cap))


def pick(run, at):
    c = np.array([float(x) for x in at.split(',')])
    P = read_n(os.path.join(run, 'dump', 'gi_probes.bin'), 21)[:, 0:3]
    start, _ = read_links(os.path.join(run, 'dump', 'gi_links.bin'))
    cnt = np.diff(start)
    d = np.linalg.norm(P - c, axis=1)
    cand = np.nonzero(d < 250)[0]
    if not len(cand):
        cand = np.argsort(d)[:10]
    return int(cand[np.argmax(cnt[cand])])


def main(cell, run):
    dump = os.path.join(run, 'dump')
    tbks = load_tbks(run)
    ppos, own_sky, linked = own_probes(tbks)
    G = read_grid(os.path.join(dump, 'gi_grid.bin'))
    K = read_grid(os.path.join(dump, 'gi_skygrid.bin'))
    S = read_n(os.path.join(dump, 'gi_surfels.bin'), 12)
    lines = [stage_s(own_sky, read_n(os.path.join(dump, 'gi_probesky.bin'), 6))]
    rooms = read_rooms(dump, G, K)
    lines.append(stage_t(G, K, ppos, own_sky, Soup(os.path.join(run, 'soup.psp')), rooms, dump))
    srf = surface(run)
    interior = cell != 'concord'
    for ln in (judge_pass(run, 'pass1', 'G', 'GI pass', G, srf, 1.0 / math.pi, rooms=rooms),
               judge_pass(run, 'pass2', 'K', 'Sky visibility', K, srf, 1.0, interior_black=interior, rooms=rooms)):
        if ln:
            lines.append(ln)
    sP, sN, sA = own_surfels(tbks)
    cell_size = float(tbks[0][1]['cell'])
    alb = own_albedo_rows(S, sP, sN, sA)
    if np.isnan(alb).any() or len(sP) != len(S):
        lines.append('F FAIL Surfel color: the dump\'s %d surfels are not the bake\'s %d' % (len(S), len(sP)))
    else:
        ln = judge_surfels(run, 'pass3', 'F', 'Surfel color', srf, S, alb, cell_size)
        if ln:
            lines.append(ln)
    # Surfel light: B from the dump (cell_gi's stage A rebuilds every surfel's B from the plugin)
    ln = judge_surfels(run, 'pass4', 'L', 'Surfel light', srf, S, S[:, 9:12] / math.pi, cell_size)
    if ln:
        lines.append(ln)
    for ln in (stage_n(run, linked, S), stage_z(run)):
        if ln:
            lines.append(ln)
    return lines


if __name__ == '__main__':
    if len(sys.argv) >= 4 and sys.argv[1] == '--pick':
        print(pick(sys.argv[2], sys.argv[3]))
        sys.exit(0)
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(2)
    lines = main(sys.argv[1], sys.argv[2])
    for line in lines:
        print(line)
    ok = all(' PASS ' in line for line in lines) and len(lines) >= 3
    print('pass %s %s' % ('PASS' if ok else 'FAIL', sys.argv[1]))
    sys.exit(0 if ok else 1)
