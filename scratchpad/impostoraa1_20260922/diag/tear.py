"""THE TEAR, taken apart (lane IMPOSTORAA1, director 00:2x 2026-09-23).

bungo on headline_before_after.png: "like somebody ripped out a piece of paper".
For one subject and one view this program

  1. picks the view (of the four the stage run grabbed) where the TEAR is worst,
     measured on the application's own grabs: mesh ink that the NEAREST-frame
     card covers and the BLENDED card does not;
  2. redraws the card with a numpy copy of res/shaders/impostor_oct.frag
     (frame pick = tests/spells/impostor_oct_ref.py, sheets decoded from the
     shipped DDS by tests/spells/impostor_bc_decode.py, the same one-step
     height parallax, bilinear taps, the coverage decode and the 128/255 cut),
     keeping every contributing frame's own sample apart;
  3. writes one picture: the app's mesh | blended card | nearest card, then the
     reference blend, its coverage with the cut, the TEAR map, then per frame:
     raw texels (colour x coverage), coverage, height -- and prints the numbers.

Camera: the reference draws ORTHOGRAPHIC along the view direction; the app's
card camera is perspective at the harness distance, so the two differ by the
eye's few degrees of spread -- the picture is registered by eye, not by pixel.

    python tear.py <setroot> <tag> <formid> <stagedir-blend> <stagedir-nearest> <out.png>
"""
import sys, os, glob, json, math, struct
import numpy as np
from PIL import Image, ImageDraw
sys.path.insert(0, 'E:/Projects/NifskopeWildWastelandEdition/tests/spells')
import impostor_oct_ref as ref
from impostor_bc_decode import load_dds

setroot, tag, fid, sdef, snear, outp = sys.argv[1:7]
CUT = 128.0 / 255.0

# ---- 1. the view with the worst tear, on the application's own grabs ------
def ink(p):
    a = np.asarray(Image.open(p).convert('RGB')).astype(int)
    return np.abs(a - a[2, 2]).sum(-1) > 30
best = None
for mp in sorted(glob.glob(f'{sdef}/{tag}/v_*_mesh.png')):
    v = os.path.basename(mp)[2:-9]
    m = ink(mp); cb = ink(f'{sdef}/{tag}/v_{v}_card.png'); cn = ink(f'{snear}/{tag}/v_{v}_card.png')
    torn = m & cn & ~cb
    row = (torn.sum(), v, (m & cb).sum() / max(1, m.sum()), (m & cn).sum() / max(1, m.sum()), m.sum())
    print('  app view %s: mesh %d px, recall blend %.3f nearest %.3f, TORN (nearest covers, blend drops) %d px'
          % (v, row[4], row[2], row[3], row[0]))
    if best is None or row[0] > best[0]:
        best = row
ntorn, view, rb, rn, _ = best
az = float(view.split('_')[0][2:]); el = float(view.split('_')[1][2:])
print('%s: worst view az %g el %g (torn %d px)' % (tag, az, el, ntorn))

# ---- 2. the reference draw ---------------------------------------------------
raw = open(f'{setroot}/{tag}/cards/{fid}_oct.lodm', 'rb').read()
card = json.loads(raw[raw.index(b'{'):].decode('utf-8', 'replace').rstrip('\x00'))['card']
N = card['oct']; hw, hh = card['half']; ctr = np.array(card['center']); span = card['depthSpan']
fo = card.get('frameOffset', [0.0] * (2 * N * N))
cf = card['coverage']['floor'] / 255.0; cbase = card['coverage']['base'] / 255.0
T = f'{setroot}/{tag}/textures/data/fo4cslod/cards'
Cs, (SW, SH), _ = load_dds(f'{T}/{fid}_oct_d.dds')
Ns, _, _ = load_dds(f'{T}/{fid}_oct_n.dds')

def bilerp(img, u, v):
    x = u * SW - 0.5; y = v * SH - 0.5
    x0 = np.floor(x).astype(int); y0 = np.floor(y).astype(int); fx = x - x0; fy = y - y0
    def g(xx, yy):
        return img[np.clip(yy, 0, SH - 1), np.clip(xx, 0, SW - 1)]
    return (g(x0, y0) * ((1 - fx) * (1 - fy))[..., None] + g(x0 + 1, y0) * (fx * (1 - fy))[..., None]
            + g(x0, y0 + 1) * ((1 - fx) * fy)[..., None] + g(x0 + 1, y0 + 1) * (fx * fy)[..., None])

def covof(a):
    return np.where(a < cbase, 0.0, np.clip(cf + (a - cbase) * (1 - cf) / (1 - cbase), cf, 1.0))

a, e = math.radians(az), math.radians(el)
cam = np.array([math.cos(a) * math.cos(e), math.sin(a) * math.cos(e), math.sin(e)])
frames = ref.pick_frames_for_camera(tuple(cam), N)
right = np.cross([0, 0, 1.0], cam); right /= np.linalg.norm(right); up = np.cross(cam, right)
ray = -cam
R = max(hw, hh) * 1.02
PX = 2.0 * R / 700.0
s = (np.arange(700) + 0.5) * PX - R
S, Tt = np.meshgrid(s, -s)
P = ctr + S[..., None] * right + Tt[..., None] * up

per = []
acc = {'c': 0, 'a': 0, 'h': 0, 'ws': 0}
for (idx, gi, gj, w) in frames:
    d = np.array(ref.frame_dir(gi, gj, N)); fr, fu, ff = (np.array(x) for x in ref.frame_basis(tuple(d)))
    ox, oy = fo[2 * idx], fo[2 * idx + 1]
    rect = ref.frame_rect(gi, gj, N)
    def uvof(p):
        r = p - ctr
        st = np.stack([r @ fr - ox, r @ fu - oy], -1)
        uv = np.stack([st[..., 0] / (2 * hw) + 0.5, 0.5 - st[..., 1] / (2 * hh)], -1)
        uv = np.clip(uv, 0, 1)
        return rect[0] + uv[..., 0] * rect[2], rect[1] + uv[..., 1] * rect[3], r @ ff
    u, v, dd = uvof(P)
    h = bilerp(Ns, u, v)[..., 2]
    want = -(h - 0.5) * span; den = ray @ ff
    if abs(den) > 0.15:
        u, v, dd = uvof(P + ray * ((want - dd) / den)[..., None])
    c = bilerp(Cs, u, v); n = bilerp(Ns, u, v)
    cov = covof(c[..., 3])
    per.append(dict(idx=idx, gi=gi, gj=gj, w=w, rgb=c[..., :3], cov=cov, h=n[..., 2]))
    acc['c'] = acc['c'] + c[..., :3] * (w * cov)[..., None]; acc['a'] = acc['a'] + w * cov
    acc['ws'] = acc['ws'] + w * cov
blend_cov = acc['a']; keep = blend_cov >= CUT
col = acc['c'] / np.maximum(acc['ws'], 1e-5)[..., None]
anyk = np.zeros_like(keep); maxk = np.zeros_like(blend_cov)
for f in per:
    anyk |= f['cov'] >= CUT; maxk = np.maximum(maxk, f['cov'])
torn = anyk & ~keep
lone = [(f['cov'] >= CUT) for f in per]
nall = np.logical_and.reduce(lone).sum(); nany = anyk.sum()
print('  frames at this view: ' + ', '.join('(%d,%d) w %.3f' % (f['gi'], f['gj'], f['w']) for f in per))
print('  reference: kept by the blend %d px; kept by ANY one frame alone %d; by ALL three %d (agreement %.3f)'
      % (keep.sum(), nany, nall, nall / max(1, nany)))
print('  TORN (some frame alone >= cut, blend < cut): %d px = %.1f%% of the any-frame silhouette'
      % (torn.sum(), 100.0 * torn.sum() / max(1, nany)))
if torn.sum():
    for f in per:
        c = f['cov'][torn]
        print('    in the torn px, frame (%d,%d) w %.3f: coverage mean %.3f, share >= cut %.3f, share == 0 %.3f'
              % (f['gi'], f['gj'], f['w'], c.mean(), (c >= CUT).mean(), (c <= 1e-6).mean()))
    print('    blended coverage in the torn px: mean %.3f (cut %.3f); max single-frame coverage mean %.3f'
          % (blend_cov[torn].mean(), CUT, maxk[torn].mean()))
    # the same view with the height step removed: does parallax make it worse?
print('SUMMARY %s view az%g el%g torn_app %d torn_ref %d any %d agree %.3f' % (tag, az, el, ntorn, torn.sum(), nany, nall / max(1, nany)))

# ---- 3. the picture -----------------------------------------------------------
G = 0.5
def over(rgb, cov):
    return np.clip(rgb * cov[..., None] + G * (1 - cov[..., None]), 0, 1)
def grey(x):
    return np.repeat(np.clip(x, 0, 1)[..., None], 3, -1)
def cutline(img, cov):
    e = (cov >= CUT) ^ np.roll(cov >= CUT, 1, 0) | (cov >= CUT) ^ np.roll(cov >= CUT, 1, 1)
    img = img.copy(); img[e] = (1, 0, 0); return img
def hmap(h, cov):
    t = np.clip(h, 0, 1)
    rgb = np.stack([t, 0.3 + 0.4 * (1 - abs(t - 0.5) * 2), 1 - t], -1)
    return np.where((cov > 0)[..., None], rgb, 0.12)
def toim(x):
    return Image.fromarray((np.clip(x, 0, 1) * 255).astype(np.uint8))
def tearmap():
    img = np.full(keep.shape + (3,), 0.12)
    img[keep] = (0.75, 0.75, 0.75); img[torn] = (1.0, 0.1, 0.1)
    return img

def appcrop(p):
    im = Image.open(p).convert('RGB'); m = ink(f'{sdef}/{tag}/v_{view}_mesh.png')
    ys, xs = np.nonzero(m); pad = 20
    box = (max(0, xs.min() - pad), max(0, ys.min() - pad), min(m.shape[1], xs.max() + pad), min(m.shape[0], ys.max() + pad))
    return im.crop(box)
tiles = [[(appcrop(f'{sdef}/{tag}/v_{view}_mesh.png'), 'app: mesh'),
          (appcrop(f'{sdef}/{tag}/v_{view}_card.png'), 'app: card as drawn (3-frame blend)'),
          (appcrop(f'{snear}/{tag}/v_{view}_card.png'), 'app: nearest frame only')],
         [(toim(over(col, keep.astype(float))), 'reference: blend, cut at 128'),
          (toim(cutline(grey(blend_cov), blend_cov)), 'blended coverage (red = the 0.5 cut)'),
          (toim(tearmap()), 'TORN: red = one frame alone keeps it, blend drops it')]]
for f in per:
    tiles.append([(toim(over(f['rgb'], f['cov'])), 'frame (%d,%d) w %.2f: raw texels' % (f['gi'], f['gj'], f['w'])),
                  (toim(cutline(grey(f['cov']), f['cov'])), 'frame (%d,%d): coverage' % (f['gi'], f['gj'])),
                  (toim(hmap(f['h'], f['cov'])), 'frame (%d,%d): height (blue near, red far)' % (f['gi'], f['gj']))])
CW = 300; CH = 380; LH = 18
out = Image.new('RGB', (CW * 3, (CH + LH) * len(tiles) + 30), (24, 24, 24))
dr = ImageDraw.Draw(out)
dr.text((6, 8), '%s  view az %g el %g   torn in the app: %d px (nearest covers, blend drops)   reference torn %d px of %d any-frame px'
        % (tag, az, el, ntorn, torn.sum(), nany), fill=(255, 255, 255))
for r, row in enumerate(tiles):
    for c, (im, lab) in enumerate(row):
        im = im.copy(); im.thumbnail((CW - 6, CH - 6))
        x = c * CW + (CW - im.width) // 2; y = 30 + r * (CH + LH)
        out.paste(im, (x, y)); dr.text((c * CW + 6, y + CH - 4), lab, fill=(230, 230, 230))
out.save(outp)
print('wrote', outp, out.size)
