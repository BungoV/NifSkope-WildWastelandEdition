"""Offline proof of the vanilla-colour fill (addenda A1-A3), BEFORE any src/ edit (ww-artefact-localise rule 5).
Region cells X0..X1 x Y0..Y1, one sample per S world units (sample centres). Four fields per sample:
  A   the shipped VT.2 texel (old law: chunk dominant base), mip 0 content, nearest texel
  B   the section-2.5 composite under the engine-default law (the committed fix), tests/spells model
  V   Bethesda dim-4 LOD diffuse, read in place, Mitchell-Netravali bicubic (B=C=1/3) at the point
  F   B blended toward tone-matched V over a band on the UNPAINTED side only: F = B + (T(V) - B) * w(d),
      d = distance to the nearest painted cell (0 inside it), w = smoothstep(0, band, d)
Painted cell = a LAND quadrant with a BTXT or an ATXT layer (his LAND wins: w = 0 on every painted cell).
T = tone + saturation match fitted on the OVERLAP (painted cells within RING cells of an unpainted one):
    luminance mean/std match, chroma (rgb - lum) scaled by the RMS ratio and shifted by the mean-chroma difference.
Band = measured: ceil(|mean lum B - mean lum T(V)| over the unpainted ring / bar) cells, min 1.
Bar = p99 of vanilla's own adjacent cell-mean luminance steps over the region.
usage: fill_model.py X0 Y0 X1 Y1 S TAG"""
import sys, os, math, pickle, numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import law_predict as lp, lodgen_terrain_model as tm, vanilla_tiles as vt, vtread

X0, X1, Y0, Y1 = int(sys.argv[1]), int(sys.argv[3]), int(sys.argv[2]), int(sys.argv[4])
S = float(sys.argv[5]); TAG = sys.argv[6]
RING = 3
P = pickle.load(open('w2_cells.pkl', 'rb'))['painted']
n = int(4096 / S); W = (X1 - X0 + 1) * n; H = (Y1 - Y0 + 1) * n
xs = X0 * 4096 + (np.arange(W) + .5) * S
ys = (Y1 + 1) * 4096 - (np.arange(H) + .5) * S          # row 0 = north
LW = np.array([.2126, .7152, .0722])


def lum(x):
    return x @ LW


cache_f = 'fill_%s_B.npy' % TAG
if os.path.exists(cache_f):
    B = np.load(cache_f)
else:
    B = np.zeros((H, W, 3))
    for j, wy in enumerate(ys):
        for i, wx in enumerate(xs):
            cx, cy = int(wx // 4096), int(wy // 4096)
            r = tm.composite(lp.e, lp.DATA, lp.cache, cx, cy, 4, lp.DEF, wx, wy, 16.0)
            B[j, i] = r['colour'][:3]
        if j % 16 == 0:
            print('  B row', j, H, flush=True)
    B *= 255
    np.save(cache_f, B)

# A: shipped texels (nearest content texel)
# A = the shipped bake by default; VT2=<a VT.2.lodt> reads another (the post-build gate: VT2 = the fill bake)
v2 = vtread.Vt(os.environ.get('VT2', r'E:/Projects/Fallout 4 Mods/mods/FO4CSLOD/FO4CSLOD/Commonwealth/Commonwealth.VT.2.lodt'))
print('A read from', v2.path)
mA, wW, nN = v2.mosaic(X0 - ((X0 + 96) % 2), Y0 - ((Y0 + 96) % 2), X1, Y1)
wW *= 4096; nN *= 4096
upt = 16.0
A = np.zeros((H, W, 3))
tx = np.clip(((xs - wW) // upt).astype(int), 0, mA.shape[1] - 1)
for j, wy in enumerate(ys):
    ty = min(max(int((nN - wy) // upt), 0), mA.shape[0] - 1)
    A[j] = mA[ty, tx, :3]

# V: vanilla dim-4 mosaic (32 u/texel), row 0 = north, Mitchell bicubic
cx0 = X0 - ((X0 + 96) % 4); cy0 = Y0 - ((Y0 + 96) % 4)
cx1 = X1 - ((X1 + 96) % 4); cy1 = Y1 - ((Y1 + 96) % 4)
mv, tn = vt.mosaic('Commonwealth', 4, cx0, cy0, cx1, cy1)
mv = mv[..., :3].astype(np.float64)
vupt = 4 * 4096 / tn; vW = cx0 * 4096; vN = (cy1 + 4) * 4096


def mitchell(t):
    t = np.abs(t); b = c = 1 / 3.
    return np.where(t < 1, ((12 - 9 * b - 6 * c) * t ** 3 + (-18 + 12 * b + 6 * c) * t ** 2 + (6 - 2 * b)) / 6,
                    np.where(t < 2, ((-b - 6 * c) * t ** 3 + (6 * b + 30 * c) * t ** 2 + (-12 * b - 48 * c) * t
                                     + (8 * b + 24 * c)) / 6, 0))


def bicubic(img, u, v):
    """u, v continuous texel coordinates; texel k's centre is at k + 0.5; edge clamp."""
    u = u - .5; v = v - .5
    iu = np.floor(u).astype(int); iv = np.floor(v).astype(int)
    out = np.zeros(u.shape + (3,)); ws = np.zeros(u.shape)
    for dv in (-1, 0, 1, 2):
        for du in (-1, 0, 1, 2):
            wgt = mitchell(u - (iu + du)) * mitchell(v - (iv + dv))
            yy = np.clip(iv + dv, 0, img.shape[0] - 1); xx = np.clip(iu + du, 0, img.shape[1] - 1)
            out += img[yy, xx] * wgt[..., None]; ws += wgt
    return out / ws[..., None]


UU, VV = np.meshgrid((xs - vW) / vupt, (vN - ys) / vupt)
V = np.clip(bicubic(mv, UU, VV), 0, 255)

# masks and distance to the painted set
cellx = np.floor(xs / 4096).astype(int); celly = np.floor(ys / 4096).astype(int)
pm = np.array([[(cx, cy) in P for cx in cellx] for cy in celly])
pcells = [c for c in P if X0 - 8 <= c[0] <= X1 + 8 and Y0 - 8 <= c[1] <= Y1 + 8]
GX, GY = np.meshgrid(xs, ys)
d = np.full(GX.shape, 1e9)
for (cx, cy) in pcells:
    dx = np.maximum(np.maximum(cx * 4096 - GX, GX - (cx + 1) * 4096), 0)
    dy = np.maximum(np.maximum(cy * 4096 - GY, GY - (cy + 1) * 4096), 0)
    d = np.minimum(d, np.hypot(dx, dy))
near_out = np.array([[any((cx + a, cy + b) not in P for a in range(-RING, RING + 1) for b in range(-RING, RING + 1))
                      for cx in cellx] for cy in celly])
overlap = pm & near_out
# ring = unpainted cells with a painted cell within RING cells, Chebyshev -- the C++ definition
near_in = np.array([[any((cx + a, cy + b) in P for a in range(-RING, RING + 1) for b in range(-RING, RING + 1))
                     for cx in cellx] for cy in celly])
ring_out = (~pm) & near_in

# tone + saturation match T, fitted on the overlap: V -> B
# fitted on CELL-MEAN colours: B is point-sampled texture detail at the VT.2 footprint and V is a 32 u/texel
# LOD sheet, so their per-sample spreads are different quantities; at one cell they are the same one.
def cellrgb(F):
    return F.reshape(H // n, n, W // n, n, 3).mean((1, 3))
ovc = overlap.reshape(H // n, n, W // n, n)[:, 0, :, 0]
Vc, Bc = cellrgb(V)[ovc], cellrgb(B)[ovc]
lv, lb = lum(Vc), lum(Bc)
ga = lb.std() / lv.std()
ga = min(ga, float(os.environ.get('GAIN_CAP', '1')))   # the C++ caps at 1 (vanilla's baked relief light)
gb = lb.mean() - ga * lv.mean()
cvv = Vc - lv[:, None]; cbb = Bc - lb[:, None]
sat = math.sqrt((cbb ** 2).sum(1).mean() / (cvv ** 2).sum(1).mean())
cshift = cbb.mean(0) - sat * cvv.mean(0)


def T(x):
    l = lum(x)
    return np.clip((ga * l + gb)[..., None] + sat * (x - l[..., None]) + cshift, 0, 255)


TV = T(V)


def cellmean(F):
    return lum(F).reshape(H // n, n, W // n, n).mean((1, 3))


cvm = cellmean(V)
# bar = p99 of vanilla adjacent cell-mean steps over pairs with BOTH cells in overlap or ring (the C++ population)
ro_c = ring_out.reshape(H // n, n, W // n, n)[:, 0, :, 0]
role = ovc | ro_c
sy = np.abs(np.diff(cvm, axis=0))[role[1:, :] & role[:-1, :]]
sx = np.abs(np.diff(cvm, axis=1))[role[:, 1:] & role[:, :-1]]
steps_v = np.concatenate([sy.ravel(), sx.ravel()])
bar = float(np.percentile(steps_v, 99))
# band: p95 over the ring of |lum B cell mean - lum T(V cell mean)|, as the C++ computes it
dl = float(np.percentile(np.abs(cellmean(B) - lum(T(cellrgb(V))))[ro_c], 95))
band = min(8, max(1, math.ceil(dl / bar))) * 4096.0
t = np.clip(d / band, 0, 1); wgt = t * t * (3 - 2 * t); wgt[pm] = 0
F = B + (TV - B) * wgt[..., None]

pmc = pm.reshape(H // n, n, W // n, n)[:, 0, :, 0]
pairs = []
for j in range(pmc.shape[0]):
    for i in range(pmc.shape[1]):
        for (jj, ii) in ((j + 1, i), (j, i + 1)):
            if jj < pmc.shape[0] and ii < pmc.shape[1] and pmc[j, i] != pmc[jj, ii]:
                pairs.append(((j, i), (jj, ii)))


def border_steps(F):
    c = cellmean(F)
    return np.array([abs(c[a] - c[b]) for a, b in pairs])


def line_steps(F):
    """the step AT the border line, one sample either side, which a cell mean can hide"""
    L = lum(F); out = []
    for (j, i), (jj, ii) in pairs:
        if jj == j + 1:
            out.append(abs(L[jj * n, i * n:(i + 1) * n].mean() - L[jj * n - 1, i * n:(i + 1) * n].mean()))
        else:
            out.append(abs(L[j * n:(j + 1) * n, ii * n].mean() - L[j * n:(j + 1) * n, ii * n - 1].mean()))
    return np.array(out)


allpairs = [((j, i), (j + 1, i)) for j in range(H // n - 1) for i in range(W // n)] +            [((j, i), (j, i + 1)) for j in range(H // n) for i in range(W // n - 1)]
_p = pairs; pairs = allpairs
Lv_all = line_steps(V); pairs = _p
lbar = float(np.percentile(Lv_all, 99))
print('line bar = p99 of vanilla at-line step over all %d interior cell borders = %.2f' % (len(allpairs), lbar))
print('region cells %d..%d x %d..%d, sample %g u; painted cells %d of %d; border pairs %d' % (
    X0, X1, Y0, Y1, S, pmc.sum(), pmc.size, len(pairs)))
print('tone fit on %d overlap cells: lum gain %.3f offset %.2f, saturation x%.3f, chroma shift %s' % (
    ovc.sum(), ga, gb, sat, np.round(cshift, 2)))
print('bar = p99 vanilla adjacent cell step = %.2f; |lum B - lum T(V)| p95 cell |lum B - lum T(V)| on the unpainted ring = %.2f -> band %d cells' % (
    bar, dl, band / 4096))
for name, Fx in (('A shipped', A), ('B engine default', B), ('F fill', F), ('V vanilla', V)):
    s = border_steps(Fx); l = line_steps(Fx)
    print('%-17s cell-mean border step median %5.2f p95 %5.2f max %5.2f | at-line median %5.2f p95 %5.2f max %5.2f'
          ' | over bar: cell %d, line %d of %d' % (name, np.median(s), np.percentile(s, 95), s.max(), np.median(l),
                                             np.percentile(l, 95), l.max(), int((s > bar).sum()), int((l > lbar).sum()), len(s)))
print('painted samples identical B vs F:', bool(np.array_equal(B[pm], F[pm])))
np.savez_compressed('fill_%s.npz' % TAG, A=A, B=B, V=V, F=F, TV=TV, pm=pm, d=d, n=n, X0=X0, Y1=Y1, S=S,
                    bar=bar, band=band)
# the band's own inside: steps between two UNPAINTED cells within band + 1 cell of the painted set
cd_ = d.reshape(H // n, n, W // n, n).min((1, 3))
inner = [((j, i), (jj, ii)) for ((j, i), (jj, ii)) in allpairs
         if not pmc[j, i] and not pmc[jj, ii] and min(cd_[j, i], cd_[jj, ii]) <= band + 4096]
for name, Fx in (('A (file)', A), ('B engine default', B), ('F fill', F), ('V vanilla', V)):
    c = cellmean(Fx); st = np.array([abs(c[a] - c[b]) for a, b in inner])
    print('%-17s unpainted-unpainted steps inside the band (%d pairs): median %.2f p95 %.2f max %.2f, over cell bar %d' % (
        name, len(inner), np.median(st), np.percentile(st, 95), st.max(), int((st > bar).sum())))
# model vs file on the UNPAINTED samples (meaningful only when VT2 is a fill bake): the C++ against its proof
upm = ~pm
print('A(file) vs F(model) on unpainted samples: mean |dlum| %.2f, p95 %.2f; vs B: mean %.2f' % (
    np.abs(lum(A) - lum(F))[upm].mean(), np.percentile(np.abs(lum(A) - lum(F))[upm], 95), np.abs(lum(A) - lum(B))[upm].mean()))
