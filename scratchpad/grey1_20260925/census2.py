"""GREY1 candidate 1, second cut (read only). The first cut (atlas_vs_full.py main) compared the LOD atlas with the
RAW diffuse of the full model -- wrong for the 44% of full-detail building area whose BGSM sets
bGrayscaleToPaletteColor: in game that surface's colour is the palette texture (BGSM texture slot 3) looked up at
U = diffuse green, V = the palette row. This cut applies it, with the row from the BGSM fGrayscaleToPaletteScale, or
from the Colour Remapping Index (MSWP CNAM) of the material swap the PLACEMENT (REFR XMSP) or its BASE (MODS)
carries; the swap's replacement material (SNAM) is used where it names one.
Unit = (base, swap) as the Commonwealth REFRs of Fallout4.esm place it, for every base whose .lodo slot-0 LOD mesh is a
building (LOD\\Architecture\\*, LOD\\Buildings\\*); weight = REFR count.
Three readings of the full model, because the engine's exact palette-row rule is not measured here:
  A  palette[g, row]            x vertex colour RGB   (row = scale or CNAM)
  B  palette[g, row x VC.r]     no further VC          (the NifSkope viewer's rule, res/shaders/fo4_default.frag)
  C  palette[g, row]            no vertex colour
usage: census2.py <out.tsv> [maxUnits]"""
import sys, os, collections, pickle
import numpy as np
HERE = os.path.dirname(os.path.abspath(__file__))
exec(open(os.path.join(HERE, 'atlas_vs_full.py')).read().split("if __name__")[0])
E = pickle.load(open(os.path.join(HERE, 'esm_swaps.pkl'), 'rb'))


def mkey(p):
    return norm(p, 'materials')


def swap_table(fid):
    if not fid or fid not in E['mswp']:
        return {}
    return {mkey(o): (rp, c) for o, rp, c in E['mswp'][fid]['subs']}


def measure_full(data, swap, K=6):
    N = nifwind.Nif(data)
    acc = {m: dict(area=0.0, lin=np.zeros(3), S=0.0) for m in 'ABC'}
    info = collections.Counter()
    for k, (t, o, sz) in enumerate(N.blocks):
        if t not in nifwind.SHAPES:
            continue
        got = shape_uv_tris(N, k)
        if got is None:
            continue
        sh, uv, tri = got
        if not (0 <= sh['shader'] < len(N.blocks)):
            continue
        f1, f2, mat = N.shader_flags(sh['shader'])
        if not mat:
            continue
        row = None
        sw = swap.get(mkey(mat))
        if sw is not None:
            if sw[0]:
                mat = sw[0]
            row = sw[1]
            info['swappedShapes'] += 1
        M = bgsm(mat)
        P = sh['pos'][tri]
        area = 0.5 * np.linalg.norm(np.cross(P[:, 1] - P[:, 0], P[:, 2] - P[:, 0]), axis=1)
        area = np.nan_to_num(area)
        A = float(area.sum())
        if A <= 0 or M is None or M.get('bgem') or not M['tex'] or not M['tex'][0]:
            info['skippedArea'] += A
            continue
        img, why = gettex(M['tex'][0])
        if img is None:
            info['noTexArea'] += A
            continue
        pal = None
        if M['g2p']:
            pal, pw = gettex(M['tex'][3]) if len(M['tex']) > 3 and M['tex'][3] else (None, 'nopal')
            if pal is None:
                info['g2pNoPaletteArea'] += A
            else:
                info['g2pArea'] += A
        if row is None:
            row = M.get('g2pScale', 1.0)
        if sw is not None and sw[1] is not None:
            info['cnamArea'] += A
        n = len(tri)
        u = rng.random((n, K))
        v = rng.random((n, K))
        flip = u + v > 1
        u[flip] = 1 - u[flip]
        v[flip] = 1 - v[flip]
        w0 = 1 - u - v
        UV = np.nan_to_num(uv[tri])
        su = w0 * UV[:, 0, 0, None] + u * UV[:, 1, 0, None] + v * UV[:, 2, 0, None]
        sv = w0 * UV[:, 0, 1, None] + u * UV[:, 1, 1, None] + v * UV[:, 2, 1, None]
        H, W = img.shape[:2]
        x = np.floor(np.mod(su, 1.0) * W).astype(int) % W
        y = np.floor(np.mod(sv, 1.0) * H).astype(int) % H
        c = img[y, x]
        wgt = np.repeat(area[:, None] / K, K, axis=1)
        if M['alphaTest']:
            wgt = wgt * (c[..., 3] * 255 >= max(M['ref'], 1))
        vc = sh['cols'] is not None and bool((f2 or 0) & 0x20)
        if vc:
            C = sh['cols'][:, :3].astype(np.float32) / 255.0
            CV = C[tri]
            vcol = w0[..., None] * CV[:, 0, None, :] + u[..., None] * CV[:, 1, None, :] + v[..., None] * CV[:, 2, None, :]
        else:
            vcol = np.ones(c.shape[:2] + (3,), np.float32)
        if pal is not None:
            PH, PW = pal.shape[:2]
            px = np.clip(np.floor(c[..., 1] * PW).astype(int), 0, PW - 1)
            pyA = np.clip(np.floor(row * PH).astype(int), 0, PH - 1) * np.ones_like(px)
            pyB = np.clip(np.floor(row * vcol[..., 0] * PH).astype(int), 0, PH - 1)
            baseA = pal[pyA, px, :3]
            baseB = pal[pyB, px, :3]
            rgbs = dict(A=baseA * vcol, B=baseB, C=baseA)
        else:
            rgbs = dict(A=c[..., :3] * vcol, B=c[..., :3] * vcol, C=c[..., :3])
        for m, rgb in rgbs.items():
            lin = s2l(rgb)
            mx = rgb.max(-1)
            mn = rgb.min(-1)
            S = np.where(mx > 1e-4, (mx - mn) / np.maximum(mx, 1e-4), 0)
            acc[m]['area'] += float(wgt.sum())
            acc[m]['lin'] += (lin * wgt[..., None]).reshape(-1, 3).sum(0)
            acc[m]['S'] += float((S * wgt).sum())
        info['area'] += A
    return acc, info


if __name__ == '__main__':
    place_lodi = collections.Counter(r['baseId'] for r in Ti['instances'])
    lodbase = {}
    for bi, B in enumerate(L['bases']):
        ms = [B['rep%d' % k] for k in range(4) if B['rep%d' % k] != 0xFFFF]
        if ms and is_building(names[ms[0]]) and (B['formId'] >> 24) == 0:
            lodbase[B['formId']] = ms[0]
    units = collections.Counter()
    for (nm, xm), c in E['refs'].items():
        if nm in lodbase:
            sw = xm or E['base'].get(nm, {}).get('mods', 0)
            units[(nm, sw, 'refr' if xm else ('base' if sw else 'none'))] += c
    tot = collections.Counter()
    for (nm, sw, kind), c in units.items():
        tot['refs'] += c
        tot['refs_swap_' + kind] += c
    print('Commonwealth REFRs of building-LOD bases: %s' % dict(tot))
    order = sorted(units.items(), key=lambda kv: -kv[1])
    maxU = int(sys.argv[2]) if len(sys.argv) > 2 else 10 ** 9
    out = open(sys.argv[1], 'w')
    out.write('refs\tbase\tswap\tswapKind\tlod\tfull\tlodY\tlodS\tlod_meanS\tA_Y\tA_S\tA_meanS\tB_Y\tB_S\tB_meanS\tC_Y\tC_S\tC_meanS\t'
              'lod_srgb\tA_srgb\tg2pShare\tcnamShare\n')
    lodcache = {}
    Wt = collections.defaultdict(float)
    done = collections.Counter()
    for (nm, sw, kind), c in order[:maxU]:
        m = lodbase[nm]
        full = E['base'][nm]['modl'] if nm in E['base'] else None
        if not full:
            done['noModl'] += c
            continue
        if m not in lodcache:
            d, _ = getfile(names[m], 'meshes')
            lodcache[m] = summ(measure(d)) if d is not None else None
        ls = lodcache[m]
        fd, _ = getfile(full, 'meshes')
        if fd is None or ls is None:
            done['missing'] += c
            continue
        acc, info = measure_full(fd, swap_table(sw))
        ss = {k: summ(a) for k, a in acc.items()}
        if any(v is None for v in ss.values()):
            done['unmeasured'] += c
            continue
        done['units'] += 1
        done['refs'] += c
        Wt['w'] += c
        Wt['lodY'] += c * ls['Y']
        Wt['lodS'] += c * ls['S_of_mean']
        Wt['lodmS'] += c * ls['mean_S']
        for k in 'ABC':
            Wt[k + 'Y'] += c * ss[k]['Y']
            Wt[k + 'S'] += c * ss[k]['S_of_mean']
            Wt[k + 'mS'] += c * ss[k]['mean_S']
        g2 = info['g2pArea'] / max(info['area'], 1e-9)
        Wt['g2p'] += c * g2
        fmt = lambda a: '%.3f,%.3f,%.3f' % tuple(a)
        out.write('%d\t%08X\t%08X\t%s\t%s\t%s\t%.4f\t%.3f\t%.3f\t%.4f\t%.3f\t%.3f\t%.4f\t%.3f\t%.3f\t%.4f\t%.3f\t%.3f\t%s\t%s\t%.2f\t%.2f\n' % (
            c, nm, sw, kind, names[m], full, ls['Y'], ls['S_of_mean'], ls['mean_S'],
            ss['A']['Y'], ss['A']['S_of_mean'], ss['A']['mean_S'], ss['B']['Y'], ss['B']['S_of_mean'], ss['B']['mean_S'],
            ss['C']['Y'], ss['C']['S_of_mean'], ss['C']['mean_S'], fmt(ls['srgb']), fmt(ss['A']['srgb']), g2,
            info['cnamArea'] / max(info['area'], 1e-9)))
        out.flush()
    print('measured: %s' % dict(done))
    w = Wt['w']
    print('REFR-weighted over %d placements (g2p share of full-detail area %.2f):' % (w, Wt['g2p'] / w))
    print('  linear luminance Y       : LOD %.4f | full A %.4f  B %.4f  C %.4f' % (Wt['lodY'] / w, Wt['AY'] / w, Wt['BY'] / w, Wt['CY'] / w))
    print('  saturation of mean colour: LOD %.3f  | full A %.3f   B %.3f   C %.3f' % (Wt['lodS'] / w, Wt['AS'] / w, Wt['BS'] / w, Wt['CS'] / w))
    print('  mean per-texel saturation: LOD %.3f  | full A %.3f   B %.3f   C %.3f' % (Wt['lodmS'] / w, Wt['AmS'] / w, Wt['BmS'] / w, Wt['CmS'] / w))
