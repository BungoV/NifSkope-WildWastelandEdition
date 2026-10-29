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
  links <dump> <viewdump> [--red nosee|firstslot] [--expect fail]
        Lane VOLVIEW2: the PRTP Debug View's probe links (WW_CELL_VOLUMES_VIEW_DUMP's probe lines) redone here from the
        run's GI dump (gi_probes, gi_grid, gi_slots, gi_rooms): per probe an up-facing surface at it reads its room
        (P + 0.75 cell, else 1.75; both solid: no links), sees the 8 voxels of its blend from the first air read
        (gi_seen), takes the slots of its room (both across a threshold) that hold light; two probes are linked when
        they share a voxel slot, "t" when their rooms differ. Every probe's room and link list must equal the view's.
        --red: the mirror takes the shader's red (a view dump made with WW_CELL_PV_RED=nosee must equal --red nosee);
        --expect fail: the lists must differ (the default mirror against a red view dump).
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
  giwall [--expect fail]
        Lane GIWALL2: synthetic, no exe, no game data. A store open to the street (one room with it, as Concord's
        hardware store is), 267-unit GI voxels, 16-unit room cells, every wall one cell thick; voxels whose centre is in
        the store's air hold the dark indoor light, every other voxel the sunlit street's. Both GI paths (the march,
        gi_sample; the volumes, gi_volume), each with the fix and with the red (GI_CHECK_GIWALL=0, the shader's
        WW_CELL_GIWALL_RED=off) in the same run:
          wall   the street wall and the ceiling hold a voxel column and layer (their centres in solid): a ceiling
                 point by the wall reads the indoor value (within 1%) with the fix, 90%+ of the street's with the red
          thin   the column and layer stand in open air just past a thin wall and ceiling: the fix reads the indoor
                 value; the red leaks (the march 5%+: its taps stepped over the wall; the volumes 25%+: no test)
          recess an outdoor recess 300 deep, 40 wide, cut into a solid block that holds a sealed dark pocket: no voxel
                 centre is seen from the back wall; the fix takes the buried voxels (centres in the block's wall it
                 faces), the street's light within 1%, never the pocket's black (GIBLACK1)
          pocket the recess read as an interior grid (the gap fill on): the room fill's buried voxels, within 1%
          taps   the red's taps are GISHADE1's exactly: 20000 random segments, gi_wall under the red == the old rule
        Run with GI_CHECK_GIWALL=0 (the fix rows take the red) and --expect fail: it FAILS.
  blotch <dump> <shots dir> --tag T --box x0,y0,x1,y1 --center cx,cy,cz [--max 400] [--osample 3000]
         [--nface mean|fit|both] [--why] [--expect fail]
        Lane GISUNBLOTCH1: the GI pass's dark points on one wall (WW_CELL_PASS=1, sum of RGB < 90 in the box) against their
        lit neighbours on the same wall (lit, within 40 px, face normal within 20 degrees, within 30 units of the dark
        point's plane), redone here (gi_volume, the GI pass's read: the room grid's volume at 0.75 / 1.75 cell) from the
        run's GI dump at the pixels' points: P from probes 2/3 (hi x 256 + lo - 32768 + 0.5 + the notes' center=), the
        shading normal from probe 4. The normal without the normal map (the shader takes the vertex normal): --nface
        mean (the default: the mean shading normal of the points within 6 px and 40 units, within 60 degrees of the
        centre's), fit (round 1's plane fit of the decoded points within 5 px: 514 of 3600 on run1), both (the fit, else
        the mean). Pictures <T>_p2/p3/p4/p95/passgi.png in <shots dir>. The fix is GI_CHECK_GEON's law (geon_parse;
        unset: the shader's, the corner set; r1: round 1's room, view point and guard; 0: the red, today's law).
          X  the twin under the red (every read along the shading normal) draws at least half the dark points under 0.5x
             their lit neighbours: it reproduces the defect (else NOT REPRODUCED: the dark read is not the one modelled)
          K  the cause (round 2), under the red: the share of the dark and of the lit points whose corner set the bump
             moves (the floor of P + N x half a voxel is not the face normal's), their kept weight (pass 0, seen) and the
             corners the wall test refused; then the dark points split into moved / not moved, red and fix in the band
          C  the chain under the red, per dark point: which read named the room (0.75 / 1.75 / none) and its kind, the
             view point, the paths (and the picture's probe 95 paths), what the fix moves (the room, the view point, the
             corner set), and the corner that carries most of the kept weight (behind the face's plane, an indoor slot)
          F  with the fix at least 80% of the dark points read 0.5x..2x of their lit neighbours
          R  the red keeps them dark: at most half in that band
          O  outside the box (--osample px spread over the frame, 0: none): the fix moves at most 0.5% of the GI pass
             px by more than 4 levels against the red (cellPassOut's curve on the twin's sample; the live judge's O is
             the gate, this is its offline stand-in)
          W  the wall's lit points: the share the fix moves by more than 5% (reported)
          A  --why: one line per arm (BLOTCH_ARMS: each read on the face's normal alone, all of them, the wall test
             off): F, O, W. The arm that reaches F >= 80% with O <= 0.5% names the read that carries the defect.
        Run with GI_CHECK_GEON=0 (the fix rows take the red) and --expect fail: it FAILS.
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
            fo = gi_fill(G, Ls, P, N, True, R, frm)
            if fo[3] > 0.01:
                GI_FILLED[0] += 1
                return fo
            if out is not None:
                return out
        if not SEE_RED[0] and (not lend or 0 in room_at(R, frm)):
            so = gi_seen_fill(G, R, P, N, frm, sky)
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


def gi_fill(G, L, P, N, rooms, R=None, frm=None):
    """cellGiFillAt (lane GICAL1): the weighted mean of the valid voxels of the 4x4x4 block round the sample (within
    2.5 voxels, weight (1 - d / 2.5)^2), of room L's slots when the grid has rooms (L None there: nothing).
    R, frm (lane GIWALL2): a room's fill keeps only the voxels seen from frm (gi_wall), none seen the buried ones, never
    one behind a wall; GI_CHECK_GIWALL=0: every one."""
    dm = np.array(G['dims'])
    g = (P + N * 0.5 * G['voxel'] - G['origin']) / G['voxel'] - 0.5
    i0 = np.floor(g).astype(int)
    sl = (0 if N[0] >= 0 else 1, 2 if N[1] >= 0 else 3, 4 if N[2] >= 0 else 5)
    n2 = N * N
    s, ws, sb, wb = np.zeros(4), 0.0, np.zeros(4), 0.0
    see = rooms and R is not None and frm is not None and not GIWALL_RED[0] and not SEE_RED[0]
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
        sn = gi_wall(R, frm, G['origin'] + (c + 0.5) * G['voxel'], True) if see else 2
        if sn == 0:
            continue   # lane GIWALL2: a wall between the surface and the voxel's centre
        w *= w
        if sn == 1:
            sb, wb = sb + w * v, wb + w   # its centre in the wall the surface faces: the last answer
            continue
        s += w * v
        ws += w
    return s / ws if ws > 0 else sb / wb if wb > 0 else np.zeros(4)


TIE_RED = [os.environ.get('GI_CHECK_TIE', '1') == '0']   # lane GIDARK1: red tiepick (WW_CELL_PV_RED=tiepick): a tie takes the answer nearer the plain trilinear (GIBLACK2)
LONE_RED = [os.environ.get('GI_CHECK_LONE', '1') == '0']   # lane GIDARK1: red lonesky (WW_CELL_PV_RED=lonesky): a buried surface with only outdoor evidence takes the outdoor answer
BOTH_RED = [os.environ.get('GI_CHECK_BOTH', '1') == '0']   # lane GIDARK1: red firstslot (WW_CELL_PV_RED=firstslot): a two-room read takes the voxel's first matching slot alone
RED_EMPTY = [False]   # red emptyslot: the twin weighs a room's empty slots (the old shader: black, the no-probe marker)
SEE_RED = [os.environ.get('GI_CHECK_SEE', '1') == '0']   # lane GISHADE1: red nosee (the shader's WW_CELL_PV_RED=nosee)
LASTFILL_RED = [os.environ.get('GI_CHECK_LASTFILL', '1') == '0']   # lane WALLSTACK1: red nolastfill (the shader's WW_CELL_PV_RED=nolastfill)
def geon_parse(v):
    """Lane GISUNBLOTCH1: which of the volumes' reads take the normal without the normal map (cellGiReadN). '0' or '':
    none (the red WW_CELL_PV_RED=bumpn, today's law); '1' (unset): the shader's law, round 2's corner set; 'r1': round
    1's law (room, frm, guard); else a comma list of: room (the volume read, cellGiVolAt), frm (the wall test's view
    point), guard (the third-cell guard), set (the corner set: the voxels round P + Nf x half a voxel, the weights
    from the shading normal's point held inside it), point (the whole sample point, the weights too), slab (the
    axis slabs n2 / sl)."""
    v = (v or '').strip()
    if v in ('', '0'):
        return frozenset()
    if v == '1':
        return frozenset(['set'])
    if v == 'r1':
        return frozenset(['room', 'frm', 'guard'])
    return frozenset(t.strip() for t in v.split(',') if t.strip())


GEON = [geon_parse(os.environ.get('GI_CHECK_GEON', '1'))]   # lane GISUNBLOTCH1: the reads on the face's normal; GI_CHECK_GEON=0: the red bumpn (WW_CELL_PV_RED=bumpn), every read along the shading normal
GIWALL_RED = [os.environ.get('GI_CHECK_GIWALL', '1') == '0']   # lane GIWALL2: red WW_CELL_GIWALL_RED=off (GISHADE1's taps, no wall test in the fill or the volumes)
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
        fo = gi_fill(G, L, P, N, True, R, Ce)
        if fo[3] > 0.01:
            return fo, 7
    if not SEE_RED[0]:
        so = gi_seen_fill(G, R, P, N, Ce, True)   # a stranded surface: a sky-lit grid
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


def gi_wall(R, frm, C, loose=False):
    """cellGiWall (lane GIWALL2): 2 seen (no solid fine cell on the segment from the view point to the voxel's centre,
    the centre air), 1 buried (loose only: the centre in the wall the surface faces, air up to the first solid tap and
    solid from there on), 0 not seen (a solid centre, or air past a wall). The taps run to the centre, never more than
    half a cell apart on any axis. GI_CHECK_GIWALL=0: GISHADE1's taps (every half cell along the segment, at most 12,
    the centre left out), 2 or 0."""
    d = np.asarray(C, float) - frm
    red = GIWALL_RED[0]
    if red:
        nt = min(max(int(math.ceil(float(np.linalg.norm(d)) / (0.5 * R['cell']))), 1), 12)
    else:
        nt = min(max(int(math.ceil(float(np.abs(d).max()) / (0.5 * R['cell']))), 1), 255)
    hit = False
    for j in range(1, (nt if red else nt + 1) + 1):
        s = room_solid(R, frm + d * (j / (nt + 1.0)))
        if s and (red or not loose):
            return 0
        if hit and not s:
            return 0
        hit = hit or s
    return 1 if hit else 2


def gi_seen(R, frm, C):
    """cellGiSeen (lane GISHADE1): no solid fine cell on the segment from the surface's view point to a voxel's centre;
    lane GIWALL2 (gi_wall): its centre in air too, the taps never more than half a cell apart."""
    if SEE_RED[0] or R is None or frm is None:
        return True
    return gi_wall(R, frm, C) == 2


def gi_seen_fill(G, R, P, N, frm, sky=False):
    """cellGiSeenFill (lane GISHADE1): no room's blend has weight: the voxels of the 4x4x4 block the surface sees
    (any room), weight (1 - d / 2.5)^2 in voxels, before the plain trilinear (which sees through walls).
    Lane GIWALL2: none seen, the buried voxels (gi_wall 1: an outdoor recess, a pocket), never one behind a wall;
    GI_CHECK_GIWALL=0: none. sky: kept for the callers (the rule is the same in every grid)."""
    dm = np.array(G['dims'])
    g = (P + N * 0.5 * G['voxel'] - G['origin']) / G['voxel'] - 0.5
    i0 = np.floor(g).astype(int)
    sl = (0 if N[0] >= 0 else 1, 2 if N[1] >= 0 else 3, 4 if N[2] >= 0 else 5)
    n2 = N * N
    s, ws, sb, wb = np.zeros(4), 0.0, np.zeros(4), 0.0
    loose = not SEE_RED[0] and not GIWALL_RED[0]
    for k in range(64):
        c = i0 + np.array([k & 3, (k >> 2) & 3, (k >> 4) & 3]) - 1
        if (c < 0).any() or (c >= dm).any():
            continue
        w = 1.0 - float(np.linalg.norm(c - g)) / 2.5
        if w <= 0:
            continue
        see = 2 if (SEE_RED[0] or R is None or frm is None) else gi_wall(R, frm, G['origin'] + (c + 0.5) * G['voxel'], loose)
        if see == 0:
            continue
        v = sum(n2[a] * G['g'][sl[a], c[2], c[1], c[0]] for a in range(3))
        if v[3] <= 0:
            continue
        if see == 1:
            sb += w * w * v
            wb += w * w
            continue
        s += w * w * v
        ws += w * w
    return s / ws if ws > 0 else sb / wb if wb > 0 else np.zeros(4)


def vol_at(R, P, N):
    """cellGiVolAt (lane PERF1): the fine room grid's two names at P + N x 0.75 cell, else 1.75; (-2, -2): unknown."""
    for k in (0.75, 1.75):
        L = room_at(R, P + N * k * R['cell'])
        if L[0] >= 0:
            return L
    return (-2, -2)


def gi_volume(G, R, P, N, L=None, sky=False, Ng=None, trace=None, use=None):
    """cellGiVolume (lane PERF1, the default GI visibility: WW_CELL_GI_VIS unset): the 8 corners' slots of the pixel's
    volume L (None: vol_at, the read a pixel without the shapes' mask takes), none with weight: the 27 round the nearest
    corner; a threshold's other side (G['thr'], per voxel (t0, t1, other0, other1); absent: none) blended in by t / 2;
    nothing: the plain trilinear. Outdoors (sky) a surface in no volume stands in the outdoors' (room 0), and buried a
    third cell deep takes the darker of that and the trilinear. Lane GIWALL2: every pass keeps only the voxels seen
    from see_from (gi_wall); pass 1 keeps the buried apart as the last answer before the trilinear. The edge
    fade (cellGiEdgeFade) is left out: a fixture keeps clear of the grid's pad. Returns (rgba, probe 95 path).
    Ng (lane GISUNBLOTCH1, cellGiReadN): the normal without the normal map; the reads `use` names (None: GEON[0], from
    GI_CHECK_GEON; geon_parse) take it when it faces N's side; Ng None, or GI_CHECK_GEON=0: every read along N
    (today's law). The shader's law is the corner set ('set'). trace: a dict to fill (L, frm, No, i0 the corner set,
    moved: the shading normal's point outside it, and per corner tested: ps, cell, slot rooms, the slot used, its a,
    its luminance / a, the wall test, the weight)."""
    P, N = np.asarray(P, float), np.asarray(N, float)
    dm = np.array(G['dims'])
    use = GEON[0] if use is None else use
    if Ng is None or float(np.dot(Ng, N)) <= 0.0:
        use = frozenset()
    Nf = np.asarray(Ng, float) if use else N
    on = lambda k: Nf if k in use else N
    # lane GISUNBLOTCH1: the corner set round the point along Nf ('set'; 'point': the weights' point too), the weights
    # from the shading normal's point, held 1/64 inside the set on an axis where the tilt takes it out (cellGiVolume gf)
    g = (P + on('point') * 0.5 * G['voxel'] - G['origin']) / G['voxel'] - 0.5
    gs = (P + (Nf if ('set' in use or 'point' in use) else N) * 0.5 * G['voxel'] - G['origin']) / G['voxel'] - 0.5
    i0 = np.floor(gs).astype(int)
    f = g - i0
    out = (f < 0.0) | (f >= 1.0)
    f = np.where(out, np.clip(f, 1.0 / 64.0, 63.0 / 64.0), f)
    r0 = np.floor(gs + 0.5).astype(int)
    it0 = np.floor(g).astype(int)   # cellGiTrilinear reads its own point (the shading normal's)
    ft = g - it0
    Ns = on('slab')
    sl = (0 if Ns[0] >= 0 else 1, 2 if Ns[1] >= 0 else 3, 4 if Ns[2] >= 0 else 5)
    n2 = Ns * Ns
    rooms = R is not None and 'slots' in G
    No = on('room')
    if L is None:
        L = vol_at(R, P, No) if rooms else (-2, -2)
    strand = rooms and sky and L[0] == -2
    if strand:
        L = (0, -1)
    any_room = not rooms or L[0] == -2
    wall = rooms and not GIWALL_RED[0] and not SEE_RED[0]
    frm = see_from(R, P, on('frm')) if wall else P
    if trace is not None:
        trace.update(L=tuple(int(v) for v in L), frm=np.asarray(frm, float), No=No, i0=tuple(int(v) for v in i0),
                     moved=bool(out.any()), corners=[])
    own, oth, bur = np.zeros(4), np.zeros(4), np.zeros(4)
    w_own = w_oth = t_acc = t_best = w_bur = 0.0
    other, near = -1, False
    thr = G.get('thr')
    for ps in range(3):
        if ps == 1 and w_own > 0:
            continue
        if ps == 2 and (other < 0 or t_acc <= 0 or w_own <= 0):
            break
        Rm = (other, other) if ps == 2 else L
        for k in range(27 if ps == 1 else 8):
            if ps == 1:
                c = r0 + np.array([k % 3, (k // 3) % 3, k // 9]) - 1
                dd = c - g
                w = 1.0 / (1.0 + float(dd @ dd))
            else:
                o = np.array([k & 1, (k >> 1) & 1, (k >> 2) & 1])
                c = i0 + o
                w = float(np.prod(np.where(o == 1, f, 1.0 - f)))
            if w <= 0 or (c < 0).any() or (c >= dm).any():
                continue
            S = G['slots'][c[2], c[1], c[0]] if rooms else (-1, -1)
            if any_room and ps < 2:
                q = 0
            elif S[0] >= 0 and S[0] in Rm:
                q = 0
            elif S[1] >= 0 and S[1] in Rm:
                q = 1
            else:
                continue
            src = G['g'] if q == 0 else G['g2']
            u = sum(n2[a] * src[sl[a], c[2], c[1], c[0]] for a in range(3))
            if u[3] <= 0:
                continue
            see = gi_wall(R, frm, G['origin'] + (c + 0.5) * G['voxel'], ps == 1) if wall else 2
            if trace is not None:
                trace['corners'].append((ps, tuple(int(v) for v in c), (int(S[0]), int(S[1])), q, float(u[3]),
                                         float(u[:3] @ (0.2126, 0.7152, 0.0722)) / max(float(u[3]), 1e-9), int(see), w))
            if see == 0:
                continue
            if see == 1:
                bur, w_bur = bur + w * u, w_bur + w
                continue
            if ps == 2:
                oth, w_oth = oth + w * u, w_oth + w
                continue
            own, w_own = own + w * u, w_own + w
            near = near or ps == 1
            if ps == 0 and not any_room and thr is not None:
                tv = thr[c[2], c[1], c[0]]
                t, oq = float(tv[q]), int(tv[2 + q])
                t_acc += w * t
                if oq >= 0 and t > t_best:
                    t_best, other = t, oq
    if w_own <= 0 and w_bur > 0:
        own, w_own, near = bur, w_bur, True
    s = own / w_own if w_own > 0 else np.zeros(4)
    if w_oth > 0:
        s = s + (oth / w_oth - s) * 0.5 * min(max(t_acc / w_own, 0.0), 1.0)
    path = 4 if any_room else (2 if near else 1) + (5 if strand else 0)
    lone = strand and w_own > 0 and room_solid(R, P + on('guard') * 2.75 * R['cell'])
    if w_own <= 0 or lone:
        tr, _ = gi_blend(G, None, it0, ft, sl, n2)
        lum = lambda v: float(v[:3] @ (0.2126, 0.7152, 0.0722))
        if w_own <= 0 or (tr[3] > 0 and lum(tr) / tr[3] < lum(s) / max(float(s[3]), 1e-6)):
            return tr, (5 if strand and w_own <= 0 else 4)
    return s, path


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


def links_mirror(dump, red):
    # lane VOLVIEW2: src/gl/cellvolumeview.cpp wwCellProbeLinksBuild, in plain floats (doubles, as there)
    b = open(dump + '/gi_probes.bin', 'rb').read()
    n = struct.unpack_from('<i', b, 0)[0]
    P = np.frombuffer(b, '<f4', n * 21, 4).reshape(n, 21)[:, :3].astype(float)
    G = read_grid(dump + '/gi_grid.bin', dump + '/gi_slots.bin')
    R = read_rooms(dump + '/gi_rooms.bin')
    go, vox, gd = [float(v) for v in G['origin']], float(G['voxel']), G['dims']
    ro, rc, rd = [float(v) for v in R['origin']], float(R['cell']), R['dims']
    ab = R['ab'].tolist()
    S = G['slots'].tolist()
    A = [G['g'][4, :, :, :, 3].tolist(), G['g2'][4, :, :, :, 3].tolist()]

    def cell(q):
        c = [int(math.floor((q[i] - ro[i]) / rc)) for i in range(3)]
        if any(c[i] < 0 or c[i] >= rd[i] for i in range(3)):
            return None
        return c

    def room(q):
        c = cell(q)
        return (-1, -1) if c is None else tuple(ab[c[2]][c[1]][c[0]])

    def solid(q):
        c = cell(q)
        return c is not None and ab[c[2]][c[1]][c[0]][0] < 0

    rooms, keys, dropped = [], [], []
    for p in range(n):
        x, y, z = P[p]
        q1, q2 = (x, y, z + 0.75 * rc), (x, y, z + 1.75 * rc)
        L = room(q1)
        if L[0] < 0:
            L = room(q2)
        rooms.append(L)
        ks, dr = set(), 0
        if L[0] >= 0:
            frm = q1 if not solid(q1) else q2
            g = [(x - go[0]) / vox - 0.5, (y - go[1]) / vox - 0.5, (z + 0.5 * vox - go[2]) / vox - 0.5]
            i0 = [int(math.floor(v)) for v in g]
            f = [g[i] - i0[i] for i in range(3)]
            for k in range(8):
                o = (k & 1, (k >> 1) & 1, (k >> 2) & 1)
                c = [min(max(i0[i] + o[i], 0), gd[i] - 1) for i in range(3)]
                w = 1.0
                for i in range(3):
                    w *= f[i] if o[i] else 1.0 - f[i]
                s0, s1 = S[c[2]][c[1]][c[0]]
                base = 0 if s0 >= 0 and s0 in L else (1 if s1 >= 0 and s1 in L else -1)
                if base < 0 or w <= 0.0:
                    continue
                if red != 'nosee':
                    d = [go[i] + (c[i] + 0.5) * vox - frm[i] for i in range(3)]
                    nt = min(max(int(math.ceil(math.sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]) / (0.5 * rc))), 1), 12)
                    if any(solid([frm[i] + d[i] * (j / float(nt + 1)) for i in range(3)]) for j in range(1, nt + 1)):
                        dr += 1
                        continue
                two = red != 'firstslot' and L[0] >= 0 and L[1] >= 0 and L[0] != L[1] and base == 0 and s1 >= 0 and s1 in L
                vi = c[0] + gd[0] * (c[1] + gd[1] * c[2])
                for slot in ((0, 1) if two else (base,)):
                    if A[slot][c[2]][c[1]][c[0]] > 0.0:
                        ks.add(vi * 2 + slot)
        keys.append(ks)
        dropped.append(dr)
    readers = {}
    for p, ks in enumerate(keys):
        for k in ks:
            readers.setdefault(k, []).append(p)
    out = []
    for p, ks in enumerate(keys):
        nb = set()
        for k in ks:
            nb.update(readers[k])
        nb.discard(p)
        out.append((rooms[p], dropped[p], sorted((q, 't' if rooms[q][0] != rooms[p][0] else 'r') for q in nb)))
    return out


def read_view_links(path):
    out = {}
    for l in open(path):
        if not l.startswith('probe '):
            continue
        w = l.split()
        a, b = w[w.index('room') + 1].split(',')
        lk = w[w.index('links') + 1:]
        out[int(w[1])] = ((int(a), int(b)), int(w[w.index('dropped') + 1]),
                          sorted((int(e.split(':')[0]), e.split(':')[1]) for e in lk))
    return out


def links_cmd(dump, view, red, expect):
    M = links_mirror(dump, red)
    V = read_view_links(view)
    bad = [p for p in range(len(M)) if V.get(p) != M[p]]
    nl = sum(len(m[2]) for m in M)
    nt = sum(1 for m in M for e in m[2] if e[1] == 't')
    print('L  %d probes in the dump, %d in the view; mirror (red %s): %d link ends (%d through thresholds), %d probes with a '
          'room, %d voxels dropped by the seen filter; %d probes differ'
          % (len(M), len(V), red or 'none', nl, nt, sum(1 for m in M if m[0][0] >= 0), sum(m[1] for m in M), len(bad)))
    for p in bad[:5]:
        print('   probe %d view %s mirror %s' % (p, str(V.get(p))[:160], str(M[p])[:160]))
    same = not bad and len(V) == len(M) and len(M) > 0
    if expect == 'fail':
        print('%s  links (red): %s' % ('PASS' if not same else 'FAIL', 'the lists differ' if not same else 'the red view equals the mirror'))
        return 0 if not same else 1
    print('%s  links: the view draws the probes each probe works with' % ('PASS' if same and nl > 0 else 'FAIL'))
    return 0 if same and nl > 0 else 1
GW_V, GW_C = 267.0, 16.0          # lane GIWALL2's fixture: Concord's voxel, the room grid's default cell
GW_IN, GW_OUT = 0.05, 2.0          # the store's own light, the sunlit street's


def giwall_scene(kind):
    """Lane GIWALL2's fixture grids (G, R) and its point (P, N). kind 'wall': the street wall (x 1200..1216) and the
    ceiling (z 656..672) hold the voxel column x 1201.5 and layer z 667.5; 'thin': both a cell short of them (x 1184,
    z 640), the column and layer in open air past the wall; 'recess': a solid block (x 800..1400) with a recess cut in
    from the street (x 800..1100, y 700..740, z 100..500) and a sealed pocket (x 1150..1350, z 200..600). Every air
    cell is room 0 (the store open to the street through its door: one room), every voxel slot room 0."""
    gd = (8, 8, 6)
    rd = tuple(int(math.ceil(gd[a] * GW_V / GW_C)) + 1 for a in range(3))
    ab = np.zeros((rd[2], rd[1], rd[0], 2), np.int16)
    ab[..., 1] = -1
    R = {'origin': np.zeros(3), 'cell': GW_C, 'dims': rd, 'rooms': 1, 'ab': ab}

    def put(x0, x1, y0, y1, z0, z1, v):
        i = lambda q: int(q // GW_C)
        ab[i(z0):i(z1), i(y0):i(y1), i(x0):i(x1), 0] = v
    lo = 384.0
    if kind == 'recess':
        put(800, 1400, lo, 1216, 0, 800, -1)
        put(800, 1100, 700, 740, 100, 500, 0)
        put(1150, 1350, 600, 900, 200, 600, 5)   # the pocket: a room of its own, sealed
        inside = lambda c: 1150 <= c[0] < 1350 and 600 <= c[1] < 900 and 200 <= c[2] < 600
        P, N = (1100.0, 720.0, 300.0), (-1.0, 0.0, 0.0)
    else:
        wx, cz = (1200.0, 656.0) if kind == 'wall' else (1184.0, 640.0)
        put(lo, wx + GW_C, lo, 1216, 0, 16, -1)
        put(lo, wx + GW_C, lo, 1216, cz, cz + GW_C, -1)
        for x0, x1, y0, y1 in ((wx, wx + GW_C, lo, 1216), (lo, lo + GW_C, lo, 1216), (lo, wx + GW_C, 1200, 1216), (lo, wx + GW_C, lo, lo + GW_C)):
            put(x0, x1, y0, y1, 0, cz + GW_C, -1)
        put(600, 800, lo, lo + GW_C, 16, 300, 0)   # the open front door, on the far side
        inside = lambda c: lo + GW_C <= c[0] < wx and lo + GW_C <= c[1] < 1200 and 16 <= c[2] < cz
        P, N = ((1180.0, 700.0, cz - 0.1) if kind == 'wall' else (964.0, 700.0, cz - 0.1)), (0.0, 0.0, -1.0)
    nv = (gd[2], gd[1], gd[0])
    g = np.zeros((6,) + nv + (4,), np.float32)
    for z in range(gd[2]):
        for y in range(gd[1]):
            for x in range(gd[0]):
                c = (np.array([x, y, z]) + 0.5) * GW_V
                e = (0.0 if kind == 'recess' else GW_IN) if inside(c) and not room_solid(R, c) else GW_OUT
                g[:, z, y, x] = (e, e, e, 1.0)
    G = {'origin': np.zeros(3), 'voxel': GW_V, 'dims': gd, 'nv': gd[0] * gd[1] * gd[2], 'g': g,
         'slots': np.tile(np.array([0, -1], np.int32), nv + (1,)), 'g2': np.zeros_like(g)}
    return G, R, np.array(P), np.array(N)


def giwall_old_seen(R, frm, C):
    """GISHADE1's cellGiSeen as it was before lane GIWALL2 (the red's reference)."""
    d = np.asarray(C, float) - frm
    nt = min(max(int(math.ceil(float(np.linalg.norm(d)) / (0.5 * R['cell']))), 1), 12)
    return not any(room_solid(R, frm + d * (j / (nt + 1.0))) for j in range(1, nt + 1))


def giwall_cmd(expect):
    fix_red = GIWALL_RED[0]   # GI_CHECK_GIWALL=0: the fix rows run the red (the refuter)
    fails, rows = [], []

    def read(G, R, P, N, red, sky=True):
        GIWALL_RED[0] = red
        try:
            m = gi_sample(G, R, P, N, fill=not sky, sky=sky)   # an exterior grid (Concord): sky lit, no gap fill
            v, path = gi_volume(G, R, P, N, sky=sky)
        finally:
            GIWALL_RED[0] = fix_red
        e = lambda s: float(s[0]) if s[3] > COVER_MIN else -1.0   # a sky grid: rgb is E x share (cellGiE0)
        return (e(m), float(m[3])), (e(v), float(v[3]), path)
    span = GW_OUT - GW_IN
    for kind in ('wall', 'thin', 'recess', 'pocket'):
        G, R, P, N = giwall_scene('recess' if kind == 'pocket' else kind)
        sky = kind != 'pocket'   # pocket: the recess read as an interior grid (the gap fill on): its fill's buried voxels
        (mf, vf), (mr, vr) = read(G, R, P, N, fix_red, sky), read(G, R, P, N, True, sky)
        rows.append('  %-6s P (%.1f, %.1f, %.1f) N (%g, %g, %g)  fix: march E %.4f (a %.2f), volumes E %.4f (a %.2f, path %d) | red: march E %.4f, volumes E %.4f'
                    % ((kind,) + tuple(P) + tuple(N) + (mf[0], mf[1], vf[0], vf[1], vf[2], mr[0], vr[0])))
        if kind == 'recess':
            g = (P + N * 0.5 * GW_V - G['origin']) / GW_V - 0.5
            i0 = np.floor(g).astype(int)
            t, _ = gi_blend(G, None, i0, g - i0, (0 if N[0] >= 0 else 1, 2 if N[1] >= 0 else 3, 4 if N[2] >= 0 else 5), N * N)
            rows.append('         no voxel seen: without the buried fallback the plain trilinear, E %.4f (the pocket\'s black in it)' % t[0])
        for name, f, r in (('march', mf, mr), ('volumes', vf, vr)):
            if kind in ('recess', 'pocket'):
                if f[1] <= 0.01 or abs(f[0] - GW_OUT) > 0.01 * GW_OUT:
                    fails.append('%s %s: the fix reads %.4f (a %.2f), not the street\'s %.2f (black, or the pocket took weight)' % (kind, name, f[0], f[1], GW_OUT))
                continue
            if abs(f[0] - GW_IN) > 0.01 * span:
                fails.append('%s %s: the fix reads %.4f, not the indoor %.2f' % (kind, name, f[0], GW_IN))
            floor = 0.9 if kind == 'wall' else 0.05 if name == 'march' else 0.25
            if (r[0] - GW_IN) / span < floor:
                fails.append('%s %s: the red reads %.4f, under %.0f%% of the way to the street\'s %.2f (the fixture shows no leak)'
                             % (kind, name, r[0], 100 * floor, GW_OUT))
    # taps: the red's segments are GISHADE1's exactly
    G, R, _, _ = giwall_scene('wall')
    rng = np.random.default_rng(20261028)
    lo, hi = np.array([300.0, 300.0, 0.0]), np.array([1300.0, 1300.0, 800.0])   # round the store: both kinds of segment
    n_seg, n_diff, n_solid = 0, 0, 0
    GIWALL_RED[0] = True
    try:
        while n_seg < 20000:
            frm = rng.uniform(lo, hi)
            if room_solid(R, frm):
                continue
            C = frm + rng.uniform(-2.5, 2.5, 3) * GW_V
            n_seg += 1
            old = giwall_old_seen(R, frm, C)
            n_solid += not old
            n_diff += old != (gi_wall(R, frm, C) == 2)
    finally:
        GIWALL_RED[0] = fix_red
    rows.append('  taps   %d random segments, %d blocked under the old rule, %d where the red differs from it' % (n_seg, n_solid, n_diff))
    if n_diff or n_solid < 1000 or n_solid > n_seg - 1000:
        fails.append('taps: %d of %d segments differ from GISHADE1 (%d blocked: both kinds must be in the sample)' % (n_diff, n_seg, n_solid))
    print('giwall (lane GIWALL2): voxel %.0f, room cell %.0f, indoor %.2f, street %.2f%s' % (GW_V, GW_C, GW_IN, GW_OUT, ' [fix rows run the red: GI_CHECK_GIWALL=0]' if fix_red else ''))
    print('\n'.join(rows))
    for f in fails:
        print('  FAIL ' + f)
    good = not fails
    if expect == 'fail':
        print('giwall: %s (expected to fail: %s)' % ('PASS' if not good else 'FAIL', 'it failed' if not good else 'it passed'))
        return 0 if not good else 1
    print('giwall: %s' % ('PASS' if good else 'FAIL'))
    return 0 if good else 1


def face_normal(Pimg, ok, y, x, Nb, r=5, reach=40.0):
    # lane GISUNBLOTCH1: the face's normal at pixel (y, x) from the decoded points (whole units): the least-squares plane
    # of the points within r px and reach units of the centre's, on Nb's side. None: under 12 points, or no plane.
    H, W = ok.shape
    ya, yb, xa, xb = max(y - r, 0), min(y + r + 1, H), max(x - r, 0), min(x + r + 1, W)
    Q = Pimg[ya:yb, xa:xb][ok[ya:yb, xa:xb]]
    Q = Q[np.linalg.norm(Q - Pimg[y, x], axis=1) < reach]
    if len(Q) < 12:
        return None
    Q = Q - Q.mean(axis=0)
    _, sv, vt = np.linalg.svd(Q, full_matrices=False)
    if sv[1] < 3.0 * sv[2] + 1e-9:
        return None
    n = vt[2]
    return n if float(n @ Nb) >= 0.0 else -n


BLOTCH_ARMS = [   # lane GISUNBLOTCH1 round 2 (blotch --why): each arm's reads on the face's normal (geon_parse's names)
    ('red: today', ''),
    ('round 1: room, view point, guard', 'r1'),
    ('round 2: the corner set', 'set'),
    ('round 2 + the view point', 'set,frm'),
    ('the whole sample point', 'point'),
    ('the slabs alone', 'slab'),
    ('every read on the face', 'room,frm,guard,point,slab'),
    ('red, no wall test', '!nowall'),
]


def arm_use(spec):
    """Lane GISUNBLOTCH1: an arm's (reads on the face's normal, the wall test off): spec a set, or a geon_parse string
    (a leading '!nowall' also turns the wall test off: GI_CHECK_GIWALL=0's law)."""
    if isinstance(spec, frozenset):
        return spec, False
    nowall = spec.startswith('!nowall')
    return geon_parse(spec[len('!nowall'):].lstrip(',') if nowall else spec), nowall


def pass_px(s):
    """cellPassOut's GI pass (WW_CELL_PASS=1) of a sample, 0..255: pow(clamp(rgb / a / pi), 1 / 2.2); a <= 0.01 the
    no-cover cyan."""
    if s[3] <= 0.01:
        return np.array([0.0, 255.0, 255.0])
    return 255.0 * np.clip(np.maximum(s[:3] / s[3], 0.0) / math.pi, 0.0, 1.0) ** (1.0 / 2.2)


def mean_normal(Nimg, Pimg, ok, y, x, r=6, reach=40.0):
    # lane GISUNBLOTCH1 round 2: the stand-in for the normal without the normal map the shader takes (fo4_default's
    # btnMatrix[2], pbrm's tbn[2]): the mean shading normal of the points within r px and reach units whose normal is
    # within 60 degrees of the centre's (a normal map's tilts average out on a flat wall). None: under 12 points.
    H, W = ok.shape
    ya, yb, xa, xb = max(y - r, 0), min(y + r + 1, H), max(x - r, 0), min(x + r + 1, W)
    m = ok[ya:yb, xa:xb]
    Q, M = Pimg[ya:yb, xa:xb][m], Nimg[ya:yb, xa:xb][m]
    k = (np.linalg.norm(Q - Pimg[y, x], axis=1) < reach) & (M @ Nimg[y, x] > 0.5)
    if int(k.sum()) < 12:
        return None
    nm = M[k].sum(axis=0)
    return nm / max(float(np.linalg.norm(nm)), 1e-9)


def blotch_cmd(dump, shots, tag, box, center, maxpts, expect, why=False, osample=3000, nface='mean'):
    # lane GISUNBLOTCH1: see the head. C: round 1's chain; K: round 2's measurement of the cause (the corner set the
    # bump moves); O: the frame outside the box; A (--why): the arms.
    from PIL import Image
    pic = lambda k: np.asarray(Image.open(os.path.join(shots, '%s_%s.png' % (tag, k))).convert('RGB')).astype(np.int64)
    hi, lo, nb, gp, pp = pic('p2'), pic('p3'), pic('p4'), pic('passgi'), pic('p95')
    Pimg = (hi * 256 + lo).astype(float) - 32768.0 + 0.5 + np.asarray(center, float)
    Nimg = nb / 255.0 * 2.0 - 1.0
    nl = np.linalg.norm(Nimg, axis=2)
    ok = (nl > 0.9) & (nl < 1.1)
    Nimg = Nimg / np.maximum(nl, 1e-6)[..., None]
    gsum = gp.sum(axis=2)
    ppath = np.rint(pp[..., 0] * 16.0 / 255.0).astype(int)
    x0, y0, x1, y1 = [int(v) for v in box]
    inb = np.zeros(ok.shape, bool)
    inb[y0:y1, x0:x1] = True
    dark_all = np.argwhere(ok & inb & (gsum < 90))
    lit_all = np.argwhere(ok & inb & (gsum >= 90))
    if len(dark_all) == 0 or len(lit_all) == 0:
        print('SKIP  blotch: %d dark, %d lit points in the box' % (len(dark_all), len(lit_all)))
        return 0
    pick = lambda A, n: A[np.unique(np.linspace(0, len(A) - 1, min(n, len(A))).astype(int))]
    dark, lit = pick(dark_all, maxpts), pick(lit_all, 8 * maxpts)
    outer = np.argwhere(ok & ~inb)
    osm = pick(outer, osample) if osample > 0 and len(outer) else outer[:0]
    G = read_grid(dump + '/gi_grid.bin', dump + '/gi_slots.bin')
    R = read_rooms(dump + '/gi_rooms.bin')
    Y = np.array([0.2126, 0.7152, 0.0722])
    vox = G['voxel']
    fix = GEON[0]   # the fix rows: GI_CHECK_GEON's reads (unset: the shader's, the corner set); 0: the red (the refuter)
    K = lambda y, x: (int(y), int(x))
    nf, nfit, nmean = {}, {}, {}
    for (y, x) in np.vstack([dark, lit, osm]):
        k = K(y, x)
        if k in nf:
            continue
        nmean[k] = mean_normal(Nimg, Pimg, ok, k[0], k[1])
        nfit[k] = face_normal(Pimg, ok, k[0], k[1], Nimg[k]) if inb[k] else None   # in the box: the B line's check
        nf[k] = nmean[k] if nface == 'mean' else nfit[k] if nface == 'fit' else (nfit[k] if nfit[k] is not None else nmean[k])
    nrm = lambda y, x: nf[K(y, x)] if nf[K(y, x)] is not None else Nimg[int(y), int(x)]
    # the judged dark points: 3+ lit neighbours on their wall (the normals above; the same for every arm)
    lp = lit.astype(float)
    sel_of, idx = {}, []
    for i, (y, x) in enumerate(dark):
        n0 = nrm(y, x)
        sel = [j for j in np.where(np.hypot(lp[:, 0] - y, lp[:, 1] - x) < 40.0)[0]
               if float(nrm(*lit[j]) @ n0) > 0.94 and abs(float((Pimg[lit[j][0], lit[j][1]] - Pimg[y, x]) @ n0)) < 30.0]
        if len(sel) >= 3:
            sel_of[i] = sel
            idx.append(i)
    n = len(idx)
    nk = [k for k in nf if inb[k]]
    ang = [math.degrees(math.acos(min(1.0, abs(float(nfit[k] @ nmean[k]))))) for k in nk
           if nfit[k] is not None and nmean[k] is not None]
    print('B  box %d,%d-%d,%d: dark (sum < 90) %d, judged %d (3+ lit neighbours on their wall), lit sample %d, outside sample %d; '
          'rooms cell %.1f, GI voxel %.1f; the normal without the normal map: %s (in the box: plane fit %d, mean %d of %d; '
          'fit vs mean median %.1f deg)' % (x0, y0, x1, y1, len(dark_all), n, len(lit), len(osm), R['cell'], vox, nface,
                                            sum(nfit[k] is not None for k in nk), sum(nmean[k] is not None for k in nk),
                                            len(nk), float(np.median(ang)) if ang else -1.0))
    if n == 0:
        print('SKIP  blotch: no dark point has lit neighbours on its wall')
        return 0

    def run(pts, spec):
        use, nowall = arm_use(spec)
        keep = GIWALL_RED[0]
        GIWALL_RED[0] = keep or nowall
        res = []
        try:
            for (y, x) in pts:
                tr = {}
                s, pth = gi_volume(G, R, Pimg[y, x], Nimg[y, x], sky=True, Ng=nf[K(y, x)], trace=tr, use=use)
                res.append((float(s[:3] @ Y) / float(s[3]) if s[3] > 0.01 else 0.0, pth, tr, pass_px(s)))
        finally:
            GIWALL_RED[0] = keep
        return res
    Dr, Lr, Or = run(dark, ''), run(lit, ''), run(osm, '')
    Df, Lf, Of = run(dark, fix), run(lit, fix), run(osm, fix)
    ratios = lambda D, L: np.array([D[i][0] / max(float(np.median([L[j][0] for j in sel_of[i]])), 1e-9) for i in idx])
    band = lambda r: float(((r >= 0.5) & (r <= 2.0)).mean()) if len(r) else 0.0
    omove = lambda A, B: float(np.mean([float(np.abs(a[3] - b[3]).max()) > 4.0 for a, b in zip(A, B)])) if len(A) else 0.0
    litmove = lambda L: float(np.mean([abs(L[j][0] / max(Lr[j][0], 1e-9) - 1.0) > 0.05 for j in range(len(lit))]))
    rr, rf = ratios(Dr, Lr), ratios(Df, Lf)
    rep = float((rr < 0.5).mean())
    print('X  the red twin draws %.1f%% of the judged dark points under 0.5x their lit neighbours (median %.3f; bar >= 50%%): %s'
          % (100 * rep, float(np.median(rr)), 'reproduced' if rep >= 0.5 else 'NOT REPRODUCED'))

    # K (round 2): does the bump move the corner set, and is that where the dark points are
    def moved(y, x):
        P, N, Nf = Pimg[y, x], Nimg[y, x], nf[K(y, x)]
        if Nf is None or float(Nf @ N) <= 0.0:
            return False
        a_ = np.floor((P + N * 0.5 * vox - G['origin']) / vox - 0.5)
        b_ = np.floor((P + Nf * 0.5 * vox - G['origin']) / vox - 0.5)
        return bool((a_ != b_).any())

    def kept0(tr):
        c0 = [cr for cr in tr['corners'] if cr[0] == 0]
        return (sum(cr[7] for cr in c0 if cr[6] == 2), sum(1 for cr in c0 if cr[6] == 0))
    mD = np.array([moved(*dark[i]) for i in idx], bool)
    mL = np.array([moved(*p) for p in lit], bool)
    kD = np.array([kept0(Dr[i][2]) for i in idx], float).reshape(-1, 2)
    kL = np.array([kept0(t[2]) for t in Lr], float).reshape(-1, 2)
    print('K  under the red: the bump moves the corner set (the floor of P + N x half a voxel is not the face normal\'s) on '
          '%.1f%% of the judged dark points, %.1f%% of the lit sample; kept weight (pass 0, seen) median dark %.3f, lit %.3f; '
          'corners the wall test refused (pass 0) median dark %.1f, lit %.1f'
          % (100 * float(mD.mean()), 100 * float(mL.mean()), float(np.median(kD[:, 0])), float(np.median(kL[:, 0])),
             float(np.median(kD[:, 1])), float(np.median(kL[:, 1]))))
    for name, m in (('moved by the bump', mD), ('not moved', ~mD)):
        if m.any():
            print('K  dark points whose corner set is %s: %d; red %.1f%% within 0.5x..2x (median %.3f), fix %.1f%% (median %.3f)'
                  % (name, int(m.sum()), 100 * band(rr[m]), float(np.median(rr[m])), 100 * band(rf[m]), float(np.median(rf[m]))))
    # C: the chain under the red (round 1's task 1)
    cnt = lambda it: dict(sorted(((k, sum(1 for v in it if v == k)) for k in set(it)), key=lambda kv: -kv[1]))
    cell = R['cell']
    reads, kinds, frmk, carry = [], [], [], []
    Lmove = Fmove = Smove = 0
    for i in idx:
        y, x = dark[i]
        P, N = Pimg[y, x], Nimg[y, x]
        tr, tf = Dr[i][2], Df[i][2]
        reads.append('0.75' if room_at(R, P + N * 0.75 * cell)[0] >= 0 else '1.75' if room_at(R, P + N * 1.75 * cell)[0] >= 0 else 'none')
        L = tr['L']
        kinds.append('unknown' if L[0] == -2 else 'outdoors' if 0 in L else 'indoor %d' % L[0])
        q = tr['frm']
        frmk.append('0.75' if np.linalg.norm(q - (P + N * 0.75 * cell)) < 0.01 else '1.75' if np.linalg.norm(q - (P + N * 1.75 * cell)) < 0.01 else 'beside' if np.linalg.norm(q - P) > 0.01 else 'P')
        Lmove += tf['L'] != L
        Fmove += float(np.linalg.norm(tf['frm'] - q)) > 1.0
        Smove += tf['i0'] != tr['i0']
        kc = [cr for cr in tr['corners'] if cr[6] == 2 and cr[0] != 2] or [cr for cr in tr['corners'] if cr[6] == 1]
        if kc:
            cr = max(kc, key=lambda c_: c_[7])
            C = G['origin'] + (np.array(cr[1]) + 0.5) * vox
            n0 = nrm(y, x)
            room = cr[2][cr[3]] if cr[3] in (0, 1) else cr[2][0]
            carry.append(('behind the face' if float((C - P) @ n0) < 0.0 else 'in front') + (', indoor slot' if room > 0 else ', outdoor slot' if room == 0 else ''))
        else:
            carry.append('no corner kept')
    print('C  the room read (shading normal): %s; the volume: %s; the view point: %s' % (cnt(reads), cnt(kinds), cnt(frmk)))
    print('C  paths, the red twin %s; the picture (probe 95, the mask) %s; the fix moves the room on %d, the view point on %d, '
          'the corner set on %d of %d' % (cnt([Dr[i][1] for i in idx]), cnt([int(ppath[dark[i][0], dark[i][1]]) for i in idx]),
                                          Lmove, Fmove, Smove, n))
    print('C  the corner carrying the most kept weight: %s' % cnt(carry))
    okF, okR = band(rf), band(rr)
    print('F  fix (%s): %.1f%% of the dark points within 0.5x..2x of their lit neighbours (median %.3f; bar >= 80%%)'
          % (','.join(sorted(fix)) or 'none: the red, GI_CHECK_GEON=0', 100 * okF, float(np.median(rf))))
    print('R  red: %.1f%% within the band (bar <= 50%%: the red keeps them dark)' % (100 * okR))
    of = omove(Of, Or)
    print('O  outside the box (%d px sampled): the fix moves %.2f%% of the GI pass px by more than 4 levels against the red '
          '(bar <= 0.5%%; the offline stand-in, the live judge\'s O is the gate)' % (len(osm), 100 * of))
    print('W  the wall\'s lit sample: the fix moves %.1f%% by more than 5%% (reported)' % (100 * litmove(Lf)))
    if why:
        for name, spec in BLOTCH_ARMS:
            Da, La, Oa = (Dr, Lr, Or) if spec == '' else (run(dark, spec), run(lit, spec), run(osm, spec))
            ra = ratios(Da, La)
            print('A  %-34s F %5.1f%% (median %.3f)  O %5.2f%%  lit moved %5.1f%%'
                  % (name, 100 * band(ra), float(np.median(ra)), 100 * omove(Oa, Or), 100 * litmove(La)))
    good = rep >= 0.5 and okF >= 0.8 and okR <= 0.5 and of <= 0.005
    if expect == 'fail':
        print('%s  blotch (red): %s' % ('PASS' if not good else 'FAIL', 'the old law keeps the points dark' if not good else 'the red passes the bar'))
        return 0 if not good else 1
    print('%s  blotch: the wall\'s dark GI points read like their lit neighbours, the frame outside the box kept%s'
          % ('PASS' if good else 'FAIL', '' if rep >= 0.5 else ' (NOT REPRODUCED by the twin)'))
    return 0 if good else 1


if __name__ == '__main__':
    a = sys.argv[1:]
    opt = (lambda k, d='': a[a.index(k) + 1] if k in a else d)
    if a and a[0] == 'links':
        sys.exit(links_cmd(a[1], a[2], opt('--red'), opt('--expect', 'pass')))
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
    if a and a[0] == 'blotch':
        sys.exit(blotch_cmd(a[1], a[2], opt('--tag', 'c3_gsb'), [float(v) for v in opt('--box', '1740,120,1920,700').split(',')],
                            [float(v) for v in opt('--center').split(',')], int(opt('--max', '400')), opt('--expect', 'pass'),
                            '--why' in a, int(opt('--osample', '3000')), opt('--nface', 'mean')))
    if a and a[0] == 'giwall':
        sys.exit(giwall_cmd(opt('--expect', 'pass')))
    if a and a[0] == 'recess':
        sys.exit(recess_cmd(a[1], a[2], float(opt('--lo', '0.5')), float(opt('--hi', '2.0')), opt('--expect', 'pass')))
    print(__doc__)
    sys.exit(2)
