#!/usr/bin/env python3
"""PRTP probe bake gates (lane PRTPBAKE, 2026-09-30; `.tbk` v4, lane BAKE4, 2026-10-01).

  synth <exe> <workdir> [--red octant|normal|oneside] [--base <exe>]
        a synthetic block (an open ground plane, one sealed room on it, a known
        albedo per surface), run `probebake`, then:
          * parse every `.tbk` with this file's own reader (v3: FO4CS's layout, exact
            size; v4: the v3 body + its tail, exact size; every link resolves to a
            surfel of its file, on the side it names);
          * re-trace EVERY probe in numpy (the same Fibonacci ray set) and
            require the same sky per octant, octant distances, link cells,
            link sides, link weights and link directions;
          * the physics: ground probes see no sky below and only sky in the
            upper octant turned from the room; room probes see no sky at all;
          * the surfels modelled here too. v4: a cell keeps the side most rays saw
            (plus the faces not opposed to it) AND the opposed side as its back
            surfel; a ray links the side whose face it hit. v3 (--tbk 3, the second
            leg): one side per cell, the other moved to a free neighbour. No link
            reaches a surfel turned away from its probe (that weight is unlinked);
            albedo = the surface's own; two thread counts give byte-identical files.
        --base <exe> (the exe before lane BAKE4): its files and `--tbk 3`'s must be
        byte-identical (v3 untouched).
  rooms <exe> <workdir> [--red rooms|glass|oneside] [--base <exe>]
        probe_place.py's block (houses, a hallway building, doors) plus a sunroom with
        tinted glass: two windows (one pane two-sided) and a free-standing glass screen
        outside, a door box at its doorway. The full v4 re-trace with doors and glass
        (each link's door and tint, each probe's sky tint per octant), and the ROOM IDS
        against the plan: every probe inside a known room names that room's id (one id
        per room, rooms distinct), probes under open sky name none, an opening names the
        rooms on its two sides, the room boxes hold their room's probes and no outdoor one.
  check <bakedir>
        the structure + budget + facing checks on a real cell's bake (v3 or v4).

One verdict line; exit 0 on PASS, 1 on FAIL. Run each once more with each `--red`:
the gate must then FAIL.
"""
import math
import os
import struct
import subprocess
import sys

import numpy as np

SOUP_MAGIC = 0x31505350
ALB_MAGIC = 0x31424C41
GLS_MAGIC = 0x31534C47
TBK_MAGIC = 0x314B4254
FOUR_PI = 4.0 * math.pi
RAY_MAX = 131072.0
ROOM_NONE = 0xFFFFFFFF

SURFEL = np.dtype([('pos', '<f4', 3), ('nrm', '<i2', 3), ('alb', 'u1', 3), ('pad0', 'u1'),
                   ('pad', 'u1', 2), ('samples', '<u4'), ('pad1', '<u4')])
PROBE = np.dtype([('pos', '<f4', 3), ('off', '<u4'), ('cnt', '<u4'), ('scale', '<f4'), ('cov', '<f4'),
                  ('unl', '<f4'), ('sky', '<f4', 8), ('dist', '<f4', 8), ('rms', '<f4', 8),
                  ('cls', '<u4'), ('lvl', '<u4'), ('res', '<u4', 2)])
LINK = np.dtype([('delta', '<i2', 3), ('dir', '<i2', 2), ('w', '<u2')])
# the v4 tail (docs/PRTP_PLAN.md, the `.tbk` v4 section)
LINKEXT = np.dtype([('side', 'u1'), ('tint', 'u1', 3), ('door', '<u4')])
PROBEEXT = np.dtype([('skytint', 'u1', (8, 3)), ('room', '<u4', 2)])
ROOMBOX = np.dtype([('room', '<u4'), ('lo', '<f4', 3), ('hi', '<f4', 3), ('res', '<u4')])
assert SURFEL.itemsize == 32 and PROBE.itemsize == 144 and LINK.itemsize == 12
assert LINKEXT.itemsize == 8 and PROBEEXT.itemsize == 32 and ROOMBOX.itemsize == 32


MAX_LINKS = 1024   # src/probebake.h ProbeBakeSpec::maxLinks
APERTURE = 3       # ProbeClass::Aperture
VOXEL = 35.0       # src/probeplace.h ProbePlaceSpec::voxel (room ids and boxes are this fine)


def floordiv(v, s):
    # the reader's key: float32 division, then floor
    return int(math.floor(float(np.float32(v) / np.float32(s))))


def read_tbk(path):
    b = open(path, 'rb').read()
    if len(b) < 64:
        raise ValueError('shorter than a header')
    h = struct.unpack('<5if4I6I', b[:64])
    magic, ver, kind, cx, cy, cs, ns, np_, nl, flags = h[:10]
    res = h[10:16]
    if (magic & 0xffffffff) != TBK_MAGIC or ver not in (3, 4) or kind != 1:
        raise ValueError('magic/version/kind %x %d %d' % (magic & 0xffffffff, ver, kind))
    need = 64 + 32 * ns + 144 * np_ + 12 * nl
    nb = nbox = 0
    if ver == 4:
        nb, nbox = res[0], res[1]
        need += 32 * nb + 8 * nl + 32 * np_ + 32 * nbox
    if len(b) != need:
        raise ValueError('size %d, the counts say %d' % (len(b), need))
    o = 64
    s = np.frombuffer(b, SURFEL, ns, o); o += 32 * ns
    p = np.frombuffer(b, PROBE, np_, o); o += 144 * np_
    lk = np.frombuffer(b, LINK, nl, o); o += 12 * nl
    if ver == 4:
        back = np.frombuffer(b, SURFEL, nb, o); o += 32 * nb
        lx = np.frombuffer(b, LINKEXT, nl, o); o += 8 * nl
        px = np.frombuffer(b, PROBEEXT, np_, o); o += 32 * np_
        bx = np.frombuffer(b, ROOMBOX, nbox, o)
    else:   # what a v3 file means in v4's terms: front sides, clear, no doors, no rooms
        back = np.zeros(0, SURFEL)
        lx = np.zeros(nl, LINKEXT)
        lx['tint'] = 255
        px = np.zeros(np_, PROBEEXT)
        px['skytint'] = 255
        px['room'][:, 1] = ROOM_NONE
        bx = np.zeros(0, ROOMBOX)
    return dict(ver=ver, content=res[2] if ver == 4 else 0, cx=cx, cy=cy, cell=cs, flags=flags, surfels=s,
                probes=p, links=lk, back=back, lext=lx, pext=px, boxes=bx)


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


def all_hits(tris, o, dirs, tmin=1e-4):
    """t of every ray against every triangle, (rays, tris); inf where it misses."""
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
    hit = ok & (u >= 0) & (u <= 1) & (v >= 0) & (u + v <= 1) & (t > tmin)
    return np.where(hit, t, np.inf)


def trace(tris, o, dirs, tmax=RAY_MAX):
    """Nearest hit per ray in (1e-4, tmax]: (t, tri) with t = inf on a miss."""
    t = all_hits(tris, o, dirs)
    t = np.where(t <= tmax, t, np.inf)
    k = np.argmin(t, 1)
    return t[np.arange(len(dirs)), k], k


def face_normals(tris):
    fn = np.cross(tris[:, 1] - tris[:, 0], tris[:, 2] - tris[:, 0])
    return fn / np.linalg.norm(fn, axis=1)[:, None]


def facing_bins(fn, k, dirs):
    """The hit face's normal turned toward the probe, and its bin: the dominant axis x 2 (+1 negative)."""
    nv = fn[k].copy()
    flip = np.einsum('rk,rk->r', nv, dirs) > 0
    nv[flip] = -nv[flip]
    ax = np.argmax(np.abs(nv), 1)
    return nv, ax * 2 + (nv[np.arange(len(nv)), ax] < 0).astype(int)


def glass_through(gtris, gT, o, dirs, tend):
    """Per ray, the product of the transmittance of every pane crossed strictly inside
    (0.01, tend - 0.01); a crossing within 0.01 of the last one counted is the same pane
    (a two-sided pane's twin face). Returns (T (rays, 3), crossed (rays,))."""
    T = np.ones((len(dirs), 3))
    crossed = np.zeros(len(dirs), bool)
    if gtris is None or not len(gtris):
        return T, crossed
    tg = all_hits(gtris, o, dirs)
    ok = (tg > 0.01) & (tg < tend[:, None] - 0.01)
    for r in np.nonzero(ok.any(1))[0]:
        idx = np.nonzero(ok[r])[0]
        last = -1.0
        for g in idx[np.argsort(tg[r, idx], kind='stable')]:
            if last >= 0 and tg[r, g] - last <= 0.01:
                continue
            T[r] *= gT[g]
            last = tg[r, g]
            crossed[r] = True
    return T, crossed


def doors_on(doors, o, dirs, tt):
    """Per ray, the door box the segment o + d [0, tt] enters first (0 = none; a tie: the lower ref)."""
    n = len(dirs)
    best = np.zeros(n, np.int64)
    bt = np.full(n, np.inf)
    for ref, lo, hi in sorted(doors, key=lambda e: e[0]):
        t0 = np.zeros(n)
        t1 = tt.copy()
        ins = np.ones(n, bool)
        for a in range(3):
            d = dirs[:, a]
            par = np.abs(d) < 1e-12
            ds = np.where(par, 1.0, d)
            ta, tb = (lo[a] - o[a]) / ds, (hi[a] - o[a]) / ds
            t0 = np.where(par, t0, np.maximum(t0, np.minimum(ta, tb)))
            t1 = np.where(par, t1, np.minimum(t1, np.maximum(ta, tb)))
            ins &= np.where(par, (o[a] >= lo[a]) & (o[a] <= hi[a]), True)
        ins &= t0 <= t1
        take = ins & (t0 < bt)
        best[take] = ref
        bt[take] = t0[take]
    return best


# ---------------------------------------------------------------- scene 1 (synth)
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


def write_soup(path, tris, alb, doors=(), glass=None):
    t = np.asarray(tris, dtype='<f4').reshape(-1, 9)
    with open(path, 'wb') as f:
        f.write(struct.pack('<III', SOUP_MAGIC, len(t), len(doors)))
        f.write(t.tobytes())
        for ref, lo, hi in doors:
            f.write(struct.pack('<I3f3f', ref, *lo, *hi))
        f.write(struct.pack('<II', ALB_MAGIC, len(t)))
        f.write(bytes([c for a in alb for c in a]))
        if glass:
            g = np.asarray([p for p, _ in glass], dtype='<f4').reshape(-1, 9)
            f.write(struct.pack('<II', GLS_MAGIC, len(g)))
            f.write(g.tobytes())
            f.write(bytes([c for _, tr in glass for c in tr]))
        # lane ROOMCLAMP1: the ground is wound down on purpose (facing is the bake's job); every face
        # two-sided keeps these gates off the outside-the-shell rule (cell_rooms gates winding)
        f.write(struct.pack('<II', 0x314F5754, len(t)))
        f.write(b'\x01' * len(t))


def in_room(p, pad=0.0):
    x0, y0, z0, x1, y1, z1 = ROOM
    return x0 - pad < p[0] < x1 + pad and y0 - pad < p[1] < y1 + pad and z0 - pad < p[2] < z1 + pad


# ---------------------------------------------------------------- scene 2 (rooms)
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
OX, OY = 7.0, 11.0      # probe_place.py's offsets (faces off the 280 lattice lines)
RECT2 = (-1500.0, -1500.0, 1500.0, 1500.0)
GLASS_S, GLASS_W, GLASS_SCREEN = (77, 153, 230), (230, 153, 77), (180, 180, 180)
DOOR_SUN = 0xB002


def scene2():
    """probe_place.py's block + the sunroom. Returns tris, alb, doors, glass, rooms, openings."""
    from probe_place import box, synth_scene, wall_with_holes
    T, doors, _, _, prooms, _ = synth_scene()

    def off(tris):
        return [tuple((p[0] + OX, p[1] + OY, p[2]) for p in tri) for tri in tris]

    # THE SUNROOM: -1300..-700 x -900..-450, walls 20, height 300; a 400-wide window south,
    # a 250-wide window west, a doorway east (with a door box)
    S = []
    S += off(wall_with_holes('x', -900, -880, -1300, -700, 0, 300, [(-1200, -800, 60, 260)]))
    S += off(wall_with_holes('x', -470, -450, -1300, -700, 0, 300, ()))
    S += off(wall_with_holes('y', -1300, -1280, -900, -450, 0, 300, [(-800, -550, 60, 260)]))
    S += off(wall_with_holes('y', -720, -700, -900, -450, 0, 300, [(-720, -600, 0, 220)]))
    S += off(box(-1300, -900, 300, -700, -450, 320))
    T = list(T) + S
    doors = list(doors) + [(DOOR_SUN, (-720 + OX, -720 + OY, 0.0), (-700 + OX, -600 + OY, 220.0))]
    alb = []
    for tri in T:
        flat = all(abs(p[2]) < 1e-6 for p in tri)
        alb.append(ALB_GROUND if flat else ((220, 215, 190) if tri in S else ALB_ROOM))

    def quad(a, b, c, d):
        return [(a, b, c), (a, c, d)]

    glass = []
    ys = -890 + OY
    south = quad((-1200 + OX, ys, 60), (-800 + OX, ys, 60), (-800 + OX, ys, 260), (-1200 + OX, ys, 260))
    glass += [(t, GLASS_S) for t in south]
    glass += [((t[0], t[2], t[1]), GLASS_S) for t in south]          # its twin, wound the other way
    xw = -1290 + OX
    glass += [(t, GLASS_W) for t in quad((xw, -800 + OY, 60), (xw, -550 + OY, 60), (xw, -550 + OY, 260), (xw, -800 + OY, 260))]
    yv = -1000 + OY
    glass += [(t, GLASS_SCREEN) for t in quad((-1250 + OX, yv, 0), (-750 + OX, yv, 0), (-750 + OX, yv, 280), (-1250 + OX, yv, 280))]
    rooms = [(n, x0, y0, x1, y1) for n, x0, y0, x1, y1, _ in prooms]
    rooms += [('A west', 20 + OX, 20 + OY, 560 + OX, 580 + OY), ('A east', 580 + OX, 20 + OY, 780 + OX, 580 + OY),
              ('B', -1180 + OX, 20 + OY, -620 + OX, 480 + OY), ('sunroom', -1280 + OX, -880 + OY, -720 + OX, -470 + OY)]
    # (x, y, the room, the room on the other side or None = outdoors)
    openings = [(360 + OX, 10 + OY, 'A west', None), (570 + OX, 310 + OY, 'A west', 'A east'),
                (790 + OX, 260 + OY, 'A east', None), (-890 + OX, 10 + OY, 'B', None),
                (1060 + OX, 810 + OY, 'E hallway', None), (460 + OX, 970 + OY, 'E hallway', 'E west room'),
                (710 + OX, 1210 + OY, 'E west room', 'E east room'), (-710 + OX, -660 + OY, 'sunroom', None),
                (-1000 + OX, -890 + OY, 'sunroom', None), (-1290 + OX, -675 + OY, 'sunroom', None)]
    return np.array(T, dtype=np.float64), alb, doors, glass, rooms, openings


# ---------------------------------------------------------------- the checks
def structure(files, fails):
    """Parse every file; per-file invariants. Returns [(file, tbk)]."""
    out = []
    sector = 4096.0
    if files:   # the far map states its own square (src/probefar.cpp far.txt); FO4CS's near bake is a cell
        man = os.path.join(os.path.dirname(files[0]), 'far.txt')
        if os.path.isfile(man):
            for line in open(man):
                if line.startswith('sector '):
                    sector = float(line.split()[1])
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
        for nm, arr in (('keys', s), ('keys_back', t['back'])):
            keys = {}
            for i, sp in enumerate(arr):
                k = tuple(floordiv(sp['pos'][a], t['cell']) for a in range(3))
                if k in keys:
                    fails.append('%s: %s surfel cell %s twice' % (name, 'back' if nm == 'keys_back' else 'front', k))
                    break
                keys[k] = i
            t[nm] = keys
        nxt = 0
        for i, pr in enumerate(p):
            if pr['off'] != nxt or pr['off'] + pr['cnt'] > len(lk):
                fails.append('%s: probe %d links %d+%d (expected offset %d of %d)' % (name, i, pr['off'], pr['cnt'], nxt, len(lk)))
                break
            nxt += pr['cnt']
            if (floordiv(pr['pos'][0], sector), floordiv(pr['pos'][1], sector)) != (t['cx'], t['cy']):
                fails.append('%s: probe %d stands in another sector' % (name, i))
                break
        if nxt != len(lk):
            fails.append('%s: %d links not owned by a probe' % (name, len(lk) - nxt))
        if t['ver'] == 4:
            if len(t['lext']) and int(t['lext']['side'].max()) > 1:
                fails.append('%s: a link side other than 0/1' % name)
            named = set(int(r) for r in t['pext']['room'].ravel()) - {0, ROOM_NONE}
            for bx in t['boxes']:
                if int(bx['room']) not in named:
                    fails.append('%s: a box of room %08x no probe of the file names' % (name, int(bx['room'])))
                    break
                if np.any(bx['lo'] >= bx['hi']):
                    fails.append('%s: an empty room box' % name)
                    break
        out.append((f, t))
    return out


def per_probe(t, fn):
    p, lk, lx = t['probes'], t['links'], t['lext']
    for i, pr in enumerate(p):
        pk = tuple(floordiv(pr['pos'][a], t['cell']) for a in range(3))
        a, b = int(pr['off']), int(pr['off'] + pr['cnt'])
        fn(i, pr, pk, lk[a:b], lx[a:b])


def surfel_of(t, pk, l, x, ignore_side=False):
    """The surfel a link names: its cell, on its side (v4); None if the file lacks it."""
    k = tuple(int(pk[a]) + int(l['delta'][a]) for a in range(3))
    if x['side'] and not ignore_side:
        j = t['keys_back'].get(k)
        return None if j is None else t['back'][j]
    j = t['keys'].get(k)
    return None if j is None else t['surfels'][j]


def budget_and_facing(tbks, fails, masses=None):
    """weights + unlinked + sky = coverage; links resolve; surfels face their probes."""
    stat = dict(probes=0, links=0, unresolved=0, facing_bad=0, budget_bad=0, worst=0.0, back=0, door=0, tinted=0)
    for f, t in tbks:
        def one(i, pr, pk, L, X):
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
            for l, x in zip(L, X):
                stat['back'] += int(x['side'])
                stat['door'] += 1 if x['door'] else 0
                stat['tinted'] += 1 if np.any(x['tint'] != 255) else 0
                sf = surfel_of(t, pk, l, x)
                if sf is None:
                    stat['unresolved'] += 1
                    continue
                n = sf['nrm'].astype(np.float64) / 32767.0
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


def model_surfels(tris, alb, probes, dirs, cell, mode='spill'):
    """Every probe's hits into per-cell bins by facing side; each cell keeps the side
    most rays saw plus the faces not opposed to it. The opposed faces (a thin wall's
    second side): mode 'spill' (v3) moves them to the free neighbour most along their
    normal, within 45 deg, in key order; mode 'v4' keeps them as the cell's back surfel.
    Returns (key -> (normal, albedo, samples), key -> second side's key (spill),
    key -> back surfel (v4), key -> the bins that make up the back side (v4))."""
    fn = face_normals(tris)
    bins = {}
    for o in probes:
        tt, k, hit, use, hp, kc = hits_of(tris, o, dirs, cell)
        nvs, bis = facing_bins(fn, k, dirs)
        for j in np.nonzero(use)[0]:
            b = bins.setdefault(tuple(kc[j]), [[0, np.zeros(3), np.zeros(3)] for _ in range(6)])[bis[j]]
            b[0] += 1
            b[1] += nvs[j]
            b[2] += np.array(alb[k[j]], dtype=np.float64) / 255.0

    def final(n, nv, al):
        return (nv / np.linalg.norm(nv), tuple(int(round(min(max(x / n, 0), 1) * 255)) for x in al), n)

    out, backs, back, mask = {}, [], {}, {}
    for key, bs in bins.items():
        w = max(range(6), key=lambda i: (bs[i][0], -i))
        n, nv, al = 0, np.zeros(3), np.zeros(3)
        bn, bv, ba = 0, np.zeros(3), np.zeros(3)
        bm = set()
        for i, b in enumerate(bs):
            if not b[0]:
                continue
            if np.dot(b[1], bs[w][1]) >= 0:
                n += b[0]; nv += b[1]; al += b[2]
            else:
                bn += b[0]; bv += b[1]; ba += b[2]
                bm.add(i)
        out[key] = final(n, nv, al)
        if bn and mode == 'v4':
            back[key] = final(bn, bv, ba)
            mask[key] = bm
        elif bn and mode == 'spill':
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
    return out, alt, back, mask


def retrace(tbks, tris, alb, n, fails, v4=True, doors=(), glass=None, physics=True):
    dirs = fib(n)
    oc = octant(dirs)
    omega = FOUR_PI / n
    masses = np.bincount(oc, minlength=8) / float(n)
    st = dict(probes=0, ground=0, room=0, sky_bad=0, dist_bad=0, link_bad=0, unl_bad=0, dir_bad=0, dirs=0,
              phys_bad=[], alb_bad=0, alb_n=0, nrm_bad=0, sf_missing=0, sf_n=0, turned=0, back_n=0,
              tint_bad=0, tint_n=0, skytint_bad=0, skytint_n=0, door_n=0, side_n=0, glass_share=0.0)
    cell = tbks[0][1]['cell'] if tbks else 70.0
    allp = [pr['pos'].astype(np.float64) for _, t in tbks for pr in t['probes']]
    model, alt, back, mask = model_surfels(tris, alb, allp, dirs, cell, 'v4' if v4 else 'spill')
    fn = face_normals(tris)
    gtris = gT = None
    if glass:
        gtris = np.array([p for p, _ in glass], dtype=np.float32).astype(np.float64)
        gT = np.array([tr for _, tr in glass], dtype=np.float64) / 255.0
    for f, t in tbks:
        for arr, mdl in ((t['surfels'], model), (t['back'], back)):
            for sp in arr:
                key = tuple(floordiv(sp['pos'][a], cell) for a in range(3))
                m = mdl.get(key)
                st['sf_n'] += 1
                st['back_n'] += 1 if mdl is back else 0
                if m is None:
                    st['sf_missing'] += 1
                    continue
                # albedo to one 8-bit step: a mean of two albedos can sit exactly halfway, and
                # the bake's float sum and this double sum round that tie apart
                if np.dot(sp['nrm'] / 32767.0, m[0]) < math.cos(math.radians(1.0)) or \
                        max(abs(int(c) - w) for c, w in zip(sp['alb'], m[1])) > 1:
                    st['nrm_bad'] += 1

        def one(i, pr, pk, L, X):
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
            T = np.ones((n, 3))
            bis = np.zeros(n, int)
            door = np.zeros(n, np.int64)
            if v4:
                T, crossed = glass_through(gtris, gT, o, dirs, np.where(hit, tt, RAY_MAX))
                st['glass_share'] += float(crossed.sum()) / n
                _, bis = facing_bins(fn, k, dirs)
                if doors:
                    door = doors_on(doors, o, dirs, np.where(hit, tt, 0.0))
                # the sky through glass, per octant
                skyT = np.zeros((8, 3))
                np.add.at(skyT, oc[~hit], T[~hit] * omega)
                want = np.where(sky[:, None] > 0, np.rint(np.clip(skyT / np.maximum(sky[:, None], 1e-30), 0, 1) * 255), 255)
                got = t['pext'][i]['skytint'].astype(np.float64)
                st['skytint_n'] += int(np.sum(want < 255))
                if np.max(np.abs(want - got)) > 2:
                    st['skytint_bad'] += 1
            cells = {}
            for j in np.nonzero(use)[0]:
                kk = tuple(kc[j])
                side = 1 if (v4 and bis[j] in mask.get(kk, ())) else 0
                key = (kk, side, int(door[j]))
                c = cells.setdefault(key, [0.0, np.zeros(3), np.zeros(3)])
                c[0] += omega
                c[1] += dirs[j] * omega
                c[2] += T[j] * omega
            # the facing rule, from the model's own surfels
            kept, turned = {}, 0.0
            for key, c in cells.items():
                kk, side, dr = key
                if v4:
                    m = (back if side else model).get(kk)
                    if m is None or np.dot(c[1], m[0]) >= 0:
                        turned += c[0]
                        st['turned'] += 1
                    else:
                        kept[key] = c
                    continue
                m = model.get(kk)
                if m is None or np.dot(c[1], m[0]) >= 0:
                    nb = alt.get(kk)
                    if nb is not None and np.dot(c[1], model[nb][0]) < 0:
                        kept[(nb, 0, 0)] = c   # the second side, housed next door
                        continue
                    turned += c[0]
                    st['turned'] += 1
                else:
                    kept[key] = c
            order = sorted(kept.items(), key=lambda e: (-e[1][0], e[0][0][2], e[0][0][1], e[0][0][0], e[0][1], e[0][2]))
            cap = sum(c[0] for _, c in order[MAX_LINKS:])
            want_unl = (turned + cap) / FOUR_PI
            got = {}
            for l, x in zip(L, X):
                kk = tuple(int(pk[a]) + int(l['delta'][a]) for a in range(3))
                got[(kk, int(x['side']), int(x['door']))] = (float(l['w']) * float(pr['scale']), l['dir'], x['tint'])
            q = 0.5 * float(pr['scale']) * len(L)
            if abs(float(pr['unl']) - want_unl) > q + 4.0 / n:
                st['unl_bad'] += 1
            l1 = sum(abs(got.get(key, (0.0,))[0] - c[0] / FOUR_PI) for key, c in kept.items())
            l1 += sum(w for key, (w, _, _) in got.items() if key not in kept)
            if l1 > cap / FOUR_PI + q + 4.0 / n:
                st['link_bad'] += 1
            for key, (w, d, tint) in got.items():
                st['side_n'] += key[1]
                st['door_n'] += 1 if key[2] else 0
                c = kept.get(key, cells.get(key))
                if c is None or np.linalg.norm(c[1]) == 0:
                    continue
                st['dirs'] += 1
                if np.dot(unpack_dir(d), c[1] / np.linalg.norm(c[1])) < math.cos(math.radians(1.0)):
                    st['dir_bad'] += 1
                if v4:
                    want = np.rint(np.clip(c[2] / c[0], 0, 1) * 255)
                    st['tint_n'] += 1 if np.any(want < 255) else 0
                    if np.max(np.abs(want - tint.astype(np.float64))) > 2:
                        st['tint_bad'] += 1
            if not physics:
                return
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
        if not physics:
            continue
        # albedo: ground cells well clear of the room, and the room's roof
        for sp in t['surfels']:
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
    if v4 and back and not st['back_n']:
        fails.append('the model holds %d back sides, the files none' % len(back))
    for key, what in (('sky_bad', 'sky per octant'), ('dist_bad', 'octant distance'), ('link_bad', 'link cells/sides/doors/weights'),
                      ('unl_bad', 'unlinked weight (facing rule + cap)')):
        if st[key]:
            fails.append('%d probes differ from the re-trace in %s' % (st[key], what))
    if st['dir_bad'] > 0.01 * max(1, st['dirs']):
        fails.append('%d of %d link directions off the re-trace by > 1 deg' % (st['dir_bad'], st['dirs']))
    # a ray grazing a pane's edge may land on either side of it: 1% slack, never more
    if st['tint_bad'] > 0.01 * max(1, st['dirs']):
        fails.append('%d of %d link tints off the re-trace by > 2/255' % (st['tint_bad'], st['dirs']))
    if st['skytint_bad'] > 0.01 * max(1, st['probes']):
        fails.append('%d of %d probes have a sky tint off the re-trace by > 2/255' % (st['skytint_bad'], st['probes']))
    if st['phys_bad']:
        fails.append('%d probes break the physics (first: %s)' % (len(st['phys_bad']), st['phys_bad'][0]))
    if physics and (not st['room'] or not st['ground']):
        fails.append('placement gave %d room and %d ground probes: the scene tests nothing' % (st['room'], st['ground']))
    if physics and (st['alb_bad'] or not st['alb_n']):
        fails.append('%d of %d surfels do not carry their surface albedo' % (st['alb_bad'], st['alb_n']))
    st['glass_share'] /= max(1, st['probes'])
    return st, masses


def rooms_check(tbks, tris, rooms, openings, fails):
    """The room ids and boxes against the known plan."""
    st = dict(named=0, outdoors=0, openings=0, boxes=0, inbox=0)
    P = []   # (pos, cls, room0, room1)
    boxes = []
    for _, t in tbks:
        if t['ver'] == 4 and not (t['content'] & 2):
            fails.append('a file does not say it modelled rooms')
            break
        for pr, px in zip(t['probes'], t['pext']):
            P.append((pr['pos'].astype(np.float64), int(pr['cls']), int(px['room'][0]), int(px['room'][1])))
        boxes += [(int(b['room']), b['lo'].astype(np.float64), b['hi'].astype(np.float64)) for b in t['boxes']]
    st['boxes'] = len(boxes)
    ids = {}
    for name, x0, y0, x1, y1 in rooms:
        inside = [p for p in P if p[1] != APERTURE and x0 < p[0][0] < x1 and y0 < p[0][1] < y1 and p[0][2] < 300]
        if not inside:
            fails.append('%s holds no probe' % name)
            continue
        vals = [p[2] for p in inside]
        rid = max(set(vals), key=vals.count)
        if rid == 0 or any(v != rid for v in vals):
            fails.append('%s: its %d probes name rooms %s' % (name, len(inside), sorted(set('%08x' % v for v in vals))))
            continue
        ids[name] = rid
        st['named'] += len(inside)
    if len(set(ids.values())) != len(ids):
        fails.append('two rooms share an id: %s' % ids)
    # under open sky: open straight up from the probe AND from a voxel (35) around it -- room
    # ids and boxes are the placer's voxels, so a probe within a voxel of a roof edge is not tested
    up = np.array([[0.0, 0.0, 1.0]])
    for pos, cls, r0, r1 in P:
        if cls == APERTURE:
            continue
        if any(np.isfinite(trace(tris, pos + np.array([dx, dy, 0.0]), up)[0][0])
               for dx, dy in ((0, 0), (VOXEL, 0), (-VOXEL, 0), (0, VOXEL), (0, -VOXEL))):
            continue
        st['outdoors'] += 1
        if r0 != 0 or r1 != ROOM_NONE:
            fails.append('a probe under open sky at %.0f,%.0f,%.0f names rooms %08x %08x' % (tuple(pos) + (r0, r1)))
            break
        if any(np.all(pos >= lo - 0.5) and np.all(pos <= hi + 0.5) for _, lo, hi in boxes):
            fails.append('a probe under open sky at %.0f,%.0f,%.0f stands in a room box' % tuple(pos))
            break
    for x, y, a, b in openings:
        near = [p for p in P if p[1] == APERTURE and math.hypot(p[0][0] - x, p[0][1] - y) <= 45]
        if not near or a not in ids or (b is not None and b not in ids):
            continue
        st['openings'] += 1
        pos, _, r0, r1 = min(near, key=lambda p: math.hypot(p[0][0] - x, p[0][1] - y))
        want = {ids[a], ids[b]} if b is not None else None
        if (want is not None and {r0, r1} != want) or (want is None and (r0, r1) != (ids[a], 0)):
            fails.append('the opening at %.0f,%.0f names %08x %08x, want %s | %s' % (x, y, r0, r1, a, b or 'outdoors'))
    if st['openings'] < 5:
        fails.append('only %d of %d openings found to test' % (st['openings'], len(openings)))
    # the boxes are the room's AIR voxels: a probe hard against a wall stands in a voxel the
    # wall makes solid, so it may sit up to a voxel outside its room's boxes (never further)
    for pos, cls, r0, _ in P:
        if cls == APERTURE or not r0:
            continue
        if any(r == r0 and np.all(pos >= lo - VOXEL) and np.all(pos <= hi + VOXEL) for r, lo, hi in boxes):
            st['inbox'] += 1
        else:
            fails.append('the probe at %.0f,%.0f,%.0f (room %08x) stands in no box of its room' % (tuple(pos) + (r0,)))
            break
    for i, (ra, la, ha) in enumerate(boxes):
        for rb, lb, hb in boxes[i + 1:]:
            if ra != rb and np.all(np.minimum(ha, hb) - np.maximum(la, lb) > 0.5):
                fails.append('boxes of rooms %08x and %08x overlap' % (ra, rb))
                break
    return st


def run_bake(exe, soup, out, rays, threads, red, extra=(), rect=RECT):
    cmd = [os.path.abspath(exe), '-no-gui', 'probebake', '--soup', soup, '--rect', ','.join('%g' % v for v in rect),
           '--out', out, '--rays', str(rays), '--threads', str(threads)] + list(extra)
    if '--adapt' not in extra:   # lane SMOOTHN1: the exact re-trace is of the base ray set (the old bake)
        cmd += ['--adapt', '1']
    if red:
        cmd += ['--red', red]
    return subprocess.run(cmd, capture_output=True, text=True, timeout=900)


def files_in(d):
    return sorted(os.path.join(d, f) for f in os.listdir(d) if f.endswith('.tbk')) if os.path.isdir(d) else []


def fresh(out):
    for f in files_in(out):
        os.remove(f)
    return out


def same_files(da, db):
    fa, fb = files_in(da), files_in(db)
    if not fa or [os.path.basename(f) for f in fa] != [os.path.basename(f) for f in fb]:
        return False
    return all(open(a, 'rb').read() == open(b, 'rb').read() for a, b in zip(fa, fb))


def v3_leg(exe, base, soup, work, tag, rays, rect, tris, alb, physics, fails):
    """--tbk 3: FO4CS's own file, re-traced in the v3 model; with a base exe, byte-identical to it."""
    out = fresh(os.path.join(work, tag + '_v3'))
    rc = run_bake(exe, soup, out, rays, 0, '', ['--tbk', '3'], rect)
    if rc.returncode != 0:
        fails.append('--tbk 3: rc %d' % rc.returncode)
        return 'v3 not run'
    f3 = []
    tb = structure(files_in(out), f3)
    if any(t['ver'] != 3 for _, t in tb):
        f3.append('a file is not v3')
    retrace(tb, tris, alb, rays, f3, v4=False, physics=physics)
    budget_and_facing(tb, f3)
    fails += ['v3: ' + f for f in f3]
    same = ''
    if base:
        ob = fresh(os.path.join(work, tag + '_base'))
        rb = run_bake(base, soup, ob, rays, 0, '', (), rect)
        ok = rb.returncode == 0 and same_files(ob, out)
        if not ok:
            fails.append('--tbk 3 differs from the base exe\'s files')
        same = ', byte-identical to the base exe' if ok else ', NOT the base exe\'s bytes'
    return 'v3 leg %d files re-traced%s' % (len(tb), same)


def adapt_leg(exe, soup, work, tbks, fails):
    """lane SMOOTHN1: the default bake (noise-driven extra batches, turned at random, up to 16 x the base set).
    Not re-traced ray by ray (the turns are the bake's own); held to what must still be true: one thread and all
    threads write the same bytes, the files parse and keep the budget, the same probes in the same places, a sealed
    room's probe sees no sky through any extra batch, and every probe's sky per octant within 0.05 of the base set's.
    A quiet synthetic scene may raise no probe (then the bytes are the base set's); the real-cell check is prtp_reference.py."""
    outs = []
    for th in (1, 0):
        out = fresh(os.path.join(work, 'bake_synth_adapt_t%d' % th))
        rc = run_bake(exe, soup, out, 1024, th, '', ['--adapt', '16'])
        if rc.returncode != 0:
            fails.append('adapt: rc %d' % rc.returncode)
            return 'adapt not run'
        outs.append(out)
    fa = []
    if not same_files(outs[0], outs[1]):
        fa.append('1 thread and all threads wrote different files')
    ta = structure(files_in(outs[0]), fa)
    budget_and_facing(ta, fa)
    moved = room_sky = 0
    worst = 0.0
    if len(ta) != len(tbks):
        fa.append('files %d vs %d' % (len(ta), len(tbks)))
    for (_, t0), (_, t1) in zip(tbks, ta):
        if t0['probes']['pos'].tobytes() != t1['probes']['pos'].tobytes():
            fa.append('the probes moved')
            break
        s0, s1 = t0['probes']['sky'].astype(np.float64), t1['probes']['sky'].astype(np.float64)
        worst = max(worst, float(np.abs(s1 - s0).max()) if len(s0) else 0.0)
        moved += int(np.any(t0['links'].tobytes() != t1['links'].tobytes()))
        for pr in t1['probes']:
            if in_room(np.array(pr['pos'], np.float64)) and np.any(pr['sky'] != 0):
                room_sky += 1
    if room_sky:
        fa.append('%d room probes see sky' % room_sky)
    if worst > 0.05:
        fa.append('sky per octant moved %.3f from the base set' % worst)
    raised = not same_files(outs[0], os.path.join(work, 'bake_synth_t0'))
    fails += ['adapt: ' + f for f in fa]
    return 'adapt leg %d files (16x cap): threads byte-identical %s, sky per octant worst %.3f of the base set, '         'room probes with sky %d, %s' % (len(ta), 'yes' if not any('threads' in f for f in fa) else 'NO', worst, room_sky, 'some probes took extra batches' if raised else 'no probe took an extra batch (a quiet scene)')


def synth(exe, work, red, base):
    os.makedirs(work, exist_ok=True)
    tris, alb = scene()
    soup = os.path.join(work, 'bake_synth.psp')
    write_soup(soup, tris, alb)
    rays = 1024
    outs = []
    for th in (1, 0):
        out = fresh(os.path.join(work, 'bake_synth%s_t%d' % ('_red_' + red if red else '', th)))
        rc = run_bake(exe, soup, out, rays, th, red)
        if rc.returncode != 0:
            print('synth FAIL: probebake rc %d %s' % (rc.returncode, rc.stderr.strip()[:300]))
            return 1
        outs.append(out)
    fails = []
    fa = files_in(outs[0])
    if not fa:
        fails.append('no .tbk written')
    if not same_files(outs[0], outs[1]):
        fails.append('1 thread and all threads wrote different files')
    tbks = structure(fa, fails)
    if any(t['ver'] != 4 for _, t in tbks):
        fails.append('the default file is not v4')
    # the soup in float32, exactly as the bake reads it
    t32 = tris.astype(np.float32).astype(np.float64)
    st, masses = retrace(tbks, t32, alb, rays, fails)
    bf = budget_and_facing(tbks, fails, masses)
    if not st['side_n']:
        fails.append('no link reaches a back side: the scene tests nothing')
    # the interior rule (--no-sky): the same links, no sky anywhere, and the sky's weight unlinked instead
    ns_moved = 0
    v3 = ''
    if not red:
        out = fresh(os.path.join(work, 'bake_synth_nosky'))
        rc = run_bake(exe, soup, out, rays, 0, '', ['--no-sky'])
        fn = files_in(out)
        if rc.returncode != 0 or [os.path.basename(f) for f in fn] != [os.path.basename(f) for f in fa]:
            fails.append('--no-sky: rc %d, files %d vs %d' % (rc.returncode, len(fn), len(fa)))
        else:
            nbad = []
            for (_, t0), f1 in zip(tbks, fn):
                t1 = read_tbk(f1)
                if t0['links'].tobytes() != t1['links'].tobytes() or t0['surfels'].tobytes() != t1['surfels'].tobytes() \
                        or t0['back'].tobytes() != t1['back'].tobytes() or t0['lext'].tobytes() != t1['lext'].tobytes():
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
        v3 = '; ' + v3_leg(exe, base, soup, work, 'bake_synth', rays, RECT, t32, alb, True, fails)
        v3 += '; ' + adapt_leg(exe, soup, work, tbks, fails)
    unl = float(np.mean([float(pr['unl']) for _, t in tbks for pr in t['probes']])) if tbks else 0.0
    verdict = 'PASS' if not fails else 'FAIL'
    print('synth %s%s: v4 %d files, %d probes (%d ground, %d room) re-traced at %d rays, %d links (to a back side %d), '
          '%d link directions, %d albedo surfels, %d surfels modelled (back %d), %d cell links refused as turned away, '
          'unlinked mean %.4f, budget worst %.2g, --no-sky moved the sky of %d probes to unlinked%s; %s' % (
              verdict, ' [red ' + red + ']' if red else '', len(fa), st['probes'], st['ground'], st['room'], rays,
              bf['links'], st['side_n'], st['dirs'], st['alb_n'], st['sf_n'], st['back_n'], st['turned'], unl,
              bf['worst'], ns_moved, v3, '; '.join(fails[:6]) if fails else 'all as traced'))
    return 0 if not fails else 1


def rooms(exe, work, red, base):
    os.makedirs(work, exist_ok=True)
    tris, alb, doors, glass, rms, openings = scene2()
    soup = os.path.join(work, 'bake_rooms.psp')
    write_soup(soup, tris, alb, doors, glass)
    rays = 1024
    outs = []
    for th in ((1, 0) if not red else (0,)):
        out = fresh(os.path.join(work, 'bake_rooms%s_t%d' % ('_red_' + red if red else '', th)))
        rc = run_bake(exe, soup, out, rays, th, red, (), RECT2)
        if rc.returncode != 0:
            print('rooms FAIL: probebake rc %d %s' % (rc.returncode, rc.stderr.strip()[:300]))
            return 1
        outs.append(out)
    fails = []
    fa = files_in(outs[0])
    if len(outs) > 1 and not same_files(outs[0], outs[1]):
        fails.append('1 thread and all threads wrote different files')
    tbks = structure(fa, fails)
    if any(t['ver'] != 4 for _, t in tbks):
        fails.append('a file is not v4')
    t32 = tris.astype(np.float32).astype(np.float64)
    st, _ = retrace(tbks, t32, alb, rays, fails, v4=True, doors=doors, glass=glass, physics=False)
    bf = budget_and_facing(tbks, fails)
    rs = rooms_check(tbks, t32, rms, openings, fails)
    for k, what in (('side_n', 'a back side'), ('door_n', 'a door'), ('tint_n', 'glass'), ('skytint_n', 'sky through glass')):
        if not st[k]:
            fails.append('no link or probe reaches %s: the scene tests nothing' % what)
    v3 = ''
    if not red:
        v3 = '; ' + v3_leg(exe, base, soup, work, 'bake_rooms', rays, RECT2, t32, alb, False, fails)
    unl = float(np.mean([float(pr['unl']) for _, t in tbks for pr in t['probes']])) if tbks else 0.0
    verdict = 'PASS' if not fails else 'FAIL'
    print('rooms %s%s: v4 %d files, %d probes re-traced at %d rays with %d doors and %d glass panes, %d links '
          '(back side %d, through a door %d, tinted %d), %d probe-octants see sky through glass (sphere share mean %.3f), '
          '%d surfels modelled (back %d), unlinked mean %.4f; rooms: %d probes named in %d known rooms, %d under open sky '
          'name none, %d openings name both sides, %d room boxes hold %d probes%s; %s' % (
              verdict, ' [red ' + red + ']' if red else '', len(fa), st['probes'], rays, len(doors), len(glass),
              bf['links'], st['side_n'], st['door_n'], st['tint_n'], st['skytint_n'], st['glass_share'], st['sf_n'],
              st['back_n'], unl, rs['named'], len(rms), rs['outdoors'], rs['openings'], rs['boxes'], rs['inbox'], v3,
              '; '.join(fails[:6]) if fails else 'all as traced'))
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
    nb = sum(len(t['back']) for _, t in tbks)
    vers = sorted(set(t['ver'] for _, t in tbks))
    # per probe, pooled over the files (a mean of file means weighs a 9-probe file like a 500-probe one)
    sky = [float(np.mean(pr['sky'])) for _, t in tbks for pr in t['probes']]
    unl = [float(pr['unl']) for _, t in tbks for pr in t['probes']]
    verdict = 'PASS' if not fails else 'FAIL'
    print('check %s: v%s, %d files, %d probes, %d surfels (back %d), %d links (back side %d, door %d, tinted %d), '
          '%d facing away, budget worst %.3f (octants as 1/8), sky mean %.3f, unlinked mean %.4f; %s' % (
              verdict, '/'.join(str(v) for v in vers), len(fs), bf['probes'], ns, nb, bf['links'], bf['back'], bf['door'],
              bf['tinted'], bf['facing_bad'], bf['worst'], float(np.mean(sky)) if sky else 0.0,
              float(np.mean(unl)) if unl else 0.0, '; '.join(fails[:6]) if fails else 'sound'))
    return 0 if not fails else 1


if __name__ == '__main__':
    a = sys.argv[1:]
    opt = (lambda k: a[a.index(k) + 1] if k in a else '')
    if a and a[0] == 'synth':
        sys.exit(synth(a[1], a[2], opt('--red'), opt('--base')))
    if a and a[0] == 'rooms':
        sys.exit(rooms(a[1], a[2], opt('--red'), opt('--base')))
    if a and a[0] == 'check':
        sys.exit(check(a[1]))
    print(__doc__)
    sys.exit(2)
