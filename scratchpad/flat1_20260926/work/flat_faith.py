"""FLAT1: an independent re-rasterisation of the flat ground objects over the Boston box (no NifSkope code).

Placements painted = the census rule's decisions (flat_rule.py -> out/flat_rule_py.pkl), keyed (REFR, SCOL part).
Per painted placement: the effective material swap (REFR XMSP, else the REFR base's MODS, else a SCOL part's own
MODS), each BSTriShape's BGSM (tex0, UV offset / scale, alpha test / blend, decal; a BGEM is not read here, its
shapes are marked and their texels left out of the sample), the diffuse sampled bilinear at the triangle's
footprint mip (UV scale in), x vertex colour where the shape has colours AND SLSF2 Vertex_Colors.

The order the game shows them in, restated: opaque shapes max-z; then decal / alpha-blended / alpha-tested shapes,
lowest mean z first, OVER what is there, dropped where 4 units under the opaque z; a fragment buried more than
8 units under LAND is not drawn.

Writes out/flat_raster.npz: rgb, a (final coverage), win (winning shape), clean (the last fragment was full
coverage), foot (every fragment of every painted shape, any coverage), mfoot (every fragment of every NON-road
STAT placement in the 2-cell margin, which the bake measures too), kind per shape, bgem per shape.

  python flat_faith.py                 -> rasterise
  python flat_faith.py cmp TAG [TAG]   -> per kind: mean |sheet - raster| over the sample, for each bake
"""
import collections
import math
import os
import pickle
import sys
import time

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import flatgeo as fg  # noqa: E402
import flat_rule as fr  # noqa: E402

rg = fg.rg
OUT = os.path.join(HERE, 'out')
UPT = 16.0
CX0, CY0, CX1, CY1 = -8, -12, 3, -1
S_W = (CX1 - CX0 + 1) * 256
S_H = (CY1 - CY0 + 1) * 256
WX0 = CX0 * 4096.0
WYTOP = (CY1 + 1) * 4096.0
BAKES = r'C:/Users/bungo/AppData/Local/Temp/claude/E--Projects-Claude/b560e4ec-6e66-4c21-9572-1ad4acca0043/scratchpad/flat1/bake/'
ROADFOOT = r'E:/Projects/NifskopeWWE-roads1/scratchpad/roads1_20260926/work/out/raster_INGAME_NEWRULE.npz'
LW = np.array([0.2126, 0.7152, 0.0722])
KINDS = ('pad', 'rail', 'path', 'decal', 'debris')


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


def tris_px(P):
    return (P[:, 0] - WX0) / UPT, (WYTOP - P[:, 1]) / UPT


def frags(xs, ys):
    """texel centres inside one triangle: (jj, ii, w0, w1, w2, d) or None"""
    i0 = max(int(math.floor(min(xs))), 0)
    i1 = min(int(math.ceil(max(xs))), S_W - 1)
    j0 = max(int(math.floor(min(ys))), 0)
    j1 = min(int(math.ceil(max(ys))), S_H - 1)
    if i1 < i0 or j1 < j0:
        return None
    d = (ys[1] - ys[2]) * (xs[0] - xs[2]) + (xs[2] - xs[1]) * (ys[0] - ys[2])
    if abs(d) < 1e-9:
        return None
    X, Y = np.meshgrid(np.arange(i0, i1 + 1) + 0.5, np.arange(j0, j1 + 1) + 0.5)
    w0 = ((ys[1] - ys[2]) * (X - xs[2]) + (xs[2] - xs[1]) * (Y - ys[2])) / d
    w1 = ((ys[2] - ys[0]) * (X - xs[2]) + (xs[0] - xs[2]) * (Y - ys[2])) / d
    w2 = 1.0 - w0 - w1
    ins = (w0 >= 0) & (w1 >= 0) & (w2 >= 0)
    if not ins.any():
        return None
    jj, ii = np.nonzero(ins)
    return jj + j0, ii + i0, w0[ins], w1[ins], w2[ins], d


def build():
    t0 = time.time()
    R = rg.Reader()
    T = fg.Terrain(R)
    dec = {(r['ref'], r['part']): r for r in pickle.load(open(os.path.join(OUT, 'flat_rule_py.pkl'), 'rb'))}
    swaps, mods_of = {}, {}

    def mods(form):
        if form not in mods_of:
            bi = R.base_info(form)
            mods_of[form] = bi['mods'] if bi else 0
        return mods_of[form]

    shapes = []
    mfoot = np.zeros((S_H, S_W), bool)
    npaint = nmargin = 0
    for d in rg.placements(R, CX0 - 2, CY0 - 2, CX1 + 2, CY1 + 2):
        bi = d['info']
        if not bi or bi['type'] != 'STAT' or not bi['modl'] or rg.is_road(bi['modl']):
            continue
        inbox = CX0 <= d['cx'] <= CX1 and CY0 <= d['cy'] <= CY1
        ws = None
        if not inbox:
            if d['refFlags'] & 0x820:
                continue
            ws = fg.world_shapes(R, d)
            for s, w in ws or []:
                px, py = tris_px(w)
                if px.max() < 0 or px.min() > S_W or py.max() < 0 or py.min() > S_H:
                    continue
                nmargin += 1
                for t in s['tris']:
                    f = frags(px[t], py[t])
                    if f:
                        mfoot[f[0], f[1]] = True
            continue
        r = dec.get((d['ref'], d['part']))
        if not r or r['dec'] != 'painted':
            continue
        npaint += 1
        eff = d['xmsp'] or mods(d['refBase']) or (mods(d['base']) if d['part'] >= 0 and d['base'] != d['refBase'] else 0)
        rows = None
        if eff:
            if eff not in swaps:
                swaps[eff] = swap_rows(R, eff)
            rows = swaps[eff]
        kind = fr.label(bi['modl'])
        for s, w in fg.world_shapes(R, d) or []:
            if not len(s['tris']):
                continue
            mat = s['mat']
            if rows and mat:
                rep = rows.get(fold(mat))
                if rep is not None and fold(rep) != fold(mat):
                    mat = 'Materials' + rg.BS + fold(rep)
            bgem = bool(mat) and mat.lower().endswith('.bgem')
            m = R.material(mat) if mat else None
            tex0 = m['tex0'] if (m and m['tex0']) else s['tex0']
            mtest = m['alphaTest'] if m else False
            atest = mtest or (s['hasAlpha'] and bool(s['alphaFlags'] & 0x200))
            ablend = (m['blend'] if m else False) or (s['hasAlpha'] and bool(s['alphaFlags'] & 1) and not atest)
            aref = (m['alphaRef'] / 255.0) if mtest else s['alphaThr'] / 255.0
            decal = bool(m and m['decal']) or bool(s.get('sf1', 0) & (1 << 26))
            land = T.at(w[:, 0], w[:, 1])
            rise = np.where(np.isnan(land), 0.0, w[:, 2] - land)
            shapes.append({'pos': w, 'uv': s['uv'], 'col': s['col'] if s['vc'] else None, 'va': s.get('va', False),
                           'tris': s['tris'], 'tex0': tex0, 'atest': atest, 'ablend': ablend, 'aref': aref,
                           'over': decal or atest or ablend, 'rise': rise, 'meanZ': float(w[:, 2].mean()),
                           'uvOff': m['uvOff'] if m else (0.0, 0.0), 'uvScale': m['uvScale'] if m else (1.0, 1.0),
                           'bgem': bgem, 'kind': kind, 'modl': bi['modl'], 'mat': mat})
    print('painted placements', npaint, 'shapes', len(shapes), 'margin shapes', nmargin, '%.0fs' % (time.time() - t0))

    zb = np.full((S_H, S_W), -1e30, np.float32)
    rgb = np.zeros((S_H, S_W, 3), np.float32)
    A = np.zeros((S_H, S_W), np.float32)
    win = np.full((S_H, S_W), -1, np.int32)
    clean = np.zeros((S_H, S_W), bool)
    foot = np.zeros((S_H, S_W), bool)
    order = [k for k, s in enumerate(shapes) if not s['over']]
    order += sorted([k for k, s in enumerate(shapes) if s['over']], key=lambda k: shapes[k]['meanZ'])
    notex = 0
    for k in order:
        sh = shapes[k]
        px, py = tris_px(sh['pos'])
        pz = sh['pos'][:, 2]
        tex = R.texture(sh['tex0']) if (sh['tex0'] and not sh['bgem']) else None
        us, vs = sh['uvScale']
        uo, vo = sh['uvOff']
        uv = sh['uv']
        col = sh['col']
        for t in sh['tris']:
            a, b, c = int(t[0]), int(t[1]), int(t[2])
            f = frags(px[[a, b, c]], py[[a, b, c]])
            if not f:
                continue
            jj, ii, w0, w1, w2, d = f
            foot[jj, ii] = True
            if sh['bgem']:
                # not read here: the texels it would win are marked unknown
                z = w0 * pz[a] + w1 * pz[b] + w2 * pz[c]
                win[jj, ii] = k
                clean[jj, ii] = False
                continue
            if tex is None:
                continue
            z = w0 * pz[a] + w1 * pz[b] + w2 * pz[c]
            keep = (z + 4.0 >= zb[jj, ii]) if sh['over'] else (z > zb[jj, ii])
            keep &= (w0 * sh['rise'][a] + w1 * sh['rise'][b] + w2 * sh['rise'][c]) >= -8.0
            if not keep.any():
                continue
            jj, ii, w0, w1, w2, z = jj[keep], ii[keep], w0[keep], w1[keep], w2[keep], z[keep]
            mip = 0.0
            if uv is not None and len(uv):
                uva = abs((uv[b, 0] - uv[a, 0]) * (uv[c, 1] - uv[a, 1]) - (uv[c, 0] - uv[a, 0]) * (uv[b, 1] - uv[a, 1])) \
                    * tex.w * tex.h * abs(us * vs)
                if uva > 0:
                    mip = min(max(0.5 * math.log2(uva / abs(d)), 0.0), float(tex.maxmip))
                u = (w0 * uv[a, 0] + w1 * uv[b, 0] + w2 * uv[c, 0]) * us + uo
                v = (w0 * uv[a, 1] + w1 * uv[b, 1] + w2 * uv[c, 1]) * vs + vo
            else:
                u = np.full(len(w0), uo)
                v = np.full(len(w0), vo)
            u = u - np.floor(u)
            v = v - np.floor(v)
            cs = sample_vec(tex, u, v, mip)
            cv = np.ones(len(u))
            if sh['atest']:
                cv = (cs[:, 3] >= sh['aref']).astype(float)
            elif sh['ablend']:
                cv = np.clip(cs[:, 3], 0, 1)
            if col is not None:
                vcol = w0[:, None] * col[a] + w1[:, None] * col[b] + w2[:, None] * col[c]
                cs[:, :3] *= vcol[:, :3]
                if sh['ablend'] and sh['va']:
                    cv *= np.clip(vcol[:, 3], 0, 1)
            ok = cv > 0
            if not ok.any():
                continue
            jj, ii, z, cv, src = jj[ok], ii[ok], z[ok], cv[ok], np.clip(cs[ok, :3], 0, 1) * 255.0
            if not sh['over']:
                zb[jj, ii] = z
                rgb[jj, ii] = src
                A[jj, ii] = 1.0
                clean[jj, ii] = True
            else:
                a0 = A[jj, ii]
                na = cv + a0 * (1 - cv)
                k0 = a0 * (1 - cv)
                rgb[jj, ii] = (src * cv[:, None] + rgb[jj, ii] * k0[:, None]) / np.maximum(na, 1e-9)[:, None]
                A[jj, ii] = na
                clean[jj, ii] = cv >= 1.0
            win[jj, ii] = k
        if tex is None and not sh['bgem']:
            notex += 1
    meta = np.array([(KINDS.index(s['kind']), s['bgem'], s['over']) for s in shapes], dtype=np.int64)
    names = np.array([s['modl'] + '|' + (s['mat'] or '') + '|' + (s['tex0'] or '') for s in shapes])
    np.savez_compressed(os.path.join(OUT, 'flat_raster.npz'), rgb=rgb, a=A, win=win, clean=clean, foot=foot,
                        mfoot=mfoot, meta=meta, names=names)
    print('texels won', int((win >= 0).sum()), 'footprint', int(foot.sum()), 'margin footprint', int(mfoot.sum()),
          'shapes without a readable diffuse', notex, '%.0fs' % (time.time() - t0))


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


def sheet(tag, level=2):
    sys.path.insert(0, os.path.join(HERE, '..', '..', '..', 'tests', 'spells'))
    sys.path.insert(0, r'E:/Projects/NifskopeWildWastelandEdition/scratchpad/seam1_20260925')
    import vtread
    v = vtread.Vt(BAKES + tag + '/mod/FO4CSLOD/Commonwealth/Commonwealth.VT.%d.lodt' % level)
    m, wW, nN = v.mosaic(CX0, CY0, CX1, CY1, 1)
    per = v.content // v.levelDim
    c0 = (CX0 - wW) * per
    r0 = (nN - (CY1 + 1)) * per
    return m[r0:r0 + (CY1 - CY0 + 1) * per, c0:c0 + (CX1 - CX0 + 1) * per, :3].astype(float)


def sample():
    """the texels the gate reads: a painted shape wins, full coverage, its 3x3 neighbourhood has the same winner,
    no road or pavement footprint there (the road z decides those), not a BGEM shape, 16 texels in from the box"""
    D = np.load(os.path.join(OUT, 'flat_raster.npz'))
    win, meta = D['win'], D['meta']
    ok = (win >= 0) & D['clean'] & (D['a'] >= 1.0)
    same = np.ones(win.shape, bool)
    for dy in (-1, 0, 1):
        for dx in (-1, 0, 1):
            same &= np.roll(np.roll(win, dy, 0), dx, 1) == win
    b = np.zeros(win.shape, bool)
    b[16:-16, 16:-16] = True
    road = np.load(ROADFOOT)['foot']
    kind = np.full(win.shape, -1)
    kind[win >= 0] = meta[win[win >= 0], 0]
    bgem = np.zeros(win.shape, bool)
    bgem[win >= 0] = meta[win[win >= 0], 1] == 1
    sel = ok & same & b & ~road & ~bgem & ~D['mfoot']
    return D, sel, kind


def cmp(tags):
    D, sel, kind = sample()
    ras = D['rgb']
    lr = ras @ LW
    print('sample: %d texels (%s)' % (sel.sum(), ', '.join('%s %d' % (k, (sel & (kind == i)).sum()) for i, k in enumerate(KINDS))))
    for tag in tags:
        s = sheet(tag)
        ls = s @ LW
        print(tag)
        for i, k in enumerate(KINDS):
            m = sel & (kind == i)
            if not m.any():
                print('   %-6s no texels' % k)
                continue
            dl = np.abs(ls - lr)[m]
            dc = np.abs(s - ras)[m].mean(0)
            print('   %-6s %7d texels  raster lum %5.1f  sheet lum %5.1f  mean|dlum| %5.2f  p90 %5.1f  R %5.2f G %5.2f B %5.2f' % (
                k, m.sum(), lr[m].mean(), ls[m].mean(), dl.mean(), np.percentile(dl, 90), *dc))


if __name__ == '__main__':
    if len(sys.argv) > 1 and sys.argv[1] == 'cmp':
        cmp(sys.argv[2:])
    else:
        build()
