"""TOWER1 step 3 (offline half): per tower, the area-weighted mean colour of the surfaces that draw it --
vanilla = the stock dim-4 .bto triangles inside the tower's box (chunk-local = (world - chunk origin) / 4),
sampled from Commonwealth.Objects.DDS (full res, loose unpacked vanilla path) at their UVs;
ours = every placement of the tower (installed library, slot 0), its LOD NIF sampled from its own BGSM diffuse
(GREY1 atlas_vs_full.measure, MO2 stack), weighted by area x scale^2. Read only."""
import sys, pickle, collections, os
import numpy as np
from PIL import Image
sys.path.insert(0, r'E:/Projects/NifskopeWWE-grey1/scratchpad/grey1_20260925')
import atlas_vs_full as A
import nifwind
VO = r'E:/Tools/Fallout 4/DataUnpacked/Data/Meshes/Terrain/Commonwealth/Objects'
ATL = np.asarray(Image.open(r'E:/Tools/Fallout 4/DataUnpacked/Data/Textures/Terrain/Commonwealth/Objects/Commonwealth.Objects.DDS').convert('RGBA'), np.float32) / 255
CL = pickle.load(open('clusters.pkl', 'rb'))[:3]
rng = np.random.default_rng(7)

def stats(rgb, w):
    lin = A.s2l(rgb); W = w.sum()
    m = (lin * w[:, None]).sum(0) / W; s = A.l2s(m)
    mx = rgb.max(-1); mn = rgb.min(-1); S = np.where(mx > 1e-4, (mx - mn) / np.maximum(mx, 1e-4), 0)
    return dict(srgb=s, S_of_mean=float((s.max() - s.min()) / max(s.max(), 1e-6)), mean_S=float((S * w).sum() / W),
                Y=float(0.2126 * m[0] + 0.7152 * m[1] + 0.0722 * m[2]), area=float(W))

def van(v, K=8):
    x0 = min(r[2] for r in v) - 600; x1 = max(r[2] for r in v) + 600
    y0 = min(r[3] for r in v) - 600; y1 = max(r[3] for r in v) + 600
    zmin = min(r[1] for r in v)
    RGB, WG = [], []
    for cx in range(int(np.floor(x0 / 16384)) * 4, int(np.floor(x1 / 16384)) * 4 + 1, 4):
        for cy in range(int(np.floor(y0 / 16384)) * 4, int(np.floor(y1 / 16384)) * 4 + 1, 4):
            f = '%s/Commonwealth.4.%d.%d.BTO' % (VO, cx, cy)
            if not os.path.isfile(f): continue
            N = nifwind.Nif(open(f, 'rb').read())
            for k, (t, o, sz) in enumerate(N.blocks):
                if t not in nifwind.SHAPES: continue
                sh, uv, tri = A.shape_uv_tris(N, k)
                P = sh['pos'] * 4 + np.array([cx * 4096, cy * 4096, 0], np.float32)
                c = P[tri].mean(1)
                sel = (c[:, 0] > x0) & (c[:, 0] < x1) & (c[:, 1] > y0) & (c[:, 1] < y1) & (c[:, 2] > zmin)
                if not sel.any(): continue
                tr = tri[sel]; Pt = P[tr]
                area = 0.5 * np.linalg.norm(np.cross(Pt[:, 1] - Pt[:, 0], Pt[:, 2] - Pt[:, 0]), axis=1)
                n = len(tr); u = rng.random((n, K)); w_ = rng.random((n, K)); fl = u + w_ > 1; u[fl] = 1 - u[fl]; w_[fl] = 1 - w_[fl]
                UV = uv[tr]; w0 = 1 - u - w_
                su = w0 * UV[:, 0, 0, None] + u * UV[:, 1, 0, None] + w_ * UV[:, 2, 0, None]
                sv = w0 * UV[:, 0, 1, None] + u * UV[:, 1, 1, None] + w_ * UV[:, 2, 1, None]
                H, W = ATL.shape[:2]
                xs = np.floor(np.mod(su, 1) * W).astype(int) % W; ys = np.floor(np.mod(sv, 1) * H).astype(int) % H
                cc = ATL[ys, xs]
                wg = np.repeat(area[:, None] / K, K, 1)
                if sh['name'] == 'obj-at': wg = wg * (cc[..., 3] >= 0.5)
                RGB.append(cc[..., :3].reshape(-1, 3)); WG.append(wg.reshape(-1))
                print('   vanilla %s shape %d %s: %d tris in box, z %.0f..%.0f, uv box %.3f..%.3f x %.3f..%.3f' % (os.path.basename(f), k, sh['name'], n, Pt[..., 2].min(), Pt[..., 2].max(), su.min(), su.max(), sv.min(), sv.max()))
    return stats(np.concatenate(RGB), np.concatenate(WG))

def ours(v):
    RGB, WG = [], []
    cache = {}
    for r in v:
        if not r[8]: continue
        m = r[8][0]
        if m not in cache:
            data, src = A.getfile(m, 'meshes'); keep = []
            A.measure(data, K=8, keep=keep); cache[m] = keep
        for rgb, w in cache[m]:
            RGB.append(rgb); WG.append(w * r[6] ** 2)
    return stats(np.concatenate(RGB), np.concatenate(WG))

res = {}
for tag, v in zip('ABC', CL):
    print('tower', tag, 'x %.0f..%.0f y %.0f..%.0f' % (min(r[2] for r in v), max(r[2] for r in v), min(r[3] for r in v), max(r[3] for r in v)))
    a = van(v); b = ours(v); res[tag] = (a, b)
    for nm, s in (('vanilla', a), ('ours   ', b)):
        print('  %s sRGB %s  S_of_mean %.3f  mean_S %.3f  Y %.4f  area %.3g' % (nm, np.round(s['srgb'], 3), s['S_of_mean'], s['mean_S'], s['Y'], s['area']))
pickle.dump(res, open('van_tower.pkl', 'wb'))
