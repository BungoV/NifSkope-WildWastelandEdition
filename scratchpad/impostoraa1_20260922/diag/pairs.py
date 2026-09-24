"""Cross-frame REGISTRATION at the bake directions: stand at frame A's own
direction (where A alone is the known answer), draw neighbour B through the
shipped lookup, and score B's cut mask against A's. Hypotheses scored side by
side: parallax steps 0 / 1 (shipped) / 8, and frameOffset read transposed.
    python pairs.py <setroot> <tag> <formid>"""
import sys, json, math, numpy as np
sys.path.insert(0, 'E:/Projects/NifskopeWildWastelandEdition/tests/spells')
import impostor_oct_ref as ref
from impostor_bc_decode import load_dds
setroot, tag, fid = sys.argv[1:4]
raw = open(f'{setroot}/{tag}/cards/{fid}_oct.lodm', 'rb').read()
card = json.loads(raw[raw.index(b'{'):].decode('utf-8', 'replace').rstrip('\x00'))['card']
N = card['oct']; hw, hh = card['half']; ctr = np.array(card['center']); span = card['depthSpan'] * float(__import__('os').environ.get('SPANK', '1'))
fo = card.get('frameOffset', [0.0] * (2 * N * N)); cf = card['coverage']['floor'] / 255.; cb = card['coverage']['base'] / 255.
T = f'{setroot}/{tag}/textures/data/fo4cslod/cards'
Cs, (SW, SH), _ = load_dds(f'{T}/{fid}_oct_d.dds'); Ns, _, _ = load_dds(f'{T}/{fid}_oct_n.dds')
import os
if os.environ.get('HPNG'):
    from PIL import Image
    Hp = np.asarray(Image.open(f'{setroot}/{tag}/cards/{fid}_oct_normal.png').convert('RGBA')).astype(float) / 255.
    Ap = np.asarray(Image.open(f'{setroot}/{tag}/cards/{fid}_oct_albedo.png').convert('RGBA'))[..., 3] >= 160
    print('HPNG: height from the 8-bit PNG where the PNG is covered (%d texels), DDS elsewhere; shape %s vs %s' % (Ap.sum(), Hp.shape, Ns.shape))
    Ns = Ns.copy(); Ns[..., 2] = np.where(Ap, Hp[..., 2], Ns[..., 2])
CUT = 128 / 255.
def bil(img, u, v):
    x = u * SW - .5; y = v * SH - .5; x0 = np.floor(x).astype(int); y0 = np.floor(y).astype(int); fx = x - x0; fy = y - y0
    g = lambda a, b: img[np.clip(b, 0, SH - 1), np.clip(a, 0, SW - 1)]
    return g(x0, y0) * ((1-fx)*(1-fy))[..., None] + g(x0+1, y0) * (fx*(1-fy))[..., None] + g(x0, y0+1) * ((1-fx)*fy)[..., None] + g(x0+1, y0+1) * (fx*fy)[..., None]
cov = lambda a: np.where(a < cb, 0., np.clip(cf + (a - cb) * (1 - cf) / (1 - cb), cf, 1.))
R = max(hw, hh) * 1.02; s = (np.arange(400) + .5) * (2 * R / 400) - R; S, Tt = np.meshgrid(s, -s)
def draw(cam, gi, gj, steps, transpose=False):
    right = np.cross([0, 0, 1.], cam)
    right = right / np.linalg.norm(right) if np.linalg.norm(right) > 1e-4 else np.array([1., 0, 0])
    up = np.cross(cam, right); ray = -cam
    P = ctr + S[..., None] * right + Tt[..., None] * up
    fr, fu, ff = (np.array(x) for x in ref.frame_basis(tuple(ref.frame_dir(gi, gj, N))))
    idx = (gj + gi * N) if transpose else (gi + gj * N)
    ox, oy = fo[2 * idx], fo[2 * idx + 1]; rect = ref.frame_rect(gi, gj, N)
    def uvof(p):
        r = p - ctr
        uv = np.clip(np.stack([(r @ fr - ox) / (2 * hw) + .5, .5 - (r @ fu - oy) / (2 * hh)], -1), 0, 1)
        return rect[0] + uv[..., 0] * rect[2], rect[1] + uv[..., 1] * rect[3], r @ ff
    u, v, d0 = uvof(P); den = ray @ ff
    if abs(den) > .15:
        for _ in range(steps):
            h = bil(Ns, u, v)[..., 2]; u, v, _ = uvof(P + ray * ((-(h - .5) * span - d0) / den)[..., None])
    return cov(bil(Cs, u, v)[..., 3]) >= CUT
res = {}
for gj in range(N):
    for gi in range(N):
        cam = np.array(ref.frame_dir(gi, gj, N))
        for tr in ((False, True) if not __import__('os').environ.get('ONLY') else (False,)):
            A = draw(cam, gi, gj, 1, tr)
            for (di, dj) in ((1, 0), (0, 1), (1, -1)):
                bi, bj = gi + di, gj + dj
                if not (0 <= bi < N and 0 <= bj < N): continue
                for st in (0, 1, 8):
                    B = draw(cam, bi, bj, st, tr)
                    iou = (A & B).sum() / max(1, (A | B).sum())
                    res.setdefault((tr, st), []).append(iou)
                    if not tr and st == 1:
                        ea = ref.frame_angles(tuple(cam)); eb = ref.frame_angles(ref.frame_dir(bi, bj, N))
                        print('  A(%d,%d) el %5.1f az %6.1f  B(%d,%d) el %5.1f az %6.1f  IoU %.3f  A px %d B px %d' % (gi, gj, ea[0], ea[1], bi, bj, eb[0], eb[1], iou, A.sum(), B.sum()))
for (tr, st), v in sorted(res.items()):
    print('%s %-24s steps %d: neighbour-vs-self IoU mean %.3f  min %.3f  (%d pairs)' % (tag, 'frameOffset TRANSPOSED' if tr else 'frameOffset as shipped', st, np.mean(v), np.min(v), len(v)))

# THE MARCH: B looked up by searching A's ray for where it enters B's depth
# hull (first point from the eye inside B's silhouette at/behind B's surface,
# within TH units of it) instead of the fixed point from the card plane.
def march(cam, gi, gj, TH, K=200):
    right = np.cross([0, 0, 1.], cam)
    right = right / np.linalg.norm(right) if np.linalg.norm(right) > 1e-4 else np.array([1., 0, 0])
    up = np.cross(cam, right); ray = -cam
    P = ctr + S[..., None] * right + Tt[..., None] * up
    fr, fu, ff = (np.array(x) for x in ref.frame_basis(tuple(ref.frame_dir(gi, gj, N))))
    idx = gi + gj * N; ox, oy = fo[2 * idx], fo[2 * idx + 1]; rect = ref.frame_rect(gi, gj, N)
    Rm = 1.1 * max(hw, hh); out = np.zeros(P.shape[:2], bool); done = np.zeros(P.shape[:2], bool)
    for s_ in np.linspace(-Rm, Rm, K):
        r = P + ray * s_ - ctr
        uv = np.clip(np.stack([(r @ fr - ox) / (2 * hw) + .5, .5 - (r @ fu - oy) / (2 * hh)], -1), 0, 1)
        u = rect[0] + uv[..., 0] * rect[2]; v = rect[1] + uv[..., 1] * rect[3]
        c = cov(bil(Cs, u, v)[..., 3]); h = bil(Ns, u, v)[..., 2]
        dz = -(h - .5) * span - (r @ ff)
        hit = (~done) & (c >= CUT) & (dz >= 0) & (dz <= TH)
        out |= hit; done |= hit
    return out
import os
if os.environ.get('MARCH'):
    for TH in [float(x) for x in os.environ['MARCH'].split(',')]:
        same, cross = [], []
        for gj in range(N):
            for gi in range(N):
                cam = np.array(ref.frame_dir(gi, gj, N)); A = draw(cam, gi, gj, 1)
                for (di, dj) in ((1, 0), (0, 1), (1, -1)):
                    bi, bj = gi + di, gj + dj
                    if not (0 <= bi < N and 0 <= bj < N): continue
                    B = march(cam, bi, bj, TH)
                    iou = (A & B).sum() / max(1, (A | B).sum())
                    ea = ref.frame_angles(tuple(cam))[0]; eb = ref.frame_angles(ref.frame_dir(bi, bj, N))[0]
                    (same if abs(ea - eb) < 1 else cross).append(iou)
        print('%s MARCH thickness %g: neighbour-vs-self IoU same ring %.3f (%d), cross ring %.3f (%d)' % (tag, TH, np.mean(same), len(same), np.mean(cross), len(cross)))
