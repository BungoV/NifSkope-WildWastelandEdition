#!/usr/bin/env python3
"""PRTP probe bake gates (lane PRTPBAKE, 2026-09-30).

  synth <exe> <workdir> [--red octant|normal]
        a synthetic block (an open ground plane, one sealed room on it, a known
        albedo per surface), run `probebake`, then:
          * parse every `.tbk` with this file's own reader (FO4CS v3 layout:
            exact size, offsets, every link resolves to a surfel of its file);
          * re-trace EVERY probe in numpy (the same Fibonacci ray set) and
            require the same sky per octant, octant distances, link cells,
            link weights and link directions;
          * the physics: ground probes see no sky below and only sky in the
            upper octant turned from the room; room probes see no sky at all;
          * the surfels modelled here too (one side per cell: the side most
            rays saw); no link reaches a surfel turned away from its probe,
            that weight is unlinked; albedo = the surface's own; two thread
            counts give byte-identical files.
  check <bakedir>
        the structure + budget + facing checks on a real cell's bake.

One verdict line; exit 0 on PASS, 1 on FAIL. Run synth once more with each
`--red`: the gate must then FAIL.
"""
import math
import os
import struct
import subprocess
import sys

import numpy as np

SOUP_MAGIC = 0x31505350
ALB_MAGIC = 0x31424C41
TBK_MAGIC = 0x314B4254
FOUR_PI = 4.0 * math.pi

SURFEL = np.dtype([('pos', '<f4', 3), ('nrm', '<i2', 3), ('alb', 'u1', 3), ('pad0', 'u1'),
                   ('pad', 'u1', 2), ('samples', '<u4'), ('pad1', '<u4')])
PROBE = np.dtype([('pos', '<f4', 3), ('off', '<u4'), ('cnt', '<u4'), ('scale', '<f4'), ('cov', '<f4'),
                  ('unl', '<f4'), ('sky', '<f4', 8), ('dist', '<f4', 8), ('rms', '<f4', 8),
                  ('cls', '<u4'), ('lvl', '<u4'), ('res', '<u4', 2)])
LINK = np.dtype([('delta', '<i2', 3), ('dir', '<i2', 2), ('w', '<u2')])
assert SURFEL.itemsize == 32 and PROBE.itemsize == 144 and LINK.itemsize == 12


MAX_LINKS = 1024   # src/probebake.h ProbeBakeSpec::maxLinks


def floordiv(v, s):
    # the reader's key: float32 division, then floor
    return int(math.floor(float(np.float32(v) / np.float32(s))))


def read_tbk(path):
    b = open(path, 'rb').read()
    if len(b) < 64:
        raise ValueError('shorter than a header')
    h = struct.unpack('<5if4I6I', b[:64])
    magic, ver, kind, cx, cy, cs, ns, np_, nl, flags = h[:10]
    if (magic & 0xffffffff) != TBK_MAGIC or ver != 3 or kind != 1:
        raise ValueError('magic/version/kind %x %d %d' % (magic & 0xffffffff, ver, kind))
    need = 64 + 32 * ns + 144 * np_ + 12 * nl
    if len(b) != need:
        raise ValueError('size %d, the counts say %d' % (len(b), need))
    o = 64
    s = np.frombuffer(b, SURFEL, ns, o); o += 32 * ns
    p = np.frombuffer(b, PROBE, np_, o); o += 144 * np_
    lk = np.frombuffer(b, LINK, nl, o)
    return dict(cx=cx, cy=cy, cell=cs, flags=flags, surfels=s, probes=p, links=lk)


def unpack_dir(d):
    x, y = d[0] / 32767.0, d[1] / 32767.0
    z = 1.0 - abs(x) - abs(y)
    if z < 0:
        x, y = (1.0 - abs(y)) * (1 if x >= 0 else -1), (1.0 - abs(x)) * (1 if y >= 0 else -1)
    v = np.array([x, y, z])
    return v / np.linalg.norm(v)


def fib(n):
    i = np.arange(n, dtype=np.float64)
    z = 1.0 - (2.0 * i + 1.0) / n
    r = np.sqrt(np.maximum(0.0, 1.0 - z * z))
    ph = math.pi * (3.0 - math.sqrt(5.0)) * i
    return np.stack([r * np.cos(ph), r * np.sin(ph), z], 1)


def octant(d):
    return (d[:, 0] < 0).astype(int) | ((d[:, 1] < 0).astype(int) << 1) | ((d[:, 2] < 0).astype(int) << 2)


def trace(tris, o, dirs, tmax=131072.0):
    """Nearest hit per ray in (1e-4, tmax]: (t, tri) with t = inf on a miss."""
    p0, e1, e2 = tris[:, 0], tris[:, 1] - tris[:, 0], tris[:, 2] - tris[:, 0]
    D = dirs[:, None, :]
    pv = np.cross(D, e2[None])
    det = np.einsum('tk,rtk->rt', e1, pv)
    ok = np.abs(det) >= 1e-12
    idt = np.where(ok, 1.0 / np.where(ok, det, 1.0), 0.0)
    tv = o[None, :] - p0
    u = np.einsum('tk,rtk->rt', tv, pv) * idt
    qv = np.cross(tv, e1)
    v = np.einsum('rk,tk->rt', dirs, qv) * idt
    t = np.einsum('tk,tk->t', e2, qv)[None, :] * idt
    hit = ok & (u >= 0) & (u <= 1) & (v >= 0) & (u + v <= 1) & (t > 1e-4) & (t <= tmax)
    t = np.where(hit, t, np.inf)
    k = np.argmin(t, 1)
    return t[np.arange(len(dirs)), k], k


# ---------------------------------------------------------------- the scene
GROUND_Z = 35.0                 # mid surfel cell: no hit sits on a cell boundary in z
ROOM = (1015.0, 1015.0, 35.0, 1715.0, 1715.0, 385.0)
ALB_GROUND = (200, 60, 40)
ALB_ROOM = (30, 180, 90)
RECT = (-1400.0, -1400.0, 2800.0, 2800.0)


def scene():
    tris, alb = [], []
    g = 300000.0
    a, b, c, d = (-g, -g, GROUND_Z), (g, -g, GROUND_Z), (g, g, GROUND_Z), (-g, g, GROUND_Z)
    for t in ((a, c, b), (a, d, c)):        # wound so its normal points DOWN: facing is the bake's job
        tris.append(t)
        alb.append(ALB_GROUND)
    x0, y0, z0, x1, y1, z1 = ROOM
    v = [(x, y, z) for z in (z0, z1) for y in (y0, y1) for x in (x0, x1)]
    # no bottom face: the ground is the room's floor (a coplanar twin would tie in every ray)
    for q in ((4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)):
        for t in ((v[q[0]], v[q[1]], v[q[2]]), (v[q[0]], v[q[2]], v[q[3]])):
            tris.append(t)
            alb.append(ALB_ROOM)
    return np.array(tris, dtype=np.float64), alb


def write_soup(path, tris, alb):
    t = np.asarray(tris, dtype='<f4').reshape(-1, 9)
    with open(path, 'wb') as f:
        f.write(struct.pack('<III', SOUP_MAGIC, len(t), 0))
        f.write(t.tobytes())
        f.write(struct.pack('<II', ALB_MAGIC, len(t)))
        f.write(bytes([c for a in alb for c in a]))


def in_room(p, pad=0.0):
    x0, y0, z0, x1, y1, z1 = ROOM
    return x0 - pad < p[0] < x1 + pad and y0 - pad < p[1] < y1 + pad and z0 - pad < p[2] < z1 + pad


# ---------------------------------------------------------------- the checks
def structure(files, fails):
    """Parse every file; per-file invariants. Returns [(file, tbk)]."""
    out = []
    for f in files:
        try:
            t = read_tbk(f)
        except Exception as e:
            fails.append('%s: %s' % (os.path.basename(f), e))
            continue
        name = 'sector_%+05d_%+05d.tbk' % (t['cx'], t['cy'])
        if os.path.basename(f) != name:
            fails.append('%s: header says %s' % (os.path.basename(f), name))
        p, s, lk = t['probes'], t['surfels'], t['links']
        want = (1 if len(lk) else 0) | (2 if len(p) else 0)
        if t['flags'] != want:
            fails.append('%s: flags %d, want %d' % (name, t['flags'], want))
        keys = {}
        for i, sp in enumerate(s):
            k = tuple(floordiv(sp['pos'][a], t['cell']) for a in range(3))
            if k in keys:
                fails.append('%s: surfel cell %s twice' % (name, k))
                break
            keys[k] = i
        nxt = 0
        for i, pr in enumerate(p):
            if pr['off'] != nxt or pr['off'] + pr['cnt'] > len(lk):
                fails.append('%s: probe %d links %d+%d (expected offset %d of %d)' % (name, i, pr['off'], pr['cnt'], nxt, len(lk)))
                break
            nxt += pr['cnt']
            if (floordiv(pr['pos'][0], 4096.0), floordiv(pr['pos'][1], 4096.0)) != (t['cx'], t['cy']):
                fails.append('%s: probe %d stands in another sector' % (name, i))
                break
        if nxt != len(lk):
            fails.append('%s: %d links not owned by a probe' % (name, len(lk) - nxt))
        t['keys'] = keys
        out.append((f, t))
    return out


def per_probe(t, fn):
    p, lk, s = t['probes'], t['links'], t['surfels']
    for i, pr in enumerate(p):
        pk = tuple(floordiv(pr['pos'][a], t['cell']) for a in range(3))
        L = lk[pr['off']:pr['off'] + pr['cnt']]
        fn(i, pr, pk, L)


def budget_and_facing(tbks, fails, masses=None):
    """weights + unlinked + sky = coverage; links resolve; surfels face their probes."""
    stat = dict(probes=0, links=0, unresolved=0, facing_bad=0, budget_bad=0, worst=0.0)
    for f, t in tbks:
        s, keys = t['surfels'], t['keys']

        def one(i, pr, pk, L):
            stat['probes'] += 1
            stat['links'] += len(L)
            wsum = float(np.sum(L['w'].astype(np.float64))) * float(pr['scale'])
            m = masses if masses is not None else np.full(8, 1.0 / 8)
            sky = float(np.dot(pr['sky'].astype(np.float64), m))
            tol = 0.5 * float(pr['scale']) * len(L) + (1e-4 if masses is not None else 0.02)
            err = abs(wsum + float(pr['unl']) + sky - float(pr['cov']))
            stat['worst'] = max(stat['worst'], err)
            if err > tol:
                stat['budget_bad'] += 1
            for l in L:
                k = tuple(int(pk[a]) + int(l['delta'][a]) for a in range(3))
                j = keys.get(k)
                if j is None:
                    stat['unresolved'] += 1
                    continue
                n = s[j]['nrm'].astype(np.float64) / 32767.0
                if np.dot(n, unpack_dir(l['dir'])) >= 0:
                    stat['facing_bad'] += 1
        per_probe(t, one)
    if stat['unresolved']:
        fails.append('%d links name no surfel of their file' % stat['unresolved'])
    if stat['budget_bad']:
        fails.append('%d probes break links + unlinked + sky = coverage (worst %.4f)' % (stat['budget_bad'], stat['worst']))
    if stat['facing_bad']:
        fails.append('%d of %d links reach a surfel turned away from the probe' % (stat['facing_bad'], stat['links']))
    return stat


def hits_of(tris, o, dirs, cell):
    tt, k = trace(tris, o, dirs)
    hit = np.isfinite(tt)
    use = hit & (tt > 1e-3)
    hp = (o[None, :] + dirs * np.where(use, tt, 0.0)[:, None]).astype(np.float32)
    kc = np.floor(hp / np.float32(cell)).astype(np.int64)
    return tt, k, hit, use, hp, kc


def zyx(k):
    return (k[2], k[1], k[0])   # the bake's Key order


def model_surfels(tris, alb, probes, dirs, cell, spill=True):
    """Every probe's hits into per-cell bins by facing side; each cell keeps the side
    most rays saw plus the faces not opposed to it. The opposed faces (a thin wall's
    second side) move to the free neighbour most along their normal, within 45 deg,
    in key order. Returns (key -> (normal, albedo, samples), key -> second side's key)."""
    fn = np.cross(tris[:, 1] - tris[:, 0], tris[:, 2] - tris[:, 0])
    fn /= np.linalg.norm(fn, axis=1)[:, None]
    bins = {}
    for o in probes:
        tt, k, hit, use, hp, kc = hits_of(tris, o, dirs, cell)
        for j in np.nonzero(use)[0]:
            nv = fn[k[j]].copy()
            if np.dot(nv, dirs[j]) > 0:
                nv = -nv
            ax = int(np.argmax(np.abs(nv)))
            bi = ax * 2 + (1 if nv[ax] < 0 else 0)
            b = bins.setdefault(tuple(kc[j]), [[0, np.zeros(3), np.zeros(3)] for _ in range(6)])[bi]
            b[0] += 1
            b[1] += nv
            b[2] += np.array(alb[k[j]], dtype=np.float64) / 255.0
    def final(n, nv, al):
        return (nv / np.linalg.norm(nv), tuple(int(round(min(max(x / n, 0), 1) * 255)) for x in al), n)

    out, backs = {}, []
    for key, bs in bins.items():
        w = max(range(6), key=lambda i: (bs[i][0], -i))
        n, nv, al = 0, np.zeros(3), np.zeros(3)
        bn, bv, ba = 0, np.zeros(3), np.zeros(3)
        for b in bs:
            if not b[0]:
                continue
            if np.dot(b[1], bs[w][1]) >= 0:
                n += b[0]; nv += b[1]; al += b[2]
            else:
                bn += b[0]; bv += b[1]; ba += b[2]
        out[key] = final(n, nv, al)
        if bn and spill:
            backs.append((key, bn, bv, ba))
    alt = {}
    for key, bn, bv, ba in sorted(backs, key=lambda e: zyx(e[0])):
        bl = np.linalg.norm(bv)
        if bl <= 0:
            continue
        tries = []
        for d in ((x, y, z) for x in (-1, 0, 1) for y in (-1, 0, 1) for z in (-1, 0, 1) if (x, y, z) != (0, 0, 0)):
            c = float(np.dot(d, bv)) / (bl * math.sqrt(d[0] ** 2 + d[1] ** 2 + d[2] ** 2))
            tries.append((c, tuple(key[a] + d[a] for a in range(3))))
        tries.sort(key=lambda e: (-e[0], zyx(e[1])))
        for c, nb in tries:
            if c < 0.7071 - 1e-6:
                break
            if nb not in bins and nb not in out:
                out[nb] = final(bn, bv, ba)
                alt[key] = nb
                break
    return out, alt


def retrace(tbks, tris, alb, n, fails):
    dirs = fib(n)
    oc = octant(dirs)
    omega = FOUR_PI / n
    masses = np.bincount(oc, minlength=8) / float(n)
    st = dict(probes=0, ground=0, room=0, sky_bad=0, dist_bad=0, link_bad=0, unl_bad=0, dir_bad=0, dirs=0,
              phys_bad=[], alb_bad=0, alb_n=0, nrm_bad=0, sf_missing=0, sf_n=0, turned=0)
    cell = tbks[0][1]['cell'] if tbks else 70.0
    allp = [pr['pos'].astype(np.float64) for _, t in tbks for pr in t['probes']]
    model, alt = model_surfels(tris, alb, allp, dirs, cell)
    for f, t in tbks:
        s, keys = t['surfels'], t['keys']
        for sp in s:
            key = tuple(floordiv(sp['pos'][a], cell) for a in range(3))
            m = model.get(key)
            st['sf_n'] += 1
            if m is None:
                st['sf_missing'] += 1
                continue
            if np.dot(sp['nrm'] / 32767.0, m[0]) < math.cos(math.radians(1.0)) or tuple(int(c) for c in sp['alb']) != m[1]:
                st['nrm_bad'] += 1

        def one(i, pr, pk, L):
            st['probes'] += 1
            o = pr['pos'].astype(np.float64)
            tt, k, hit, use, hp, kc = hits_of(tris, o, dirs, cell)
            sky = np.zeros(8); surf = np.zeros(8); dsum = np.zeros(8)
            np.add.at(sky, oc[~hit], omega)
            np.add.at(surf, oc[use], omega)
            np.add.at(dsum, oc[use], tt[use] * omega)
            sv = np.where(sky + surf > 0, sky / np.maximum(sky + surf, 1e-30), 0.0)
            if np.max(np.abs(sv - pr['sky'])) > 1.5 / n * 8:
                st['sky_bad'] += 1
            dm = np.where(surf > 0, dsum / np.maximum(surf, 1e-30), 0.0)
            if np.max(np.abs(dm - pr['dist']) / np.maximum(dm, 1.0)) > 2e-3:
                st['dist_bad'] += 1
            cells = {}
            for j in np.nonzero(use)[0]:
                c = cells.setdefault(tuple(kc[j]), [0.0, np.zeros(3)])
                c[0] += omega
                c[1] += dirs[j] * omega
            # the facing rule, from the model's own surfels
            kept, turned = {}, 0.0
            for kk, c in cells.items():
                m = model.get(kk)
                if m is None or np.dot(c[1], m[0]) >= 0:
                    nb = alt.get(kk)
                    if nb is not None and np.dot(c[1], model[nb][0]) < 0:
                        kept[nb] = c   # the second side, housed next door
                        continue
                    turned += c[0]
                    st['turned'] += 1
                else:
                    kept[kk] = c
            order = sorted(kept.items(), key=lambda e: (-e[1][0], e[0][2], e[0][1], e[0][0]))
            cap = sum(c[0] for _, c in order[MAX_LINKS:])
            want_unl = (turned + cap) / FOUR_PI
            got = {}
            for l in L:
                kk = tuple(int(pk[a]) + int(l['delta'][a]) for a in range(3))
                got[kk] = (float(l['w']) * float(pr['scale']), l['dir'])
            q = 0.5 * float(pr['scale']) * len(L)
            if abs(float(pr['unl']) - want_unl) > q + 4.0 / n:
                st['unl_bad'] += 1
            l1 = sum(abs(got.get(kk, (0.0,))[0] - c[0] / FOUR_PI) for kk, c in kept.items())
            l1 += sum(w for kk, (w, _) in got.items() if kk not in kept)
            if l1 > cap / FOUR_PI + q + 4.0 / n:
                st['link_bad'] += 1
            for kk, (w, d) in got.items():
                c = kept.get(kk, cells.get(kk))
                if c is None or np.linalg.norm(c[1]) == 0:
                    continue
                st['dirs'] += 1
                if np.dot(unpack_dir(d), c[1] / np.linalg.norm(c[1])) < math.cos(math.radians(1.0)):
                    st['dir_bad'] += 1
            # the physics
            if in_room(o):
                st['room'] += 1
                if np.max(pr['sky']) != 0 or abs(pr['cov'] - 1) > 1e-4:
                    st['phys_bad'].append('room probe %.0f,%.0f,%.0f sees sky' % tuple(o))
            elif not in_room(o, 70) and o[2] > GROUND_Z:
                st['ground'] += 1
                away = (0 if o[0] > (ROOM[0] + ROOM[3]) / 2 else 1) | ((0 if o[1] > (ROOM[1] + ROOM[4]) / 2 else 1) << 1)
                if pr['sky'][away] != 1.0 or np.max(pr['sky'][4:]) > 1.5 / (n / 8):
                    st['phys_bad'].append('ground probe %.0f,%.0f,%.0f sky %s' % (tuple(o) + (np.round(pr['sky'], 3).tolist(),)))
        per_probe(t, one)
        # albedo: ground cells well clear of the room, and the room's roof
        for sp in s:
            p = sp['pos']
            want = None
            if abs(p[2] - GROUND_Z) < 1 and not in_room((p[0], p[1], ROOM[2] + 1), 140):
                want = ALB_GROUND
            elif abs(p[2] - ROOM[5]) < 1 and in_room((p[0], p[1], ROOM[2] + 1), -140):
                want = ALB_ROOM
            if want:
                st['alb_n'] += 1
                if tuple(int(c) for c in sp['alb']) != want:
                    st['alb_bad'] += 1
    if st['nrm_bad'] or st['sf_missing']:
        fails.append('%d of %d surfels differ from the modelled cell (normal/albedo), %d not modelled at all' % (
            st['nrm_bad'], st['sf_n'], st['sf_missing']))
    for key, what in (('sky_bad', 'sky per octant'), ('dist_bad', 'octant distance'), ('link_bad', 'link cells/weights'),
                      ('unl_bad', 'unlinked weight (facing rule + cap)')):
        if st[key]:
            fails.append('%d probes differ from the re-trace in %s' % (st[key], what))
    if st['dir_bad'] > 0.01 * max(1, st['dirs']):
        fails.append('%d of %d link directions off the re-trace by > 1 deg' % (st['dir_bad'], st['dirs']))
    if st['phys_bad']:
        fails.append('%d probes break the physics (first: %s)' % (len(st['phys_bad']), st['phys_bad'][0]))
    if not st['room'] or not st['ground']:
        fails.append('placement gave %d room and %d ground probes: the scene tests nothing' % (st['room'], st['ground']))
    if st['alb_bad'] or not st['alb_n']:
        fails.append('%d of %d surfels do not carry their surface albedo' % (st['alb_bad'], st['alb_n']))
    return st, masses


def run_bake(exe, soup, out, rays, threads, red, extra=()):
    cmd = [os.path.abspath(exe), '-no-gui', 'probebake', '--soup', soup, '--rect', ','.join('%g' % v for v in RECT),
           '--out', out, '--rays', str(rays), '--threads', str(threads)] + list(extra)
    if red:
        cmd += ['--red', red]
    return subprocess.run(cmd, capture_output=True, text=True, timeout=900)


def files_in(d):
    return sorted(os.path.join(d, f) for f in os.listdir(d) if f.endswith('.tbk')) if os.path.isdir(d) else []


def synth(exe, work, red):
    os.makedirs(work, exist_ok=True)
    tris, alb = scene()
    soup = os.path.join(work, 'bake_synth.psp')
    write_soup(soup, tris, alb)
    rays = 1024
    outs = []
    for th in (1, 0):
        out = os.path.join(work, 'bake_synth%s_t%d' % ('_red_' + red if red else '', th))
        for f in files_in(out):
            os.remove(f)
        rc = run_bake(exe, soup, out, rays, th, red)
        if rc.returncode != 0:
            print('synth FAIL: probebake rc %d %s' % (rc.returncode, rc.stderr.strip()[:300]))
            return 1
        outs.append(out)
    fails = []
    fa, fb = files_in(outs[0]), files_in(outs[1])
    if not fa:
        fails.append('no .tbk written')
    if [os.path.basename(f) for f in fa] != [os.path.basename(f) for f in fb] or \
            any(open(a, 'rb').read() != open(b, 'rb').read() for a, b in zip(fa, fb)):
        fails.append('1 thread and all threads wrote different files')
    tbks = structure(fa, fails)
    # the soup in float32, exactly as the bake reads it
    t32 = tris.astype(np.float32).astype(np.float64)
    st, masses = retrace(tbks, t32, alb, rays, fails)
    bf = budget_and_facing(tbks, fails, masses)
    # the interior rule (--no-sky): the same links, no sky anywhere, and the sky's weight unlinked instead
    ns_moved = 0
    if not red:
        out = os.path.join(work, 'bake_synth_nosky')
        for f in files_in(out):
            os.remove(f)
        rc = run_bake(exe, soup, out, rays, 0, '', ['--no-sky'])
        fn = files_in(out)
        if rc.returncode != 0 or [os.path.basename(f) for f in fn] != [os.path.basename(f) for f in fa]:
            fails.append('--no-sky: rc %d, files %d vs %d' % (rc.returncode, len(fn), len(fa)))
        else:
            nbad = []
            for (_, t0), f1 in zip(tbks, fn):
                t1 = read_tbk(f1)
                if t0['links'].tobytes() != t1['links'].tobytes() or t0['surfels'].tobytes() != t1['surfels'].tobytes():
                    nbad.append('links or surfels differ')
                    break
                for p0, p1 in zip(t0['probes'], t1['probes']):
                    if np.any(p1['sky'] != 0):
                        nbad.append('a probe keeps sky')
                        break
                    sky0 = float(np.dot(p0['sky'].astype(np.float64), masses))
                    if abs(float(p1['unl']) - float(p0['unl']) - sky0) > 1e-4:
                        nbad.append('unlinked %.4f, want %.4f + %.4f' % (p1['unl'], p0['unl'], sky0))
                        break
                    ns_moved += 1 if sky0 > 0 else 0
            budget_and_facing(structure(fn, nbad), nbad, masses)
            if not ns_moved:
                nbad.append('no probe had sky to move')
            fails += ['--no-sky: ' + b for b in nbad]
    verdict = 'PASS' if not fails else 'FAIL'
    print('synth %s: %d files, %d probes (%d ground, %d room) re-traced at %d rays, %d links, %d link directions, '
          '%d albedo surfels, %d surfels modelled, %d cell links refused as turned away, budget worst %.2g, '
          '--no-sky moved the sky of %d probes to unlinked; %s' % (
              verdict, len(fa), st['probes'], st['ground'], st['room'], rays, bf['links'], st['dirs'], st['alb_n'],
              st['sf_n'], st['turned'], bf['worst'], ns_moved, '; '.join(fails[:6]) if fails else 'all as traced'))
    return 0 if not fails else 1


def check(d):
    fails = []
    fs = files_in(d)
    if not fs:
        print('check FAIL: no .tbk in %s' % d)
        return 1
    tbks = structure(fs, fails)
    bf = budget_and_facing(tbks, fails)
    ns = sum(len(t['surfels']) for _, t in tbks)
    # per probe, pooled over the files (a mean of file means weighs a 9-probe file like a 500-probe one)
    sky = [float(np.mean(pr['sky'])) for _, t in tbks for pr in t['probes']]
    verdict = 'PASS' if not fails else 'FAIL'
    print('check %s: %d files, %d probes, %d surfels, %d links, %d facing away, budget worst %.3f (octants as 1/8), '
          'sky mean %.3f; %s' % (verdict, len(fs), bf['probes'], ns, bf['links'], bf['facing_bad'], bf['worst'],
                                 float(np.mean(sky)) if sky else 0.0, '; '.join(fails[:6]) if fails else 'sound'))
    return 0 if not fails else 1


if __name__ == '__main__':
    a = sys.argv[1:]
    if a and a[0] == 'synth':
        red = a[a.index('--red') + 1] if '--red' in a else ''
        sys.exit(synth(a[1], a[2], red))
    if a and a[0] == 'check':
        sys.exit(check(a[1]))
    print(__doc__)
    sys.exit(2)
