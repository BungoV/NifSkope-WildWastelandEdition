#!/usr/bin/env python3
"""PRTP probe placement gates (lane PRTPPLACE, 2026-09-30).

  synth   <exe> <workdir>          build a synthetic block with KNOWN openings and
                                   traps, run `probeplace` on it, check every
                                   opening is found where it is and no trap is;
                                   then re-trace every column and wall stack in
                                   numpy and require the same probe set.
  retrace <soup> <tsv> [n]         independent numpy re-trace of n random columns
                                   (first-hit + interior levels + wall stacks)
                                   plus the wall-stack invariant on a sample of
                                   wall probes, for a real cell's dumped soup.

Both print one verdict line and exit 0 on PASS, 1 on FAIL. Run each once more
with the placer's deliberate defect on (`--red wall|aperture|frames|coverage` for synth; the
cell view's WW_PROBE_RED=wall for retrace): the gate must then FAIL.
"""
import math
import os
import random
import struct
import subprocess
import sys

import numpy as np

# ----------------------------------------------------------------- soup I/O
MAGIC = 0x31505350


def read_soup(path):
    with open(path, 'rb') as f:
        magic, ntri, ndoor = struct.unpack('<III', f.read(12))
        assert magic == MAGIC, 'not a PSP1 soup'
        tris = np.frombuffer(f.read(ntri * 36), dtype='<f4').reshape(ntri, 3, 3).astype(np.float64)
        doors = []
        for _ in range(ndoor):
            ref, = struct.unpack('<I', f.read(4))
            lo = struct.unpack('<3f', f.read(12))
            hi = struct.unpack('<3f', f.read(12))
            doors.append((ref, lo, hi))
    return tris, doors


def write_soup(path, tris, doors):
    t = np.asarray(tris, dtype='<f4').reshape(-1, 9)
    with open(path, 'wb') as f:
        f.write(struct.pack('<III', MAGIC, len(t), len(doors)))
        f.write(t.tobytes())
        for ref, lo, hi in doors:
            f.write(struct.pack('<I3f3f', ref, *lo, *hi))


def read_tsv(path):
    rows = []
    with open(path) as f:
        head = None
        for line in f:
            if line.startswith('#'):
                continue
            parts = line.rstrip('\n').split('\t')
            if head is None:
                head = parts
                continue
            rows.append(dict(zip(head, parts)))
    for r in rows:
        for k in ('x', 'y', 'z', 'nx', 'ny', 'nz', 'width', 'height', 'sill'):
            r[k] = float(r[k])
        r['level'] = int(r['level'])
    return rows


# --------------------------------------------------------------- the tracer
class Tracer:
    """Moller-Trumbore, double-sided, nearest hit in (1e-4, len], every
    triangle whose box the segment's box touches. Independent of the C++ BVH."""

    def __init__(self, tris):
        self.t = tris
        self.lo = tris.min(axis=1)
        self.hi = tris.max(axis=1)
        self.e1 = tris[:, 1] - tris[:, 0]
        self.e2 = tris[:, 2] - tris[:, 0]

    def ray(self, a, b):
        a = np.asarray(a, dtype=np.float64)
        b = np.asarray(b, dtype=np.float64)
        d = b - a
        # the placer's own arithmetic, operation for operation (no FMA there either)
        ln = math.sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2])
        if ln < 1e-9:
            return None
        d = np.array([d[0] / ln, d[1] / ln, d[2] / ln])
        slo = np.minimum(a, b) - 1e-3
        shi = np.maximum(a, b) + 1e-3
        m = np.all((self.hi >= slo) & (self.lo <= shi), axis=1)
        idx = np.nonzero(m)[0]
        if not len(idx):
            return None
        e1, e2, p0 = self.e1[idx], self.e2[idx], self.t[idx, 0]
        def dot(x, y):
            return (x[:, 0] * y[:, 0] + x[:, 1] * y[:, 1]) + x[:, 2] * y[:, 2]

        def cross(x, y):
            return np.stack([x[:, 1] * y[:, 2] - x[:, 2] * y[:, 1], x[:, 2] * y[:, 0] - x[:, 0] * y[:, 2],
                             x[:, 0] * y[:, 1] - x[:, 1] * y[:, 0]], axis=1)
        D = np.broadcast_to(d, e2.shape)
        pv = cross(D, e2)
        det = dot(e1, pv)
        ok = np.abs(det) >= 1e-12
        inv = np.where(ok, 1.0 / np.where(ok, det, 1.0), 0.0)
        tv = a - p0
        u = dot(tv, pv) * inv
        qv = cross(tv, e1)
        v = dot(D, qv) * inv
        t = dot(e2, qv) * inv
        hit = ok & (u >= 0) & (u <= 1) & (v >= 0) & (u + v <= 1) & (t > 1e-4) & (t <= ln)
        if not hit.any():
            return None
        return float(t[hit].min())


# ------------------------------------------------- FO4CS rules, re-derived
SPACING, EYE, STEP, GAP, LEVELS = 280.0, 120.0, 4.0, 140.0, 6
SEARCH, STANDOFF, HEIGHTS = 280.0, 96.0, (240.0, 480.0, 720.0, 960.0)
COVER_R, HALL_R = 200.0, 70.0   # the interior rule's radii (probeplace.h)
DIRS = ((1, 0), (-1, 0), (0, 1), (0, -1))


def column(tr, x, y, top, bottom):
    """Every probe this column places: [(class, level, x, y, z)]."""
    out = []
    surf = []
    cur = top
    while len(surf) < LEVELS * 2 + 2 and cur > bottom:
        t = tr.ray((x, y, cur), (x, y, bottom))
        if t is None:
            break
        hz = cur - t
        if surf and not (hz < surf[-1] - 1e-3):
            break
        surf.append(hz)
        cur = hz - STEP
    if not surf:
        return out
    origins = [(x, y, surf[0] + EYE)]
    out.append(('first-hit', 0, x, y, surf[0] + EYE))
    level = 0
    for s in range(1, len(surf)):
        gap = surf[s - 1] - surf[s]
        if not gap >= GAP:
            continue
        if level + 1 >= LEVELS:
            break
        level += 1
        z = surf[s] + min(EYE, gap * 0.5)
        origins.append((x, y, z))
        out.append(('interior', level, x, y, z))
    for ox, oy, oz in origins:
        for dx, dy in DIRS:
            t = tr.ray((ox, oy, oz), (ox + dx * SEARCH, oy + dy * SEARCH, oz))
            if t is None:
                continue
            so = t - STANDOFF
            if not so > 16.0:
                continue
            last = (ox + dx * so, oy + dy * so, oz)
            k = 0
            for h in HEIGHTS:
                cand = (last[0], last[1], oz + h)
                if not cand[2] - last[2] > 0:
                    break
                if tr.ray(last, cand) is not None:
                    break
                if tr.ray(cand, (cand[0] + dx * STANDOFF * 2, cand[1] + dy * STANDOFF * 2, cand[2])) is None:
                    break
                k += 1
                out.append(('wall', k, cand[0], cand[1], cand[2]))
                last = cand
    return out


def lattice(minx, miny, maxx, maxy):
    i0 = math.ceil(np.float32(minx) / np.float32(SPACING))
    j0 = math.ceil(np.float32(miny) / np.float32(SPACING))
    cols = []
    j = j0
    while j * SPACING <= maxy:
        i = i0
        while i * SPACING <= maxx:
            cols.append((i * SPACING, j * SPACING))
            i += 1
        j += 1
    return cols


def match(expected, got, tol=0.1):
    """Multiset match on (class, level, xyz within tol). Returns (missing, extra)."""
    pool = {}
    for g in got:
        pool.setdefault((g[0], g[1]), []).append(g)
    missing = []
    for e in expected:
        cand = pool.get((e[0], e[1]), [])
        best = None
        for i, g in enumerate(cand):
            if abs(g[2] - e[2]) <= tol and abs(g[3] - e[3]) <= tol and abs(g[4] - e[4]) <= tol:
                best = i
                break
        if best is None:
            missing.append(e)
        else:
            cand.pop(best)
    extra = [g for v in pool.values() for g in v]
    return missing, extra


# ---------------------------------------------------------- synthetic block
def box(x0, y0, z0, x1, y1, z1):
    v = [(x, y, z) for z in (z0, z1) for y in (y0, y1) for x in (x0, x1)]
    f = [(0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)]
    out = []
    for a, b, c, d in f:
        out.append((v[a], v[b], v[c]))
        out.append((v[a], v[c], v[d]))
    return out


def wall_with_holes(axis, c0, c1, lo, hi, z0, z1, holes):
    """A wall along `axis` ('x' = spans x, thin in y between c0..c1) from lo..hi,
    z0..z1, with rectangular holes (a0, a1, hz0, hz1) cut out."""
    pieces = []
    cuts = sorted(holes)
    cur = lo
    for a0, a1, hz0, hz1 in cuts:
        if a0 > cur:
            pieces.append((cur, a0, z0, z1))
        if hz0 > z0:
            pieces.append((a0, a1, z0, hz0))
        if hz1 < z1:
            pieces.append((a0, a1, hz1, z1))
        cur = a1
    if cur < hi:
        pieces.append((cur, hi, z0, z1))
    out = []
    for a0, a1, pz0, pz1 in pieces:
        if axis == 'x':
            out += box(a0, c0, pz0, a1, c1, pz1)
        else:
            out += box(c0, a0, pz0, c1, a1, pz1)
    return out


def synth_scene():
    OX, OY = 7.0, 11.0      # keep faces off the 280 lattice lines
    T = []

    def B(x0, y0, z0, x1, y1, z1):
        return box(x0 + OX, y0 + OY, z0, x1 + OX, y1 + OY, z1)

    def W(axis, c0, c1, lo, hi, z0, z1, holes=()):
        if axis == 'x':
            return [tuple((p[0] + OX, p[1] + OY, p[2]) for p in tri)
                    for tri in wall_with_holes('x', c0, c1, lo, hi, z0, z1, holes)]
        return [tuple((p[0] + OX, p[1] + OY, p[2]) for p in tri)
                for tri in wall_with_holes('y', c0, c1, lo, hi, z0, z1, holes)]

    # ground
    g = 2600.0
    T += [((-g, -g, 0), (g, -g, 0), (g, g, 0)), ((-g, -g, 0), (g, g, 0), (-g, g, 0))]
    expect = []   # (kind, x, y, z-range lo, roomToRoom, doored)
    # HOUSE A: 0..800 x 0..600, walls 20, height 300, roof 300..320
    T += W('x', 0, 20, 0, 800, 0, 300, [(300, 420, 0, 220)])            # front: doorway
    T += W('x', 580, 600, 0, 800, 0, 300)                                 # back: solid
    T += B(300, 588, 100, 420, 592, 205)                                  # boarded window: board
    T += W('y', 0, 20, 0, 600, 0, 300)                                    # left
    T += W('y', 780, 800, 0, 600, 0, 300, [(200, 320, 100, 205)])       # right: window
    T += W('y', 560, 580, 20, 580, 0, 300, [(250, 370, 0, 220)])        # partition: doorway
    T += B(0, 0, 300, 800, 600, 320)                                      # roof
    doors = [(0xA001, (300 + OX, 0 + OY, 0), (420 + OX, 20 + OY, 220))]
    expect += [('doorway', 360 + OX, 10 + OY, False, True),
               ('doorway', 570 + OX, 310 + OY, True, False),
               ('window', 790 + OX, 260 + OY, False, False)]
    # HOUSE B with a deep porch: -1200..-600 x 0..500
    T += W('x', 0, 20, -1200, -600, 0, 300, [(-950, -830, 0, 220)])
    T += W('x', 480, 500, -1200, -600, 0, 300)
    T += W('y', -1200, -1180, 0, 500, 0, 300)
    T += W('y', -620, -600, 0, 500, 0, 300)
    T += B(-1200, 0, 300, -600, 500, 320)
    T += B(-1200, -300, 280, -600, 0, 300)                                # porch roof, 300 deep
    for px in (-1200, -910, -620):
        T += B(px, -300, 0, px + 20, -280, 280)                           # porch posts
    expect += [('doorway', -890 + OX, 10 + OY, True, False)]
    # CORRIDOR: two long walls 120 apart with a roof, open both ends
    T += B(-1200, -1200, 0, -400, -1180, 250)
    T += B(-1200, -1060, 0, -400, -1040, 250)
    T += B(-1200, -1200, 250, -400, -1040, 270)
    # FENCE with a gap
    T += W('x', -800, -790, 0, 1000, 0, 100, [(400, 500, 0, 100)])
    # TWO-STOREY C: 400..1200 x -1200..-600, slab 290..310, roof 600..620
    T += W('x', -620, -600, 400, 1200, 0, 600)
    T += W('x', -1200, -1180, 400, 1200, 0, 600)
    T += W('y', 400, 420, -1200, -600, 0, 600)
    T += W('y', 1180, 1200, -1200, -600, 0, 600, [(-1000, -880, 400, 505)])
    T += B(400, -1200, 290, 1200, -600, 310)
    T += B(400, -1200, 600, 1200, -600, 620)
    expect += [('window', 1190 + OX, -940 + OY, False, False)]
    # HOUSE D, turned 45 degrees (only the rotated opening frames see it): 500 x 400 about (-500, 1050)
    cs = math.sqrt(0.5)

    def r45(x, y):
        return (x * cs - y * cs - 500 + OX, x * cs + y * cs + 1050 + OY)

    D = []
    D += wall_with_holes('x', -200, -180, -250, 250, 0, 300, [(-60, 60, 0, 220)])    # front: doorway
    D += wall_with_holes('x', 180, 200, -250, 250, 0, 300, ())
    D += wall_with_holes('y', -250, -230, -200, 200, 0, 300, ())
    D += wall_with_holes('y', 230, 250, -200, 200, 0, 300, [(-60, 60, 100, 205)])    # right: window
    D += box(-250, -200, 300, 250, 200, 320)
    T += [tuple(r45(p[0], p[1]) + (p[2],) for p in tri) for tri in D]
    expect += [('doorway',) + r45(0, -190) + (False, False), ('window',) + r45(240, 0) + (False, False)]
    # BUILDING E (the interior rule): a 140-wide hallway along the south, two rooms north of
    # it; the hallway opens outside (south) and into the west room, the west room into the east.
    # (A door as wide as the hallway at its END is not found: its jamb is the hallway wall,
    # too long to be a jamb. The rooms still cover that hallway; docs/PRTP_PLAN.md 2c.)
    T += W('x', 800, 820, 150, 1350, 0, 300, [(1000, 1120, 0, 220)])            # south wall: doorway out
    T += W('x', 1380, 1400, 150, 1350, 0, 300)                                    # north wall
    T += W('y', 150, 170, 800, 1400, 0, 300)                                      # west wall
    T += W('y', 1330, 1350, 800, 1400, 0, 300)                                    # east wall
    T += W('x', 960, 980, 170, 1330, 0, 300, [(400, 520, 0, 220)])              # hall | rooms: doorway
    T += W('y', 700, 720, 980, 1380, 0, 300, [(1150, 1270, 0, 220)])            # room | room: doorway
    T += B(150, 800, 300, 1350, 1400, 320)                                        # roof
    expect += [('doorway', 1060 + OX, 810 + OY, False, False),
               ('doorway', 460 + OX, 970 + OY, True, False),
               ('doorway', 710 + OX, 1210 + OY, True, False)]
    rooms = [('E hallway', 170 + OX, 820 + OY, 1330 + OX, 960 + OY, True),
             ('E west room', 170 + OX, 980 + OY, 700 + OX, 1380 + OY, False),
             ('E east room', 720 + OX, 980 + OY, 1330 + OX, 1380 + OY, False)]
    traps = [('boarded window', 360 + OX, 590 + OY), ('porch front', -900 + OX, -290 + OY),
             ('corridor', -800 + OX, -1120 + OY), ('fence gap', 450 + OX, -795 + OY)]
    return T, doors, expect, traps, rooms, (-1500.0, -1500.0, 1500.0, 1500.0)


def synth(exe, work, red):
    os.makedirs(work, exist_ok=True)
    tris, doors, expect, traps, rooms, rect = synth_scene()
    soup = os.path.join(work, 'synth.psp')
    tsv = os.path.join(work, 'synth%s.tsv' % ('_red_' + red if red else ''))
    write_soup(soup, tris, doors)
    cmd = [os.path.abspath(exe), '-no-gui', 'probeplace', '--soup', soup, '--rect', ','.join('%g' % v for v in rect), '--out', tsv]
    if red:
        cmd += ['--red', red]
    rc = subprocess.run(cmd, capture_output=True, text=True, timeout=600)
    if rc.returncode != 0 or not os.path.exists(tsv):
        print('synth FAIL: probeplace rc %d %s' % (rc.returncode, rc.stderr.strip()[:300]))
        return 1
    rows = read_tsv(tsv)
    aps = [r for r in rows if r['class'] == 'aperture']
    fails = []
    used = set()
    for kind, x, y, r2r, doored in expect:
        best = None
        for i, a in enumerate(aps):
            d = math.hypot(a['x'] - x, a['y'] - y)
            if d <= 45 and i not in used and (best is None or d < best[1]):
                best = (i, d)
        if best is None:
            fails.append('missing %s at %.0f,%.0f' % (kind, x, y))
            continue
        used.add(best[0])
        a = aps[best[0]]
        if a['kind'] != kind:
            fails.append('%s at %.0f,%.0f came out %s' % (kind, x, y, a['kind']))
        if (a['roomToRoom'] == '1') != r2r:
            fails.append('%s at %.0f,%.0f roomToRoom %s' % (kind, x, y, a['roomToRoom']))
        if (int(a['doorRef'], 16) != 0) != doored:
            fails.append('%s at %.0f,%.0f doorRef %s' % (kind, x, y, a['doorRef']))
    for name, x, y in traps:
        for a in aps:
            if math.hypot(a['x'] - x, a['y'] - y) <= 120:
                fails.append('trap %s produced a %s' % (name, a['kind']))
    extra = [a for i, a in enumerate(aps) if i not in used]
    for a in extra:
        fails.append('unexpected %s at %.0f,%.0f,%.0f' % (a['kind'], a['x'], a['y'], a['z']))
    # the lattice probes, re-traced in full
    tr = Tracer(np.asarray(tris, dtype=np.float32).astype(np.float64))
    allz = np.asarray(tris, dtype=np.float64)[:, :, 2]
    top, bottom = float(allz.max()) + 16.0, float(allz.min()) - 16.0
    expected = []
    for x, y in lattice(*rect):
        expected += column(tr, x, y, top, bottom)
    got = [(r['class'], r['level'], r['x'], r['y'], r['z']) for r in rows if r['class'] in ('first-hit', 'interior', 'wall')]
    missing, extra2 = match(expected, got)
    if missing or extra2:
        fails.append('re-trace: %d missing, %d extra of %d lattice probes' % (len(missing), len(extra2), len(expected)))
    # the two-storey building: every inner column has both floors
    inner = [(x, y) for x, y in lattice(*rect) if 480 + 7 < x < 1120 + 7 and -1120 + 11 < y < -680 + 11]
    for x, y in inner:
        lv = sorted(r['z'] for r in rows if r['class'] == 'interior' and abs(r['x'] - x) < .01 and abs(r['y'] - y) < .01)
        if [round(z) for z in lv] != [120, 430]:
            fails.append('two-storey column %g,%g interior z %s' % (x, y, [round(z) for z in lv]))
            break
    # THE INTERIOR RULE, checked from the known plan of building E, not from the placer's voxels:
    # each room holds a probe (the placer adds a room probe unless one stands within 70 of the
    # room's middle); every point of its floor (a 35 grid at eye height) sees a
    # probe within the radius (hallway 70, room 200, plus 45 for the placer's voxel sampling);
    # probes along the hallway stand at most 140 + one voxel apart.
    ninner = 0
    blind = []
    for name, x0, y0, x1, y1, hall in rooms:
        inside = [r for r in rows if x0 < r['x'] < x1 and y0 < r['y'] < y1 and r['z'] < 300]
        if not inside:
            fails.append('%s has no probe' % name)
        rad = (HALL_R if hall else COVER_R) + 45.0
        pts = [(r['x'], r['y'], r['z']) for r in inside]
        pts += [(r['x'], r['y'], r['z']) for r in rows if r not in inside and r['z'] < 300
                and x0 - rad < r['x'] < x1 + rad and y0 - rad < r['y'] < y1 + rad]
        for y in np.arange(y0 + 17.5, y1, 35.0):
            for x in np.arange(x0 + 17.5, x1, 35.0):
                ninner += 1
                s = (float(x), float(y), EYE)
                ok = False
                for p in pts:
                    d = math.dist(p, s)
                    if d <= rad:
                        t = tr.ray(p, s)
                        if t is None or t >= d - 1.0:
                            ok = True
                            break
                if not ok:
                    blind.append('%s %.0f,%.0f' % (name, x, y))
        if hall:
            xs = sorted(r['x'] for r in inside)
            gaps = [b - a for a, b in zip(xs, xs[1:])]
            if gaps and max(gaps) > 140.0 + 35.0:
                fails.append('%s probes %.0f apart' % (name, max(gaps)))
    if blind:
        fails.append('%d of %d floor points see no probe (first %s)' % (len(blind), ninner, blind[0]))
    n = {k: sum(1 for r in rows if r['class'] == k) for k in ('first-hit', 'interior', 'wall', 'aperture', 'room', 'cover')}
    verdict = 'PASS' if not fails else 'FAIL'
    print('synth %s%s: %d probes (first-hit %d, interior %d, wall %d, opening %d, room %d, cover %d), %d openings expected, '
          '%d traps, %d inner two-storey columns, %d floor points in %d rooms; %s' % (
              verdict, ' [RED %s]' % red if red else '', len(rows), n['first-hit'], n['interior'], n['wall'],
              n['aperture'], n['room'], n['cover'], len(expect), len(traps), len(inner), ninner, len(rooms),
              '; '.join(fails[:6]) if fails else 'all as built'))
    return 0 if not fails else 1


def retrace(soup, tsv, n):
    tris, _ = read_soup(soup)
    rows = read_tsv(tsv)
    rect = None
    with open(tsv) as f:
        for line in f:
            if line.startswith('# probeplace'):
                p = line.split()
                i = p.index('rect')
                rect = tuple(float(v) for v in p[i + 1:i + 5])
                break
    # the placer traces relative to the rect's center, stored as float32
    # (probeplace.cpp); a column grazing a triangle edge only matches when the
    # re-trace works in that same frame
    ox, oy = 0.5 * (rect[0] + rect[2]), 0.5 * (rect[1] + rect[3])
    tris = (tris - np.array([ox, oy, 0.0])).astype(np.float32).astype(np.float64)
    tr = Tracer(tris)
    top, bottom = float(tris[:, :, 2].max()) + 16.0, float(tris[:, :, 2].min()) - 16.0
    rnd = random.Random(20260930)
    cols = lattice(*rect)
    pick = rnd.sample(cols, min(n, len(cols)))
    fails = []
    byxy = {}
    for r in rows:
        if r['class'] in ('first-hit', 'interior'):
            byxy.setdefault((round(r['x'], 1), round(r['y'], 1)), []).append((r['class'], r['level'], r['x'], r['y'], r['z']))
    walls = [(r['class'], r['level'], r['x'], r['y'], r['z'], r['nx'], r['ny']) for r in rows if r['class'] == 'wall']
    wall_miss = 0
    ties = 0
    wall_first = []
    nexp_wall = 0
    for x, y in pick:
        g_lv = byxy.get((round(x, 1), round(y, 1)), [])
        # a column exactly on a mesh edge is a tie: the placer's ray code and this one
        # may round to either side. Accept it only when a 0.01 nudge reproduces the
        # placer exactly; a real defect does not vanish under a nudge that small.
        for k, (nx_, ny_) in enumerate(((0, 0), (0.01, 0), (-0.01, 0), (0, 0.01), (0, -0.01))):
            exp = [(e[0], e[1], e[2] - nx_ + ox, e[3] - ny_ + oy, e[4])
                   for e in column(tr, x - ox + nx_, y - oy + ny_, top, bottom)]
            e_lv = [e for e in exp if e[0] != 'wall']
            m, ex = match(e_lv, list(g_lv))
            if not (m or ex):
                break
        if m or ex:
            fails.append('column %g,%g levels: %d missing %d extra' % (x, y, len(m), len(ex)))
        elif k:
            ties += 1
        e_w = [e for e in exp if e[0] == 'wall']
        nexp_wall += len(e_w)
        near = [w for w in walls if abs(w[2] - x) <= 200 and abs(w[3] - y) <= 200]
        m, _ = match(e_w, near)
        wall_miss += len(m)
        wall_first = wall_first or ['%.1f,%.1f,%.1f' % tuple(w[2:5]) for w in m][:1]
    if wall_miss:
        fails.append('%d of %d re-traced wall probes missing (first %s)' % (wall_miss, nexp_wall, wall_first[0]))
    # the stack invariant: each wall probe still sees its wall 192 ahead
    sample = rnd.sample(walls, min(n, len(walls)))
    bad = 0
    for w in sample:
        x, y, z = w[2], w[3], w[4]
        # the placer writes the direction toward the wall in nx, ny (guessing it from the
        # lattice flips whenever the standoff passes half a spacing)
        d = (w[5], w[6])
        if abs(d[0]) + abs(d[1]) < 0.5:
            bad += 1
            continue
        x, y = x - ox, y - oy
        if tr.ray((x, y, z), (x + d[0] * STANDOFF * 2, y + d[1] * STANDOFF * 2, z)) is None:
            bad += 1
    if bad:
        fails.append('%d of %d sampled wall probes no longer see their wall' % (bad, len(sample)))
    verdict = 'PASS' if not fails else 'FAIL'
    print('retrace %s: %d columns re-traced of %d (%d edge ties), %d wall probes re-traced, %d wall probes checked; %s' % (
        verdict, len(pick), len(cols), ties, nexp_wall, len(sample), '; '.join(fails[:6]) if fails else 'identical'))
    return 0 if not fails else 1


if __name__ == '__main__':
    a = sys.argv[1:]
    if a and a[0] == 'synth':
        red = ''
        if '--red' in a:
            red = a[a.index('--red') + 1]
        sys.exit(synth(a[1], a[2], red))
    if a and a[0] == 'retrace':
        sys.exit(retrace(a[1], a[2], int(a[3]) if len(a) > 3 else 300))
    print(__doc__)
    sys.exit(2)
