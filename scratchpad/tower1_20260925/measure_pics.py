"""TOWER1 step 2+3 (rendered half). Composite the 18 stock vanilla chunk renders (9 BTR then 9 BTO, far to near
along the 08 view axis; a chunk's pixel counts where it differs from the background) into one 08-camera picture
per arm, then measure each tower through ONE mask on every picture: the vanilla tower triangles (world = local*4 +
chunk origin, z > the tower's lowest placement) projected with the 08 camera and eroded 2 px.
usage: python measure_pics.py   (reads pics/*.png, .cam; writes pics/van_lit.png, van_base.png, mask_*.png)"""
import sys, os, glob, pickle
import numpy as np
from PIL import Image, ImageDraw, ImageFilter
sys.path.insert(0, r'E:/Projects/NifskopeWWE-grey1/scratchpad/grey1_20260925')
import atlas_vs_full as A
import nifwind
P = 'pics/'
VO = r'E:/Tools/Fallout 4/DataUnpacked/Data/Meshes/Terrain/Commonwealth/Objects'
L = np.array([-2048., -26624., 0.])
VX = np.array([-0.6859, -0.7277, 0.0]); VY = np.array([0.3240, -0.3055, 0.8952]); VZ = np.array([-0.6516, 0.6142, 0.4453])
CHUNKS = [(x, y) for y in (-12, -8, -4) for x in (-8, -4, 0)]

def load(tag):
    return np.asarray(Image.open(P + tag + '.png').convert('RGB'), np.float32) / 255

def composite(arm):
    order = sorted(CHUNKS, key=lambda c: np.dot(np.array([c[0] * 4096 + 8192, c[1] * 4096 + 8192, 0]) - L, VZ))
    out = None; bg = None; n = {}
    for kind in ('BTR', 'BTO'):
        for X, Y in order:
            im = load('van_%s_%s_%d_%d' % (arm, kind, X, Y))
            if out is None:
                out = im.copy(); bg = im[2, 2].copy(); out[:] = bg
            assert im.shape == out.shape, (im.shape, out.shape)
            m = np.abs(im - bg).max(-1) > 1.5 / 255
            out[m] = im[m]; n['%s %d,%d' % (kind, X, Y)] = int(m.sum())
    Image.fromarray((out * 255 + .5).astype(np.uint8)).save(P + 'van_%s.png' % arm)
    return out, bg, n

def masks(W, H, upp):
    CL = pickle.load(open('clusters.pkl', 'rb'))[:3]
    res = {}
    for tag, v in zip('ABC', CL):
        x0 = min(r[2] for r in v) - 600; x1 = max(r[2] for r in v) + 600
        y0 = min(r[3] for r in v) - 600; y1 = max(r[3] for r in v) + 600; zmin = min(r[1] for r in v)
        img = Image.new('L', (W, H), 0); d = ImageDraw.Draw(img); nt = 0
        for cx, cy in CHUNKS:
            f = '%s/Commonwealth.4.%d.%d.BTO' % (VO, cx, cy)
            if not os.path.isfile(f): continue
            N = nifwind.Nif(open(f, 'rb').read())
            for k, (t, o, sz) in enumerate(N.blocks):
                if t not in nifwind.SHAPES: continue
                sh, uv, tri = A.shape_uv_tris(N, k)
                Pw = sh['pos'] * 4 + np.array([cx * 4096, cy * 4096, 0], np.float32)
                c = Pw[tri].mean(1)
                sel = (c[:, 0] > x0) & (c[:, 0] < x1) & (c[:, 1] > y0) & (c[:, 1] < y1) & (c[:, 2] > zmin)
                q = Pw[tri[sel]] - L
                u = W / 2 + (q @ VX) / upp; vv = H / 2 - (q @ VY) / upp
                for a, b in zip(u, vv):
                    d.polygon(list(zip(a.tolist(), b.tolist())), fill=255)
                nt += int(sel.sum())
        full = img
        er = img.filter(ImageFilter.MinFilter(5))
        er.save(P + 'mask_%s.png' % tag)
        res[tag] = (np.asarray(er) > 0, nt, int((np.asarray(full) > 0).sum()))
    return res

def stats(img, m):
    rgb = img[m]; lin = A.s2l(rgb); mean = lin.mean(0); s = A.l2s(mean)
    mx = rgb.max(-1); mn = rgb.min(-1); S = np.where(mx > 1e-4, (mx - mn) / np.maximum(mx, 1e-4), 0)
    return s, float((s.max() - s.min()) / max(s.max(), 1e-6)), float(S.mean()), float(0.2126 * mean[0] + 0.7152 * mean[1] + 0.0722 * mean[2])

if __name__ == '__main__':
    arms = {}
    for arm in ('lit', 'base'):
        img, bg, n = composite(arm)
        arms['vanilla_' + arm] = img
        print(arm, 'composite', img.shape, 'bg', np.round(bg * 255), 'pixels per layer', n)
    for tag in ('ours_lit', 'ours_lit_vc', 'ours_base'):
        arms[tag] = load(tag)
    H, W = arms['ours_lit'].shape[:2]
    for k, v in arms.items(): assert v.shape[:2] == (H, W), (k, v.shape)
    upp = 2 * 16384 / W
    print('frame %dx%d, upp %.4f' % (W, H, upp))
    M = masks(W, H, upp)
    out = {}
    for tag, (m, nt, full) in M.items():
        ys, xs = np.nonzero(m)
        print('tower %s: %d vanilla tris, mask %d px (eroded from %d), bbox x %d..%d y %d..%d' % (tag, nt, m.sum(), full, xs.min(), xs.max(), ys.min(), ys.max()))
        for k, img in arms.items():
            bgm = np.abs(img - img[2, 2]).max(-1) > 1.5 / 255
            s, Sm, mS, Y = stats(img, m & bgm)
            out[(tag, k)] = (s, Sm, mS, Y, int((m & bgm).sum()))
            print('   %-13s sRGB %s  S_of_mean %.3f  mean_S %.3f  Y %.4f  (%d px)' % (k, np.round(s, 3), Sm, mS, Y, (m & bgm).sum()))
    pickle.dump(out, open('measure_pics.pkl', 'wb'))
