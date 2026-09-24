"""TILING3 -- the picture bungo can read, on the SAME crop as cmp_tiling2.png.

bungo's two complaints are one sentence each, and they pull opposite ways:

    the rung   "you can see the tiling pattern of each texture, which is not
                good ... it's muddy or blurry looking"
    `average`  "it lost all the texture to it, now it's only solid color blobs"

So the picture has to show a panel that has neither, and it has to make the
trade visible rather than assert it.  Four panels, chunk (-20,24), the SAME 128
texels at (224,96) -- the window TILING2's instrument picked on the RUNG bake as
the one where the repeat reads strongest -- at 4x nearest neighbour, numbers
burned in (`ww-texel-picture`).

EVERY PANEL IS A REAL DDS OFF DISK, written by a real exe.  An earlier revision
of this script modelled the last two panels offline because Fallout4.exe was up
and the build could not be spent; the build has since been spent, so the model
is gone and with it the "compare down a row, never across" caveat.  Nothing in
this image is a prediction.

    vanilla  Bethesda's shipped sheet
    rung     release/NifSkope.before_tiling3.exe -- today's bake
    ship     the new exe at its SHIPPED DEFAULTS: vanilla's `_msn` byte for
             byte, and the crevice term on our colour composite
    stoch    those defaults plus `--land-sample stochastic` -- the PROPOSAL,
             off by default, because its 7-sheet selection scored 6 of 7

    python make_pics3.py  ->  images/cmp_tiling3.png
"""
import json
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
T2 = os.path.join(os.path.dirname(HERE), 'tiling2_20260911')
SP = os.path.join(os.path.dirname(HERE), 'splat1_20260911')
for p in (HERE, T2, SP):
    sys.path.insert(0, p)
import splatlib as S                                          # noqa: E402
import t1_lib as T                                            # noqa: E402

CX, CY = -20, 24
TILE = 't2024'
P = 341.3333 / 32.0
CROP, MAG = 128, 4
CELL_W = CELL_H = CROP * MAG
PAD, HEAD, CAP = 18, 150, 96
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
    """The three numbers this lane is graded on, on THIS crop."""
    L = L_of(a)
    vis, fl = T.tiling_visibility(L, P)
    return vis, fl, float(T.hp_residual(L, r=2).std()), float(S.local_var(L).mean())


def main():
    x, y = 224, 96

    def cut(a):
        return a[y:y + CROP, x:x + CROP]

    panels = []
    for title, arr, col in (
            ('VANILLA   Commonwealth.4.%d.%d.DDS' % (CX, CY), cut(van_rgb()), (225, 225, 230)),
            ('THE RUNG  today`s bake', cut(bake_rgb('rung')), (255, 190, 120)),
            ('SHIPPED   the new default', cut(bake_rgb('ship')), (150, 210, 255)),
            ('PROPOSAL  + --land-sample stochastic', cut(bake_rgb('stoch')), (150, 230, 150))):
        vis, fl, hp, lv = nums(arr)
        panels.append((title, arr, col, vis, fl, hp, lv))
    vanV, vanHP = panels[0][3], panels[0][5]
    rungV, rungHP = panels[1][3], panels[1][5]

    W = 2 * (CELL_W + PAD) + PAD
    H = HEAD + 2 * (CELL_H + CAP + PAD) + PAD + 34
    img = Image.new('RGB', (W, H), (26, 26, 30))
    d = ImageDraw.Draw(img)
    d.text((PAD, 12), 'The grain without the repeat', font=FH, fill=(240, 240, 235))
    for i, ln in enumerate([
            'chunk (%d,%d), dim 4. The SAME %d x %d texels at (%d,%d) in every panel, %dx nearest'
            % (CX, CY, CROP, CROP, x, y, MAG),
            'neighbour -- the window TILING2`s instrument picked on the RUNG bake as the one where',
            'the repeat reads strongest. 128 texels = exactly 12 repeats, so the FFT bin is integer.',
            'repeat = the repeat`s amplitude at 10.667 texels in 8-bit luminance units (lower is better);',
            'grain = the SD of everything finer than 5 texels (vanilla`s value is the target, not zero).',
            'ALL FOUR PANELS ARE REAL DDS OFF DISK, written by a real exe. Nothing here is modelled,',
            'and the three bakes differ only by the switch named under each one.']):
        d.text((PAD, 44 + i * 15), ln, font=F, fill=(170, 170, 180))

    for k, (title, arr, col, vis, fl, hp, lv) in enumerate(panels):
        px = PAD + (k % 2) * (CELL_W + PAD)
        py = HEAD + (k // 2) * (CELL_H + CAP + PAD)
        im = Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8), 'RGB')
        im = im.resize((CELL_W, CELL_H), Image.NEAREST)
        img.paste(im, (px, py))
        d.rectangle([px - 1, py - 1, px + CELL_W, py + CELL_H], outline=(70, 70, 78))
        d.text((px, py + CELL_H + 6), title, font=FB, fill=col)
        lines = ['repeat %.3f   grain %.3f   local var %.2f' % (vis, hp, lv)]
        if k == 0:
            lines.append('the target: whole sheet repeat 0.201, worst of 22 shipped 0.264')
            lines.append('whole-sheet grain 4.476, local variance 19.81')
        elif k == 1:
            lines.append('the repeat bungo can see: %.1fx vanilla`s on this crop,' % (vis / max(vanV, 1e-9)))
            lines.append('and the grain is %+.0f%% of vanilla`s -- "muddy or blurry"'
                         % (100 * (hp / max(vanHP, 1e-9) - 1)))
        elif k == 2:
            lines.append('vanilla`s `_msn` byte for byte + the crevice term. The repeat is')
            lines.append('UNTOUCHED here on purpose: %.3f against the rung`s %.3f' % (vis, rungV))
        else:
            lines.append('repeat %.2fx the rung`s, grain %.2fx it -- whole sheet 1.037 ->'
                         % (vis / max(rungV, 1e-9), hp / max(rungHP, 1e-9)))
            lines.append('0.183, grain 3.342 -> 4.981 against vanilla`s 4.476')
        for i, ln in enumerate(lines):
            d.text((px, py + CELL_H + 30 + i * 17), ln, font=F, fill=(170, 170, 180))

    d.text((PAD, H - 30),
           'The PROPOSAL is OFF by default. Its 7-sheet selection scored 6 of 7 and this'
           ' lane does not ship a 6-of-7 as a pass:',
           font=F, fill=(230, 200, 140))
    d.text((PAD, H - 14),
           'one chunk, (-36,-20), keeps a repeat of 0.095 with a ratio of 0.698 against a'
           ' 0.448 ceiling. `--land-sample stochastic` turns it on.',
           font=F, fill=(230, 200, 140))

    os.makedirs(IMAGES, exist_ok=True)
    out = os.path.join(IMAGES, 'cmp_tiling3.png')
    img.save(out)
    print('wrote %s  (%d x %d)' % (out, W, H))
    rec = dict(x=x, y=y, crop=CROP,
               panels=[dict(title=t, vis=v, floor=f, hp=h, lv=l)
                       for (t, _a, _c, v, f, h, l) in panels])
    json.dump(rec, open(os.path.join(HERE, 'pics3.json'), 'w'), indent=1)
    for p in rec['panels']:
        print('   %-42s repeat %.3f grain %.3f locVar %.2f'
              % (p['title'], p['vis'], p['hp'], p['lv']))


if __name__ == '__main__':
    main()
