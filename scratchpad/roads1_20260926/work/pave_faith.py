"""ROADS1: an independent re-rasterisation of the road + pavement stamp over the Boston bake box, two ways:

  INGAME  the game's surface: the placement's effective material swap applied (REFR XMSP, else the base's
          MODS, else a SCOL part's own MODS), the swapped material's diffuse
  OLD     the stamp as coded at 6382a09a: no material swap

Both: max-z winner per texel (texel centres, row 0 north, 16 world units a texel = VT.2), the diffuse sampled
bilinear at the triangle's footprint mip (0.5*log2(uv area * texels / sheet texels)), x vertex colour where the
shape carries colours and SLSF2 Vertex_Colors, coverage from the alpha test / blend, ground-material shapes
dropped (roadGroundPaint 0), raised bases (MNAM LOD or the overpass/bridge folders) refused, disabled refs skipped.

Writes out/raster_<tag>.npz: rgb (float 0..255), cov, winner shape id, is-sidewalk, is-swapped, and the
footprint mask (any road/pavement fragment, all coverage, pavements included) for the confinement gate.

  python pave_faith.py            -> rasterises and saves (the 6382a09a placement rule)
  RULE=new python pave_faith.py   -> the same with ROADS1's placement rule (has-LOD ground pieces stamped),
                                     saved as raster_<tag>_NEWRULE.npz
"""
import math
import os
import pickle
import sys
import time

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import roadgeo as rg  # noqa: E402

OUT = os.path.join(HERE, 'out')
UPT = 16.0
# the bake region's cells (bake_region.sh --terrain-region -8 -12 3 -1)
CX0, CY0, CX1, CY1 = -8, -12, 3, -1
S_W = (CX1 - CX0 + 1) * 256
S_H = (CY1 - CY0 + 1) * 256
WX0 = CX0 * 4096.0
WYTOP = (CY1 + 1) * 4096.0


def fold(m):
    p = m.lower().replace('/', rg.BS)
    i = p.rfind('materials' + rg.BS)
    if i >= 0:
        p = p[i + 10:]
    return p


def swap_rows(R, w):
    b = R.W.base(w)
    if not b or b[0] != 'MSWP':
        return None
    rows = {}
    cur = None
    for ft, p in b[2]:
        if ft == b'BNAM':
            cur = p.split(b'\0')[0].decode('latin-1')
        elif ft == b'SNAM' and cur is not None:
            k = fold(cur)
            rep = p.split(b'\0')[0].decode('latin-1')
            if k and rep and k not in rows:
                rows[k] = rep
            cur = None
    return rows


def main():
    t0 = time.time()
    R = rg.Reader()
    pl = pickle.load(open(os.path.join(OUT, 'road_placements.pkl'), 'rb'))
    swaps = {}
    mods_of = {}

    def mods(form):
        if form not in mods_of:
            bi = R.base_info(form)
            mods_of[form] = bi['mods'] if bi else 0
        return mods_of[form]

    # ---------------------------------------------------------------- shapes, both variants
    shapes = []      # dict per (placement, shape, variant)
    nsw = {'placements': 0, 'swapped': 0}
    # RULE=new (ROADS1's rule): the has-LOD bases outside the overpass / bridge folders are stamped too
    new_rule = os.environ.get('RULE') == 'new'
    for d in pl:
        if d['decision'] != 'stamped' and not (new_rule and d['decision'] == 'refused raised-haslod'
                                               and not rg.is_raised_folder(d['modl'])):
            continue
        eff = d['xmsp'] or mods(d['refBase']) or (mods(d['base']) if d['part'] >= 0 and d['base'] != d['refBase'] else 0)
        rows = None
        if eff:
            if eff not in swaps:
                swaps[eff] = swap_rows(R, eff)
            rows = swaps[eff]
        model = R.model(d['modl'])
        if not model:
            continue
        nsw['placements'] += 1
        sw = rg.is_sidewalk(d['modl'])
        rot = np.asarray(d['rot'])
        pos = np.asarray(d['pos'])
        for s in model:
            if s['effect']:
                pass
            wp = pos + (s['pos'] * d['scale']) @ rot.T
            for variant in ('INGAME', 'OLD'):
                mat = s['mat']
                swapped = False
                if variant == 'INGAME' and rows and mat:
                    rep = rows.get(fold(mat))
                    if rep is not None and fold(rep) != fold(mat):
                        mat = 'Materials' + rg.BS + fold(rep)
                        swapped = True
                m = R.material(mat) if mat else None
                tex0 = m['tex0'] if (m and m['tex0']) else s['tex0']
                ground = 'materials/landscape/ground/' in rg.mat_path(mat).lower().replace(rg.BS, '/') if mat else False
                mtest = m['alphaTest'] if m else False
                atest = mtest or (s['hasAlpha'] and bool(s['alphaFlags'] & 0x200))
                ablend = (m['blend'] if m else False) or (s['hasAlpha'] and bool(s['alphaFlags'] & 1) and not atest)
                aref = (m['alphaRef'] / 255.0) if mtest else s['alphaThr'] / 255.0
                shapes.append({'variant': variant, 'pos': wp, 'uv': s['uv'], 'col': s['col'], 'vc': s['vc'],
                               'tris': s['tris'], 'tex0': tex0, 'ground': ground, 'atest': atest, 'ablend': ablend,
                               'aref': aref, 'sidewalk': sw, 'swapped': swapped, 'ref': d['ref'], 'part': d['part'],
                               'modl': d['modl'], 'mat': mat})
                if variant == 'INGAME' and swapped:
                    nsw['swapped'] += 1
    print('shapes', len(shapes), nsw, 'swap forms', len(swaps), '%.0fs' % (time.time() - t0))

    for variant in ('INGAME', 'OLD'):
        raster(R, [s for s in shapes if s['variant'] == variant], variant + ('_NEWRULE' if new_rule else ''))
    print('done %.0fs' % (time.time() - t0))


def raster(R, shapes, tag):
    t0 = time.time()
    zb = np.full((S_H, S_W), -1e30, np.float32)
    rgb = np.zeros((S_H, S_W, 3), np.float32)
    cov = np.zeros((S_H, S_W), np.float32)
    win = np.full((S_H, S_W), -1, np.int32)
    foot = np.zeros((S_H, S_W), bool)
    ntri = 0
    for si, sh in enumerate(shapes):
        if sh['ground']:
            # dropped: roadGroundPaint 0 multiplies coverage by 0 -- no paint, no z; it is still a footprint
            pass
        tex = R.texture(sh['tex0']) if sh['tex0'] else None
        if tex is None:
            continue
        P = sh['pos']
        px = (P[:, 0] - WX0) / UPT
        py = (WYTOP - P[:, 1]) / UPT
        pz = P[:, 2]
        uv = sh['uv']
        col = sh['col'] if (sh['col'] is not None) else None
        for t in sh['tris']:
            a, b, c = int(t[0]), int(t[1]), int(t[2])
            xs = (px[a], px[b], px[c])
            ys = (py[a], py[b], py[c])
            i0 = max(int(math.floor(min(xs))), 0)
            i1 = min(int(math.ceil(max(xs))), S_W - 1)
            j0 = max(int(math.floor(min(ys))), 0)
            j1 = min(int(math.ceil(max(ys))), S_H - 1)
            if i1 < i0 or j1 < j0:
                continue
            d = (ys[1] - ys[2]) * (xs[0] - xs[2]) + (xs[2] - xs[1]) * (ys[0] - ys[2])
            if abs(d) < 1e-9:
                continue
            ntri += 1
            X, Y = np.meshgrid(np.arange(i0, i1 + 1) + 0.5, np.arange(j0, j1 + 1) + 0.5)
            w0 = ((ys[1] - ys[2]) * (X - xs[2]) + (xs[2] - xs[1]) * (Y - ys[2])) / d
            w1 = ((ys[2] - ys[0]) * (X - xs[2]) + (xs[0] - xs[2]) * (Y - ys[2])) / d
            w2 = 1.0 - w0 - w1
            ins = (w0 >= 0) & (w1 >= 0) & (w2 >= 0)
            if not ins.any():
                continue
            jj, ii = np.nonzero(ins)
            jj = jj + j0
            ii = ii + i0
            w0 = w0[ins]; w1 = w1[ins]; w2 = w2[ins]
            foot[jj, ii] = True
            if sh['ground']:
                continue
            z = w0 * pz[a] + w1 * pz[b] + w2 * pz[c]
            front = z > zb[jj, ii]
            if not front.any():
                continue
            jj, ii, w0, w1, w2, z = jj[front], ii[front], w0[front], w1[front], w2[front], z[front]
            uva = abs((uv[b, 0] - uv[a, 0]) * (uv[c, 1] - uv[a, 1]) - (uv[c, 0] - uv[a, 0]) * (uv[b, 1] - uv[a, 1])) * tex.w * tex.h
            mip = 0.0
            if uva > 0:
                mip = min(max(0.5 * math.log2(uva / abs(d)), 0.0), float(tex.maxmip))
            u = w0 * uv[a, 0] + w1 * uv[b, 0] + w2 * uv[c, 0]
            v = w0 * uv[a, 1] + w1 * uv[b, 1] + w2 * uv[c, 1]
            u = u - np.floor(u)
            v = v - np.floor(v)
            cs = sample_vec(tex, u, v, mip)
            cv = np.ones(len(u))
            if sh['atest']:
                cv = (cs[:, 3] >= sh['aref']).astype(float)
            elif sh['ablend']:
                cv = np.clip(cs[:, 3], 0, 1)
            if col is not None and (sh['vc'] or sh['variant'] == 'OLD'):
                vcol = w0[:, None] * col[a] + w1[:, None] * col[b] + w2[:, None] * col[c]
                cs[:, :3] *= vcol[:, :3]
                if sh['ablend']:
                    cv *= np.clip(vcol[:, 3], 0, 1)
            ok = cv > 0
            if not ok.any():
                continue
            jj, ii, z = jj[ok], ii[ok], z[ok]
            zb[jj, ii] = z
            rgb[jj, ii] = cs[ok, :3] * 255.0
            cov[jj, ii] = cv[ok]
            win[jj, ii] = si
    meta = np.array([(s['sidewalk'], s['swapped'], s['ref'], s['part']) for s in shapes], dtype=np.int64)
    names = np.array([s['modl'] + '|' + s['mat'] + '|' + s['tex0'] for s in shapes])
    np.savez_compressed(os.path.join(OUT, 'raster_%s.npz' % tag), rgb=rgb, cov=cov, win=win, foot=foot, meta=meta,
                        names=names)
    print(tag, 'triangles', ntri, 'texels painted', int((win >= 0).sum()), 'footprint', int(foot.sum()),
          '%.0fs' % (time.time() - t0))


def sample_vec(tex, u, v, mip):
    m0 = int(math.floor(mip))
    t = mip - m0
    c = bil_vec(tex.mips[m0], u, v)
    if t > 0 and m0 < tex.maxmip:
        c = c * (1 - t) + bil_vec(tex.mips[m0 + 1], u, v) * t
    return c


def bil_vec(a, u, v):
    h, w = a.shape[:2]
    x = u * w - 0.5
    y = v * h - 0.5
    x0 = np.floor(x)
    y0 = np.floor(y)
    fx = (x - x0)[:, None]
    fy = (y - y0)[:, None]
    x0 = x0.astype(np.int64) % w
    y0 = y0.astype(np.int64) % h
    x1 = (x0 + 1) % w
    y1 = (y0 + 1) % h
    return (a[y0, x0] * (1 - fx) * (1 - fy) + a[y0, x1] * fx * (1 - fy)
            + a[y1, x0] * (1 - fx) * fy + a[y1, x1] * fx * fy)


if __name__ == '__main__':
    main()
