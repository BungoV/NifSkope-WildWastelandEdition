"""Where in vanilla's Commonwealth.Objects.DDS the tower triangles sample: atlas at 1/2 with the used texels kept and
the rest dimmed; plus the three HitTech LOD diffuse textures ours samples, at the same texel scale. Read only."""
import sys, pickle, os
import numpy as np
from PIL import Image
sys.path.insert(0, r'E:/Projects/NifskopeWWE-grey1/scratchpad/grey1_20260925')
import atlas_vs_full as A
import nifwind
VO = r'E:/Tools/Fallout 4/DataUnpacked/Data/Meshes/Terrain/Commonwealth/Objects'
atl = Image.open(r'E:/Tools/Fallout 4/DataUnpacked/Data/Textures/Terrain/Commonwealth/Objects/Commonwealth.Objects.DDS').convert('RGB')
W, H = atl.size
used = np.zeros((H, W), bool)
CL = pickle.load(open('clusters.pkl', 'rb'))[:3]
rng = np.random.default_rng(3)
for v in CL[:2]:
    x0 = min(r[2] for r in v) - 600; x1 = max(r[2] for r in v) + 600; y0 = min(r[3] for r in v) - 600; y1 = max(r[3] for r in v) + 600; zmin = min(r[1] for r in v)
    for cx in (-4, 0):
        f = '%s/Commonwealth.4.%d.-8.BTO' % (VO, cx); N = nifwind.Nif(open(f, 'rb').read())
        for k, (t, o, sz) in enumerate(N.blocks):
            if t not in nifwind.SHAPES: continue
            sh, uv, tri = A.shape_uv_tris(N, k)
            P = sh['pos'] * 4 + np.array([cx * 4096, -8 * 4096, 0], np.float32); c = P[tri].mean(1)
            sel = (c[:, 0] > x0) & (c[:, 0] < x1) & (c[:, 1] > y0) & (c[:, 1] < y1) & (c[:, 2] > zmin)
            UV = uv[tri[sel]]
            for _ in range(200):
                a = rng.random(len(UV)); b = rng.random(len(UV)); fl = a + b > 1; a[fl] = 1 - a[fl]; b[fl] = 1 - b[fl]
                u = (1 - a - b) * UV[:, 0, 0] + a * UV[:, 1, 0] + b * UV[:, 2, 0]; w = (1 - a - b) * UV[:, 0, 1] + a * UV[:, 1, 1] + b * UV[:, 2, 1]
                used[(np.mod(w, 1) * H).astype(int) % H, (np.mod(u, 1) * W).astype(int) % W] = True
from scipy import ndimage
used = ndimage.binary_dilation(used, iterations=2)
a = np.asarray(atl, np.float32)
a[~used] *= 0.25
out = Image.fromarray(a.astype(np.uint8)).resize((W // 2, H // 2), Image.LANCZOS)
out.save('pics/atlas_used_by_towers.png')
ys, xs = np.nonzero(used); print('used texels', used.sum(), 'u %.3f..%.3f v %.3f..%.3f' % (xs.min() / W, xs.max() / W, ys.min() / H, ys.max() / H))
b = np.asarray(atl, np.float32)[used] / 255; print('mean of used atlas texels sRGB', b.mean(0).round(3))
