"""LAND1 Part A -- the pictures (ww-texel-picture).

SIX PANELS, the same ground in every one, on the window warp_sweep.py already
uses so the comparison lines up with the one bungo has already looked at:
x0=300, y0=40, 128 texels, 3:1 nearest neighbour, plus the WHOLE sheet at 1:1
underneath because a swirl is a 30-100 texel feature and a 128-texel crop can
hide one.

    vanilla         Bethesda's shipped sheet
    plain           the rung: --road-detail 1 and nothing else (the DEFAULT,
                    and what ships unless bungo rules otherwise)
    best of (a)     --land-guide drag:341           DOWNHILL DRAG
    best of (b)     --land-guide aspecthex:1.0 --land-guide-scale 256 on the hex
                    tiling                          ASPECT ROTATION  (the winner)
    best of (c)     --land-guide flatwarp:1.0 --land-warp 341 on the hex tiling
                                                    SLOPE-MODULATED HASH WARP
    warp 683        --land-sample warp -- TILING3's shipped preset, the one he
                    called too strong ("the warp is too strong, what is it set
                    to?" -- 683 units = two repeats)

EVERY PANEL IS A REAL DDS OFF DISK, written by the real exe, and every bake
carries --road-detail 1.  Nothing here is modelled.

Two sheets get the same six panels:
    (-20,20)   the FLATTEST of the selection seven, macro tan 0.0760 (4.35 deg)
    (20,-24)   the STEEPEST of the validation seven, macro tan 0.3467 (19.1 deg)
-- chosen by the measured macro slope, not by eye, so the pair answers "what do
these rules do where there is no slope to steer by, and where there is a lot".

    python a_pics.py  ->  images/a_land_guide_flat.png, images/a_land_guide_slope.png
"""
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
T4 = os.path.join(os.path.dirname(HERE), 'tiling4_20260912')
T3 = os.path.join(os.path.dirname(HERE), 'tiling3_20260911')
T2 = os.path.join(os.path.dirname(HERE), 'tiling2_20260911')
SP = os.path.join(os.path.dirname(HERE), 'splat1_20260911')
for p in (T4, T3, T2, SP):
    if p not in sys.path:
        sys.path.insert(0, p)
import splatlib as S                                          # noqa: E402
import t1_lib as T                                            # noqa: E402
import t4_lib as W                                            # noqa: E402

P = 341.3333 / 32.0
X0, Y0, CROP, MAG = 300, 40, 128, 3
PAD = 12
IMAGES = os.path.join(HERE, 'images')
SHEET = 512

try:
    F = ImageFont.truetype('consola.ttf', 14)
    FB = ImageFont.truetype('consolab.ttf', 16)
    FH = ImageFont.truetype('consolab.ttf', 24)
except Exception:
    F = FB = FH = ImageFont.load_default()

ARMS = [
    ('vanilla', None, 'VANILLA  Bethesda`s own sheet', (225, 225, 230)),
    ('p_plain', '--road-detail 1 (nothing else)',
     'PLAIN  the default, and what still ships', (255, 255, 255)),
    ('p_drag', '--land-guide drag:341',
     '(a) DOWNHILL DRAG  best of its family', (255, 150, 150)),
    ('p_asp', '--land-guide aspecthex:1.0 --land-guide-scale 256 + hex 256',
     '(b) ASPECT ROTATION  THE WINNER', (150, 230, 150)),
    ('p_warp', '--land-guide flatwarp:1.0 --land-warp 341 + hex 256',
     '(c) SLOPE-MODULATED HASH WARP  best of its family', (150, 200, 255)),
    ('p_683', '--land-sample warp  (683 units = two repeats)',
     'THE SHIPPED WARP  the one he called too strong', (255, 190, 120)),
]


def load(variant, cx, cy):
    if variant == 'vanilla':
        return S.Dds(S.van_sheet(cx, cy)).level(0)[:, :, :3].astype(np.float64)
    tag = 'r_%d_%d_%d_%d' % (cx, cy, cx + 3, cy + 3)
    p = os.path.join(HERE, 'out', variant, tag, 'tex',
                     'Commonwealth.4.%d.%d.DDS' % (cx, cy))
    if not os.path.exists(p):
        raise SystemExit('REFUSED: %s is missing -- bake it before drawing it' % p)
    return S.Dds(p).level(0)[:, :, :3].astype(np.float64)


def L_of(a):
    return S.lum(np.dstack([a, np.full(a.shape[:2], 255.0)]))


def nums(a):
    L = L_of(a)
    vis, _fl = T.tiling_visibility(L, P)
    r = W.swirl(L) / max(W.swirl_floor(L), 1e-9)
    return float(vis), float(T.hp_residual(L, r=2).std()), float(r)


def u8(a):
    return np.clip(a, 0, 255).astype(np.uint8)


def draw(cx, cy, slope_deg, what, out_name):
    arms = []
    for v, cmdline, title, col in ARMS:
        a = load(v, cx, cy)
        arms.append((v, cmdline, title, col, a) + nums(a))
    cw = CROP * MAG
    panel = max(cw, SHEET)
    head = 150
    cap1 = 26
    cap2 = 74
    Wd = len(arms) * (panel + PAD) + PAD
    Ht = head + cw + cap1 + SHEET + cap2 + PAD
    img = Image.new('RGB', (Wd, Ht), (22, 22, 26))
    d = ImageDraw.Draw(img)
    d.text((PAD, 12),
           'Terrain-guided land sampling, chunk (%d,%d) -- %s' % (cx, cy, what),
           font=FH, fill=(240, 240, 235))
    lines = [
        'Macro slope of this chunk, measured from the heightmap at scale 1024: median tan %.4f = %.2f degrees. It was chosen by that number, not by eye.'
        % (np.tan(np.radians(slope_deg)), slope_deg),
        'TOP: the same %d x %d texels at (%d,%d) in every panel, %d:1 nearest neighbour -- warp_sweep.py`s own window, so this lines up with the picture already seen.'
        % (CROP, CROP, X0, Y0, MAG),
        'BOTTOM: the WHOLE %d-texel sheet at 1:1, because a swirl is a 30-100 texel feature and a 128-texel crop can hide one. The box marks the crop.' % SHEET,
        '',
        'repeat = amplitude of the 10.667-texel land-texture repeat in 8-bit luminance units. LOWER IS BETTER; the gate is 0.264 (or this sheet`s own no-repeat control).',
        'grain  = SD of everything finer than 5 texels. VANILLA`s value is the target, not zero.   swirl r = orientation coherence over the sheet`s own phase twin, repeat notched out; 1.00 = none.',
        'EVERY PANEL IS A REAL DDS OFF DISK, written by release/NifSkope.exe, and every bake carries --road-detail 1. Nothing is modelled.',
    ]
    for i, ln in enumerate(lines):
        d.text((PAD, 46 + i * 15), ln, font=F, fill=(170, 170, 180))

    van = arms[0]
    for k, (v, cmdline, title, col, a, vis, hp, r) in enumerate(arms):
        px = PAD + k * (panel + PAD)
        crop = Image.fromarray(u8(a)[Y0:Y0 + CROP, X0:X0 + CROP]).resize(
            (cw, cw), Image.NEAREST)
        img.paste(crop, (px + (panel - cw) // 2, head))
        d.rectangle([px + (panel - cw) // 2 - 1, head - 1,
                     px + (panel - cw) // 2 + cw, head + cw],
                    outline=(70, 70, 78))
        d.text((px, head + cw + 5), title, font=FB, fill=col)
        yb = head + cw + cap1
        img.paste(Image.fromarray(u8(a)), (px + (panel - SHEET) // 2, yb))
        d.rectangle([px + (panel - SHEET) // 2 + X0, yb + Y0,
                     px + (panel - SHEET) // 2 + X0 + CROP, yb + Y0 + CROP],
                    outline=col)
        cl = ['repeat %.3f   grain %.3f   swirl r %.3f' % (vis, hp, r)]
        if k == 0:
            cl.append('the target. Every number below is against this row.')
            cl.append('(vanilla`s own sheet, untouched)')
        else:
            cl.append('repeat %.2fx vanilla`s, grain %+.0f%% of it, swirl r %.2fx it.'
                      % (vis / max(van[5], 1e-9),
                         100 * (hp / max(van[6], 1e-9) - 1), r / max(van[7], 1e-9)))
            cl.append(cmdline)
        for i, ln in enumerate(cl):
            d.text((px, yb + SHEET + 6 + i * 16), ln, font=F,
                   fill=(180, 180, 190) if i else col)
    os.makedirs(IMAGES, exist_ok=True)
    out = os.path.join(IMAGES, out_name)
    img.save(out)
    print('wrote %s  (%d x %d)' % (out, Wd, Ht))
    return [(v, vis, hp, r) for v, _c, _t, _co, _a, vis, hp, r in arms]


def main():
    rows = {}
    rows['flat'] = draw(-20, 20, 4.35,
                        'the FLATTEST of the selection seven',
                        'a_land_guide_flat.png')
    rows['slope'] = draw(20, -24, 19.12,
                         'the STEEPEST of the validation seven',
                         'a_land_guide_slope.png')
    print()
    print('%-10s %-10s %8s %8s %8s' % ('sheet', 'arm', 'repeat', 'grain', 'swirl r'))
    for k in ('flat', 'slope'):
        for v, vis, hp, r in rows[k]:
            print('%-10s %-10s %8.3f %8.3f %8.3f' % (k, v, vis, hp, r))
    return 0


if __name__ == '__main__':
    sys.exit(main())
