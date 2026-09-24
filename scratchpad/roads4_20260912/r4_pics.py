"""ROADS4's pictures. Every panel is a REAL BAKE written by the 06:31:05 exe
on 2026-09-12, never a synthetic or a recoloured copy; the vanilla panel is
Bethesda's shipped sheet read off disk.

  1  road_ground_look.png    the Sanctuary window at 4:1 --
                             vanilla | the default | ground paint 0 | opacity
                             0.326 -- with each panel's numbers under it
  2  road_ground_where.png   WHICH texels are terrain-material, drawn on the
                             sheet, which is the finding itself
  3  road_ground_profile.png the cross-road profile, vanilla against the
                             default and the two candidates
"""
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
SCRATCH = os.path.dirname(HERE)
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(SCRATCH, 'roads3_20260911'))
import r4lib as R                                              # noqa: E402
import r3lib as R3                                             # noqa: E402

IMG = os.path.join(HERE, 'images')
os.makedirs(IMG, exist_ok=True)
TILE = 't2020'


def sheet(variant, tile=TILE):
    t = R.TILES[tile]
    import splatlib as S
    return S.Dds(os.path.join(HERE, 'out', variant, tile, 'tex',
                              'Commonwealth.4.%d.%d.DDS'
                              % (t['cx'], t['cy']))).level(0)[:, :, :3] \
        .astype(np.float64)


def u8(a):
    return np.clip(a, 0, 255).astype(np.uint8)


def font(sz=15):
    try:
        return ImageFont.truetype('consola.ttf', sz)
    except Exception:
        return ImageFont.load_default()


van = R.vanilla_sheet(TILE)
ground = sheet('r4_noroads')
d0 = sheet('r4_default')
gp0 = sheet('r4_gp0')
op = sheet('r4_op0326')
mask = R.painted_mask(d0, ground)

pr = R.project(TILE)
shp = pr['shp']
gOf = np.array(['landscape/ground/' in (s['mat'] or '').replace(chr(92), '/')
                .lower() for s in pr['shapes']])
idx = np.clip(shp, 0, None)
road0 = mask & (shp >= 0)
isT = road0 & gOf[idx]

# the 96x96 window with the most terrain-class texels: the picture is of the
# thing being measured, chosen by the measurement and not by eye.
m = isT.astype(np.int64)
c = np.pad(m.cumsum(0).cumsum(1), ((1, 0), (1, 0)))
s = 96
win = c[s:, s:] - c[:-s, s:] - c[s:, :-s] + c[:-s, :-s]
y0, x0 = np.unravel_index(win.argmax(), win.shape)
y1, x1 = y0 + s, x0 + s
print('window', (x0, y0), 'terrain texels in it', int(win.max()))

# --------------------------------------------------------------- picture 1
F = font()
zf = 4
pw = 512
gap = 12
capH = 62
cols = [(van, 'VANILLA  chunk (-20,20)', (120, 220, 120)),
        (d0, 'ours, the DEFAULT (detail 1, ground paint 1)', (255, 255, 255)),
        (gp0, 'ours, --road-ground-paint 0  (REFUTED)', (255, 130, 130)),
        (op, 'ours, --road-opacity 0.326  (not shipped)', (255, 190, 80))]
out = Image.new('RGB', (4 * pw + 5 * gap, 56 + s * zf + capH + pw + capH + gap),
                (16, 16, 16))
d = ImageDraw.Draw(out)
d.text((gap, 8), 'The far road at Sanctuary, chunk (-20,20). The window is the '
       '96x96 with the MOST terrain-material texels in it, chosen by the '
       'measurement. Top 96x96 at 4:1, bottom the whole sheet.', font=F,
       fill=(200, 200, 200))
d.text((gap, 28), 'Terrain-material shapes (materials/Landscape/Ground/) win '
       '8,337 of 23,116 road texels here. Taking them out (panel 3) does not '
       'flatten the seam -- it doubles it: 15.387 -> 33.352 against vanilla 5.362.',
       font=F, fill=(200, 200, 200))
for i, (a, title, tint) in enumerate(cols):
    x = gap + i * (pw + gap)
    crop = Image.fromarray(u8(a)[y0:y1, x0:x1]).resize((s * zf, s * zf),
                                                       Image.NEAREST)
    out.paste(crop, (x + (pw - s * zf) // 2, 56))
    d.text((x, 56 + s * zf + 6), title[:64], font=F, fill=tint)
    L = R3.L(a)
    gy, gx = np.gradient(L)
    gm = np.hypot(gx, gy)
    edge = np.zeros_like(isT)
    for dj, di in ((0, 1), (0, -1), (1, 0), (-1, 0)):
        nb = np.roll(np.roll(isT, dj, axis=0), di, axis=1)
        edge |= isT & ~nb & np.roll(np.roll(road0, dj, axis=0), di, axis=1)
    d.text((x, 56 + s * zf + 26),
           'road L %.2f   terrain L %.2f   surface L %.2f'
           % (L[road0].mean(), L[isT].mean(), L[road0 & ~isT].mean()),
           font=F, fill=tint)
    d.text((x, 56 + s * zf + 44),
           'seam gradient %.3f  (two-tone step %.2f)'
           % (gm[edge].mean(), L[road0 & ~isT].mean() - L[isT].mean()),
           font=F, fill=tint)
    yb = 56 + s * zf + capH
    out.paste(Image.fromarray(u8(a)), (x, yb))
    d.rectangle([x + x0, yb + y0, x + x1, yb + y1], outline=tint)
p1 = os.path.join(IMG, 'road_ground_look.png')
out.save(p1)
print(p1, out.size)

# --------------------------------------------------------------- picture 2
sc = 2
big = Image.new('RGB', (2 * pw * sc + 3 * gap, 40 + pw * sc + 30), (16, 16, 16))
d = ImageDraw.Draw(big)
d.text((gap, 8), 'WHERE the terrain-material shapes are. Left: our default '
       'bake. Right: the same sheet with the terrain-material texels in red '
       'and the road surface in blue -- both are the ROAD PLANE.', font=F,
       fill=(200, 200, 200))
d.text((gap, 24), 'They are the verge and the junction fill, modelled inside '
       'the road NIFs (SancRoadStr01.nif and siblings) and materialled from '
       'materials/Landscape/Ground/.', font=F, fill=(200, 200, 200))
left = Image.fromarray(u8(d0)).resize((pw * sc, pw * sc), Image.NEAREST)
ov = u8(d0).copy()
ov[isT] = (ov[isT] * 0.35 + np.array([255, 60, 60]) * 0.65).astype(np.uint8)
sel = road0 & ~isT
ov[sel] = (ov[sel] * 0.45 + np.array([80, 140, 255]) * 0.55).astype(np.uint8)
right = Image.fromarray(ov).resize((pw * sc, pw * sc), Image.NEAREST)
big.paste(left, (gap, 40))
big.paste(right, (2 * gap + pw * sc, 40))
p2 = os.path.join(IMG, 'road_ground_where.png')
big.save(p2)
print(p2, big.size)

# --------------------------------------------------------------- picture 3
W, H = 1100, 620
ch = Image.new('RGB', (W, H), (16, 16, 16))
d = ImageDraw.Draw(ch)
d.text((14, 8), 'The cross-road profile on chunk (-20,20): mean luminance '
       'against signed distance to the painted road edge (+ inside).', font=F,
       fill=(200, 200, 200))
d.text((14, 26), 'Vanilla is read on the DEFAULT bake\'s own distance axis, so '
       'every curve is the same texels. The number in the key is the largest '
       'second difference over the road (the brief\'s G2).', font=F,
       fill=(200, 200, 200))
series = [('VANILLA', van, (120, 220, 120)),
          ('default (detail 1)', d0, (255, 255, 255)),
          ('--road-ground-paint 0', gp0, (255, 130, 130)),
          ('--road-opacity 0.326', op, (255, 190, 80)),
          ('our ground (--no-roads)', ground, (130, 130, 170))]
sd = R3.signed_dist(mask)
curves = []
for name, a, col in series:
    pf = R3.profile(R3.L(a), sd)
    p2v, at = R3.second_difference(pf)
    curves.append((name, pf, col, p2v))
xs = sorted(set(k for _, pf, _, _ in curves for k in pf))
lo, hi = min(xs), max(xs)
vals = [v[0] for _, pf, _, _ in curves for v in pf.values()]
ymin, ymax = min(vals) - 4, max(vals) + 4
L_, Rr, T_, B_ = 70, W - 330, 60, H - 46


def px(x):
    return L_ + (x - lo) / float(hi - lo) * (Rr - L_)


def py(y):
    return B_ - (y - ymin) / float(ymax - ymin) * (B_ - T_)


d.rectangle([L_, T_, Rr, B_], outline=(70, 70, 70))
for g in range(0, 6):
    yv = ymin + (ymax - ymin) * g / 5.0
    d.line([L_, py(yv), Rr, py(yv)], fill=(40, 40, 40))
    d.text((14, py(yv) - 7), '%6.1f' % yv, font=F, fill=(150, 150, 150))
for xv in range(lo, hi + 1, 2):
    d.line([px(xv), T_, px(xv), B_], fill=(40, 40, 40))
    d.text((px(xv) - 8, B_ + 6), '%+d' % xv, font=F, fill=(150, 150, 150))
d.line([px(0), T_, px(0), B_], fill=(110, 110, 110))
for k, (name, pf, col, p2v) in enumerate(curves):
    pts = [(px(x), py(pf[x][0])) for x in sorted(pf)]
    d.line(pts, fill=col, width=2)
    d.text((Rr + 16, T_ + 8 + k * 34), name, font=F, fill=col)
    d.text((Rr + 16, T_ + 24 + k * 34), '  2nd difference %.3f' % p2v, font=F,
           fill=col)
d.text((L_, B_ + 24), 'texels from the road edge (+ inside the paint)', font=F,
       fill=(150, 150, 150))
p3 = os.path.join(IMG, 'road_ground_profile.png')
ch.save(p3)
print(p3, ch.size)
