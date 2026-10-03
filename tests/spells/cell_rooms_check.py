#!/usr/bin/env python3
"""Lane ROOMCLAMP1 (2026-10-03) checker: load doors shut, probes outside the shell. Independent of NifSkope:
the plugin is read here, every ray is traced here through the run's own soup.psp (numpy, every triangle).

  doors <esm> <run> --cell <EDID> [--eye x,y,z]
        Every DOOR reference of the cell with a teleport (XTEL), read from the plugin here, must be in the soup
        as solid (WW_CELL_PROBE_SOUP_REFS, role 1, not 2 = an opening box); and from the probe nearest each
        load door (inside 900 units) a fan of rays into the door's doorway must meet geometry: no ray escapes
        into the void; --eye: from that point (CAPTURE1's Museum cube probe), no ray in a load door's cone (its
        half size 160 at its distance) escapes ("the light-blue patch" of the capture cube). Red WW_CELL_PROBE_LOADDOOR_RED=open FAILS.
  back <run> [--ref <run before the lane>] [--sample n]
        P  the placer: no probe stands on (first hit straight down within PLACE_REACH) the BACK of a one-sided
           face. Red WW_PROBE_RED=floor FAILS.
        B  the bake: every probe's back-face share (rays whose first hit is a one-sided face seen from behind)
           re-traced here on a sample agrees with the bake's list (back.tsv, WW_CELL_PROBE_BACKDUMP); every
           probe the bake kept has a share <= BACK_MAX (this file's, not the run's); moved probes re-traced at
           their new point are under it too. Reds WW_PROBE_BAKE_RED=backface and WW_CELL_PROBE_BACKMAX=1 FAIL.
        R  roof tops: probes with nothing above and a one-sided face's back straight below = 0.
        The run before the lane (--ref) is classified the same way against this run's soup, for the report.
        A probe whose re-traced share differs past SHARE_TOL is let off by its tied rays (coincident faces).
  synth <exe> <work> [--red noclamp|conn26|boxes|glasswall]
        Room labels (part C): five scenes built here (SCENES), each through `<exe> -no-gui probegi`; the labels
        (gi_rooms.bin) against each scene's own rooms (same room one label, rooms apart, outdoors 0), the panes and
        the hatch naming both sides; the 2-unit wall: cellGiRoomSample redone here (gi_sample) on the dark side
        reads no more than the dark room's own brightest probe face, and every sample has weight.
One verdict line per subcommand last; exit 0 on PASS, 1 on FAIL.
"""
import math
import os
import struct
import sys
import zlib

import numpy as np

SOUP_MAGIC, TWO_MAGIC, ALB_MAGIC, GLS_MAGIC = 0x31505350, 0x314F5754, 0x31424C41, 0x31534C47
BACK_MAX = 0.25        # the gate's own bar (src/probebake.h's default, chosen from the Museum histogram)
PLACE_REACH = 200.0    # the placer puts its probes at eye height 120 over the face they stand on
SHARE_TOL = 0.02       # 256 rays, same Fibonacci set: only ties at a triangle edge may differ
DOOR_REACH = 900.0
DOOR_HALF = 160.0      # the eye cone takes a door's half size at its distance (no wider than 25 degrees)


# ---------------------------------------------------------------- soup
def read_soup(path):
    b = open(path, 'rb').read()
    magic, ntri, ndoor = struct.unpack_from('<III', b, 0)
    assert magic == SOUP_MAGIC, 'not a PSP1 soup'
    p = 12
    tris = np.frombuffer(b, '<f4', ntri * 9, p).reshape(ntri, 3, 3).astype(np.float64)
    p += ntri * 36 + ndoor * 28
    two = np.zeros(ntri, bool)
    while p + 8 <= len(b):
        m, n = struct.unpack_from('<II', b, p)
        p += 8
        if m == ALB_MAGIC:
            p += n * 3
        elif m == GLS_MAGIC:
            p += n * 36 + n * 3
        elif m == TWO_MAGIC and n == ntri:
            two = np.frombuffer(b, 'u1', n, p).astype(bool)
            p += n
        else:
            break
    return tris, two


class Tracer:
    def __init__(self, tris, two):
        self.v0 = tris[:, 0]
        self.e1 = tris[:, 1] - tris[:, 0]
        self.e2 = tris[:, 2] - tris[:, 0]
        self.n = np.cross(self.e1, self.e2)
        self.two = two
        self.lo = tris.min(axis=1)
        self.hi = tris.max(axis=1)

    def first(self, o, dirs, sel=None, chunk=6):
        """First hit of each ray from o: (t, tri) with t = inf, tri = -1 for a miss."""
        idx = np.arange(len(self.v0)) if sel is None else sel
        v0, e1, e2 = self.v0[idx], self.e1[idx], self.e2[idx]
        s = o - v0
        q = np.cross(s, e1)
        tb = np.full(len(dirs), np.inf)
        ib = np.full(len(dirs), -1)
        for a in range(0, len(dirs), chunk):
            d = dirs[a:a + chunk]
            pv = np.cross(d[:, None, :], e2[None])
            det = (pv * e1[None]).sum(-1)
            ok = np.abs(det) > 1e-12
            inv = np.where(ok, 1.0 / np.where(ok, det, 1.0), 0.0)
            u = (pv * s[None]).sum(-1) * inv
            v = (d[:, None, :] * q[None]).sum(-1) * inv
            t = (q[None] * e2[None]).sum(-1) * inv
            hit = ok & (u >= 0) & (v >= 0) & (u + v <= 1) & (t > 1e-4)
            t = np.where(hit, t, np.inf)
            j = t.argmin(axis=1)
            tb[a:a + chunk] = t[np.arange(len(d)), j]
            ib[a:a + chunk] = np.where(np.isfinite(tb[a:a + chunk]), idx[j], -1)
        return tb, ib

    def back(self, tri, d):
        """A hit on the back of a one-sided face (winding normal along the ray)."""
        return tri >= 0 and not self.two[tri] and float(self.n[tri] @ d) > 0

    def column(self, p):
        """Triangles whose xy box holds p (for the vertical rays)."""
        return np.nonzero((self.lo[:, 0] <= p[0]) & (self.hi[:, 0] >= p[0]) &
                          (self.lo[:, 1] <= p[1]) & (self.hi[:, 1] >= p[1]))[0]

    def vertical(self, p):
        """(t, tri) down and up. A tie (coplanar twins, or a ray on a shared edge) names a front face when any
        of the tied faces is one: the tracer may meet either, so only an all-back tie counts as a back."""
        sel = self.column(p)
        if len(sel) == 0:
            return (np.inf, -1), (np.inf, -1)
        o = np.asarray(p, float)
        v0, e1, e2 = self.v0[sel], self.e1[sel], self.e2[sel]
        s = o - v0
        q = np.cross(s, e1)
        out = []
        for d in (np.array([0, 0, -1.0]), np.array([0, 0, 1.0])):
            pv = np.cross(d, e2)
            det = (pv * e1).sum(-1)
            ok = np.abs(det) > 1e-12
            inv = np.where(ok, 1.0 / np.where(ok, det, 1.0), 0.0)
            u, w, t = (pv * s).sum(-1) * inv, (q @ d) * inv, (q * e2).sum(-1) * inv
            t = np.where(ok & (u >= -1e-6) & (w >= -1e-6) & (u + w <= 1 + 1e-6) & (t > 1e-4), t, np.inf)
            k = int(t.argmin())
            if not np.isfinite(t[k]):
                out.append((np.inf, -1))
                continue
            tied = sel[np.abs(t - t[k]) < 0.05]
            front = [j for j in tied if not self.back(j, d)]
            out.append((t[k], front[0] if front else sel[k]))
        return out[0], out[1]


def fib(n):
    i = np.arange(n)
    z = 1 - (2 * i + 1) / n
    r = np.sqrt(np.maximum(0, 1 - z * z))
    ph = i * math.pi * (3 - math.sqrt(5))
    return np.stack([r * np.cos(ph), r * np.sin(ph), z], 1)


def first_near(T, o, dirs, R=600.0):
    """first() in two steps, the same answer: the faces whose box comes within R of o settle every ray that
    meets one of them by R (a face beyond R is wholly farther); the rest go through every face."""
    gap = np.maximum(0, np.maximum(T.lo - o, o - T.hi))
    sel = np.nonzero(np.sum(gap * gap, 1) <= R * R)[0]
    t, i = T.first(o, dirs, sel) if len(sel) else (np.full(len(dirs), np.inf), np.full(len(dirs), -1))
    rest = np.nonzero(t > R)[0]
    if len(rest):
        t2, i2 = T.first(o, dirs[rest])
        t[rest], i[rest] = t2, i2
    return t, i


def share(T, p, dirs):
    t, i = first_near(T, np.asarray(p, float), dirs)
    hit = i >= 0
    back = np.zeros(len(dirs), bool)
    back[hit] = (~T.two[i[hit]]) & ((T.n[i[hit]] * dirs[hit]).sum(-1) > 0)
    return back.mean()


def tie_share(tris, two, o, dirs, eps=0.5):
    """The rays whose first hits within `eps` of each other disagree (a one-sided face's back and another face at
    the same place: coincident shells): the share there is the order of a tie, either answer is right."""
    v0 = tris[:, 0]
    e1, e2 = tris[:, 1] - v0, tris[:, 2] - v0
    n = np.cross(e1, e2)
    n /= np.linalg.norm(n, axis=1, keepdims=True) + 1e-30
    tv = o - v0
    qv = np.cross(tv, e1)
    ties = 0
    for d in dirs:
        pv = np.cross(d, e2)
        det = (e1 * pv).sum(1)
        ok = np.abs(det) > 1e-12
        inv = np.where(ok, 1.0 / np.where(ok, det, 1.0), 0.0)
        u = (tv * pv).sum(1) * inv
        v = (qv @ d) * inv
        t = (e2 * qv).sum(1) * inv
        idx = np.nonzero(ok & (u >= 0) & (u <= 1) & (v >= 0) & (u + v <= 1) & (t > 1e-4))[0]
        if len(idx) < 2:
            continue
        t0 = t[idx].min()
        near = idx[t[idx] - t0 < eps]
        ties += len({bool((not two[j]) and n[j] @ d > 0) for j in near}) > 1
    return ties / len(dirs)


# ---------------------------------------------------------------- probes
def read_probes(path):
    out = []
    for l in open(path):
        if l.startswith('#') or l.startswith('id\t'):
            continue
        c = l.rstrip('\n').split('\t')
        out.append((int(c[0]), c[1], np.array([float(c[3]), float(c[4]), float(c[5])])))
    return out


def read_back(path):
    rows, bm = [], None
    for l in open(path):
        if l.startswith('#'):
            if 'backMax' in l:
                bm = float(l.split('backMax')[1].split()[0])
            continue
        c = l.rstrip('\n').split('\t')
        rows.append(dict(i=int(c[0]), p=np.array([float(c[1]), float(c[2]), float(c[3])]), share=float(c[4]),
                         fate=c[5], q=np.array([float(c[6]), float(c[7]), float(c[8])])))
    return rows, bm


def on_back(T, p):
    (td, idn), _ = T.vertical(p)
    return td <= PLACE_REACH and T.back(idn, np.array([0, 0, -1.0]))


def roof_top(T, p):
    (td, idn), (tu, iu) = T.vertical(p)
    return iu < 0 and T.back(idn, np.array([0, 0, -1.0]))


def back_cmd(run, ref, sample):
    tris, two = read_soup(os.path.join(run, 'soup.psp'))
    T = Tracer(tris, two)
    fails, lines = [], []
    if not two.any():
        fails.append('the soup carries no two-sided tail (TWO1)')
    # P
    placed = read_probes(os.path.join(run, 'probes.tsv'))
    onb = [(i, c, p) for i, c, p in placed if on_back(T, p)]
    lines.append('P placer: %d probes, %d stand on the back of a one-sided face%s' % (
        len(placed), len(onb), (' (' + ', '.join('%d %s %.0f,%.0f,%.0f' % (i, c, *p) for i, c, p in onb[:6]) + ')') if onb else ''))
    if onb:
        fails.append('P: %d placer probes on a back face' % len(onb))
    # B
    rows, bm = read_back(os.path.join(run, 'back.tsv'))
    hist = np.histogram([r['share'] for r in rows], bins=20, range=(0, 1))[0]
    over = [r for r in rows if r['share'] > BACK_MAX]
    keptover = [r for r in over if r['fate'] == 'kept']
    lines.append('B bake list: %d probes, rule threshold %s; share histogram (0.05 steps) %s; over %.2f: %d (moved %d, '
                 'dropped %d, KEPT %d)' % (len(rows), bm, ' '.join(map(str, hist)), BACK_MAX, len(over),
                                           sum(r['fate'] == 'moved' for r in over), sum(r['fate'] == 'dropped' for r in over),
                                           len(keptover)))
    if keptover:
        fails.append('B: %d probes over %.2f kept where they stand' % (len(keptover), BACK_MAX))
    if not any(r['share'] > BACK_MAX for r in rows):
        fails.append('B: no probe over the bar at all (nothing measured the rule)')
    dirs = fib(256)
    rng = np.random.default_rng(7)
    pick = sorted(over, key=lambda r: -r['share'])[:sample // 2]
    rest = [r for r in rows if r not in pick]
    pick += [rest[k] for k in rng.choice(len(rest), min(len(rest), sample - len(pick)), replace=False)]
    worst, movedBad, tied = 0.0, [], 0
    for r in pick:
        s = share(T, r['p'], dirs)
        dv = abs(s - r['share'])
        if dv > SHARE_TOL:     # lane ROOMCLAMP1: a coincident shell's rays may go either way; past them it counts
            tv = tie_share(tris, two, r['p'], dirs)
            tied += tv > 0
            dv = max(0.0, dv - tv)
        worst = max(worst, dv)
        if r['fate'] == 'moved':
            sm = share(T, r['q'], dirs)
            if sm > BACK_MAX:
                movedBad.append((r['i'], sm))
    lines.append('B re-trace: %d probes (%d over the bar first) at 256 rays, worst share difference %.4f past the tied rays '
                 '(%d probes with coincident faces); moved probes re-traced at their new point over %.2f: %d'
                 % (len(pick), min(len(over), sample // 2), worst, tied, BACK_MAX, len(movedBad)))
    if worst > SHARE_TOL:
        fails.append('B: share re-trace differs by %.3f' % worst)
    if movedBad:
        fails.append('B: %d moved probes still over the bar' % len(movedBad))
    # R
    final = [r['q'] if r['fate'] == 'moved' else r['p'] for r in rows if r['fate'] != 'dropped']
    roofs = [p for p in final if roof_top(T, p)]
    lines.append('R roof tops (nothing above, a one-sided back below): %d of %d final probes' % (len(roofs), len(final)))
    if roofs:
        fails.append('R: %d roof-top probes' % len(roofs))
    if ref:
        rp = read_probes(os.path.join(ref, 'probes.tsv'))
        rr = sum(roof_top(T, p) for _, _, p in rp)
        rb = sum(on_back(T, p) for _, _, p in rp)
        lines.append('ref (before the lane, %d probes, classified on this soup): roof tops %d, on a back face %d'
                     % (len(rp), rr, rb))
    for l in lines:
        print(l)
    print('back %s%s' % ('PASS' if not fails else 'FAIL', (': ' + '; '.join(fails)) if fails else ''))
    return 0 if not fails else 1


# ---------------------------------------------------------------- plugin (doors)
def records(data, off, end, out, want_cell):
    """Walk groups; collect DOOR form ids, the interior cell by EDID and every cell-children REFR."""
    while off < end:
        typ, size = struct.unpack_from('<4sI', data, off)
        if typ == b'GRUP':
            label, gtype = struct.unpack_from('<4sI', data, off + 8)
            if gtype == 0 and label not in (b'DOOR', b'CELL'):
                off += size
                continue
            out['grp'].append((gtype, struct.unpack_from('<I', label)[0]))
            records(data, off + 24, off + size, out, want_cell)
            out['grp'].pop()
            off += size
            continue
        flags, fid = struct.unpack_from('<II', data, off + 8)
        body = data[off + 24:off + 24 + size]
        if flags & 0x00040000:
            body = zlib.decompress(body[4:])
        if typ in (b'DOOR', b'CELL', b'REFR'):
            f, b = {}, 0
            while b + 6 <= len(body):
                ft, n = struct.unpack_from('<4sH', body, b)
                f.setdefault(ft, body[b + 6:b + 6 + n])
                b += 6 + n
            if typ == b'DOOR':
                out['doors'].add(fid)
            elif typ == b'CELL' and f.get(b'EDID', b'').split(b'\0')[0] == want_cell:
                out['cell'] = fid
            elif typ == b'REFR' and out['grp'] and out['grp'][-1][0] in (8, 9, 10) and len(out['grp']) >= 2:
                parent = out['grp'][-2][1] if out['grp'][-2][0] == 6 else None
                if parent is not None and b'NAME' in f:
                    pos = struct.unpack('<6f', f[b'DATA'][:24]) if b'DATA' in f else (0,) * 6
                    out['refs'].setdefault(parent, []).append(
                        (fid, struct.unpack('<I', f[b'NAME'][:4])[0], b'XTEL' in f, pos[:3]))
        off += 24 + size


def door_cmd(esm, run, cell, eye=''):
    data = open(esm, 'rb').read()
    out = dict(doors=set(), cell=None, refs={}, grp=[])
    hsz = struct.unpack_from('<I', data, 4)[0]
    records(data, 24 + hsz, len(data), out, cell.encode())
    fails, lines = [], []
    if out['cell'] is None:
        print('doors FAIL: no cell %s' % cell)
        return 1
    refs = out['refs'].get(out['cell'], [])
    doors = [r for r in refs if r[1] in out['doors']]
    load = [r for r in doors if r[2]]
    roles = {}
    for l in open(os.path.join(run, 'refs.tsv')):
        c = l.split('\t')
        if len(c) >= 2:
            roles[int(c[0], 16)] = int(c[1])
    lines.append('plugin: %s %08X, %d references, %d doors, %d with a teleport (load doors)' % (
        cell, out['cell'], len(refs), len(doors), len(load)))
    notsolid = [r for r in load if roles.get(r[0]) != 1]
    lines.append('soup: load doors solid %d of %d%s; ordinary doors as openings %d of %d' % (
        len(load) - len(notsolid), len(load), (' (not: ' + ', '.join('%08X role %s' % (r[0], roles.get(r[0])) for r in notsolid) + ')') if notsolid else '',
        sum(roles.get(r[0]) == 2 for r in doors if not r[2]), sum(1 for r in doors if not r[2] and r[0] in roles)))
    if not load:
        fails.append('no load door in this cell (nothing measured)')
    if notsolid:
        fails.append('%d load doors not solid in the soup' % len(notsolid))
    # the doorway fan
    tris, two = read_soup(os.path.join(run, 'soup.psp'))
    T = Tracer(tris, two)
    rows, _ = read_back(os.path.join(run, 'back.tsv')) if os.path.exists(os.path.join(run, 'back.tsv')) else ([], None)
    probes = [r['q'] if r['fate'] == 'moved' else r['p'] for r in rows if r['fate'] != 'dropped'] or \
        [p for _, _, p in read_probes(os.path.join(run, 'probes.tsv'))]
    P = np.array(probes)
    for fid, base, _, pos in load:
        c = np.array(pos) + np.array([0, 0, 100.0])        # a door's origin is at its foot: aim at its middle
        dd = np.linalg.norm(P - c, axis=1)
        k = int(dd.argmin())
        if dd[k] > DOOR_REACH:
            lines.append('door %08X: no probe within %.0f (nearest %.0f)' % (fid, DOOR_REACH, dd[k]))
            continue
        o = P[k]
        axis = (c - o) / np.linalg.norm(c - o)
        # a fan of 81 rays inside a 12-degree cone round the door's middle
        up = np.array([0, 0, 1.0]) if abs(axis[2]) < 0.9 else np.array([1.0, 0, 0])
        a1 = np.cross(axis, up); a1 /= np.linalg.norm(a1)
        a2 = np.cross(axis, a1)
        g = np.linspace(-1, 1, 9) * math.tan(math.radians(12))
        dirs = np.array([axis + x * a1 + y * a2 for x in g for y in g])
        dirs /= np.linalg.norm(dirs, axis=1)[:, None]
        t, i = first_near(T, o, dirs)
        esc = int((i < 0).sum())
        lines.append('door %08X at %.0f,%.0f,%.0f: probe %d at %.0f,%.0f,%.0f (%.0f away), %d of 81 doorway rays escape '
                     'into the void' % (fid, *pos, k, *o, dd[k], esc))
        if esc:
            fails.append('door %08X: %d doorway rays escape' % (fid, esc))
    if eye:
        # the capture cube's eye (CAPTURE1's Museum probe): every ray of a 16384-ray sphere within the door's cone of
        # a load door that escapes into the void is the cube view's light-blue patch
        o = np.array([float(v) for v in eye.split(',')])
        sph = fib(16384)
        for fid, base, _, pos in load:
            c = np.array(pos) + np.array([0, 0, 100.0])
            ax = (c - o) / np.linalg.norm(c - o)
            half = min(25.0, math.degrees(math.atan(DOOR_HALF / np.linalg.norm(c - o))))   # the door's own size
            dirs = sph[sph @ ax > math.cos(math.radians(half))]
            t, i = first_near(T, o, dirs)
            esc = int((i < 0).sum())
            lines.append('eye %.0f,%.0f,%.0f -> door %08X (%.0f away): %d of %d rays in a %.1f-degree cone escape '
                         '(the patch: %.4f sr)' % (*o, fid, np.linalg.norm(c - o), esc, len(dirs), half, esc * 4 * math.pi / 16384))
            if esc:
                fails.append('eye: %d rays escape at door %08X' % (esc, fid))
    for l in lines:
        print(l)
    print('doors %s%s' % ('PASS' if not fails else 'FAIL', (': ' + '; '.join(fails)) if fails else ''))
    return 0 if not fails else 1


# ---------------------------------------------------------------- synth (part C: room labels)
# Every scene is built here, its rooms known here by construction; NifSkope's probegi command places, bakes and
# relights it and dumps the labels (gi_rooms.bin) and the two-slot grid (gi_grid.bin, gi_slots.bin). The shader's
# blend (res/shaders/cell_lights.glsl cellGiRoomSample, and the plain trilinear one without rooms) is redone here.
LEAK_TOL = 1.02      # the dark side of a shared wall reads no more than the dark room's own brightest probe face
COVER_MIN = 0.01     # cellGiE's own floor: a sample with less weight paints magenta in the pass view
QUAD = lambda a, b, c, d: [(a, b, c), (a, c, d)]


def read_rooms(path):
    b = open(path, 'rb').read()
    o = struct.unpack_from('<4f', b, 0)
    d = struct.unpack_from('<4i', b, 16)
    n = d[0] * d[1] * d[2]
    ab = np.frombuffer(b, '<i2', 2 * n, 32).reshape(d[2], d[1], d[0], 2)
    return {'origin': np.array(o[:3]), 'cell': o[3], 'dims': d[:3], 'rooms': d[3], 'ab': ab}


def read_grid(path, slots=None):
    b = open(path, 'rb').read()
    h = struct.unpack_from('<5f', b, 0)
    d = struct.unpack_from('<3i', b, 20)
    nv = d[0] * d[1] * d[2]
    g = {'origin': np.array(h[:3]), 'voxel': h[3], 'dims': d, 'nv': nv,
         'g': np.frombuffer(b, '<f4', 6 * nv * 4, 32).reshape(6, d[2], d[1], d[0], 4)}
    if slots and os.path.exists(slots):
        s = open(slots, 'rb').read()
        lab = np.frombuffer(s, '<i4', 2 * nv, 32).reshape(d[2], d[1], d[0], 2)
        g2 = np.frombuffer(s, '<f4', 6 * nv * 4, 32 + 8 * nv).reshape(6, d[2], d[1], d[0], 4)
        g['slots'], g['g2'] = lab, g2
    return g


def room_at(R, q):
    c = np.floor((np.asarray(q, float) - R['origin']) / R['cell']).astype(int)
    if (c < 0).any() or (c >= np.array(R['dims'])).any():
        return (-1, -1)
    a, b = R['ab'][c[2], c[1], c[0]]
    return (int(a), int(b))


def surface_room(R, P, N):
    """A surface's room: at P + N x 0.75 cell, else 1.75 cell, else the air cell beside either read along the surface
    (the two axes off the normal's largest) whose face is nearest its read, the 0.75 read first on a tie (a floor
    beside an outer wall: the 0.75 read's only air neighbour is the outdoors across the wall, the 1.75 read's nearer
    face the room). None: no room."""
    L, cand = surface_reads(R, P, N)
    if L is not None:
        return L
    best = 3.0
    for d, c in cand:
        if d < best:
            best, L = d, c
    return L


def surface_reads(R, P, N):
    """The direct read (P + N x 0.75 cell, else 1.75; None in a wall's cells) and the fallback's candidates (the shader
    also searches them when the direct room's blend has no weight) in the rule's order: (face distance, the air cell's rooms), the 0.75 read's neighbours first. The
    neighbours: the 8 cells round the read in the plane of the two axes off the normal's largest (an inner corner's
    room is the diagonal one); the distance: the sum over the steps of the read's distance to the face crossed."""
    P, N = np.asarray(P, float), np.asarray(N, float)
    L = room_at(R, P + N * 0.75 * R['cell'])
    if L[0] < 0:
        L = room_at(R, P + N * 1.75 * R['cell'])
    L = L if L[0] >= 0 else None
    m = int(np.argmax(np.abs(N)))
    ta, tb = [t for t in range(3) if t != m]
    cand = []
    for k in (0.75, 1.75):
        q = P + N * k * R['cell']
        u = (q - R['origin']) / R['cell']
        f = u - np.floor(u)
        for sa in (-1, 0, 1):
            for sb in (-1, 0, 1):
                if sa == 0 and sb == 0:
                    continue
                e = np.zeros(3)
                e[ta], e[tb] = sa, sb
                c = room_at(R, q + e * R['cell'])
                if c[0] >= 0:
                    d = sum((1.0 - f[t] if s > 0 else f[t]) if s else 0.0 for t, s in ((ta, sa), (tb, sb)))
                    cand.append((d, c))
    return L, cand


def gi_sample(G, R, P, N):
    """cellGiSample: with rooms the blend of the 8 voxels' slots of the surface's room (renormalized); without
    (R None) the hardware trilinear of the one grid. Returns rgba (cellGiE divides by a)."""
    P, N = np.asarray(P, float), np.asarray(N, float)
    dm = G['dims']
    g = (P + N * 0.5 * G['voxel'] - G['origin']) / G['voxel'] - 0.5
    i0 = np.floor(g).astype(int)
    f = g - i0
    sl = (0 if N[0] >= 0 else 1, 2 if N[1] >= 0 else 3, 4 if N[2] >= 0 else 5)
    n2 = N * N
    if R is not None:
        # the shader (lane ROOMCLAMP1): the read room's blend; without weight (or no read: a wall's cells) of the read's
        # air neighbours the nearest whose blend has weight (an inner corner's nearest air is the outdoors across the
        # wall, which no voxel there holds; a pocket behind a pipe no voxel holds); none: the plain trilinear
        L, cand = surface_reads(R, P, N)
        if L is not None:
            s, ws = gi_blend(G, L, i0, f, sl, n2)
            if ws > 0:
                return s / ws
        best, out = 3.0, None
        for d, c in cand:
            if d >= best:
                continue
            s, ws = gi_blend(G, c, i0, f, sl, n2)
            if ws > 0:
                best, out = d, s / ws
        if out is not None:
            return out
        GI_PLAIN[0] += 1
    s, ws = gi_blend(G, None, i0, f, sl, n2)
    return s


RED_EMPTY = [False]   # red emptyslot: the twin weighs a room's empty slots (the old shader: black, magenta)
GI_PLAIN = [0]     # samples that found rooms but no room with weight: the shader's plain (unclamped) trilinear


def gi_blend(G, L, i0, f, sl, n2):
    """The 8 voxels' slots of room L (None: the one grid, the hardware trilinear). Returns (sum, weight)."""
    dm = G['dims']
    s, ws = np.zeros(4), 0.0
    for k in range(8):
        o = np.array([k & 1, (k >> 1) & 1, (k >> 2) & 1])
        c = np.clip(i0 + o, 0, np.array(dm) - 1)
        w = float(np.prod(np.where(o == 1, f, 1.0 - f)))
        src = G['g']
        if L is not None:
            S = G['slots'][c[2], c[1], c[0]]
            if S[0] >= 0 and S[0] in L:
                src = G['g']
            elif S[1] >= 0 and S[1] in L:
                src = G['g2']
            else:
                continue
        if w <= 0:
            continue
        v = sum(n2[a] * src[sl[a], c[2], c[1], c[0]] for a in range(3))
        if L is not None and v[3] <= 0 and not RED_EMPTY[0]:
            continue   # lane ROOMCLAMP1: a slot no probe of its room reached (empty): no weight
        s += w * v
        ws += w
    return s, ws


def gi_E(G, R, P, N):
    s = gi_sample(G, R, P, N)
    return (max(s[0] / s[3], 0.0) if s[3] > COVER_MIN else -1.0), s[3]


def sc_wall():
    """Two rooms behind one 2-unit wall: A (x 0..600) lit, B (x 602..1202) dark, no opening anywhere."""
    T = box(0, 0, 0, 1202, 600, 300) + box(600, 0, 0, 602, 600, 300)
    pts = [('A', (300, 300, 150)), ('A', (560, 100, 40)), ('B', (900, 300, 150)), ('B', (640, 500, 260)),
           ('out', (600, 300, 360)), ('out', (-60, 300, 150)), ('out', (1260, 300, 150))]
    return dict(T=T, rect=(-100, -100, 1300, 700), light='300,300,220,900,1,1,1', pts=pts)


def sc_l():
    """An L-shaped room (one room round its corner), the notch outdoors; and beside it two closets of 2 x 2 fine
    cells (cubes snapped inside the cells: the anchor triangle fixes the grid) that touch only along an edge:
    6-connected two rooms, 26-connected one."""
    T = []
    H = 300
    P = [(0, 0), (800, 0), (800, 300), (300, 300), (300, 800), (0, 800)]
    for i in range(6):
        (x0, y0), (x1, y1) = P[i], P[(i + 1) % 6]
        T += QUAD((x0, y0, 0), (x1, y1, 0), (x1, y1, H), (x0, y0, H))
    T += QUAD((0, 0, 0), (800, 0, 0), (800, 300, 0), (0, 300, 0)) + QUAD((0, 300, 0), (300, 300, 0), (300, 800, 0), (0, 800, 0))
    T += QUAD((0, 0, H), (800, 0, H), (800, 300, H), (0, 300, H)) + QUAD((0, 300, H), (300, 300, H), (300, 800, H), (0, 800, H))
    ax, ay, az = -200.0, -200.0, -32.0
    T.append(((ax, ay, az), (ax + 1, ay, az), (ax, ay + 1, az)))   # the anchor: the soup's min corner
    cs = 16.0
    bx, by, bz = 45, 45, 6        # the closets' first cell (x 520.., y 520.., z 64..)
    cav = set()
    for kz in range(6):
        for k in ((0, 0), (0, 1), (1, 0), (1, 1)):
            cav.add((k[0], k[1], kz))
            cav.add((k[0] + 2, k[1] + 2, kz))
    shell = set()
    for (i, j, k) in cav:
        for di in (-1, 0, 1):
            for dj in (-1, 0, 1):
                for dk in (-1, 0, 1):
                    q = (i + di, j + dj, k + dk)
                    if q not in cav:
                        shell.add(q)
    for (i, j, k) in sorted(shell):
        x0, y0, z0 = ax + (bx + i) * cs + 2, ay + (by + j) * cs + 2, az + (bz + k) * cs + 2
        T += box(x0, y0, z0, x0 + 12, y0 + 12, z0 + 12)
    cA = (ax + (bx + 1) * cs + 8, ay + (by + 1) * cs + 8, az + (bz + 3) * cs + 8)     # A's cell at the shared edge
    cB = (ax + (bx + 2) * cs + 8, ay + (by + 2) * cs + 8, az + (bz + 3) * cs + 8)
    pts = [('L', (150, 150, 150)), ('L', (700, 150, 150)), ('L', (150, 700, 150)), ('L', (280, 280, 280)),
           ('out', (720, 720, 250)), ('out', (450, 700, 150)), ('out', (700, 450, 280)),
           ('closetA', cA), ('closetB', cB)]
    return dict(T=T, rect=(-100, -100, 900, 900), light='150,150,220,900,1,1,1', pts=pts)


def sc_church():
    """An A-frame nave: walls to 300, a pitched roof to the ridge at 600; the air under the ridge is the nave's."""
    T = []
    X1, W, E, R = 1200, 600, 300, 600
    T += QUAD((0, 0, 0), (X1, 0, 0), (X1, W, 0), (0, W, 0))
    T += QUAD((0, 0, 0), (X1, 0, 0), (X1, 0, E), (0, 0, E)) + QUAD((0, W, 0), (X1, W, 0), (X1, W, E), (0, W, E))
    T += QUAD((0, 0, E), (X1, 0, E), (X1, W / 2, R), (0, W / 2, R)) + QUAD((0, W, E), (X1, W, E), (X1, W / 2, R), (0, W / 2, R))
    for x in (0, X1):
        T += QUAD((x, 0, 0), (x, W, 0), (x, W, E), (x, 0, E))
        T.append(((x, 0, E), (x, W, E), (x, W / 2, R)))
    pts = [('nave', (300, 300, 150)), ('nave', (900, 300, 150)), ('nave', (600, 300, 560)), ('nave', (600, 300, 420)),
           ('nave', (40, 40, 40)), ('out', (600, 60, 560)), ('out', (600, 540, 560)), ('out', (600, 300, 700)),
           ('out', (-80, 300, 300))]
    return dict(T=T, rect=(-100, -100, 1300, 700), light='600,300,250,1200,1,1,1', pts=pts)


def sc_lighthouse():
    """A round tower (24 sides, radius 250) of two floors; the slab between them has a 64-unit hatch (narrower than
    two pinches): two rooms, the hatch an opening naming both."""
    T = []
    n, r, Z1, Z2, S = 24, 250.0, 400.0, 800.0, 2.0
    ring = [(r * math.cos(2 * math.pi * i / n), r * math.sin(2 * math.pi * i / n)) for i in range(n)]
    for i in range(n):
        (x0, y0), (x1, y1) = ring[i], ring[(i + 1) % n]
        T += QUAD((x0, y0, 0), (x1, y1, 0), (x1, y1, Z2), (x0, y0, Z2))
        T.append(((0, 0, 0), (x0, y0, 0), (x1, y1, 0)))
        T.append(((0, 0, Z2), (x0, y0, Z2), (x1, y1, Z2)))
    hx = 32.0
    for zz in (Z1, Z1 + S):     # the slab: four boxes' worth of faces round the hatch, each face clipped to the round wall
        T += QUAD((-r, -r, zz), (r, -r, zz), (r, -hx, zz), (-r, -hx, zz))
        T += QUAD((-r, hx, zz), (r, hx, zz), (r, r, zz), (-r, r, zz))
        T += QUAD((-r, -hx, zz), (-hx, -hx, zz), (-hx, hx, zz), (-r, hx, zz))
        T += QUAD((hx, -hx, zz), (r, -hx, zz), (r, hx, zz), (hx, hx, zz))
    for a, b in (((-hx, -hx), (hx, -hx)), ((hx, -hx), (hx, hx)), ((hx, hx), (-hx, hx)), ((-hx, hx), (-hx, -hx))):
        T += QUAD((a[0], a[1], Z1), (b[0], b[1], Z1), (b[0], b[1], Z1 + S), (a[0], a[1], Z1 + S))
    # the slab's square faces poke past the round wall into the outdoors (a ledge); the wall seals the inside
    pts = [('low', (120, 0, 200)), ('low', (-150, 60, 60)), ('high', (120, 0, 600)), ('high', (-150, -60, 760)),
           ('out', (0, 0, 850)), ('out', (330, 0, 200))]
    return dict(T=T, rect=(-400, -400, 400, 400), light='120,0,250,800,1,1,1', pts=pts,
                hatch=[(0.0, 0.0, z) for z in np.arange(Z1 - 64, Z1 + 66, 4.0)], hatchTags=('low', 'high'))


def sc_glass():
    """Two rooms behind a 2-unit wall with a glass window (200 x 150) between them, and a window to the outdoors:
    each pane names the rooms on its two sides."""
    T = box(0, 0, 0, 1202, 600, 300)
    T = [t for t in T if not all(abs(p[1]) < 1e-6 for p in t)]       # the y = 0 face: cut for the outdoor window
    T += wall_with_holes('x', -2, 0, 0, 1202, 0, 300, [(200, 400, 80, 230)])
    T += wall_with_holes('y', 600, 602, 0, 600, 0, 300, [(200, 400, 80, 230)])
    G = []
    for x in (600, 602):   # the inner pane: two faces of the wall's thickness, blue-tinted
        G += [(t, (90, 150, 230)) for t in QUAD((x, 200, 80), (x, 400, 80), (x, 400, 230), (x, 200, 230))]
    G += [(t, (90, 150, 230)) for t in QUAD((200, -1, 80), (400, -1, 80), (400, -1, 230), (200, -1, 230))]
    pts = [('A', (300, 300, 150)), ('B', (900, 300, 150)), ('out', (300, -50, 150))]
    panes = [((601, 300, 155), ('A', 'B')), ((300, -1, 155), ('A', 'out'))]
    return dict(T=T, glass=G, rect=(-100, -100, 1300, 700), light='300,300,220,900,1,1,1', pts=pts, panes=panes)


SCENES = [('wall', sc_wall), ('lroom', sc_l), ('church', sc_church), ('lighthouse', sc_lighthouse), ('glass', sc_glass)]


def synth_cmd(exe, work, red):
    import subprocess
    import time
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    global box, wall_with_holes
    from probe_place import box, wall_with_holes
    from probe_bake import write_soup
    os.makedirs(work, exist_ok=True)
    fails, lines = [], []
    for name, fn in SCENES:
        s = fn()
        soup = os.path.join(work, name + '.psp')
        write_soup(soup, s['T'], [(160, 160, 160)] * len(s['T']), glass=s.get('glass'))
        out = os.path.join(work, name)
        cmd = [exe, '-no-gui', 'probegi', '--soup', soup, '--rect', ','.join('%g' % v for v in s['rect']), '--out', out,
               '--light', s['light'], '--spacing', '140', '--rays', '512']
        if red == 'noclamp':
            cmd += ['--red', 'noclamp']
        elif red:
            cmd += ['--rooms-red', red]
        t0 = time.time()
        p = subprocess.run(cmd, capture_output=True, text=True)
        cen = [l.strip() for l in p.stdout.splitlines() if l.strip().startswith(('rooms:', 'gi rooms:'))]
        lines.append('%s: exit %d, %.1f s; %s' % (name, p.returncode, time.time() - t0, ' | '.join(cen)))
        if p.returncode != 0:
            fails.append('%s: probegi exit %d %s' % (name, p.returncode, p.stderr.strip()[-200:]))
            continue
        rp = os.path.join(out, 'gi_rooms.bin')
        R = read_rooms(rp) if os.path.exists(rp) else None
        if R is None:
            fails.append('%s: no room labels (gi_rooms.bin)' % name)
        else:
            # the labels against the scene's own rooms
            tag = {}
            for t, q in s['pts']:
                a = room_at(R, q)[0]
                if t == 'out':
                    if a != 0:
                        fails.append('%s: %s outdoors has room %d' % (name, q, a))
                    continue
                if a < 1:
                    fails.append('%s: %s (%s) has room %d' % (name, q, t, a))
                    continue
                if tag.setdefault(t, a) != a:
                    fails.append('%s: room %s split (%d, %d at %s)' % (name, t, tag[t], a, q))
            seen = {}
            for t, a in tag.items():
                if a in seen:
                    fails.append('%s: rooms %s and %s share label %d' % (name, seen[a], t, a))
                seen[a] = t
            tag['out'] = 0
            for q, (t1, t2) in s.get('panes', []):
                ab = room_at(R, q)
                want = {tag.get(t1, -9), tag.get(t2, -9)}
                if set(ab) != want:
                    fails.append('%s: the pane at %s names %s, not %s|%s %s' % (name, q, ab, t1, t2, sorted(want)))
            if 'hatch' in s:
                want = {tag.get(s['hatchTags'][0], -9), tag.get(s['hatchTags'][1], -9)}
                col = [room_at(R, q) for q in s['hatch']]
                both = [ab for ab in col if set(ab) == want]
                bad = [ab for ab in col if ab[0] >= 0 and ab[0] not in want]
                if not both or bad:
                    fails.append('%s: the hatch names both rooms in %d cells, another room in %d' % (name, len(both), len(bad)))
            lines.append('  %s: %d rooms, grid %s cell %g; %s' % (name, R['rooms'], 'x'.join(map(str, R['dims'])), R['cell'],
                                                                   ', '.join('%s=%d' % kv for kv in sorted(tag.items()))))
        if name == 'wall':
            G = read_grid(os.path.join(out, 'gi_grid.bin'), os.path.join(out, 'gi_slots.bin'))
            use = R if (R is not None and 'slots' in G) else None
            side = {'A': [], 'B': []}
            holes = 0
            GI_PLAIN[0] = 0
            # the floor and ceiling beside the 2-unit wall, and (4 and 596, 4 and 1198) in the inner corners against the
            # outer walls, where the nearest air cell is the outdoors across the wall
            for room, xs, nx in (('A', (4.0, 596.0, 584.0, 570.0, 556.0), -1.0), ('B', (606.0, 618.0, 632.0, 646.0, 1198.0), 1.0)):
                for x in xs:
                    for y in (4.0, 60.0, 180.0, 300.0, 420.0, 540.0, 596.0):
                        for P, N in (((x, y, 0.0), (0, 0, 1)), ((x, y, 300.0), (0, 0, -1))):
                            e, a = gi_E(G, use, P, N)
                            holes += e < 0
                            side[room].append(max(e, 0.0))
                for y in (60.0, 300.0, 540.0):
                    for z in (40.0, 150.0, 260.0):
                        P = (600.0 if room == 'A' else 602.0, y, z)
                        e, a = gi_E(G, use, P, (nx, 0, 0))
                        holes += e < 0
                        side[room].append(max(e, 0.0))
            # the bar: the dark room's own probes (what the bake's surfels gave them, the grid's job is to add nothing)
            b = open(os.path.join(out, 'gi_probes.bin'), 'rb').read()
            npb = struct.unpack_from('<i', b, 0)[0]
            pe = np.frombuffer(b, '<f4', npb * 21, 4).reshape(npb, 21)
            pr = np.frombuffer(open(os.path.join(out, 'gi_proberooms.bin'), 'rb').read(), '<i4', 2 * npb, 4).reshape(npb, 2) \
                if os.path.exists(os.path.join(out, 'gi_proberooms.bin')) else None
            pB = [i for i in range(npb) if pe[i, 0] > 602.0 and 0.0 < pe[i, 2] < 300.0]   # inside B, by the scene
            eBp = float(pe[pB, 3::3].max()) if pB else 0.0
            eA = float(np.median(side['A']))
            wB = max(side['B'])
            lines.append('  wall: lit side E median %.4g; dark side worst %.4g, the dark room\'s brightest probe face %.4g '
                         '(the bake\'s own share through the wall %.1f%%); samples with no weight %d of %d'
                         % (eA, wB, eBp, 100 * eBp / max(eA, 1e-9), holes, len(side['A']) + len(side['B'])))
            if eA <= 0:
                fails.append('wall: the lit side reads no light')
            if wB > eBp * LEAK_TOL + 1e-6:
                fails.append('wall: the dark side reads %.3g, past its own probes\' %.3g (%.0f%% of the lit side leaks through '
                             'the 2-unit wall)' % (wB, eBp, 100 * wB / max(eA, 1e-9)))
            if pr is not None and len({int(pr[i, 0]) for i in pB}) != 1:
                fails.append('wall: the dark room\'s probes in rooms %s' % sorted({int(pr[i, 0]) for i in pB}))
            if holes:
                fails.append('wall: %d samples with no weight (magenta)' % holes)
            if GI_PLAIN[0]:
                fails.append('wall: %d samples with no room with weight (the unclamped blend)' % GI_PLAIN[0])
    for l in lines:
        print(l)
    print('synth%s %s%s' % (' (red %s)' % red if red else '', 'PASS' if not fails else 'FAIL',
                            (': ' + '; '.join(fails)) if fails else ''))
    return 0 if not fails else 1


def holes_cmd(run, sample, red):
    """H (lane ROOMCLAMP1): the clamp makes no hole. On the dump's own surfels (a fixed sample), the shader's twin with
    the rooms against the plain trilinear: a surface the plain grid covers must stay covered (a room's slot no probe of
    that room reached is empty: weighing it painted pipes black in the lit view, magenta in the pass). Red emptyslot:
    the twin weighs empty slots (the old shader)."""
    d = os.path.join(run, 'dump')
    RED_EMPTY[0] = red == 'emptyslot'
    G = read_grid(os.path.join(d, 'gi_grid.bin'), os.path.join(d, 'gi_slots.bin'))
    R = read_rooms(os.path.join(d, 'gi_rooms.bin'))
    if 'slots' not in G:
        print('H FAIL no slots in the dump (gi_slots.bin)')
        return 1
    b = open(os.path.join(d, 'gi_surfels.bin'), 'rb').read()
    n = struct.unpack_from('<i', b)[0]
    S = np.frombuffer(b, '<f4', n * 12, 4).reshape(n, 12).astype(np.float64)
    pick = np.random.default_rng(3).choice(n, min(n, sample), replace=False)
    GI_PLAIN[0] = 0
    cov = holes = 0
    for i in pick:
        P, N = S[i, 0:3], S[i, 3:6]
        if gi_sample(G, None, P, N)[3] <= COVER_MIN:
            continue
        cov += 1
        holes += gi_sample(G, R, P, N)[3] <= COVER_MIN
    ok = cov > 0 and holes == 0
    print('H %s the rooms make no hole%s: %d surfels, %d covered by the plain grid, %d of them uncovered with the rooms '
          '(%d fell to the plain trilinear: no room of theirs with weight)'
          % ('PASS' if ok else 'FAIL', ' (red %s)' % red if red else '', len(pick), cov, holes, GI_PLAIN[0]))
    return 0 if ok else 1


if __name__ == '__main__':
    a = sys.argv[1:]
    opt = (lambda k, d='': a[a.index(k) + 1] if k in a else d)
    if a and a[0] == 'doors':
        sys.exit(door_cmd(a[1], a[2], opt('--cell'), opt('--eye')))
    if a and a[0] == 'synth':
        sys.exit(synth_cmd(a[1], a[2], opt('--red')))
    if a and a[0] == 'holes':
        sys.exit(holes_cmd(a[1], int(opt('--sample', '3000')), opt('--red')))
    if a and a[0] == 'back':
        sys.exit(back_cmd(a[1], opt('--ref'), int(opt('--sample', '24'))))
    print(__doc__)
    sys.exit(2)
