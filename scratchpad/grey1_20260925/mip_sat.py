"""GREY1 candidate 4d: does mip averaging grey the LOD atlases? Per LOD atlas diffuse (the building LOD meshes of
c2_all.tsv, REFR-weighted), mean per-texel HSV saturation at mip 0 and after linear-light box filters of 4x4, 16x16
and 64x64 texels (mips 2, 4, 6). Averaging can only lower per-texel S toward the S of the mean colour; the game
samples the same mips at the same distance, so this is common to both sides unless the viewer picks a coarser mip."""
import os, collections
import numpy as np
HERE = os.path.dirname(os.path.abspath(__file__))
exec(open(os.path.join(HERE, 'atlas_vs_full.py')).read().split("if __name__")[0])
w = collections.Counter()
for ln in open(os.path.join(HERE, 'c2_all.tsv')).read().splitlines()[1:]:
    f = ln.split('\t')
    w[f[4]] += int(f[0])
texw = collections.Counter()
for lod, c in w.items():
    d, _ = getfile(lod, 'meshes')
    if d is None:
        continue
    N = nifwind.Nif(d)
    for k, (t, o, sz) in enumerate(N.blocks):
        if t not in nifwind.SHAPES:
            continue
        got = shape_uv_tris(N, k)
        if got is None or not (0 <= got[0]['shader'] < len(N.blocks)):
            continue
        M = bgsm(N.shader_flags(got[0]['shader'])[2] or '')
        if M and M['tex'] and M['tex'][0]:
            texw[M['tex'][0].lower()] += c


def S(a):
    mx = a.max(-1)
    return np.where(mx > 1e-3, (mx - a.min(-1)) / np.maximum(mx, 1e-3), 0)


acc = collections.defaultdict(float)
for t, c in texw.most_common():
    img, why = gettex(t)
    if img is None:
        print('unresolved', t, why)
        continue
    lin = s2l(img[..., :3])
    row = []
    for b in (1, 4, 16, 64):
        H, W = (lin.shape[0] // b) * b, (lin.shape[1] // b) * b
        if H == 0 or W == 0:
            break
        m = lin[:H, :W].reshape(H // b, b, W // b, b, 3).mean((1, 3))
        s = float(S(l2s(m)).mean())
        acc[b] += c * s
        row.append('%.3f' % s)
    acc['w'] += c
    print('%6d  %-60s mean S at mip0/2/4/6: %s' % (c, t[-60:], ' '.join(row)))
print('REFR-weighted mean per-texel S at mip 0/2/4/6: ' + ' '.join('%.3f' % (acc[b] / acc['w']) for b in (1, 4, 16, 64)))
