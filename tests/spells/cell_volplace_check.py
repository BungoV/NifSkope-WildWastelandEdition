# lane VOLPLACE2 gate (tests/spells/cell_volplace.sh): rooms, boxes and thresholds of a volumes run against the soup's real walls (independent of the fitter).
#   usage: python -I placegate.py <space root (dir holding <world>/<cell>/<cell>.space, or an interior dir)> <soup.psp>
#          [--cell C (room cell size, default from the .space)] [--rooms 0x0011,..] [--cells -15_17,..] [--tsv out] [--bar 1]
# Per room: boxes, rect (plan union of its boxes over their best oriented rectangle; >= 0.9 = a rectangular plan),
#   mid-room boxes (no vertical face with a wall within half a room cell for >= half its samples).
# Per threshold (non-horizontal): ring = share of rays just beside its jambs (16/32/48 u out, 3 heights, along the
#   normal through its depth +-32) that hit a wall: >= 0.5 = IN a real opening, else ELSEWHERE; open = share of a
#   32 x 32 ray grid along the normal through its depth that no solid triangle crosses (glass is a tail: passes).
import sys, os, json, math, glob, struct
import numpy as np

args = sys.argv[1:]
root, soupf = args[0], args[1]
opt = {}
for i, a in enumerate(args):
    if a.startswith('--'):
        opt[a[2:]] = args[i + 1] if i + 1 < len(args) else ''
want = set(opt['rooms'].split(',')) if 'rooms' in opt else None
wantc = set(opt['cells'].split(',')) if 'cells' in opt else None

spaces = sorted(glob.glob(os.path.join(root, '**', '*.space'), recursive=True))
rooms, thrs = [], []
for f in spaces:
    cell = os.path.basename(f)[:-6]
    if wantc and cell not in wantc:
        continue
    j = json.load(open(f, encoding='utf-8'))
    for r in j.get('rooms', []):
        if want and r['room'] not in want:
            continue
        r['_cell'] = cell
        rooms.append(r)
    for t in j.get('thresholds', []):
        if want and not (set(t.get('between', [])) & want):
            continue
        t['_cell'] = cell
        thrs.append(t)
CELL = float(opt.get('cell', 0) or 0)
if not CELL:
    for f in spaces:
        j = json.load(open(f, encoding='utf-8'))
        CELL = float(j.get('grid', {}).get('cell', 0) or j.get('cell_size', 0) or 0)
        if CELL:
            break
CELL = CELL or 61.0

b = open(soupf, 'rb').read()
magic, nt, nd = struct.unpack_from('<3I', b, 0)
assert magic == 0x31505350
T32 = np.frombuffer(b, dtype='<f4', count=nt * 9, offset=12).reshape(-1, 9)
LO = np.minimum(np.minimum(T32[:, 0:3], T32[:, 3:6]), T32[:, 6:9])
HI = np.maximum(np.maximum(T32[:, 0:3], T32[:, 3:6]), T32[:, 6:9])


def tris_in(lo, hi):
    k = np.all(HI >= lo, axis=1) & np.all(LO <= hi, axis=1)
    t = T32[k].astype(np.float64)
    return t[:, 0:3], t[:, 3:6] - t[:, 0:3], t[:, 6:9] - t[:, 0:3]


def cast(tr, o, d, maxt):
    """first hit t per ray (inf = none) against the triangles tr = (A, E1, E2)"""
    A, E1, E2 = tr
    out = np.full(len(o), np.inf)
    if not len(A):
        return out
    step = max(1, 4000000 // max(1, len(A)))
    for s0 in range(0, len(o), step):
        oo, dd = o[s0:s0 + step, None, :], d[s0:s0 + step, None, :]
        p = np.cross(dd, E2[None])
        det = np.einsum('rtk,tk->rt', p, E1)
        ok = np.abs(det) > 1e-9
        inv = np.where(ok, 1.0 / np.where(ok, det, 1.0), 0.0)
        s = oo - A[None]
        u = np.einsum('rtk,rtk->rt', s, p) * inv
        q = np.cross(s, E1[None])
        v = np.einsum('rtk,rtk->rt', np.broadcast_to(dd, q.shape), q) * inv
        t = np.einsum('tk,rtk->rt', E2, q) * inv
        hit = ok & (u >= 0) & (v >= 0) & (u + v <= 1) & (t > 1e-4) & (t <= maxt)
        t = np.where(hit, t, np.inf)
        out[s0:s0 + step] = t.min(axis=1)
    return out


def frame(s):
    a = math.radians(s.get('rotation', [0, 0, 0])[2])
    return np.array(s['center'], float), np.array(s['size'], float), np.array([math.cos(a), math.sin(a), 0.0]), \
        np.array([-math.sin(a), math.cos(a), 0.0]), np.array([0.0, 0.0, 1.0])


def plan_rect(r):
    """union plan area of the room's boxes over the area of their best oriented rectangle (yaw of each box tried)"""
    boxes = [s for s in r['shapes'] if s['type'] == 'box']
    if not boxes:
        return 0.0
    pts = []
    for s in boxes:
        c, sz, ex, ey, _ = frame(s)
        for kx in (-1, 1):
            for ky in (-1, 1):
                pts.append(c[:2] + ex[:2] * kx * sz[0] / 2 + ey[:2] * ky * sz[1] / 2)
    pts = np.array(pts)
    best = None
    for s in boxes:
        _, _, ex, ey, _ = frame(s)
        u, v = pts @ ex[:2], pts @ ey[:2]
        a = (u.max() - u.min()) * (v.max() - v.min())
        if best is None or a < best[0]:
            best = (a, ex, ey, u.min(), u.max(), v.min(), v.max())
    a, ex, ey, u0, u1, v0, v1 = best
    n = 64
    uu, vv = np.meshgrid(np.linspace(u0, u1, n, endpoint=False) + (u1 - u0) / n / 2,
                         np.linspace(v0, v1, n, endpoint=False) + (v1 - v0) / n / 2)
    P = np.outer(uu.ravel(), ex[:2]) + np.outer(vv.ravel(), ey[:2])
    inn = np.zeros(len(P), bool)
    for s in boxes:
        c, sz, sx, sy, _ = frame(s)
        d = P - c[:2]
        inn |= (np.abs(d @ sx[:2]) <= sz[0] / 2 + 1) & (np.abs(d @ sy[:2]) <= sz[1] / 2 + 1)
    return float(inn.mean())


def mid_room(s):
    c, sz, ex, ey, ez = frame(s)
    reach = CELL * 0.5
    tr = tris_in(c - sz.max() / 2 - reach - 64, c + sz.max() / 2 + reach + 64)
    walled = 0
    for ax, n in ((0, ex), (1, ey)):
        for sg in (1, -1):
            other = ey if ax == 0 else ex
            w = sz[1] if ax == 0 else sz[0]
            O, D = [], []
            for i in range(5):
                for k in range(3):
                    p = c + n * sg * (sz[ax] / 2 - 2) + other * (-w / 2 + (i + 0.5) * w / 5) + ez * (-sz[2] / 2 + (k + 0.5) * sz[2] / 3)
                    O.append(p)
                    D.append(n * sg)
            t = cast(tr, np.array(O), np.array(D), reach + 2)
            if np.isfinite(t).mean() >= 0.5:
                walled += 1
    return walled


def thr_check(t):
    s = t['shape']
    c, sz, ex, ey, ez = frame(s)
    hx, hy, hz = sz / 2
    tr = tris_in(c - max(hx, hz) - 128, c + max(hx, hz) + 128)
    O, D = [], []
    for side in (-1, 1):
        for off in (16, 32, 48):
            for k in range(3):
                O.append(c + ex * side * (hx + off) + ez * (-hz + (k + 0.5) * hz * 2 / 3) - ey * (hy + 32))
                D.append(ey)
    ring = float(np.isfinite(cast(tr, np.array(O), np.array(D), 2 * hy + 64)).mean())
    n = 32
    O, D = [], []
    for i in range(n):
        for k in range(n):
            O.append(c + ex * (-hx + (i + 0.5) * 2 * hx / n) + ez * (-hz + (k + 0.5) * 2 * hz / n) - ey * (hy + 16))
            D.append(ey)
    op = float(np.isinf(cast(tr, np.array(O), np.array(D), 2 * hy + 32)).mean())
    return ring, op


lines = []
print('cell C %.1f rooms %d thresholds %d' % (CELL, len(rooms), len(thrs)))
tot = {'rooms': 0, 'boxes': 0, 'rect': 0, 'rect1': 0, 'mid': 0, 'in': 0, 'else': 0, 'open': []}
for r in rooms:
    nb = len(r['shapes'])
    rect = plan_rect(r)
    mids = [i for i, s in enumerate(r['shapes']) if s['type'] == 'box' and mid_room(s) == 0]
    tot['rooms'] += 1
    tot['boxes'] += nb
    if rect >= 0.9:
        tot['rect'] += 1
        tot['rect1'] += nb == 1
    tot['mid'] += len(mids)
    lines.append('room\t%s\t%s\tboxes %d\ttypes %s\trect %.2f\tmid %s' % (r['_cell'], r['room'], nb,
                 ''.join(s['type'][0] for s in r['shapes']), rect, ','.join(map(str, mids)) or '-'))
for t in thrs:
    if abs(t.get('normal', [0, 0, 0])[2]) > 0.7:
        continue
    ring, op = thr_check(t)
    tot['in' if ring >= 0.5 else 'else'] += 1
    tot['open'].append(op)
    s = t['shape']
    lines.append('thr\t%s\t%s\t%s\t%s\tsize %s\tring %.2f\topen %.2f\t%s' % (t['_cell'], t.get('threshold', ''), '<->'.join(t.get('between', [])),
                 t.get('opening', ''), 'x'.join('%.0f' % v for v in s['size']), ring, op, 'IN' if ring >= 0.5 else 'ELSEWHERE'))
for l in lines:
    print(l)
o = tot['open']
print('TOTAL rooms %d boxes %d (%.2f per room) rect rooms %d of them one box %d; mid-room boxes %d; thresholds in a real opening %d '
      'elsewhere %d; open share mean %.3f min %.3f' % (tot['rooms'], tot['boxes'], tot['boxes'] / max(1, tot['rooms']), tot['rect'], tot['rect1'],
                                                      tot['mid'], tot['in'], tot['else'], np.mean(o) if o else 0, min(o) if o else 0))
if 'tsv' in opt:
    open(opt['tsv'], 'w').write('\n'.join(lines) + '\n')
if 'bar' in opt:
    # lane VOLPLACE2 bars: no room<->room threshold away from a real opening, and few boxes per room
    rr = 0
    for t in thrs:
        if abs(t.get('normal', [0, 0, 0])[2]) > 0.7 or 'outside' in t.get('between', []):
            continue
        if thr_check(t)[0] < 0.5:
            rr += 1
    per = tot['boxes'] / max(1, tot['rooms'])
    fails = []
    if rr:
        fails.append('room<->room thresholds away from a real opening %d (bar 0)' % rr)
    if per > 3.0:
        fails.append('boxes per room %.2f (bar 3.0)' % per)
    print('VERDICT %s%s' % ('FAIL: ' if fails else 'PASS', '; '.join(fails)))
    sys.exit(1 if fails else 0)
