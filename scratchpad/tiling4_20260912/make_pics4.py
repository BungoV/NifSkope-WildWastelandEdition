"""TILING4 -- the two pictures, on the SAME crop as cmp_tiling3.png.

bungo's sentence over cmp_tiling3.png's PROPOSAL panel was "looks pretty good,
but maybe it could use some improvement", and the improvement is the swirls --
the strain the warp needs in order to break the repeat.  So the picture has to
put the warp and the hex tiling side by side on the same ground, at the scale
the swirls live at (the crop) and at the scale he actually looks at a sheet
(1:1, the whole thing).

  cmp_tiling4.png   vanilla | TILING3's proposal (the warp) | this lane's
                    winner (the hex tiling), the SAME 128 texels at (224,96)
                    on chunk (-20,24) at 4x nearest neighbour, numbers burned
                    in (`ww-texel-picture`).
  sheet_tiling4.png the same three sheets WHOLE, at 1:1, 512 texels each: a
                    swirl is a 30-100 texel feature and a 128-texel crop can
                    hide one.

EVERY PANEL IS A REAL DDS OFF DISK, written by a real exe in gate F2:

    vanilla  Bethesda's shipped sheet
    warp     release/NifSkope.exe --land-sample warp    (== TILING3's bake,
             byte for byte -- gate F2 arm E)
    stoch    release/NifSkope.exe --land-sample stochastic  (the hex tiling)

Nothing here is modelled, and the two bakes differ only by that one word.

    python make_pics4.py  ->  images/cmp_tiling4.png, images/sheet_tiling4.png
"""
import json
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
T2 = os.path.join(os.path.dirname(HERE), 'tiling2_20260911')
T3 = os.path.join(os.path.dirname(HERE), 'tiling3_20260911')
SP = os.path.join(os.path.dirname(HERE), 'splat1_20260911')
for p in (HERE, T3, T2, SP):
    if p not in sys.path:
        sys.path.insert(0, p)
import splatlib as S                                          # noqa: E402
import t1_lib as T                                            # noqa: E402
import t4_lib as W                                            # noqa: E402

CX, CY = -20, 24
TILE = 't2024'
P = 341.3333 / 32.0
CROP, MAG = 128, 4
CELL = CROP * MAG
PAD, HEAD, CAP = 18, 168, 96
IMAGES = os.path.join(HERE, 'images')

try:
    F = ImageFont.truetype('consola.ttf', 15)
    FB = ImageFont.truetype('consolab.ttf', 19)
    FH = ImageFont.truetype('consolab.ttf', 24)
except Exception:
    F = FB = FH = ImageFont.load_default()


def bake_rgb(variant):
    p = os.path.join(HERE, 'out', variant, TILE, 'tex',
                     'Commonwealth.4.%d.%d.DDS' % (CX, CY))
    if not os.path.exists(p):
        raise SystemExit('REFUSED: %s is missing -- bake it before drawing it' % p)
    return S.Dds(p).level(0)[:, :, :3].astype(np.float64)


def van_rgb():
    return S.Dds(S.van_sheet(CX, CY)).level(0)[:, :, :3].astype(np.float64)


def L_of(a):
    return S.lum(np.dstack([a, np.full(a.shape[:2], 255.0)]))


def nums(a):
    """The four numbers this lane is graded on, measured on what is drawn."""
    L = L_of(a)
    vis, fl = T.tiling_visibility(L, P)
    r = W.swirl(L) / max(W.swirl_floor(L), 1e-9)
    return (float(vis), float(fl), float(T.hp_residual(L, r=2).std()),
            float(S.local_var(L).mean()), float(r))


def main():
    x, y = 224, 96
    arms = [('VANILLA   Commonwealth.4.%d.%d.DDS' % (CX, CY), van_rgb(),
             (225, 225, 230)),
            ('TILING3   --land-sample warp  (the proposal he saw)',
             bake_rgb('warp'), (255, 190, 120)),
            ('TILING4   --land-sample stochastic  (the hex tiling)',
             bake_rgb('stoch'), (150, 230, 150))]
    whole = [(t, a, c) + nums(a) for t, a, c in arms]
    crops = [(t, a[y:y + CROP, x:x + CROP], c) for t, a, c in arms]
    crops = [(t, a, c) + nums(a) for t, a, c in crops]

    # ---------------------------------------------------------- the crop image
    W_ = 3 * (CELL + PAD) + PAD
    H_ = HEAD + CELL + CAP + PAD + 46
    img = Image.new('RGB', (W_, H_), (26, 26, 30))
    d = ImageDraw.Draw(img)
    d.text((PAD, 12), 'The repeat broken without the swirls', font=FH,
           fill=(240, 240, 235))
    for i, ln in enumerate([
            'chunk (%d,%d), dim 4. The SAME %d x %d texels at (%d,%d) in every panel, %dx nearest'
            % (CX, CY, CROP, CROP, x, y, MAG),
            'neighbour -- the same window as cmp_tiling2.png and cmp_tiling3.png, the one TILING2`s',
            'instrument picked on the RUNG bake as where the repeat reads strongest.',
            'repeat = the repeat`s amplitude at 10.667 texels in 8-bit luminance units, lower better.',
            'grain  = the SD of everything finer than 5 texels; VANILLA`s value is the target.',
            'swirl r = orientation coherence of the 1-5 texel grain over the sheet`s own phase twin,',
            '          with the repeat notched out. 1.00 = no orientation beyond the twin`s.',
            'ALL THREE PANELS ARE REAL DDS OFF DISK. The two bakes differ by ONE switch word.']):
        d.text((PAD, 44 + i * 15), ln, font=F, fill=(170, 170, 180))

    vanV, vanHP, vanR = crops[0][3], crops[0][5], crops[0][7]
    wV, wHP, wR = crops[1][3], crops[1][5], crops[1][7]
    for k, (title, arr, col, vis, _fl, hp, lv, r) in enumerate(crops):
        px = PAD + k * (CELL + PAD)
        py = HEAD
        im = Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8), 'RGB')
        img.paste(im.resize((CELL, CELL), Image.NEAREST), (px, py))
        d.rectangle([px - 1, py - 1, px + CELL, py + CELL], outline=(70, 70, 78))
        d.text((px, py + CELL + 6), title, font=FB, fill=col)
        lines = ['repeat %.3f   grain %.3f   swirl r %.3f' % (vis, hp, r)]
        if k == 0:
            lines.append('the target on this crop. Whole sheet: repeat 0.201,')
            lines.append('grain 4.476, swirl r 1.994 (its own ceiling x1.20 = 2.393).')
        elif k == 1:
            lines.append('swirl r %.2fx vanilla`s on this crop -- the strain he saw.'
                         % (r / max(vanR, 1e-9)))
            lines.append('grain %+.0f%% of vanilla`s, repeat %.2fx vanilla`s.'
                         % (100 * (hp / max(vanHP, 1e-9) - 1), vis / max(vanV, 1e-9)))
        else:
            lines.append('swirl r %.2fx the warp`s on the same ground, at'
                         % (r / max(wR, 1e-9)))
            lines.append('repeat %.2fx the warp`s and grain %.2fx it.'
                         % (vis / max(wV, 1e-9), hp / max(wHP, 1e-9)))
        for i, ln in enumerate(lines):
            d.text((px, py + CELL + 30 + i * 17), ln, font=F, fill=(170, 170, 180))

    d.text((PAD, H_ - 30),
           'The hex tiling is `--land-sample stochastic` as of this lane, and it'
           ' is STILL OFF BY DEFAULT: on the fourteen',
           font=F, fill=(230, 200, 140))
    d.text((PAD, H_ - 14),
           'sheets of the frozen split it is 6 of 7 and 6 of 7 on the repeat, not'
           ' 7 of 7. The swirls went 2 of 7 -> 7 of 7 and 7 of 7.',
           font=F, fill=(230, 200, 140))
    os.makedirs(IMAGES, exist_ok=True)
    out1 = os.path.join(IMAGES, 'cmp_tiling4.png')
    img.save(out1)
    print('wrote %s  (%d x %d)' % (out1, W_, H_))

    # --------------------------------------------------- the whole-sheet image
    n = whole[0][1].shape[0]
    HEAD2, CAP2 = 132, 60
    W2 = 3 * (n + PAD) + PAD
    H2 = HEAD2 + n + CAP2 + PAD
    im2 = Image.new('RGB', (W2, H2), (26, 26, 30))
    d2 = ImageDraw.Draw(im2)
    d2.text((PAD, 12), 'The same three sheets WHOLE, at 1:1', font=FH,
            fill=(240, 240, 235))
    for i, ln in enumerate([
            'chunk (%d,%d), %d x %d texels each, no magnification: one screen texel per sheet texel.'
            % (CX, CY, n, n),
            'A swirl is a 30-100 texel feature, so a 128-texel crop can hide one and this image cannot.',
            'The numbers are the WHOLE-SHEET readings, which is what every gate in this lane is graded on;',
            'the crop image`s numbers are the crop`s and the two do not have to agree.']):
        d2.text((PAD, 46 + i * 15), ln, font=F, fill=(170, 170, 180))
    for k, (title, arr, col, vis, _fl, hp, lv, r) in enumerate(whole):
        px = PAD + k * (n + PAD)
        py = HEAD2
        im2.paste(Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8), 'RGB'),
                  (px, py))
        d2.rectangle([px - 1, py - 1, px + n, py + n], outline=(70, 70, 78))
        d2.text((px, py + n + 6), title.split('  ')[0] + '  ' + title.split('  ')[-1],
                font=FB, fill=col)
        d2.text((px, py + n + 30),
                'repeat %.3f  grain %.3f  swirl r %.3f  local var %.2f'
                % (vis, hp, r, lv), font=F, fill=(170, 170, 180))
    out2 = os.path.join(IMAGES, 'sheet_tiling4.png')
    im2.save(out2)
    print('wrote %s  (%d x %d)' % (out2, W2, H2))

    rec = dict(x=x, y=y, crop=CROP,
               crops=[dict(title=t, vis=v, floor=f, hp=h, lv=l, swirl_r=r)
                      for (t, _a, _c, v, f, h, l, r) in crops],
               whole=[dict(title=t, vis=v, floor=f, hp=h, lv=l, swirl_r=r)
                      for (t, _a, _c, v, f, h, l, r) in whole])
    json.dump(rec, open(os.path.join(HERE, 'pics4.json'), 'w'), indent=1)
    for w in rec['whole']:
        print('   whole %-52s repeat %.3f grain %.3f swirl %.3f'
              % (w['title'], w['vis'], w['hp'], w['swirl_r']))
    for c in rec['crops']:
        print('   crop  %-52s repeat %.3f grain %.3f swirl %.3f'
              % (c['title'], c['vis'], c['hp'], c['swirl_r']))


if __name__ == '__main__':
    main()
