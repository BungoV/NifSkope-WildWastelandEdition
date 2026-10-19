#!/usr/bin/env python3
"""Lane VOLFIT1 (2026-10-19) checker: the Space stage (src/probevolumes.h). Independent of NifSkope: each scene is
built here with its own truth (points inside each room, points outdoors), run through `<exe> -no-gui volumes`, and
the written .space / .spacegrid files judged here -- the shapes by this file's own primitive test, the coverage
re-counted on the grid.

  synth <exe> <work> [--red <name>]
    house     one room (1 box, coverage >= 0.95), a door threshold with the door box as evidence, a window
    twins     two rooms joined by a doorway: a room-room threshold and a room-outside one
    gimap     the twins onto a GI room grid (--gi-map): two distinct GI ids, nothing logged, the thresholds on
              those ids, every shape carried (reds nogimap, gione FAIL); the wide twins relabeled by the Space
              rooms (probeRoomsFromSpace): both interior, GI ids 1 and 2 (red spaceshapes FAILS)
    lroom     an L room: 2 boxes, the notch outdoors (red onebox FAILS)
    church    a pitched roof: <= 3 shapes, coverage >= 0.9, the air over the roof slope in no shape (red nowedge)
    silo      a round tower: one cylinder (red boxonly FAILS)
    rock      a closed one-sided shell seen from inside: no room (red noback FAILS)
    canopy    walls round a yard under a roof 2000 up: outdoors (red nosky FAILS)
    deck      a wide roof on pillars, part of it past the ground's edge: outdoors; the sky
              rays asked only where the air is roofed AND floored (red nofloor FAILS the coarse-cell bound)
    market    a 6000-square roof on pillars, closed enough for the openness bar: outdoors (red nowall FAILS)
    leanto    a shallow shed open on its long side: outdoors (red noopen FAILS)
    carport   a garage open at one end: a room whose shapes reach the open edge (red nogrow FAILS)
    closet    a closet behind a narrow gap folds into its room, a sealed hollow is dropped (red nofold FAILS)
              (every room scene: a room's floor layer by its doorway is the room's, not a second room: red nomerge FAILS)
    glass     the house's window is a threshold (red noglass FAILS); the door's evidence (red noevidence FAILS)
    recx      hand shapes replace the fitted ones, a misspelled type is named (red norecx FAILS)
    record    built / skipped (clean) / rebuilt with the reason (input soup, identity, output) (reds noskip, fpsoup)
    ids       a third room added: the old rooms keep their ids, the new one takes the next (red noids FAILS)
    cells     an exterior block: each room in its anchor's cell folder <World>/<X>_<Y>/<X>_<Y>.space
  Every room: its reported coverage agrees with the grid re-count (0.01); its anchor is one of its own cells.
"""

import json
import math
import os
import shutil
import struct
import subprocess
import sys

import numpy as np

SOUP_MAGIC = 0x31505350
ALB_MAGIC = 0x31424C41
GLS_MAGIC = 0x31534C47
T = 16.0   # wall thickness


# ---------------------------------------------------------------- geometry (outward winding = the front side)

def quad(a, b, c, d):
    return [(a, b, c), (a, c, d)]


def sbox(x0, y0, z0, x1, y1, z1):
    """A solid box, every face wound outward."""
    out = []
    out += quad((x0, y0, z0), (x0, y1, z0), (x1, y1, z0), (x1, y0, z0))   # -z
    out += quad((x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1))   # +z
    out += quad((x0, y0, z0), (x1, y0, z0), (x1, y0, z1), (x0, y0, z1))   # -y
    out += quad((x0, y1, z0), (x0, y1, z1), (x1, y1, z1), (x1, y1, z0))   # +y
    out += quad((x0, y0, z0), (x0, y0, z1), (x0, y1, z1), (x0, y1, z0))   # -x
    out += quad((x1, y0, z0), (x1, y1, z0), (x1, y1, z1), (x1, y0, z1))   # +x
    return out


def ground(x0, y0, x1, y1, z=0.0):
    return quad((x0, y0, z), (x1, y0, z), (x1, y1, z), (x0, y1, z))


def wall(axis, c0, c1, lo, hi, z0, z1, holes=()):
    """A slab along `axis` ('x': spans x lo..hi, thin in y c0..c1), holes (a0, a1, hz0, hz1)."""
    pieces, cur = [], lo
    for a0, a1, hz0, hz1 in sorted(holes):
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
    for a0, a1, p0, p1 in pieces:
        out += sbox(a0, c0, p0, a1, c1, p1) if axis == 'x' else sbox(c0, a0, p0, c1, a1, p1)
    return out


def room(x0, y0, x1, y1, h, holes=None, roof=True):
    """Walls round the interior x0..x1, y0..y1, z 0..h (the floor is the scene's ground); holes per side
    '-y' '+y' (a0, a1 along x) and '-x' '+x' (along y)."""
    holes = holes or {}
    out = []
    out += wall('x', y0 - T, y0, x0 - T, x1 + T, 0, h, holes.get('-y', ()))
    out += wall('x', y1, y1 + T, x0 - T, x1 + T, 0, h, holes.get('+y', ()))
    out += wall('y', x0 - T, x0, y0, y1, 0, h, holes.get('-x', ()))
    out += wall('y', x1, x1 + T, y0, y1, 0, h, holes.get('+x', ()))
    if roof:
        out += sbox(x0 - T, y0 - T, h, x1 + T, y1 + T, h + T)
    return out


def flip(tris):
    return [(a, c, b) for a, b, c in tris]


# ---------------------------------------------------------------- scenes

def scene_house(window_y=200.0):
    tris = ground(-800, -800, 1600, 1400)
    tris += room(0, 0, 800, 600, 300, {'-y': [(300, 428, 0, 220)], '+x': [(window_y, window_y + 200, 100, 250)]})
    glass = [((808, window_y, 100), (808, window_y + 200, 100), (808, window_y + 200, 250)),
             ((808, window_y, 100), (808, window_y + 200, 250), (808, window_y, 250))]
    return dict(tris=tris, glass=glass, doors=[(0x00A1B2, (300, -16, 0), (428, 0, 220))],
                rooms={'A': [(400, 300, 150), (40, 40, 30), (760, 560, 270)]},
                outside=[(400, -400, 150), (-400, 300, 150), (1200, 300, 150)])


def scene_twins(third=False, door=128):
    # door 80: under the room grid's pinch (2 x 48), so the GI's own room grid also names A and B apart
    tris = ground(-1600 if third else -800, -800, 2000, 1300)
    tris += room(0, 0, 600, 500, 300, {'-y': [(200, 200 + door, 0, 220)], '+x': [(200, 200 + door, 0, 220)]})
    tris += room(616, 0, 1216, 500, 300, {'-x': [(200, 200 + door, 0, 220)]})   # its -x wall is A's +x wall: same doorway
    rooms = {'A': [(300, 250, 150)], 'B': [(900, 250, 150)]}
    if third:
        tris += room(-700, 0, -100, 500, 300, {'-y': [(-500, -372, 0, 220)]})
        rooms['C'] = [(-400, 250, 150)]
    return dict(tris=tris, rooms=rooms, outside=[(300, -400, 150)])


def scene_lroom():
    tris = ground(-800, -800, 1600, 1600)
    h = 300
    tris += wall('x', -T, 0, -T, 800 + T, 0, h, [(100, 228, 0, 220)])
    tris += wall('y', 800, 800 + T, 0, 300, 0, h)
    tris += wall('x', 300, 300 + T, 300 + T, 800 + T, 0, h)
    tris += wall('y', 300, 300 + T, 300 + T, 800, 0, h)
    tris += wall('x', 800, 800 + T, -T, 300 + T, 0, h)
    tris += wall('y', -T, 0, 0, 800, 0, h)
    tris += sbox(-T, -T, h, 800 + T, 300 + T, h + T)
    tris += sbox(-T, 300 + T, h, 300 + T, 800 + T, h + T)
    return dict(tris=tris, rooms={'A': [(600, 150, 150), (150, 650, 150), (150, 150, 150)]},
                outside=[(600, 600, 150), (550, 1200, 150)])


def scene_church():
    x0, y0, x1, y1, h, ztop = 0.0, 0.0, 1000.0, 600.0, 300.0, 600.0
    ym = (y0 + y1) / 2
    tris = ground(-800, -800, 1800, 1400)
    tris += room(x0, y0, x1, y1, h, {'-x': [(236, 364, 0, 220)]}, roof=False)
    e = 12.0
    for side in (0, 1):
        ya = y0 - T if side == 0 else y1 + T
        # the outer face (out and up), then the inner face 12 below (into the nave)
        out_q = quad((x0 - T, ya, h), (x1 + T, ya, h), (x1 + T, ym, ztop), (x0 - T, ym, ztop))
        in_q = quad((x0 - T, ya, h - e), (x1 + T, ya, h - e), (x1 + T, ym, ztop - e), (x0 - T, ym, ztop - e))
        if side == 1:
            out_q, in_q = flip(out_q), flip(in_q)
        tris += out_q + flip(in_q)
    for xs, inner_sign in ((x0, 1), (x1, -1)):
        tri_in = ((xs, y0, h), (xs, y1, h), (xs, ym, ztop - e))    # normal +x
        tri_out = tuple((p[0] - T * inner_sign, p[1], p[2]) for p in tri_in)
        tris += [tri_in] if inner_sign > 0 else flip([tri_in])
        tris += flip([tri_out]) if inner_sign > 0 else [tri_out]
    return dict(tris=tris, rooms={'A': [(500, 300, 500), (500, 60, 280), (100, 300, 150)]},
                outside=[(500, 40, 420), (500, 560, 420), (-400, 300, 150)])


def scene_silo():
    R, H, n, cx, cy = 300.0, 800.0, 32, 0.0, 0.0
    tris = ground(-900, -900, 900, 900)
    ring = [(math.cos(2 * math.pi * k / n), math.sin(2 * math.pi * k / n)) for k in range(n)]
    for k in range(n):
        c0, c1 = ring[k], ring[(k + 1) % n]
        for r, outward in ((R + 12, True), (R, False)):
            a, b = (cx + r * c0[0], cy + r * c0[1]), (cx + r * c1[0], cy + r * c1[1])
            q = quad((a[0], a[1], 0), (b[0], b[1], 0), (b[0], b[1], H), (a[0], a[1], H))   # outward (ccw ring)
            tris += q if outward else flip(q)
        # caps: the inner face down at H, the outer face up at H + 12
        tris += [((cx, cy, H), (cx + R * c1[0], cy + R * c1[1], H), (cx + R * c0[0], cy + R * c0[1], H))]
        tris += [((cx, cy, H + 12), (cx + (R + 12) * c0[0], cy + (R + 12) * c0[1], H + 12),
                  (cx + (R + 12) * c1[0], cy + (R + 12) * c1[1], H + 12))]
    return dict(tris=tris, rooms={'A': [(0, 0, 400), (200, 0, 100), (0, -200, 700)]},
                outside=[(280, 280, 400), (-600, 0, 100)])


def scene_rock():
    tris = ground(-600, -600, 1200, 1200)
    tris += sbox(0, 0, 0, 600, 600, 600)      # one-sided, outward: from inside every face is a back
    return dict(tris=tris, rooms={}, outside=[(-300, 300, 150)], void=[(300, 300, 300)])


def scene_canopy():
    tris = ground(-1000, -1000, 1800, 1800)
    tris += room(0, 0, 800, 800, 500, {'-y': [(336, 464, 0, 220)]}, roof=False)
    for px, py in ((-100, -100), (876, -100), (-100, 876), (876, 876)):
        tris += sbox(px, py, 0, px + 24, py + 24, 2000)
    tris += sbox(-100, -100, 2000, 900, 900, 2016)
    return dict(tris=tris, rooms={}, outside=[(400, 400, 200), (400, 400, 800)])


def scene_deck():
    tris = ground(-600, -600, 1500, 2600)          # ends at x 1500: the deck's far third has no floor under it
    for px in (0, 976, 1952):
        for py in (0, 976, 1952):
            tris += sbox(px, py, 0, px + 32, py + 32, 300)
    tris += sbox(0, 0, 300, 2000, 2000, 316)
    return dict(tris=tris, rooms={}, outside=[(1000, 1000, 150), (500, 500, 150)])


def scene_market():
    """A market roof 6000 square on pillars: closed enough (openness ~0.1), but no wall within reach: outdoors."""
    tris = ground(-800, -800, 6800, 6800)
    for px in range(0, 6001, 1500):
        for py in range(0, 6001, 1500):
            tris += sbox(px - 16, py - 16, 0, px + 16, py + 16, 400)
    tris += sbox(-16, -16, 400, 6016, 6016, 416)
    return dict(tris=tris, rooms={}, outside=[(3000, 3000, 200), (750, 750, 200), (5200, 3700, 100)])


def scene_leanto():
    tris = ground(-800, -800, 2000, 1100)
    h = 300
    tris += wall('x', 300, 300 + T, -T, 1200 + T, 0, h)   # the back wall
    tris += wall('y', -T, 0, 0, 300, 0, h)
    tris += wall('y', 1200, 1200 + T, 0, 300, 0, h)
    tris += sbox(-T, 0, h, 1200 + T, 300 + T, h + T)
    return dict(tris=tris, rooms={}, outside=[(600, 150, 150), (600, 250, 100)])


def scene_carport():
    tris = ground(-800, -1000, 1400, 1400)
    h = 300
    tris += wall('x', 900, 900 + T, -T, 600 + T, 0, h)    # the back wall; open at y = 0
    tris += wall('y', -T, 0, 0, 900, 0, h)
    tris += wall('y', 600, 600 + T, 0, 900, 0, h)
    tris += sbox(-T, 0, h, 600 + T, 900 + T, h + T)
    return dict(tris=tris, rooms={'A': [(300, 600, 150), (300, 200, 150), (300, 60, 150)]},
                outside=[(300, -300, 150)])


def scene_closet():
    # a closet behind the +y wall, through a 48-wide gap (the pinch closes it): 80 x 80 x 200
    tris = ground(-800, -800, 1600, 1400)
    tris += room(0, 0, 800, 600, 300, {'-y': [(300, 428, 0, 220)], '+y': [(116, 164, 0, 200)]})
    tris += room(100, 616, 180, 696, 200, {'-y': [(116, 164, 0, 200)]})
    # a sealed hollow: 64 x 64 x 64 inside walls, away from the house
    tris += room(1200, 900, 1264, 964, 64)
    return dict(tris=tris, doors=[], rooms={'A': [(400, 300, 150)]}, closet=(140, 656, 100), hollow=(1232, 932, 32),
                outside=[(400, -400, 150)])


SCENES = {
    'house': scene_house, 'twins': scene_twins, 'lroom': scene_lroom, 'church': scene_church, 'silo': scene_silo,
    'rock': scene_rock, 'canopy': scene_canopy, 'deck': scene_deck, 'leanto': scene_leanto, 'carport': scene_carport,
    'closet': scene_closet,
}


def write_soup(path, s):
    t = np.asarray(s['tris'], dtype='<f4').reshape(-1, 9)
    doors = s.get('doors', [])
    with open(path, 'wb') as f:
        f.write(struct.pack('<III', SOUP_MAGIC, len(t), len(doors)))
        f.write(t.tobytes())
        for ref, lo, hi in doors:
            f.write(struct.pack('<I3f3f', ref, *lo, *hi))
        f.write(struct.pack('<II', ALB_MAGIC, len(t)))
        f.write(b'\x80' * (3 * len(t)))
        g = s.get('glass')
        if g:
            ga = np.asarray(g, dtype='<f4').reshape(-1, 9)
            f.write(struct.pack('<II', GLS_MAGIC, len(ga)))
            f.write(ga.tobytes())
            f.write(b'\xc0' * (3 * len(ga)))
        # no TWO1 tail: every face is one-sided (the back-face rule reads the winding)


# ---------------------------------------------------------------- the shapes, this file's own test

def rot(deg):
    ax, ay, az = (math.radians(v) for v in deg)
    cx, sx, cy, sy, cz, sz = math.cos(ax), math.sin(ax), math.cos(ay), math.sin(ay), math.cos(az), math.sin(az)
    Rx = np.array([[1, 0, 0], [0, cx, -sx], [0, sx, cx]])
    Ry = np.array([[cy, 0, sy], [0, 1, 0], [-sy, 0, cy]])
    Rz = np.array([[cz, -sz, 0], [sz, cz, 0], [0, 0, 1]])
    return Rz @ Ry @ Rx


def inside(shape, P):
    """P: (n, 3) world points -> bool (n,). VOLUMES.md section 6 sizes: wedge full height at -y, zero at +y;
    capsule along z (size [d, d, length]); cylinder [dx, dy, h]; sphere diameters."""
    P = np.atleast_2d(np.asarray(P, dtype=float))
    R = rot(shape['rotation'])
    L = (P - np.asarray(shape['center'], dtype=float)) @ R      # R^T (p - c), row form
    sx, sy, sz = (float(v) for v in shape['size'])
    hx, hy, hz = sx / 2, sy / 2, sz / 2
    x, y, z = L[:, 0], L[:, 1], L[:, 2]
    t = shape['type']
    if t == 'box':
        return (abs(x) <= hx) & (abs(y) <= hy) & (abs(z) <= hz)
    if t == 'cylinder':
        return (abs(z) <= hz) & ((x / hx) ** 2 + (y / hy) ** 2 <= 1.0)
    if t == 'wedge':
        return (abs(x) <= hx) & (abs(y) <= hy) & (z >= -hz) & (z + hz <= sz * (0.5 - y / sy))
    if t == 'capsule':
        r = min(hx, hy)
        zc = np.clip(z, -max(0.0, hz - r), max(0.0, hz - r))
        return x * x + y * y + (z - zc) ** 2 <= r * r
    if t == 'sphere':
        return (x / hx) ** 2 + (y / hy) ** 2 + (z / hz) ** 2 <= 1.0
    raise ValueError('unknown shape type %r' % t)


def in_room(room, P):
    P = np.atleast_2d(np.asarray(P, dtype=float))
    m = np.zeros(len(P), dtype=bool)
    for s in room['shapes']:
        m |= inside(s, P)
    return m


def read_grid(path):
    with open(path, 'rb') as f:
        b = f.read()
    o = struct.unpack_from('<3f', b, 0)
    c = struct.unpack_from('<f', b, 12)[0]
    d = struct.unpack_from('<3i', b, 16)
    lab = np.frombuffer(b, dtype='<i4', offset=28).reshape(d[2], d[1], d[0])
    return np.asarray(o), c, d, lab


# ---------------------------------------------------------------- runs

def run(exe, soup, out, key, red, extra=()):
    cmd = [exe, '-no-gui', 'volumes', '--soup', soup, '--out', out, '--interior', '--key', key, '--grid'] + list(extra)
    if red:
        cmd += ['--red', red]
    p = subprocess.run(cmd, capture_output=True, text=True)
    return p.returncode, p.stdout + p.stderr


def load(out, key):
    with open(os.path.join(out, 'Interiors', key, key + '.space')) as f:
        sp = json.load(f)
    return sp, read_grid(os.path.join(out, 'Interiors', key, key + '.spacegrid'))


def census(text):
    for line in text.splitlines():
        if line.startswith('space: rooms'):
            return line
    return ''


def census_num(line, label):
    i = line.find(label + ' ')
    if i < 0:
        return None
    w = line[i + len(label) + 1:].split()[0].rstrip(',;)')
    try:
        return float(w)
    except ValueError:
        return None


def judge_common(name, sp, grid, fails, log):
    """Every room: coverage re-counted on the grid; the anchor one of its own cells."""
    o, c, d, lab = grid
    ids = {}
    for k, r in enumerate(sp['rooms']):
        ids[r['room']] = k + 1
    # labels in the grid are the build's room order (1-based), the file's rooms in the same order
    for k, r in enumerate(sp['rooms']):
        if r['fit'].get('hand'):
            continue
        zz, yy, xx = np.nonzero(lab == k + 1)
        if len(xx) == 0:
            fails.append('%s: room %s has no cells in the grid' % (name, r['room']))
            continue
        P = np.stack([o[0] + (xx + 0.5) * c, o[1] + (yy + 0.5) * c, o[2] + (zz + 0.5) * c], axis=1)
        cov = float(in_room(r, P).mean())
        if abs(cov - r['fit']['coverage']) > 0.01:
            fails.append('%s: room %s coverage %.3f reported, %.3f re-counted' % (name, r['room'], r['fit']['coverage'], cov))
        a = r['anchor']
        g = [int(math.floor((a[i] - o[i]) / c)) for i in range(3)]
        if not all(0 <= g[i] < d[i] for i in range(3)) or lab[g[2], g[1], g[0]] != k + 1:
            fails.append('%s: room %s anchor %s is not its own cell' % (name, r['room'], a))
        log.append('  %s %s: %d shapes %s cov %.3f oa %.3f os %.3f sky %.3f open %.3f src %s' % (
            name, r['room'], len(r['shapes']), ','.join(s['type'] for s in r['shapes']), cov, r['fit']['overshootAir'],
            r['fit']['overshootSolid'], r['classify']['sky'], r['classify']['openness'], r['source']))


def room_of(sp, p):
    """The file's room whose shapes hold p (the smallest by count of shapes' volume), else None."""
    best = None
    for r in sp['rooms']:
        if in_room(r, [p])[0]:
            v = sum(np.prod(s['size']) for s in r['shapes'])
            if best is None or v < best[0]:
                best = (v, r)
    return best[1] if best else None


def judge_truth(name, s, sp, fails):
    got = {}
    for rn, pts in s.get('rooms', {}).items():
        rs = [room_of(sp, p) for p in pts]
        if any(r is None for r in rs):
            fails.append('%s: room %s point %s in no room' % (name, rn, pts[[r is None for r in rs].index(True)]))
            continue
        if len(set(r['room'] for r in rs)) != 1:
            fails.append('%s: room %s points in rooms %s' % (name, rn, sorted(set(r['room'] for r in rs))))
            continue
        got[rn] = rs[0]['room']
    if len(set(got.values())) != len(got):
        fails.append('%s: two truth rooms share one room %s' % (name, got))
    for p in s.get('outside', []):
        r = room_of(sp, p)
        if r is not None:
            fails.append('%s: outdoor point %s inside room %s' % (name, p, r['room']))
    for p in s.get('void', []):
        r = room_of(sp, p)
        if r is not None:
            fails.append('%s: point %s inside the rock is in room %s' % (name, p, r['room']))
    if len(sp['rooms']) != len(s.get('rooms', {})):
        fails.append('%s: %d rooms, the scene has %d' % (name, len(sp['rooms']), len(s.get('rooms', {}))))
    return got


def thresholds_between(sp, a, b):
    return [t for t in sp['thresholds'] if sorted(t['between']) == sorted([a, b])]


def near(t, p, r):
    return math.dist(t['shape']['center'], p) <= r


def synth(exe, work, red):
    fails, log = [], []
    os.makedirs(work, exist_ok=True)

    def build(name, s, key=None, extra=(), fresh=True, r=None):
        key = key or name
        out = os.path.join(work, 'out_' + name)
        if fresh and os.path.isdir(out):
            shutil.rmtree(out)
        soup = os.path.join(work, key + '.psp')
        write_soup(soup, s)
        rc, text = run(exe, soup, out, key, red if r is None else r, extra)
        log.append('%s: rc %d | %s' % (name, rc, ' | '.join(l for l in text.splitlines() if l.strip())[:900]))
        if rc != 0:
            fails.append('%s: exe rc %d' % (name, rc))
            return None, None, text, out
        sp, grid = load(out, key)
        judge_common(name, sp, grid, fails, log)
        return sp, grid, text, out

    # house / glass
    s = scene_house()
    vd = os.path.join(work, 'house.viewdump')
    if os.path.exists(vd):
        os.remove(vd)
    sp, grid, text, _ = build('house', s, extra=('--view-dump', vd))
    if sp:
        # the debug view's geometry: every room, shape and threshold drawn (a box is 12 edges; a threshold 2 tris, 5 lines)
        want = {'rooms': len(sp['rooms']), 'shapes': sum(len(r['shapes']) for r in sp['rooms']), 'thresholds': len(sp['thresholds'])}
        got_v = {}
        if os.path.exists(vd):
            got_v = dict(kv.split('=') for kv in open(vd).read().split())
        log.append('  view: %s' % got_v)
        if not got_v or any(int(got_v.get(k, -1)) != n for k, n in want.items()) \
                or int(got_v['tris']) != 2 * want['thresholds'] or int(got_v['lines']) < 12 * want['shapes'] + 5 * want['thresholds']:
            fails.append('view: the debug view draws %s for %s' % (got_v, want))
        got = judge_truth('house', s, sp, fails)
        A = got.get('A')
        if A:
            r = [x for x in sp['rooms'] if x['room'] == A][0]
            if len(r['shapes']) != 1 or r['fit']['coverage'] < 0.95 or r['fit']['overshootAir'] > 0.02:
                fails.append('house: %d shapes, coverage %.3f, overshoot air %.3f (want 1, >= 0.95, <= 0.02)' % (
                    len(r['shapes']), r['fit']['coverage'], r['fit']['overshootAir']))
            ts = thresholds_between(sp, A, 'outside')
            door = [t for t in ts if near(t, (364, -8, 110), 96)]
            if not door or door[0]['opening'] != 'door' or 'door:0xA1B2' not in ' '.join(door[0]['evidence']).replace('0x00A1B2', '0xA1B2'):
                fails.append('house: no door threshold with the door box as evidence at the doorway: %s' % [
                    (t['opening'], t['shape']['center'], t['evidence']) for t in ts])
            win = [t for t in ts if near(t, (808, 300, 175), 96) and t['opening'] == 'window']
            if not win:
                fails.append('glass: no window threshold at the pane: %s' % [(t['opening'], t['shape']['center']) for t in ts])
            for t in ts:
                n = t['normal']
                c = t['shape']['center']
                # the normal points from the room outward: the point a step along it is not the room's
                q = [c[i] + 64 * n[i] for i in range(3)]
                if room_of(sp, q) is not None and room_of(sp, q)['room'] == A:
                    fails.append('house: threshold %s normal %s points into the room' % (t['threshold'], n))

    # twins
    s = scene_twins()
    sp, grid, text, out_tw = build('twins', s)
    twins_ids = {}
    if sp:
        got = judge_truth('twins', s, sp, fails)
        twins_ids = got
        if 'A' in got and 'B' in got:
            if not thresholds_between(sp, got['A'], got['B']):
                fails.append('twins: no threshold between the rooms: %s' % [t['between'] for t in sp['thresholds']])
            if not thresholds_between(sp, got['A'], 'outside'):
                fails.append('twins: no threshold from A to the outside')
            if thresholds_between(sp, got['B'], 'outside'):
                fails.append('twins: B (no outer door) has a threshold to the outside')

    # the GI map (src/probevolumesgi.h): each twin onto one GI room id of its own, its thresholds re-named onto those ids
    def gimap(name, sc):
        gm = os.path.join(work, name + '.gimap')
        if os.path.exists(gm):
            os.remove(gm)
        spg, _, _, _ = build(name, sc, extra=('--gi-map', gm))
        if not spg:
            return None, None, None
        gotg = judge_truth(name, sc, spg, fails)
        rows = [l.rstrip('\n').split('\t') for l in open(gm)] if os.path.exists(gm) else []
        log.append('  %s: %s' % (name, [r for r in rows if r[0] in ('census', 'girooms', 'room', 'threshold', 'log')]))
        return spg, gotg, rows

    # wide doorways: the GI's room grid calls both rooms outdoors, so each maps to none, logged, nothing fed
    _, gotw, rows = gimap('twins_gi_wide', s)
    if rows is not None:
        logs = [r[1] for r in rows if r[0] == 'log']
        nshapes = [int(r[1]) for r in rows if r[0] == 'shapes']
        if len([l for l in logs if 'maps to no GI room' in l]) != 2 or nshapes != [0]:
            fails.append('gimap: wide twins (no GI rooms) logged %s, shapes %s (want 2 "maps to no GI room", 0)' % (logs, nshapes))
        # the Space stage as the room source (probeRoomsFromSpace): both wide-door rooms interior, each its own id
        sline = [r[1] for r in rows if r[0] == 'spacegi']
        sgi = {r[1]: int(r[2]) for r in rows if r[0] == 'sroom'}
        slogs = [r[1] for r in rows if r[0] == 'slog']
        want = 'rooms 2, mapped 2, unmapped 0, interior where the room grid had outdoors 2;'
        if len(sline) != 1 or want not in sline[0]:
            fails.append('spacegi: wide twins relabeled as %s (want "%s")' % (sline, want))
        elif sorted(sgi.values()) != [1, 2] or slogs:
            fails.append('spacegi: wide twins map to %s, logged %s (want GI ids 1 and 2, nothing logged)' % (sgi, slogs))
    sn = scene_twins(door=80)
    spg, gotg, rows = gimap('twins_gi', sn)
    if rows is not None:
        gi_of = {int(r[1]): int(r[2]) for r in rows if r[0] == 'room'}
        logs = [r[1] for r in rows if r[0] == 'log']
        thr = [(int(r[1]), int(r[2])) for r in rows if r[0] == 'threshold']
        nshapes = [int(r[1]) for r in rows if r[0] == 'shapes']
        ids = {k: gi_of.get(int(str(v), 16), -1) for k, v in gotg.items() if k in ('A', 'B')}
        if not rows or len(ids) != 2 or min(ids.values()) < 1 or ids['A'] == ids['B']:
            fails.append('gimap: the twins map to %s (want two distinct GI ids >= 1)' % ids)
        elif logs:
            fails.append('gimap: rooms logged as none or several: %s' % logs)
        else:
            if sorted(ids.values()) not in [sorted(t) for t in thr]:
                fails.append('gimap: no threshold between GI rooms %s: %s' % (sorted(ids.values()), thr))
            if not any(ids['A'] in t and 0 in t for t in thr):
                fails.append('gimap: no threshold from A\'s GI room to the outdoors: %s' % (thr,))
            want = sum(len(r['shapes']) for r in spg['rooms'])
            if nshapes != [want]:
                fails.append('gimap: shapes %s, the rooms hold %d' % (nshapes, want))

    # ids: a third room; A and B keep theirs
    s3 = scene_twins(third=True)
    if twins_ids:
        sp3, _, text, _ = build('twins', s3, fresh=False)
        if sp3:
            got3 = judge_truth('ids', s3, sp3, fails)
            for k in ('A', 'B'):
                if got3.get(k) != twins_ids.get(k):
                    fails.append('ids: room %s was %s, now %s' % (k, twins_ids.get(k), got3.get(k)))
            want = '0x%04X' % (max(int(v, 16) for v in twins_ids.values()) + 1)
            if got3.get('C') != want:
                fails.append('ids: the new room is %s, want %s' % (got3.get('C'), want))

    # L
    s = scene_lroom()
    sp, grid, text, _ = build('lroom', s)
    if sp:
        got = judge_truth('lroom', s, sp, fails)
        if 'A' in got:
            r = [x for x in sp['rooms'] if x['room'] == got['A']][0]
            if not 2 <= len(r['shapes']) <= 3 or r['fit']['coverage'] < 0.92 or r['fit']['overshootAir'] > 0.03:
                fails.append('lroom: %d shapes, coverage %.3f, overshoot air %.3f (want 2-3, >= 0.92, <= 0.03)' % (
                    len(r['shapes']), r['fit']['coverage'], r['fit']['overshootAir']))

    # church
    s = scene_church()
    sp, grid, text, _ = build('church', s)
    if sp:
        got = judge_truth('church', s, sp, fails)
        if 'A' in got:
            r = [x for x in sp['rooms'] if x['room'] == got['A']][0]
            if len(r['shapes']) > 3 or r['fit']['coverage'] < 0.9 or r['fit']['overshootAir'] > 0.05:
                fails.append('church: %d shapes, coverage %.3f, overshoot air %.3f (want <= 3, >= 0.9, <= 0.05)' % (
                    len(r['shapes']), r['fit']['coverage'], r['fit']['overshootAir']))

    # silo
    s = scene_silo()
    sp, grid, text, _ = build('silo', s)
    if sp:
        got = judge_truth('silo', s, sp, fails)
        if 'A' in got:
            r = [x for x in sp['rooms'] if x['room'] == got['A']][0]
            if len(r['shapes']) != 1 or r['fit']['coverage'] < 0.9 or r['fit']['overshootAir'] > 0.05:
                fails.append('silo: %d shapes (%s), coverage %.3f, overshoot air %.3f (want 1, >= 0.9, <= 0.05)' % (
                    len(r['shapes']), ','.join(x['type'] for x in r['shapes']), r['fit']['coverage'], r['fit']['overshootAir']))

    # rock, canopy, leanto
    for nm in ('rock', 'canopy', 'leanto'):
        s = SCENES[nm]()
        sp, grid, text, _ = build(nm, s)
        if sp:
            judge_truth(nm, s, sp, fails)

    # deck + the floored rule's bound
    s = scene_deck()
    sp, grid, text, _ = build('deck', s)
    if sp:
        judge_truth('deck', s, sp, fails)
        o, c, d, lab = grid
        solid = lab < 0
        roofed = np.flip(np.logical_or.accumulate(np.flip(solid, 0), 0), 0)
        roofed = np.concatenate([roofed[1:], np.zeros_like(roofed[:1])], 0)        # something strictly above
        floored = np.logical_or.accumulate(solid, 0)
        floored = np.concatenate([np.zeros_like(floored[:1]), floored[:-1]], 0)    # something strictly below
        ask = (~solid) & roofed & floored
        Z, Y, X = ask.shape
        pad = np.zeros(((Z + 1) // 2 * 2, (Y + 1) // 2 * 2, (X + 1) // 2 * 2), dtype=bool)
        pad[:Z, :Y, :X] = ask
        coarse = pad.reshape(pad.shape[0] // 2, 2, pad.shape[1] // 2, 2, pad.shape[2] // 2, 2).any(axis=(1, 3, 5))
        bound = int(coarse.sum())
        got = census_num(census(text), 'coarse')
        log.append('  deck: sky coarse cells %s, the roofed-and-floored bound %d' % (got, bound))
        if got is None or got > bound:
            fails.append('deck: %s coarse cells asked for the sky, more than the %d roofed-and-floored ones' % (got, bound))

    # market: closed enough to pass the openness bar; only the wall rule keeps it outdoors (red nowall)
    s = scene_market()
    sp, grid, text, _ = build('market', s)
    if sp:
        judge_truth('market', s, sp, fails)

    # carport: the room reaches its open edge (grown back)
    s = scene_carport()
    sp, grid, text, _ = build('carport', s)
    if sp:
        judge_truth('carport', s, sp, fails)

    # closet + hollow
    s = scene_closet()
    sp, grid, text, _ = build('closet', s)
    if sp:
        got = judge_truth('closet', s, sp, fails)
        o, c, d, lab = grid
        for nm, p, want in (('closet', s['closet'], 'A'), ('hollow', s['hollow'], None)):
            g = [int(math.floor((p[i] - o[i]) / c)) for i in range(3)]
            l = int(lab[g[2], g[1], g[0]])
            name = sp['rooms'][l - 1]['room'] if l > 0 else l
            if want is None and l > 0:
                fails.append('closet: the sealed hollow is room %s' % name)
            if want and name != got.get(want):
                fails.append('closet: the closet is %s in the grid, not the room %s' % (name, got.get(want)))

    # recx
    s = scene_house()
    recx = os.path.join(work, 'house.recx')
    hand = {'type': 'box', 'center': [400, 300, 140], 'size': [780, 580, 260], 'rotation': [0, 0, 0]}
    with open(recx, 'w') as f:
        json.dump({'entries': [{'name': 'recx', 'cell': 'recx', 'rooms': [
            {'anchor': [400, 300, 150], 'shapes': [hand, {'type': 'boxx', 'center': [0, 0, 0], 'size': [1, 1, 1]}]}]}]}, f)
    sp, grid, text, _ = build('recx', s, extra=('--recx', recx))
    if sp:
        r = room_of(sp, (400, 300, 150))
        if not r or len(r['shapes']) != 1 or r['shapes'][0]['size'] != [780, 580, 260] or '+recx' not in r['source']:
            fails.append('recx: the hand shapes did not replace the fitted ones: %s' % str(r and (r['shapes'], r['source'])))
        if 'did you mean "box"' not in text:
            fails.append('recx: the misspelled type "boxx" was not named')

    # the stage record
    out = os.path.join(work, 'out_record')
    if os.path.isdir(out):
        shutil.rmtree(out)
    soup = os.path.join(work, 'record.psp')
    soup2 = os.path.join(work, 'record2.psp')
    write_soup(soup, scene_house())
    write_soup(soup2, scene_house(window_y=260.0))
    steps = [('first', soup, (), 'rebuild (record: never built)'),
             ('again', soup, (), 'space: skipped (clean)'),
             ('soup', soup2, (), 'rebuild (record: input soup)'),
             ('spec', soup2, ('--skymax', '0.3'), 'rebuild (record: identity)')]
    for tag, sp_, extra, want in steps:
        rc, text = run(exe, sp_, out, 'record', red, extra)
        log.append('record %s: %s' % (tag, text.splitlines()[0] if text else ''))
        if want not in text:
            fails.append('record %s: want "%s", got "%s"' % (tag, want, text.splitlines()[0] if text else ''))
    f = os.path.join(out, 'Interiors', 'record', 'record.space')
    with open(f, 'ab') as fh:
        fh.write(b' ')
    rc, text = run(exe, soup2, out, 'record', red, ('--skymax', '0.3'))
    if 'rebuild (record: output record.space)' not in text:
        fails.append('record output: want the edited output named, got "%s"' % (text.splitlines()[0] if text else ''))

    # exterior cells: the anchor's cell folder
    out = os.path.join(work, 'out_cells')
    if os.path.isdir(out):
        shutil.rmtree(out)
    tris = ground(-500, -500, 5700, 1100) + room(0, 0, 800, 600, 300, {'-y': [(300, 428, 0, 220)]})
    tris += room(4400, 0, 5200, 600, 300, {'-y': [(4700, 4828, 0, 220)]})
    soup = os.path.join(work, 'cells.psp')
    write_soup(soup, dict(tris=tris))
    cmd = [exe, '-no-gui', 'volumes', '--soup', soup, '--out', out, '--world', 'TestWorld', '--rect', '-500,-500,5700,1100',
           '--apertures'] + (['--red', red] if red else [])
    p = subprocess.run(cmd, capture_output=True, text=True)
    log.append('cells: rc %d | %s' % (p.returncode, ' | '.join(p.stdout.splitlines())[:600]))
    for key, pt in (('0_0', (400, 300, 150)), ('1_0', (4800, 300, 150))):
        fn = os.path.join(out, 'TestWorld', key, key + '.space')
        if not os.path.isfile(fn) or not os.path.isfile(fn + '.manifest'):
            fails.append('cells: %s or its manifest missing' % fn)
            continue
        with open(fn) as fh:
            sp = json.load(fh)
        if len(sp['rooms']) != 1 or not room_of(sp, pt):
            fails.append('cells: %s holds %d rooms, want the one at %s' % (key, len(sp['rooms']), pt))
        ev = [e for t in sp['thresholds'] for e in t['evidence']]
        log.append('  cells %s: thresholds %s' % (key, [(t['between'], t['opening'], t['evidence']) for t in sp['thresholds']]))
    return fails, log


def main():
    a = sys.argv[1:]
    if len(a) < 3 or a[0] != 'synth':
        print(__doc__)
        return 2
    red = a[a.index('--red') + 1] if '--red' in a else ''
    fails, log = synth(a[1], a[2], red)
    for l in log:
        print(l)
    for f in fails:
        print('FAIL ' + f)
    print('VERDICT %s (%d failures)%s' % ('PASS' if not fails else 'FAIL', len(fails), ' red ' + red if red else ''))
    return 0 if not fails else 1


if __name__ == '__main__':
    sys.exit(main())
