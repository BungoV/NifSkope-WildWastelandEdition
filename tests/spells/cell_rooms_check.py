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
  synth <exe> <work> [--red noclamp|conn26|boxes|glasswall|nomask|noscale]
        Room labels (part C): seven scenes built here (SCENES; fence and fade: lane ALPHATEST2), each through `<exe> -no-gui probegi`; the labels
        (gi_rooms.bin) against each scene's own rooms (same room one label, rooms apart, outdoors 0), the panes and
        the hatch naming both sides; the 2-unit wall: cellGiRoomSample redone here (gi_sample) on the dark side
        reads no more than the dark room's own brightest probe face, and every sample has weight.
        Lane GIBLACK2: scene "recess" (a 130-unit recess open to the sky, a far triangle that makes the soup's box
        huge): the recess air is outdoors (0). Red --red soupbox (the rooms grid over the soup's box) FAILS.
  recess <dump> <fixture> [--lo 0.5] [--hi 2.0] [--expect fail]
        Lane GIBLACK2: a run's GI dump (WW_CELL_GI_DUMP) at a recess against the tracer (tests/fixtures/giblack2_memden.tsv:
        world points, normals, bounce_reference cosine sky share). C the rooms grid spans the GI grid's box, not the
        soup's; S per point set: the sky the frame draws (the twin's sky-share sample: sky x k + 1 - k, cellGiSample's
        exterior path) has a mean within lo..hi times the tracer's, no point is lent the flat ambient and no 0.75 read
        lands in an indoor room. Red WW_CELL_ROOMS_RED=soupbox FAILS (run with --expect fail).
  strand <dump> <fixture> [--margin 0.03] [--margin1 0.05] [--expect fail]
        Lane GIBLACK2: stranded surfaces (both room reads solid: the lend rule) in a sky-lit grid against the tracer
        (tests/fixtures/giblack2_strand_*.tsv: set px py x y z nx ny nz tracer; set 0 surfaces a viewer can stand
        before, set 1 tight spaces). Their answer comes from the evidence search (strand_pick): the nearest indoor
        or outdoors air within 3 cells, never a guess at the outdoors. Per set: the points the tracer puts indoors
        (sky share <= 0.02) gain no sky (mean drawn - tracer <= margin, none drawn 0.5+, at most a twentieth 0.1+
        over), the rest no black (drawn < 0.01 where the tracer gives 0.03+) and no open-sky bright (drawn 0.5+
        where it gives under 0.25); set 1 (tight) the indoors rule only, mean within margin1. Red
        GI_CHECK_STRAND=1 (the shader's WW_CELL_PV_RED=strand: GIBLACK1's law) FAILS.
  points <dump> <fixture> [--box x0,y0,x1,y1] [--maxx X] [--darkshare S] [--expect fail]
        Lane GIDARK1: surfaces against the tracer's GI term (tests/fixtures/gidark1_*.tsv: px py x y z nx ny nz tracer).
        recess2 (Goodneighbor, 22h): --darkshare 0.1; reds GI_CHECK_TIE=0 (tiepick) and GI_CHECK_BOTH=0 (firstslot)
        FAIL. hinside corner (Concord Hardware, noon) --box 945,200,1055,410 --maxx 5; red GI_CHECK_LONE=0 (lonesky) FAILS.
        hinside floor by the open front: --upshare 0.01 --minmed 0.5; the red dump (WW_CELL_GI_RED=noskyfloor) FAILS.
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
COVER_MIN = 0.01     # cellGiE's own floor: a sample with less weight paints the no-probe marker (cyan since FIXPACK1) in the pass view
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


def gi_sample(G, R, P, N, fill=None, sky=False):
    """cellGiSample: with rooms the blend of the 8 voxels' slots of the surface's room (renormalized); without
    (R None) the hardware trilinear of the one grid. Returns rgba (cellGiE divides by a).
    fill (lane GICAL1; None = GI_FILL[0]): the gap fill (gi_fill) where the blend has no valid weight. The shader
    fills the GI grid of a grid that is not sky lit only: pass fill=False for a sky grid or the sky-share grid.
    sky (lane GIBLACK1): the grid is sky lit (an exterior; the shader's cellGiSky): the lend rule. A surface whose own
    reads (0.75 and 1.75 cell) are both solid borrows only the outdoors (room 0): its neighbour search keeps outdoors
    cells, its seen fill runs only from an outdoors view point, and with neither it takes the plain trilinear (lane
    GIBLACK2; it was zeros, the weather's flat ambient: the open sky's light in a recess). The same when the 0.75 read is solid and the 1.75 read lands in an indoors room (a normal turned toward
    the pocket), whose direct read is then not taken either. GI_CHECK_BORROW=1: the old search (the shader's red WW_CELL_PV_RED=borrow)."""
    P, N = np.asarray(P, float), np.asarray(N, float)
    fill = GI_FILL[0] if fill is None else fill
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
        out, Ls = None, L
        frm = see_from(R, P, N)
        L2 = room_at(R, P + N * 1.75 * R['cell'])
        lend = sky and not BORROW_RED[0] and room_solid(R, P + N * 0.75 * R['cell']) and (L2[0] < 0 or (L2[0] != 0 and L2[1] != 0))
        if lend and not STRAND_RED[0]:
            # lane GIBLACK2: a stranded surface takes the room its evidence search reaches (strand_pick), never a
            # guess at the outdoors; nothing reached: the plain trilinear
            GI_STRAND[0] += 1
            v, code = strand_pick(G, R, P, N, i0, f, sl, n2)
            STRAND_LAST[0] = code
            if v is not None:
                return v
            GI_LENT[0] += 1
            if LENDAMB_RED[0]:
                return np.zeros(4)
            s, ws = gi_blend(G, None, i0, f, sl, n2)
            return s
        if L is not None and not (lend and L[0] != 0 and L[1] != 0):
            s, ws = gi_blend(G, L, i0, f, sl, n2, R, frm)
            if ws > 0:
                out = s / ws
        if out is None:
            best, best_any = 3.0, 3.0
            for d, c in cand:
                if L is None and d < best_any:
                    best_any, Ls = d, c   # lane GICAL1: the nearest air cell, with weight or not (the fill's room)
                if d >= best or (lend and c[0] != 0 and c[1] != 0):
                    continue
                s, ws = gi_blend(G, c, i0, f, sl, n2, R, frm)
                if ws > 0:
                    best, out = d, s / ws
        if out is not None and (out[3] > 0.01 or not fill):
            return out
        if fill:
            fo = gi_fill(G, Ls, P, N, True)
            if fo[3] > 0.01:
                GI_FILLED[0] += 1
                return fo
            if out is not None:
                return out
        if not SEE_RED[0] and (not lend or 0 in room_at(R, frm)):
            so = gi_seen_fill(G, R, P, N, frm)
            if so[3] > 0.01:
                return so
        if lend:
            GI_LENT[0] += 1   # lane GIBLACK2: the plain trilinear below, never the flat ambient (zeros)
            if LENDAMB_RED[0] or STRAND_RED[0]:
                return np.zeros(4)
        else:
            GI_PLAIN[0] += 1
    s, ws = gi_blend(G, None, i0, f, sl, n2)
    # lane WALLSTACK1: with rooms too, the last fill (the shader's probe 95 path 10): a pocket no probe reached whose seen
    # fill finds nothing; the trilinear's reach and rule (every voxel's first slot), weighted over the valid voxels only.
    # GI_CHECK_LASTFILL=0: the plain trilinear's black (the shader's red WW_CELL_PV_RED=nolastfill)
    if fill and (R is None or not LASTFILL_RED[0]) and s[3] <= 0.01:
        fo = gi_fill(G, None, P, N, False)
        if fo[3] > 0.01:
            GI_FILLED[0] += 1
            return fo
    return s


def gi_fill(G, L, P, N, rooms):
    """cellGiFillAt (lane GICAL1): the weighted mean of the valid voxels of the 4x4x4 block round the sample (within
    2.5 voxels, weight (1 - d / 2.5)^2), of room L's slots when the grid has rooms (L None there: nothing)."""
    dm = np.array(G['dims'])
    g = (P + N * 0.5 * G['voxel'] - G['origin']) / G['voxel'] - 0.5
    i0 = np.floor(g).astype(int)
    sl = (0 if N[0] >= 0 else 1, 2 if N[1] >= 0 else 3, 4 if N[2] >= 0 else 5)
    n2 = N * N
    s, ws = np.zeros(4), 0.0
    if rooms and L is None:
        return s
    for k in range(64):
        c = i0 + np.array([k & 3, (k >> 2) & 3, (k >> 4) & 3]) - 1
        if (c < 0).any() or (c >= dm).any():
            continue
        w = 1.0 - float(np.linalg.norm(c - g)) / 2.5
        if w <= 0:
            continue
        src = G['g']
        if rooms:
            S = G['slots'][c[2], c[1], c[0]]
            if S[0] >= 0 and S[0] in L:
                src = G['g']
            elif S[1] >= 0 and S[1] in L:
                src = G['g2']
            else:
                continue
        v = sum(n2[a] * src[sl[a], c[2], c[1], c[0]] for a in range(3))
        if v[3] <= 0:
            continue
        w *= w
        s += w * v
        ws += w
    return s / ws if ws > 0 else np.zeros(4)


TIE_RED = [os.environ.get('GI_CHECK_TIE', '1') == '0']   # lane GIDARK1: red tiepick (WW_CELL_PV_RED=tiepick): a tie takes the answer nearer the plain trilinear (GIBLACK2)
LONE_RED = [os.environ.get('GI_CHECK_LONE', '1') == '0']   # lane GIDARK1: red lonesky (WW_CELL_PV_RED=lonesky): a buried surface with only outdoor evidence takes the outdoor answer
BOTH_RED = [os.environ.get('GI_CHECK_BOTH', '1') == '0']   # lane GIDARK1: red firstslot (WW_CELL_PV_RED=firstslot): a two-room read takes the voxel's first matching slot alone
RED_EMPTY = [False]   # red emptyslot: the twin weighs a room's empty slots (the old shader: black, the no-probe marker)
SEE_RED = [os.environ.get('GI_CHECK_SEE', '1') == '0']   # lane GISHADE1: red nosee (the shader's WW_CELL_PV_RED=nosee)
LASTFILL_RED = [os.environ.get('GI_CHECK_LASTFILL', '1') == '0']   # lane WALLSTACK1: red nolastfill (the shader's WW_CELL_PV_RED=nolastfill)
GI_PLAIN = [0]     # samples that found rooms but no room with weight: the shader's plain (unclamped) trilinear
GI_FILL = [os.environ.get('GI_CHECK_FILL', '1') != '0']   # lane GICAL1: the gap fill (GI_CHECK_FILL=0: the shader pin WW_CELL_GI_FILL=0)
GI_FILLED = [0]    # samples the fill answered
BORROW_RED = [os.environ.get('GI_CHECK_BORROW', '0') == '1']   # lane GIBLACK1: red borrow (the shader's WW_CELL_PV_RED=borrow)
LENDAMB_RED = [os.environ.get('GI_CHECK_LENDAMB', '0') == '1']   # lane GIBLACK2: red lendamb (WW_CELL_PV_RED=lendamb): a lent surface with no answer takes the flat ambient
GI_LENT = [0]      # lane GIBLACK1: samples the lend rule left with no answer (lane GIBLACK2: they take the plain trilinear)
STRAND_RED = [os.environ.get('GI_CHECK_STRAND', '0') == '1']   # lane GIBLACK2: red strand (WW_CELL_PV_RED=strand): GIBLACK1's whole law (the outdoors-only borrow, then the flat ambient) instead of the evidence search
STRAND_LAST = [0]  # lane GIBLACK2: the stranded branch of the last sample (5 trilinear, 6 blend, 7 room fill, 8 seen fill, 9 indoor no data)
GI_STRAND = [0]    # lane GIBLACK2: samples of stranded surfaces (the evidence search ran)
STRAND_T = (0.75, 1.75, 2.75)   # the evidence search: along the normal (cells) ...
STRAND_S = 2                    # ... and up to 2 cells round it in the plane of the two axes off the normal's largest
STRAND_TIE = 1.0                # indoor and outdoor cells within this many cells of each other: the tie rule (strand_pick)


def strand_evidence(R, P, N):
    """cellGiStrandSearch (lane GIBLACK2): a stranded surface (both its reads solid, or the 1.75 read indoors) in a
    sky-lit grid: the air cells at P + N x (0.75, 1.75, 2.75) cell, each with up to 2 cells round it in the plane of
    the two axes off the normal's largest (75 cells, only those whose centre is in front of the surface). Returns the
    nearest indoor and the nearest outdoor (room 0) cell, each (distance from P to the cell's box, room, the cell's
    centre) or None."""
    c = R['cell']
    m = int(np.argmax(np.abs(N)))
    ta, tb = [t for t in range(3) if t != m]
    best = [None, None]
    for t in STRAND_T:
        q0 = P + N * t * c
        for sa in range(-STRAND_S, STRAND_S + 1):
            for sb in range(-STRAND_S, STRAND_S + 1):
                e = np.zeros(3)
                e[ta], e[tb] = sa, sb
                q = q0 + e * c
                L = room_at(R, q)
                if L[0] < 0:
                    continue
                lo = R['origin'] + np.floor((q - R['origin']) / c) * c
                Cc = lo + 0.5 * c
                if float((Cc - P) @ N) <= 0.0:
                    continue
                d = float(np.linalg.norm(np.maximum(np.maximum(lo - P, P - lo - c), 0.0)))
                k = 1 if (L[0] == 0 or L[1] == 0) else 0
                if best[k] is None or d < best[k][0]:
                    best[k] = (d, L, Cc)
    return best[0], best[1]


def strand_answer(G, R, P, N, i0, f, sl, n2, L, Ce):
    """A stranded surface's answer from room L seen from the air cell's centre Ce: its blend (6), indoors the room's
    fill (7), the seen fill from Ce (8), indoors with no light in reach the sealed room's dark (9: sky 0, the ambient
    replaced); outdoors with nothing: (None, 5)."""
    indoor = L[0] != 0 and L[1] != 0
    s, ws = gi_blend(G, L, i0, f, sl, n2, R, Ce)
    if ws > 0:
        return s / ws, 6
    if indoor:
        fo = gi_fill(G, L, P, N, True)
        if fo[3] > 0.01:
            return fo, 7
    if not SEE_RED[0]:
        so = gi_seen_fill(G, R, P, N, Ce)
        if so[3] > 0.01:
            return so, 8
    if indoor:
        return np.array([0.0, 0.0, 0.0, 1.0]), 9
    return None, 5


def strand_pick(G, R, P, N, i0, f, sl, n2):
    """Lane GIBLACK2: every reached cell indoors: the indoor room; only the outdoors: room 0; both: the nearer, and
    when they lie within STRAND_TIE cell of each other (the room grid cannot tell which side of a thin wall the
    surface is on) the answer whose light is nearer the surface's own baked light (the plain trilinear, which sees
    through walls but is the bake's own visibility there); nothing reached: (None, 5), the plain trilinear."""
    a, o = strand_evidence(R, P, N)
    if a is None and o is None:
        return None, 5
    if a is None and not LONE_RED[0] and all(room_solid(R, P + N * k * R['cell']) for k in (0.75, 1.75, 2.75)):
        # lane GIDARK1: buried three cells deep with only the outdoors in reach, the search may have gone round a
        # building's corner (Concord Hardware's front pillar, 6.5x the tracer): the darker of the outdoor answer and
        # the plain trilinear (the sealed dark blacked out Goodneighbor's recess walls)
        vo, co = strand_answer(G, R, P, N, i0, f, sl, n2, o[1], o[2])
        t, w = gi_blend(G, None, i0, f, sl, n2)
        lum = lambda v: float(v[:3] @ (0.2126, 0.7152, 0.0722)) / max(float(v[3]), 1e-6)
        if vo is not None and w > 0 and t[3] > 0 and lum(t) < lum(vo):
            return t / w, 4
        return vo, co
    if a is None or o is None:
        d, L, Ce = a if a is not None else o
        return strand_answer(G, R, P, N, i0, f, sl, n2, L, Ce)
    if abs(a[0] - o[0]) >= STRAND_TIE * R['cell']:
        d, L, Ce = a if a[0] < o[0] else o
        return strand_answer(G, R, P, N, i0, f, sl, n2, L, Ce)
    va, ca = strand_answer(G, R, P, N, i0, f, sl, n2, a[1], a[2])
    vo, co = strand_answer(G, R, P, N, i0, f, sl, n2, o[1], o[2])
    if vo is None:
        return va, ca
    # the shader decides on the grid it samples (the light, sky and bounce): with a dump's full grid beside the sky
    # grid (G['full']) the twin decides there too and answers from G
    H = G.get('full', G)
    ha, _ = strand_answer(H, R, P, N, i0, f, sl, n2, a[1], a[2])
    ho, _ = strand_answer(H, R, P, N, i0, f, sl, n2, o[1], o[2])
    t, _ = gi_blend(H, None, i0, f, sl, n2)
    lum = lambda v: float(v[:3] @ (0.2126, 0.7152, 0.0722))
    if not TIE_RED[0] and ho is not None:
        # lane GIDARK1: a tie is the room grid saying it cannot tell the sides apart, and the plain trilinear reads the
        # voxels' first slot only (the indoor room beside a doorway: Goodneighbor's recess2 took the sealed room's
        # dark, 0.0001 where the tracer says 0.007-0.01): the mean of both answers
        return 0.5 * (va + vo), 20
    if ho is None or abs(lum(ha) - lum(t)) <= abs(lum(ho) - lum(t)):
        return va, ca + 10
    return vo, co + 10


def room_solid(R, q):
    """cellRoomSolid: the fine room grid's cell at q is solid (no name); outside the grid: not."""
    c = np.floor((np.asarray(q, float) - R['origin']) / R['cell']).astype(int)
    if (c < 0).any() or (c >= np.array(R['dims'])).any():
        return False
    return int(R['ab'][c[2], c[1], c[0]][0]) < 0


def see_from(R, P, N):
    """cellGiSeeFrom (lane GISHADE1): where a surface's view of the voxels starts: its room read (P + N x 0.75 cell,
    else 1.75) when that is air, else the centre of the air cell beside either read nearest it (surface_reads' rule)."""
    P, N = np.asarray(P, float), np.asarray(N, float)
    for k in (0.75, 1.75):
        q = P + N * k * R['cell']
        if not room_solid(R, q):
            return q
    m = int(np.argmax(np.abs(N)))
    ta, tb = [t for t in range(3) if t != m]
    best, bq = 3.0, None
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
                qq = q + e * R['cell']
                if room_solid(R, qq):
                    continue
                d = sum((1.0 - f[t] if s_ > 0 else f[t]) if s_ else 0.0 for t, s_ in ((ta, sa), (tb, sb)))
                if d < best:
                    best, bq = d, R['origin'] + (np.floor((qq - R['origin']) / R['cell']) + 0.5) * R['cell']
    return bq if bq is not None else P + N * 0.75 * R['cell']


def gi_seen(R, frm, C):
    """cellGiSeen (lane GISHADE1): no solid fine cell on the segment from the surface's view point to a voxel's centre,
    tapped every half cell, the centre itself left out (a voxel's centre may sit in the wall it straddles)."""
    if SEE_RED[0] or R is None or frm is None:
        return True
    d = np.asarray(C, float) - frm
    nt = min(max(int(math.ceil(float(np.linalg.norm(d)) / (0.5 * R['cell']))), 1), 12)
    return not any(room_solid(R, frm + d * (j / (nt + 1.0))) for j in range(1, nt + 1))


def gi_seen_fill(G, R, P, N, frm):
    """cellGiSeenFill (lane GISHADE1): no room's blend has weight: the voxels of the 4x4x4 block the surface sees
    (any room), weight (1 - d / 2.5)^2 in voxels, before the plain trilinear (which sees through walls)."""
    dm = np.array(G['dims'])
    g = (P + N * 0.5 * G['voxel'] - G['origin']) / G['voxel'] - 0.5
    i0 = np.floor(g).astype(int)
    sl = (0 if N[0] >= 0 else 1, 2 if N[1] >= 0 else 3, 4 if N[2] >= 0 else 5)
    n2 = N * N
    s, ws = np.zeros(4), 0.0
    for k in range(64):
        c = i0 + np.array([k & 3, (k >> 2) & 3, (k >> 4) & 3]) - 1
        if (c < 0).any() or (c >= dm).any():
            continue
        w = 1.0 - float(np.linalg.norm(c - g)) / 2.5
        if w <= 0 or not gi_seen(R, frm, G['origin'] + (c + 0.5) * G['voxel']):
            continue
        v = sum(n2[a] * G['g'][sl[a], c[2], c[1], c[0]] for a in range(3))
        if v[3] <= 0:
            continue
        s += w * w * v
        ws += w * w
    return s / ws if ws > 0 else np.zeros(4)


def gi_blend(G, L, i0, f, sl, n2, R=None, frm=None):
    """The 8 voxels' slots of room L (None: the one grid, the hardware trilinear). Returns (sum, weight).
    R, frm (lane GISHADE1): only the voxels the surface sees from frm (gi_seen)."""
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
        if L is not None and not gi_seen(R, frm, G['origin'] + (c + 0.5) * G['voxel']):
            continue   # lane GISHADE1: a wall between the surface and the voxel's centre
        v = sum(n2[a] * src[sl[a], c[2], c[1], c[0]] for a in range(3))
        if L is not None and not BOTH_RED[0] and L[0] >= 0 and L[1] >= 0 and L[0] != L[1] and S[0] in L and S[1] in L:
            # lane GIDARK1: the read cell holds both rooms (an opening's cell: it sees both) and so does the voxel: the
            # mean of both slots that hold light (the first slot alone left a recess beside a doorway on the dark room)
            v2 = sum(n2[a] * G['g2'][sl[a], c[2], c[1], c[0]] for a in range(3))
            if v[3] > 0 and v2[3] > 0:
                v = 0.5 * (v + v2)
            elif v2[3] > 0:
                v = v2
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


def masked_plane(x, y0, y1, z0, z1, tile, mp, model, scale=None):
    """lane ALPHATEST2: a two-sided plane at x, its UVs (y, z) / tile; returns tris and records for write_soup's amask
    (tri index filled in by the scene)"""
    T = QUAD((x, y0, z0), (x, y1, z0), (x, y1, z1), (x, y0, z1))
    recs = []
    for t in T:
        uv = [c for p in t for c in (p[1] / tile, p[2] / tile)]
        recs.append((mp, model, 128, uv, scale))
    return T, recs


def sc_fence():
    """lane ALPHATEST2: one hall 1800 long; at x 601 a chain-link fence (alpha-tested, 77% holes: the wires every 8th
    texel), at x 1201 boards with 4-unit gaps (6% holes). The fence must not close a room (A and B one room); the
    boards still do (C its own). Red "nomask": the fence closes, A and B split."""
    T = box(0, 0, 0, 1802, 600, 300)
    link = bytes(255 if (i % 8 == 0 or j % 8 == 0) else 0 for j in range(32) for i in range(32))
    board = bytes(0 if j == 0 else 255 for j in range(16) for i in range(16))
    F, fr = masked_plane(601, 0, 600, 0, 300, 16.0, 0, 0)
    Bd, br = masked_plane(1201, 0, 600, 0, 300, 64.0, 1, 1)
    recs = [(len(T) + k,) + r for k, r in enumerate(fr)] + [(len(T) + len(F) + k,) + r for k, r in enumerate(br)]
    T = T + F + Bd
    am = dict(maps=[('synth/chainlink', 32, 32, link), ('synth/boards', 16, 16, board)], models=['fence', 'boards'], recs=recs)
    pts = [('AB', (300, 300, 150)), ('AB', (900, 300, 150)), ('AB', (560, 100, 40)), ('AB', (640, 500, 260)),
           ('C', (1500, 300, 150)), ('out', (-60, 300, 150)), ('out', (1860, 300, 150))]
    return dict(T=T, amask=am, rect=(-100, -100, 1900, 700), light='300,300,220,900,1,1,1', pts=pts)


def sc_fade():
    """lane ALPHATEST2: two rooms A | B behind a wall whose map is opaque (alpha 200 against 128) but whose vertex
    alpha x material alpha is 0.5: the renderer draws it all holes (200 x 0.5 < 128), so A and B are one room.
    Red "noscale" (the soup written without its scales, the ALPHATEST1 test): the wall stands, A and B split."""
    T = box(0, 0, 0, 1202, 600, 300)
    W, wr = masked_plane(601, 0, 600, 0, 300, 64.0, 0, 0, scale=(0.5, 0.5, 0.5))
    recs = [(len(T) + k,) + r for k, r in enumerate(wr)]
    T = T + W
    am = dict(maps=[('synth/opaque200', 4, 4, bytes([200] * 16))], models=['fadewall'], recs=recs)
    pts = [('AB', (300, 300, 150)), ('AB', (900, 300, 150)), ('out', (-60, 300, 150))]
    return dict(T=T, amask=am, rect=(-100, -100, 1300, 700), light='300,300,220,900,1,1,1', pts=pts)


def sc_recess():
    """lane GIBLACK2: a 130-unit recess between two closed blocks on a street, open to the sky; and a far triangle
    (x 40000, z 8000, a far soup's reach) that makes the soup's box huge. Over the soup's box the rooms grid's cell grows
    to ~119 units and the recess air is solid (red "soupbox"); clipped to the GI grid's box the cell stays fine and
    the recess is outdoors (0), the same as the street."""
    T = box(-800, -200, -10, 800, 1200, 0)
    T += box(-600, 0, 0, 0, 400, 400) + box(-600, 530, 0, 0, 1000, 400) + box(-600, 400, 0, -130, 530, 400)
    T.append(((40000.0, 40000.0, 8000.0), (40001.0, 40000.0, 8000.0), (40000.0, 40001.0, 8000.0)))
    pts = [('out', (-65, 465, 60)), ('out', (-65, 465, 200)), ('out', (-100, 440, 330)), ('out', (300, 465, 150)),
           ('blockA', (-300, 200, 200)), ('blockB', (-300, 760, 200))]
    return dict(T=T, rect=(-700, -100, 700, 1100), light='300,465,220,900,1,1,1', pts=pts)


SCENES = [('wall', sc_wall), ('lroom', sc_l), ('church', sc_church), ('lighthouse', sc_lighthouse), ('glass', sc_glass),
          ('fence', sc_fence), ('fade', sc_fade), ('recess', sc_recess)]


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
        am = s.get('amask')
        if am and red == 'noscale':   # lane ALPHATEST2's scale red: the masks without their scales (AMK1)
            am = dict(am, recs=[r[:5] + (None,) for r in am['recs']])
        write_soup(soup, s['T'], [(160, 160, 160)] * len(s['T']), glass=s.get('glass'), amask=am)
        out = os.path.join(work, name)
        cmd = [exe, '-no-gui', 'probegi', '--soup', soup, '--rect', ','.join('%g' % v for v in s['rect']), '--out', out,
               '--light', s['light'], '--spacing', '140', '--rays', '512']
        if red == 'noclamp':
            cmd += ['--red', 'noclamp']
        elif red and red != 'noscale':
            cmd += ['--rooms-red', red]
        cmd += os.environ.get('CELL_ROOMS_PROBEGI_ARGS', '').split()   # lane GAPBAKE1: bake flags, e.g. --room-split 1
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
                fails.append('wall: %d samples with no weight (no-probe marker)' % holes)
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
    that room reached is empty: weighing it painted pipes black in the lit view, the no-probe marker in the pass). Red emptyslot:
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
        if gi_sample(G, None, P, N, fill=False)[3] <= COVER_MIN:   # the plain grid as it was
            continue
        cov += 1
        holes += gi_sample(G, R, P, N)[3] <= COVER_MIN
    ok = cov > 0 and holes == 0
    print('H %s the rooms make no hole%s: %d surfels, %d covered by the plain grid, %d of them uncovered with the rooms '
          '(%d fell to the plain trilinear: no room of theirs with weight)'
          % ('PASS' if ok else 'FAIL', ' (red %s)' % red if red else '', len(pick), cov, holes, GI_PLAIN[0]))
    return 0 if ok else 1


def recess_cmd(dump, fixture, lo_x, hi_x, expect):
    """Lane GIBLACK2: a recess's sky share against the tracer, from a run's GI dump (WW_CELL_GI_DUMP)."""
    R = read_rooms(dump + '/gi_rooms.bin')
    G = read_grid(dump + '/gi_skygrid.bin', dump + '/gi_slots.bin')
    nv, d = G['nv'], G['dims']
    # the sky part of gi_slots.bin: slot 1's grid, then slot 1's sky grid (the shader's sky lookup reads that one)
    G['g2'] = np.frombuffer(open(dump + '/gi_slots.bin', 'rb').read(), '<f4', 6 * nv * 4,
                            32 + 8 * nv + 6 * nv * 4 * 4).reshape(6, d[2], d[1], d[0], 4)
    G['full'] = read_grid(dump + '/gi_grid.bin', dump + '/gi_slots.bin')   # the grid the shader samples (strand_pick's tie)
    F = np.loadtxt(fixture, comments='#')
    rlo, rhi = R['origin'], R['origin'] + np.array(R['dims']) * R['cell']
    glo, ghi = G['origin'], G['origin'] + np.array(d) * G['voxel']
    tol = (np.ceil(48.0 / R['cell']) + 4) * R['cell']   # the rooms grid's pad (ceil(pinch / cell) + 2 cells), its rounding up
    okC = bool((rlo[:2] >= glo[:2] - tol).all() and (rhi[:2] <= ghi[:2] + tol).all())
    print('C  rooms grid cell %.1f box x %.0f..%.0f y %.0f..%.0f; GI grid voxel %.1f box x %.0f..%.0f y %.0f..%.0f: %s'
          % (R['cell'], rlo[0], rhi[0], rlo[1], rhi[1], G['voxel'], glo[0], ghi[0], glo[1], ghi[1],
             'ok (clipped to the GI grid)' if okC else 'WIDER (the soup box: coarse cells)'))
    good = okC
    for s, name in ((0, 'wall'), (1, 'spot2'), (2, 'open')):
        A = F[F[:, 0] == s]
        if not len(A):
            continue
        solid, indoor, lent, drawn = 0, 0, 0, []
        for row in A:
            P, N = row[3:6], row[6:9]
            a = room_at(R, P + N * 0.75 * R['cell'])
            solid += a[0] < 0
            indoor += a[0] > 0 and a[1] != 0
            GI_LENT[0] = 0
            v = gi_sample(G, R, P, N, fill=False, sky=True)
            lent += GI_LENT[0] > 0
            k = min(max(float(v[3]), 0.0), 1.0)
            drawn.append(float(v[:3].mean()) + 1.0 - k)   # the frame's sky in units of the flat ambient (linear law)
        drawn, tr = np.array(drawn), A[:, 9]
        r = drawn.mean() / max(tr.mean(), 1e-6)
        # the lend rule stays a safety net only: it may not decide the set (at most a tenth of its points lent, a tenth
        # reading an indoors room at 0.75 cell)
        # per point: none open-sky bright (drawn 0.5+ where the tracer gives under 0.25: the flat ambient) and none black
        # (drawn under 0.01 where the tracer gives 0.02+)
        bright = int(((drawn >= 0.5) & (tr < 0.25)).sum())
        black = int(((drawn < 0.01) & (tr >= 0.02)).sum())
        okS = lo_x <= r <= hi_x and lent <= 0.1 * len(A) and indoor <= 0.1 * len(A) and bright == 0 and black == 0
        good = good and okS
        print('S  %-5s %3d points: tracer sky %.3f  drawn sky %.3f (x%.2f, band %.2f..%.2f)  0.75 read solid %d indoor %d  '
              'lent %d  open-sky bright %d black %d: %s' % (name, len(A), tr.mean(), drawn.mean(), r, lo_x, hi_x, solid, indoor, lent, bright, black,
                                              'ok' if okS else 'OFF'))
    if expect == 'fail':
        print('%s  red recess: %s' % ('PASS' if not good else 'FAIL', 'the recess reads off the tracer' if not good else 'the recess still matches'))
        return 0 if not good else 1
    print('%s  recess: the recess takes its measured sky share' % ('PASS' if good else 'FAIL'))
    return 0 if good else 1


def strand_cmd(dump, fixture, margin, margin1, expect):
    """Lane GIBLACK2: stranded surfaces' sky against the tracer (see the head)."""
    R = read_rooms(dump + '/gi_rooms.bin')
    G = read_grid(dump + '/gi_skygrid.bin', dump + '/gi_slots.bin')
    nv, d = G['nv'], G['dims']
    G['g2'] = np.frombuffer(open(dump + '/gi_slots.bin', 'rb').read(), '<f4', 6 * nv * 4,
                            32 + 8 * nv + 6 * nv * 4 * 4).reshape(6, d[2], d[1], d[0], 4)
    G['full'] = read_grid(dump + '/gi_grid.bin', dump + '/gi_slots.bin')
    F = np.loadtxt(fixture, comments='#', ndmin=2)
    good = True
    print('rooms grid cell %.1f, %d rooms; GI grid voxel %.1f; %s' % (R['cell'], R['rooms'], G['voxel'], os.path.basename(fixture)))
    for s, name in ((0, 'open'), (1, 'tight')):
        A = F[F[:, 0] == s]
        if not len(A):
            continue
        st, drawn, codes = 0, [], []
        for row in A:
            P, N = row[3:6], row[6:9]
            st += room_solid(R, P + N * 0.75 * R['cell']) and (lambda L2: L2[0] < 0 or (L2[0] != 0 and L2[1] != 0))(room_at(R, P + N * 1.75 * R['cell']))
            STRAND_LAST[0] = 0
            v = gi_sample(G, R, P, N, fill=False, sky=True)
            codes.append(STRAND_LAST[0])
            k = min(max(float(v[3]), 0.0), 1.0)
            drawn.append(float(v[:3].mean()) + 1.0 - k)
        drawn, tr, codes = np.array(drawn), A[:, 9], np.array(codes)
        ind = tr <= 0.02
        gi = drawn[ind] - tr[ind]
        mg = margin if s == 0 else margin1
        # tight spaces (attic and wall voids): the mean, and at most 1% open-sky bright (a rooms-grid leak to room 0 there,
        # e.g. the Concord Hardware attic at -58744 69998 6973: GIBLACK1's law draws it the same)
        okI = (not ind.any()) or (gi.mean() <= mg and (drawn[ind] >= 0.5).sum() <= (0 if s == 0 else 0.01 * ind.sum())
                                  and (s == 1 or (gi > 0.1).sum() <= 0.05 * ind.sum()))
        black = int(((drawn < 0.01) & (tr >= 0.03)).sum())
        bright = int(((drawn >= 0.5) & (tr < 0.25)).sum())
        okR = s == 1 or (black == 0 and bright == 0)   # tight spaces: the indoors rule only, the rest reported
        good = good and okI and okR
        print('S  %-5s %3d points (%d stranded here): indoors by the tracer %d: tracer %.3f drawn %.3f gain %+.3f (margin %.3f), '
              'drawn 0.5+ %d, 0.1+ over %d; the rest %d: tracer %.3f drawn %.3f, black %d open-sky bright %d: %s'
              % (name, len(A), st, ind.sum(), tr[ind].mean() if ind.any() else 0, drawn[ind].mean() if ind.any() else 0,
                 gi.mean() if ind.any() else 0, mg, (drawn[ind] >= 0.5).sum(), (gi > 0.1).sum(), (~ind).sum(),
                 tr[~ind].mean() if (~ind).any() else 0, drawn[~ind].mean() if (~ind).any() else 0, black, bright,
                 'ok' if okI and okR else 'OFF'))
        print('   branches %s (5 trilinear, 6 room blend, 7 room fill, 8 seen fill, 9 sealed dark; +10 tie; 0 not stranded)'
              % {int(c): int((codes == c).sum()) for c in sorted(set(codes))})
    if expect == 'fail':
        print('%s  red strand: %s' % ('PASS' if not good else 'FAIL', 'stranded indoor surfaces take sky' if not good else 'the stranded surfaces still match'))
        return 0 if not good else 1
    print('%s  strand: stranded surfaces take their measured sky (indoors none)' % ('PASS' if good else 'FAIL'))
    return 0 if good else 1


def points_cmd(dump, fixture, box, max_x, dark_share, expect, up_share=-1.0, min_med=0.0):
    """Lane GIDARK1: surface points against the tracer's GI term, from a run's GI dump (WW_CELL_GI_DUMP). The drawn term
    is what the shader composes on a lit surface: the grid (sky + bounce, store units) + pi x DALC(N) x (1 - k).
    In the box (fixture px py; empty: every point): R the drawn mean is at most max_x times the tracer's mean (0: no
    bar); D at most dark_share of the points draw under a tenth of their tracer value (no bar when dark_share < 0).
    up_share >= 0: only up-facing points (N.z > 0.9) whose tracer sky share (fixture column 10) is at least that
    (the floor the open front lights); M the median drawn/tracer ratio is at least min_med."""
    G = read_grid(dump + '/gi_grid.bin', dump + '/gi_slots.bin')
    R = read_rooms(dump + '/gi_rooms.bin')
    amb = {int(l.split()[1]): np.array(list(map(float, l.split()[2:5]))) for l in open(dump + '/gi_sky.txt') if l.startswith('amb')}
    F = np.loadtxt(fixture, comments='#')
    Y = np.array([0.2125, 0.7154, 0.0721])
    if box:
        x0, y0, x1, y1 = box
        F = F[(F[:, 0] >= x0) & (F[:, 0] < x1) & (F[:, 1] >= y0) & (F[:, 1] < y1)]
    if up_share >= 0:
        F = F[(F[:, 7] / np.linalg.norm(F[:, 5:8], axis=1) > 0.9) & (F[:, 9] >= up_share)]
    drawn = []
    for row in F:
        P, N = row[2:5], row[5:8] / np.linalg.norm(row[5:8])
        STRAND_LAST[0] = None
        s = gi_sample(G, R, P, N, fill=False, sky=True)
        k = float(np.clip(s[3], 0.0, 1.0))
        dalc = sum(N[i] ** 2 * amb[2 * i + (0 if N[i] >= 0 else 1)] for i in range(3))
        drawn.append(float((s[:3] + np.pi * dalc * (1.0 - k)) @ Y))
    drawn, tr = np.array(drawn), F[:, 8]
    x = drawn.mean() / max(tr.mean(), 1e-9)
    dark = int((drawn < 0.1 * tr).sum())
    okR = max_x <= 0 or x <= max_x
    okD = dark_share < 0 or dark <= dark_share * len(F)
    print('R  %d points: tracer %.4f drawn %.4f (x%.2f, bar %s) median ratio %.2f: %s'
          % (len(F), tr.mean(), drawn.mean(), x, ('<= %.2f' % max_x) if max_x > 0 else 'none',
             float(np.median(drawn / np.maximum(tr, 1e-9))), 'ok' if okR else 'OVER'))
    print('D  under a tenth of the tracer: %d (%.1f%%, bar %s): %s'
          % (dark, 100.0 * dark / max(len(F), 1), ('%.0f%%' % (100 * dark_share)) if dark_share >= 0 else 'none',
             'ok' if okD else 'DARK'))
    med = float(np.median(drawn / np.maximum(tr, 1e-9)))
    okM = min_med <= 0 or med >= min_med
    if min_med > 0:
        print('M  median drawn/tracer %.2f (bar >= %.2f), under half the tracer %d: %s' % (med, min_med, int((drawn < 0.5 * tr).sum()), 'ok' if okM else 'LOW'))
    good = okR and okD and okM
    if expect == 'fail':
        print('%s  points (red): %s' % ('PASS' if not good else 'FAIL', 'the old law misses the bar' if not good else 'the red passes the bar'))
        return 0 if not good else 1
    print('%s  points: the surfaces draw their measured GI' % ('PASS' if good else 'FAIL'))
    return 0 if good else 1


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
    if a and a[0] == 'strand':
        sys.exit(strand_cmd(a[1], a[2], float(opt('--margin', '0.03')), float(opt('--margin1', '0.05')), opt('--expect', 'pass')))
    if a and a[0] == 'points':
        bx = opt('--box')
        sys.exit(points_cmd(a[1], a[2], [float(v) for v in bx.split(',')] if bx else None, float(opt('--maxx', '0')),
                            float(opt('--darkshare', '-1')), opt('--expect', 'pass'), float(opt('--upshare', '-1')),
                            float(opt('--minmed', '0'))))
    if a and a[0] == 'recess':
        sys.exit(recess_cmd(a[1], a[2], float(opt('--lo', '0.5')), float(opt('--hi', '2.0')), opt('--expect', 'pass')))
    print(__doc__)
    sys.exit(2)
