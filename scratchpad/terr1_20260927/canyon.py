"""TERR1 sky gates: street canyons vs open ground, before (terrain-only mask B) and after.

usage: python canyon.py <before VT.2.lodt> <after VT.2.lodt> <objh dump> <cellnames.json> <out.json> [stride]

Texel classes, from the object height field (the bake's own lattice) and the height sheet, every `stride` texels:
  under   an object square over the texel stands 128+ units above it (a building, a roof, a deck: excluded)
  canyon  not under; in at least 4 of the 8 march directions an object wall within 648 units rises at 30+ degrees
          (tan >= 0.577) -- a street between buildings
  open    not under; no object square within the march reach (1458) stands 64+ units above the texel
  other   the rest
Numbers: mean mask B per class before / after; per named cell (Fallout4.esm CELL EditorID) the canyon and open means;
open |after - before| distribution (gate: within 1 level); corr(after, before) on open texels (gate > 0.5).
"""
import sys, json, struct
import numpy as np
import vtmosaic as VM

DIRS = [(1, 0), (-1, 0), (0, 1), (0, -1), (0.7071, 0.7071), (0.7071, -0.7071), (-0.7071, 0.7071), (-0.7071, -0.7071)]
STEPS = [128.0 * 1.5 ** k for k in range(7)]


def load_objh(path):
    b = open(path, 'rb').read()
    gx0, gy0, gw, gh = struct.unpack_from('<4i', b, 4)
    cell = struct.unpack_from('<f', b, 20)[0]
    n = gw * gh
    hi = np.frombuffer(b, dtype='<f4', count=n, offset=24).reshape(gh, gw)
    lo = np.frombuffer(b, dtype='<f4', count=n, offset=24 + 4 * n).reshape(gh, gw)
    return gx0, gy0, gw, gh, cell, hi, lo


def predict_union(Hfull, x, y, h, field):
    """OFFLINE PREDICTION of the shipped law (lodgenSkyDirBlocked): per direction the terrain march's maxSlope
    (7 steps, bilinear heights) unioned with the object lattice read every 64 units (slab law), summed, vis byte."""
    gx0, gy0, gw, gh, cell, HI, LO = field
    N = Hfull.shape[0]

    def hat(px, py):
        fi = np.clip((px - VM.WX0) / VM.UPT - 0.5, 0, N - 1.001)
        fj = np.clip((VM.WYTOP - py) / VM.UPT - 0.5, 0, N - 1.001)
        i0 = np.floor(fi).astype(int); j0 = np.floor(fj).astype(int)
        ti = fi - i0; tj = fj - j0
        return ((Hfull[j0, i0] * (1 - ti) + Hfull[j0, i0 + 1] * ti) * (1 - tj)
                + (Hfull[j0 + 1, i0] * (1 - ti) + Hfull[j0 + 1, i0 + 1] * ti) * tj)

    def span(px, py):
        gx = np.floor(px / cell).astype(np.int64) - gx0
        gy = np.floor(py / cell).astype(np.int64) - gy0
        ok = (gx >= 0) & (gy >= 0) & (gx < gw) & (gy < gh)
        gxc = np.clip(gx, 0, gw - 1); gyc = np.clip(gy, 0, gh - 1)
        hi = np.where(ok, HI[gyc, gxc], -1e30)
        lo = np.where(ok, LO[gyc, gxc], 1e30)
        return lo, hi

    occl = np.zeros_like(h)
    occlT = np.zeros_like(h)
    lo0, hi0 = span(x, y)
    hs = np.where((hi0 > -1e29) & (hi0 > h) & (hi0 - h <= 128.0), hi0, h)   # lodgenSkySurface
    ht = h
    for dx, dy in DIRS:
        ms = np.zeros_like(h)
        for d in STEPS:
            dh = hat(x + dx * d, y + dy * d) - h
            ms = np.maximum(ms, np.where(dh > 0, dh / d, 0))
        wall = np.zeros_like(h); ceil_open = np.zeros_like(h)
        have = np.zeros_like(h, dtype=bool); covered = np.ones_like(h, dtype=bool)
        for k in range(1, 24):
            d = 1458.0 if k == 23 else 64.0 * k
            lo, hi = span(x + dx * d, y + dy * d)
            empty = hi < -1e29
            isw = ~empty & (lo <= hs + 128.0)
            wall = np.where(isw & (hi - hs > 0), np.maximum(wall, (hi - hs) / d), wall)
            isc = ~empty & ~isw & covered
            op = (lo - hs) / d
            upd = isc & (~have | (op < ceil_open))
            ceil_open = np.where(upd, op, ceil_open)
            have |= isc
            covered &= ~(empty | isw)
        wu = np.maximum(ms, wall)
        wb = wu / (1 + wu)
        occl += np.where(have, np.minimum(1, wb + 1 - ceil_open / (1 + ceil_open)), wb)
        occlT += ms / (1 + ms)
    vis = np.clip(1 - occl / 8 * 1.6, 0, 1)
    visT = np.clip(1 - occlT / 8 * 1.6, 0, 1)
    return np.floor(vis * 255 + 0.5), np.floor(visT * 255 + 0.5)


def main():
    before = VM.Sheets(sys.argv[1])
    predict = sys.argv[2] == 'predict'
    after = before if predict else VM.Sheets(sys.argv[2])
    gx0, gy0, gw, gh, cell, HI, LO = load_objh(sys.argv[3])
    names = json.load(open(sys.argv[4]))
    stride = int(sys.argv[6]) if len(sys.argv) > 6 else 4
    Hfull = VM.height_units(after.mosaic(4))
    h = Hfull[::stride, ::stride]
    bB = before.mosaic(5)[::stride, ::stride, 2].astype(np.float64)
    n = h.shape[0]
    jj, ii = np.mgrid[0:n, 0:n]
    x = VM.WX0 + (ii * stride + 0.5) * VM.UPT
    y = VM.WYTOP - (jj * stride + 0.5) * VM.UPT
    if predict:
        # prediction vs prediction: "before" becomes the Python terrain march, so the open-ground check does not
        # measure the Python march's own 4-level sampling difference from the bake
        aB, bB = predict_union(Hfull, x, y, h, (gx0, gy0, gw, gh, cell, HI, LO))
    else:
        aB = after.mosaic(5)[::stride, ::stride, 2].astype(np.float64)

    def span(px, py):
        gx = np.floor(px / cell).astype(np.int64) - gx0
        gy = np.floor(py / cell).astype(np.int64) - gy0
        ok = (gx >= 0) & (gy >= 0) & (gx < gw) & (gy < gh)
        gxc = np.clip(gx, 0, gw - 1); gyc = np.clip(gy, 0, gh - 1)
        hi = np.where(ok, HI[gyc, gxc], -1e30)
        lo = np.where(ok, LO[gyc, gxc], 1e30)
        hi = np.where(hi < -1e29, -1e30, hi)
        return lo, hi

    lo0, hi0 = span(x, y)
    under = hi0 > h + 128.0     # a building / roof / deck over the texel; a low cover (road) is ground
    walls = np.zeros_like(h, dtype=np.int32)
    tallnear = np.zeros_like(h, dtype=bool)
    for dx, dy in DIRS:
        steep = np.zeros_like(h, dtype=bool)
        for d in STEPS:
            lo, hi = span(x + dx * d, y + dy * d)
            dh = hi - h
            if d <= 648:
                steep |= (dh / d) >= 0.577
        walls += steep.astype(np.int32)
    # "open": nothing tall within the reach, on a 128-unit disc
    for oy in np.arange(-1458, 1459, 128.0):
        for ox in np.arange(-1458, 1459, 128.0):
            if ox * ox + oy * oy > 1458 * 1458:
                continue
            lo, hi = span(x + ox, y + oy)
            tallnear |= hi > h + 64
    canyon = ~under & (walls >= 4)
    opn = ~under & ~tallnear
    out = dict(stride=stride, samples=int(h.size))
    for nm, m in (('under', under), ('canyon', canyon), ('open', opn), ('other', ~under & ~canyon & ~opn)):
        out[nm] = dict(n=int(m.sum()), before=round(float(bB[m].mean()), 2) if m.any() else None,
                       after=round(float(aB[m].mean()), 2) if m.any() else None)
    d = np.abs(aB - bB)[opn]
    out['open_absdiff'] = dict(max=float(d.max()), le1_share=float((d <= 1).mean()), mean=float(d.mean()))
    out['open_corr_after_vs_before'] = float(np.corrcoef(aB[opn], bB[opn])[0, 1])
    # per named cell
    cx = np.floor(x / 4096).astype(int); cy = np.floor(y / 4096).astype(int)
    rows = []
    for key, ed in names.items():
        X, Y = map(int, key.split(','))
        m = (cx == X) & (cy == Y)
        c = m & canyon; o = m & opn
        if c.sum() >= 20:
            rows.append(dict(cell=key, edid=ed, canyon_n=int(c.sum()), canyon_before=round(float(bB[c].mean()), 1),
                             canyon_after=round(float(aB[c].mean()), 1),
                             open_n=int(o.sum()), open_after=round(float(aB[o].mean()), 1) if o.sum() else None))
    rows.sort(key=lambda r: r['canyon_after'] - r['canyon_before'])
    out['named'] = rows
    json.dump(out, open(sys.argv[5], 'w'), indent=1)
    for k in ('under', 'canyon', 'open', 'other', 'open_absdiff', 'open_corr_after_vs_before'):
        print(k, out[k])
    for r in rows[:12]:
        print(r)


if __name__ == '__main__':
    main()
