"""TERR1 continuation: the sky law against a PHYSICAL ray cast (cosine-weighted hemisphere about +Z).

Samples: the canyon texels of canyon_check.py (8 named Boston cells, same candidate rule, same seed, `per` each), its
open-ground calibration texels, and a spread of `near` / `deck` texels from sky_on.json (skycast.py's 512-unit grid).
Per sample it writes
  * the reference: cosine-weighted sky fraction (7 bands 0-10-20-30-45-60-75-90 deg, weight sin^2 b - sin^2 a,
    32 azimuths a band) through the terrain height sheet and every level-0 triangle of the bake's .lodo/.lodi,
    reach 1458 u and 10000 u, from 2 u over the visible surface (road-lifted, lodgenSkySurface's rule);
  * the LAW'S INPUTS per march direction (8): terrain maxSlope (7 steps 128 * 1.5^k, bilinear heights), and the
    object lattice's wall / ceiling opening read every 64 u out to 1458 (lodgenSkyDirBlocked's walk, slab bar on),
    so any law over them is scored offline by lawfit.py without casting again;
  * mask B of the after and before bakes at the texel.
usage: python physlaw.py <after VT.2.lodt> <before VT.2.lodt> <objh dump> <lodo/lodi stem> <sky_on.json> <out.json>
       [per_cell] [n_near] [n_deck]
"""
import sys, json, math, time
import numpy as np

T1 = 'E:/Projects/NifskopeWWE-night/scratchpad/merge1_20260929/terr1_tools'
sys.path.insert(0, T1)
import vtmosaic as VM
import skycast as SC

CELLS = {'3,-7': 'TheaterDistrictExt02', '3,-3': 'VaultTecOfficeExt02', '-5,-7': 'FensStreetSewerExt',
         '-4,-8': 'DiamondCityExt', '-3,-5': 'FensBankExt', '3,-1': 'BeaconHillApartmentsExt02',
         '0,-7': 'BackBayFence01', '1,-5': 'HubrisComicsExt'}
BANDS = [0, 10, 20, 30, 45, 60, 75, 90]
NAZ = 32
CELL = 128.0


def main():
    after, before, objh, stem, skyon, outp = sys.argv[1:7]
    per = int(sys.argv[7]) if len(sys.argv) > 7 else 12
    n_near = int(sys.argv[8]) if len(sys.argv) > 8 else 120
    n_deck = int(sys.argv[9]) if len(sys.argv) > 9 else 60
    t0 = time.time()
    A = VM.Sheets(after); Bf = VM.Sheets(before)
    H = SC.Height(VM.height_units(A.mosaic(4)))
    mA = A.mosaic(5)[..., 2]; mB = Bf.mosaic(5)[..., 2]
    F = SC.load_objh(objh)
    L = SC.ND.read_lodo(stem + '.lodo'); Ti = SC.ND.read_lodi(stem + '.lodi')
    tris, npl = SC.world_triangles(L, Ti, -8, -12, 3, -1)
    print('triangles', len(tris), 'placements', npl, '%.0f s' % (time.time() - t0), flush=True)
    BIN = 256.0
    tmin = tris[:, :, :2].min(axis=1); tmax = tris[:, :, :2].max(axis=1)
    bx0 = np.floor(tmin[:, 0] / BIN).astype(int); bx1 = np.floor(tmax[:, 0] / BIN).astype(int)
    by0 = np.floor(tmin[:, 1] / BIN).astype(int); by1 = np.floor(tmax[:, 1] / BIN).astype(int)
    bins = {}
    for k in range(len(tris)):
        for by in range(by0[k], by1[k] + 1):
            for bx in range(bx0[k], bx1[k] + 1):
                bins.setdefault((bx, by), []).append(k)
    bins = {k: np.array(v) for k, v in bins.items()}

    # ---- samples: canyon (canyon_check.py's rule and seed), open calibration, near / deck spread
    rng = np.random.default_rng(20260927)
    samples = []
    for key, name in CELLS.items():
        X, Y = map(int, key.split(','))
        cand = []
        for yy in np.arange(Y * 4096 + 40, (Y + 1) * 4096, 48.0):
            for xx in np.arange(X * 4096 + 40, (X + 1) * 4096, 48.0):
                h0 = float(H.at(np.array([xx]), np.array([yy]))[0])
                lo, hi = SC.span_at(F, xx, yy)
                if hi > h0 + 128.0:
                    continue
                walls = 0
                for dx, dy in SC.DIRS:
                    for d in (128, 192, 288, 432, 648):
                        l2, h2 = SC.span_at(F, xx + dx * d, yy + dy * d)
                        if h2 > -1e29 and (h2 - h0) / d >= 0.577:
                            walls += 1
                            break
                if walls >= 4:
                    cand.append((xx, yy))
        pick = rng.choice(len(cand), size=min(per, len(cand)), replace=False) if cand else []
        for k in pick:
            samples.append(('canyon', key, name, cand[k][0], cand[k][1]))
        print(key, name, 'canyon candidates', len(cand), flush=True)
    ncal = 0; tries = 0
    while ncal < 3 * per and tries < 20000:
        tries += 1
        xx = float(rng.uniform(-6 * 4096, 2 * 4096)); yy = float(rng.uniform(-10 * 4096, -2 * 4096))
        h0 = float(H.at(np.array([xx]), np.array([yy]))[0])
        tall = False
        for oy in np.arange(-1458, 1459, 128.0):
            for ox in np.arange(-1458, 1459, 128.0):
                if ox * ox + oy * oy > 1458 * 1458:
                    continue
                l2, h2 = SC.span_at(F, xx + ox, yy + oy)
                if h2 > -1e29 and h2 > h0 + 64:
                    tall = True
                    break
            if tall:
                break
        if not tall:
            samples.append(('open', 'open', 'open ground', xx, yy)); ncal += 1
    rows0 = json.load(open(skyon))['rows']
    for cls, n in (('near', n_near), ('deck', n_deck)):
        c = [r for r in rows0 if r['cls'] == cls]
        for k in rng.choice(len(c), size=min(n, len(c)), replace=False):
            samples.append((cls, cls, cls, float(c[k]['x']), float(c[k]['y'])))
    print('samples', len(samples), flush=True)

    dirs_phys = []
    for a, b in zip(BANDS[:-1], BANDS[1:]):
        sa, sb = math.sin(math.radians(a)), math.sin(math.radians(b))
        el = math.asin(math.sqrt((sa * sa + sb * sb) / 2)); wgt = (sb * sb - sa * sa) / NAZ
        for k in range(NAZ):
            az = 2 * math.pi * (k + 0.5 * (len(dirs_phys) // NAZ % 2)) / NAZ
            dirs_phys.append((math.cos(az) * math.cos(el), math.sin(az) * math.cos(el), math.sin(el), wgt))

    def hit(o, d, reach):
        """first hit distance along unit dir d within horizontal reach, terrain or triangle; None if none
        (canyon_check.py's caster)"""
        hz = math.hypot(d[0], d[1])
        tmax_ = reach / hz
        s = np.arange(16.0, reach + 0.1, 16.0)
        px = o[0] + d[0] / hz * s; py = o[1] + d[1] / hz * s
        ok = (px > VM.WX0) & (px < VM.WX0 + 12 * 4096) & (py < 0) & (py > -12 * 4096)
        best = None
        if ok.any():
            tz = H.at(px[ok], py[ok]); rz = o[2] + d[2] / hz * s[ok]
            k = np.nonzero(tz > rz)[0]
            if len(k):
                best = s[ok][k[0]] / hz
        seen = set(); dv = np.array(d)
        for ss in np.arange(0, reach + BIN, BIN / 2):
            if best is not None and ss / hz > best + BIN:
                break
            bx = int(math.floor((o[0] + d[0] / hz * ss) / BIN)); by = int(math.floor((o[1] + d[1] / hz * ss) / BIN))
            ks = []
            for ox in (-1, 0, 1):
                for oy in (-1, 0, 1):
                    kk = (bx + ox, by + oy)
                    if kk in seen:
                        continue
                    seen.add(kk)
                    a_ = bins.get(kk)
                    if a_ is not None:
                        ks.append(a_)
            if not ks:
                continue
            tr = tris[np.unique(np.concatenate(ks))]
            v0 = tr[:, 0]; e1 = tr[:, 1] - v0; e2 = tr[:, 2] - v0
            p = np.cross(dv, e2); det = np.einsum('ij,ij->i', e1, p)
            okd = np.abs(det) > 1e-9
            inv = np.where(okd, 1.0 / np.where(okd, det, 1.0), 0.0)
            tv = o - v0
            u = np.einsum('ij,ij->i', tv, p) * inv
            q = np.cross(tv, e1)
            v = (q @ dv) * inv
            t = np.einsum('ij,ij->i', e2, q) * inv
            h = okd & (u >= 0) & (v >= 0) & (u + v <= 1) & (t > 0.5) & (t <= tmax_)
            if h.any():
                tt = float(t[h].min())
                if best is None or tt < best:
                    best = tt
        return best

    rows = []
    for cls, key, name, sx, sy in samples:
        h0 = float(H.at(np.array([sx]), np.array([sy]))[0])
        lo, hi = SC.span_at(F, sx, sy)
        if hi > -1e29 and hi > h0 and hi - h0 > 128.0 and lo <= h0 + 128.0:
            continue            # under a building: not seen
        hs = hi if (hi > -1e29 and hi > h0 and hi - h0 <= 128.0) else h0
        o = np.array([sx, sy, hs + 2.0])
        v1458 = v10k = 0.0
        for d in dirs_phys:
            t = hit(o, d[:3], 10000.0)
            hz = math.hypot(d[0], d[1])
            if t is None:
                v10k += d[3]; v1458 += d[3]
            elif t * hz > 1458.0:
                v1458 += d[3]
        # the law's inputs, exactly as the bake walks them
        MS, WALL, COPEN, HAVE = [], [], [], []
        for dx, dy in SC.DIRS:
            ms = 0.0
            dist = 128.0
            while dist <= 2048.0:
                dh = float(H.at(np.array([sx + dx * dist]), np.array([sy + dy * dist]))[0]) - h0
                if dh > 0:
                    ms = max(ms, dh / dist)
                dist *= 1.5
            wall, copen, have, covered = 0.0, 0.0, False, True
            for k in range(1, 24):
                dd = 1458.0 if k == 23 else 64.0 * k
                l2, h2 = SC.span_at(F, sx + dx * dd, sy + dy * dd)
                if h2 < -1e29:
                    covered = False
                    continue
                if l2 <= hs + CELL:
                    covered = False
                    if h2 - hs > 0:
                        wall = max(wall, (h2 - hs) / dd)
                elif covered:
                    op = (l2 - hs) / dd
                    if not have or op < copen:
                        copen, have = op, True
            MS.append(ms); WALL.append(wall); COPEN.append(copen); HAVE.append(have)
        j, i = VM.texel_of(sx, sy)
        r = dict(cls=cls, cell=key, name=name, x=round(sx), y=round(sy), maskB_after=int(mA[j, i]),
                 maskB_before=int(mB[j, i]), phys1458=round(255 * v1458, 2), phys10k=round(255 * v10k, 2),
                 ms=MS, wall=WALL, copen=COPEN, have=HAVE)
        rows.append(r)
        print(cls, key, r['maskB_after'], r['maskB_before'], r['phys1458'], r['phys10k'], '%.0f s' % (time.time() - t0),
              flush=True)
    json.dump(dict(rows=rows), open(outp, 'w'))
    print('wrote', outp, len(rows), '%.0f s' % (time.time() - t0))


if __name__ == '__main__':
    main()
