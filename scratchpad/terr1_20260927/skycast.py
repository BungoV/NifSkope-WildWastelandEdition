"""TERR1 sky measurement: which object sky term matches a RAY CAST through the placed objects' LOD meshes.

Reference (per sample texel): 8 directions (the bake's `dirs`), J elevations uniform in the march's own measure
F = t / (1 + t) (t = tan elevation), each ray cast from 1 unit above the ground out to 1458 units horizontally
(the march's reach) against
  * the terrain: the VT.2 height sheet, sampled every 16 units along the ray (bilinear), and
  * the objects: every level-0 LOD triangle of the Boston box (.lodo/.lodi of ao2 reg_x7, slot = first authored,
    the object height field's own rule), Moller-Trumbore.
  blocked(dir) = fraction of the J rays that hit; vis_ref = clamp(1 - 1.6 * sum(blocked) / 8).

Candidates, computed from the SAME inputs the bake has (height sheet for the terrain march, the object height field
dump `--dump-object-ao` for the objects):
  T      terrain march only (today's mask B law, 7 steps 128 * 1.5^k)
  A(s)   T * objectTerm(s)          -- lane GROUND1's product, strength s
  B      per-direction UNION: wall = max(terrain slope, object wall) then the slab law's ceiling, one F sum
Sample classes (from the object height field): `under` (an object square over the texel standing above it -- not
seen, excluded), `near` (an occupied square within the reach standing 64+ units above the texel), `open` (none).

usage: python skycast.py <sheet VT.2.lodt> <objh dump> <out.json> [step_units] [J]
"""
import sys, os, json, math, time, struct
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, 'E:/Projects/NifskopeWWE-bake2/tests/spells')
import vtmosaic as VM
import lodgen_native_decode as ND
import lodl_channels_table as CT

OBJ = ('C:/Users/bungo/AppData/Local/Temp/claude/E--Projects-Claude/b560e4ec-6e66-4c21-9572-1ad4acca0043/'
       'scratchpad/ao2/reg_x7/mod/FO4CSLOD/Commonwealth/Commonwealth')
DIRS = [(1, 0), (-1, 0), (0, 1), (0, -1), (0.7071, 0.7071), (0.7071, -0.7071), (-0.7071, 0.7071), (-0.7071, -0.7071)]
STEPS = []
d = 128.0
while d <= 2048.0:
    STEPS.append(d)
    d *= 1.5
REACH = STEPS[-1]            # 1458
DSTEPS = [64.0 * k for k in range(1, 23)] + [REACH]
SAMPLE_CELLS = (-6, -10, 1, -3)   # x0 y0 x1 y1: the reach (1458) stays inside the 12 x 12 box


def load_objh(path):
    b = open(path, 'rb').read()
    assert b[:4] == b'OBJH'
    gx0, gy0, gw, gh = struct.unpack_from('<4i', b, 4)
    cell = struct.unpack_from('<f', b, 20)[0]
    n = gw * gh
    hi = np.frombuffer(b, dtype='<f4', count=n, offset=24).reshape(gh, gw)
    lo = np.frombuffer(b, dtype='<f4', count=n, offset=24 + 4 * n).reshape(gh, gw)
    return dict(gx0=gx0, gy0=gy0, gw=gw, gh=gh, cell=cell, hi=hi, lo=lo)


def span_at(F, x, y):
    gx = int(math.floor(x / F['cell'])) - F['gx0']
    gy = int(math.floor(y / F['cell'])) - F['gy0']
    if gx < 0 or gy < 0 or gx >= F['gw'] or gy >= F['gh']:
        return 1e30, -1e30
    return float(F['lo'][gy, gx]), float(F['hi'][gy, gx])


class Height:
    def __init__(self, h):
        self.h = h          # game units, mosaic

    def at(self, x, y):
        """bilinear on texel centres; x, y arrays"""
        fi = (x - VM.WX0) / VM.UPT - 0.5
        fj = (VM.WYTOP - y) / VM.UPT - 0.5
        n = self.h.shape[0]
        fi = np.clip(fi, 0, n - 1.001)
        fj = np.clip(fj, 0, n - 1.001)
        i0 = np.floor(fi).astype(int)
        j0 = np.floor(fj).astype(int)
        ti = fi - i0
        tj = fj - j0
        h = self.h
        return ((h[j0, i0] * (1 - ti) + h[j0, i0 + 1] * ti) * (1 - tj)
                + (h[j0 + 1, i0] * (1 - ti) + h[j0 + 1, i0 + 1] * ti) * tj)


def world_triangles(L, T, x0, y0, x1, y1):
    sel = CT.drawn(L, T, x0, y0, x1, y1, -1, 0)
    cache = {}
    P = []
    for ii, inst, mesh in sel:
        if mesh not in cache:
            me = L['meshes'][mesh]
            pts = []
            tri = []
            for c in range(me['clusterFirst'], me['clusterFirst'] + me['clusterCount']):
                cll = L['clusterLods'][c]
                if not (cll['level'] == 0 or (cll['level'] < 0 and cll['parentCount'] == 0)):
                    continue
                cl = L['clusters'][c]
                base = len(pts)
                for v in range(cl['vertexCount']):
                    lv = L['vertices'][cl['vertexBase'] + v]
                    pts.append([me['aabbMin'][0] + lv['px'] / 65535.0 * me['aabbExtent'][0],
                                me['aabbMin'][1] + lv['py'] / 65535.0 * me['aabbExtent'][1],
                                me['aabbMin'][2] + lv['pz'] / 65535.0 * me['aabbExtent'][2]])
                li = c * 48
                for t in range(cl['triangleCount']):
                    a, b, cc = L['localIndices'][li + 3 * t:li + 3 * t + 3]
                    if max(a, b, cc) < cl['vertexCount']:
                        tri.append([base + a, base + b, base + cc])
            cache[mesh] = (np.array(pts, dtype=np.float64).reshape(-1, 3), np.array(tri, dtype=np.int64).reshape(-1, 3))
        lp, tri = cache[mesh]
        if not len(tri):
            continue
        m = np.array(inst['m'], dtype=np.float64).reshape(3, 3)
        wp = np.array([inst['x'], inst['y'], inst['z']]) + (lp * inst['scaleF']) @ m.T
        P.append(wp[tri])
    return np.concatenate(P, axis=0), len(sel)


def main():
    sheet, objh, outp = sys.argv[1], sys.argv[2], sys.argv[3]
    step = float(sys.argv[4]) if len(sys.argv) > 4 else 512.0
    J = int(sys.argv[5]) if len(sys.argv) > 5 else 16
    t0 = time.time()
    S = VM.Sheets(sheet)
    H = Height(VM.height_units(S.mosaic(4)))
    maskB = S.mosaic(5)[..., 2]
    F = load_objh(objh)
    L = ND.read_lodo(OBJ + '.lodo')
    Ti = ND.read_lodi(OBJ + '.lodi')
    tris, nplace = world_triangles(L, Ti, -8, -12, 3, -1)
    print('triangles', len(tris), 'placements', nplace, 'load %.0f s' % (time.time() - t0), flush=True)
    # bucket triangles by xy bbox into 256-unit bins
    BIN = 256.0
    tmin = tris[:, :, :2].min(axis=1)
    tmax = tris[:, :, :2].max(axis=1)
    bx0 = np.floor(tmin[:, 0] / BIN).astype(int); bx1 = np.floor(tmax[:, 0] / BIN).astype(int)
    by0 = np.floor(tmin[:, 1] / BIN).astype(int); by1 = np.floor(tmax[:, 1] / BIN).astype(int)
    bins = {}
    for k in range(len(tris)):
        for by in range(by0[k], by1[k] + 1):
            for bx in range(bx0[k], bx1[k] + 1):
                bins.setdefault((bx, by), []).append(k)
    bins = {k: np.array(v) for k, v in bins.items()}
    print('bins', len(bins), '%.0f s' % (time.time() - t0), flush=True)

    Fs = (np.arange(J) + 0.5) / J
    tans = Fs / (1 - Fs)
    xs0, ys0, xs1, ys1 = SAMPLE_CELLS
    rows = []
    sx_list = np.arange(xs0 * 4096 + step / 2, (xs1 + 1) * 4096, step)
    sy_list = np.arange(ys0 * 4096 + step / 2, (ys1 + 1) * 4096, step)
    for sy in sy_list:
        for sx in sx_list:
            h0 = float(H.at(np.array([sx]), np.array([sy]))[0])
            lo, hi = span_at(F, sx, sy)
            # the ground's visible surface: a LOW cover (road, pavement, rubble; top within one cell, 128 u, of the
            # terrain) lifts the sample to its top; a deck whose underside is a cell or more up leaves it; an object
            # reaching from the ground past one cell is a building over the texel ('under', excluded)
            h0o = h0
            deck = False
            cls = None
            if hi > -1e29 and hi > h0:
                if hi - h0 <= 128.0:
                    h0o = hi
                elif lo > h0 + 128.0:
                    deck = True
                else:
                    cls = 'under'
            if cls is None:
                near = False
                for gy in np.arange(sy - REACH, sy + REACH + 1, 128.0):
                    for gx in np.arange(sx - REACH, sx + REACH + 1, 128.0):
                        if (gx - sx) ** 2 + (gy - sy) ** 2 > REACH * REACH:
                            continue
                        l2, h2 = span_at(F, gx, gy)
                        if h2 > -1e29 and h2 > h0o + 64:
                            near = True
                            break
                    if near:
                        break
                cls = 'deck' if deck else ('near' if near else 'open')
                lifted = h0o > h0
            if cls == 'under':
                rows.append(dict(x=sx, y=sy, cls=cls))
                continue
            # ---- candidates' per-direction terms
            FT, FO, FB, FD, FE = [], [], [], [], []
            for (dx, dy) in DIRS:
                ms = 0.0
                for dist in STEPS:
                    dh = float(H.at(np.array([sx + dx * dist]), np.array([sy + dy * dist]))[0]) - h0
                    if dh > 0:
                        ms = max(ms, dh / dist)
                wall, ceil_open, have_ceil, covered = 0.0, 0.0, False, True
                for dist in STEPS:
                    l2, h2 = span_at(F, sx + dx * dist, sy + dy * dist)
                    if h2 < -1e29:
                        covered = False
                        continue
                    if l2 <= h0:
                        covered = False
                        if h2 - h0 > 0:
                            wall = max(wall, (h2 - h0) / dist)
                    elif covered:
                        op = (l2 - h0) / dist
                        if not have_ceil or op < ceil_open:
                            ceil_open, have_ceil = op, True
                ceil_b = (1 - ceil_open / (1 + ceil_open)) if have_ceil else 0.0
                FT.append(ms / (1 + ms))
                FO.append(min(1.0, wall / (1 + wall) + ceil_b))
                wu = max(ms, wall)
                FB.append(min(1.0, wu / (1 + wu) + ceil_b))
                # dense variant: the object lattice read every 64 units (terrain march unchanged)
                wall, ceil_open, have_ceil, covered = 0.0, 0.0, False, True
                for dist in DSTEPS:
                    l2, h2 = span_at(F, sx + dx * dist, sy + dy * dist)
                    if h2 < -1e29:
                        covered = False
                        continue
                    if l2 <= h0:
                        covered = False
                        if h2 - h0 > 0:
                            wall = max(wall, (h2 - h0) / dist)
                    elif covered:
                        op = (l2 - h0) / dist
                        if not have_ceil or op < ceil_open:
                            ceil_open, have_ceil = op, True
                ceil_b = (1 - ceil_open / (1 + ceil_open)) if have_ceil else 0.0
                wu = max(ms, wall)
                FD.append(min(1.0, wu / (1 + wu) + ceil_b))
                # FE: dense, from the ground's visible surface h0o, a ceiling only a cell (128) or more above it
                wall, ceil_open, have_ceil, covered = 0.0, 0.0, False, True
                for dist in DSTEPS:
                    l2, h2 = span_at(F, sx + dx * dist, sy + dy * dist)
                    if h2 < -1e29:
                        covered = False
                        continue
                    if l2 <= h0o + 128.0:
                        covered = False
                        if h2 - h0o > 0:
                            wall = max(wall, (h2 - h0o) / dist)
                    elif covered:
                        op = (l2 - h0o) / dist
                        if not have_ceil or op < ceil_open:
                            ceil_open, have_ceil = op, True
                ceil_b = (1 - ceil_open / (1 + ceil_open)) if have_ceil else 0.0
                wu = max(ms, wall)
                FE.append(min(1.0, wu / (1 + wu) + ceil_b))
            # ---- reference ray cast (from the visible surface h0o)
            h0 = h0o
            RT, RB = [], []
            ss = np.arange(16.0, REACH + 0.1, 16.0)
            for (dx, dy) in DIRS:
                hz = H.at(sx + dx * ss, sy + dy * ss)                     # (ns,)
                rz = h0 + 1.0 + tans[:, None] * ss[None, :]                 # (J, ns)
                hitT = (hz[None, :] > rz).any(axis=1)
                # objects: triangles in bins along the segment
                ks = set()
                for s in np.arange(0, REACH + BIN, BIN / 2):
                    bx = int(math.floor((sx + dx * s) / BIN)); by = int(math.floor((sy + dy * s) / BIN))
                    for ox in (-1, 0, 1):
                        for oy in (-1, 0, 1):
                            a = bins.get((bx + ox, by + oy))
                            if a is not None:
                                ks.update(a.tolist())
                hitO = np.zeros(J, dtype=bool)
                if ks:
                    tr = tris[np.fromiter(ks, dtype=np.int64)]
                    v0 = tr[:, 0]; e1 = tr[:, 1] - v0; e2 = tr[:, 2] - v0
                    o = np.array([sx, sy, h0 + 1.0])
                    for jj in range(J):
                        dvec = np.array([dx, dy, tans[jj]])
                        p = np.cross(dvec, e2)
                        det = np.einsum('ij,ij->i', e1, p)
                        ok = np.abs(det) > 1e-9
                        inv = np.where(ok, 1.0 / np.where(ok, det, 1.0), 0.0)
                        tv = o - v0
                        u = np.einsum('ij,ij->i', tv, p) * inv
                        q = np.cross(tv, e1)
                        v = (q @ dvec) * inv
                        t = np.einsum('ij,ij->i', e2, q) * inv
                        hit = ok & (u >= 0) & (v >= 0) & (u + v <= 1) & (t > 0.5) & (t <= REACH)
                        hitO[jj] = bool(hit.any())
                RT.append(float(hitT.mean()))
                RB.append(float((hitT | hitO).mean()))
            j, i = VM.texel_of(sx, sy)
            rows.append(dict(x=sx, y=sy, cls=cls, lifted=bool(lifted), h0=h0, maskB=int(maskB[j, i]), FT=FT, FO=FO, FB=FB, FD=FD, FE=FE,
                             RT=RT, RB=RB))
        print('row y=%.0f done, %d samples, %.0f s' % (sy, len(rows), time.time() - t0), flush=True)
    json.dump(dict(step=step, J=J, triangles=int(len(tris)), placements=nplace, rows=rows), open(outp, 'w'))
    print('wrote', outp, '%.0f s' % (time.time() - t0))


if __name__ == '__main__':
    main()
