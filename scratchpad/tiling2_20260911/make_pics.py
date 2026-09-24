"""TILING2 work item 6 -- the three pictures, in cmp_tiling_fixed.png's layout.

Same 2x2 grid, same 128-texel crop at 4x nearest neighbour, the SAME texels in
every panel of a picture, the crop chosen BY THE INSTRUMENT on the BEFORE
artefact, numbers burned in (`ww-texel-picture`). Every panel is a DDS file off
disk: nothing here is modelled.

    python make_pics.py            -> images/cmp_tiling2.png
                                      images/cmp_edges.png
                                      images/cmp_detail_spectrum.png
"""
import json
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(os.path.dirname(HERE), 'splat1_20260911'))
import splatlib as S                                          # noqa: E402
import t1_lib as T                                            # noqa: E402
from t3b_seam import seam                                     # noqa: E402

CX, CY, TILE = -20, 24, 't2024'
P = 341.3333 / 32.0
CROP, MAG = 128, 4
CELL_W = CELL_H = CROP * MAG
PAD, HEAD, CAP = 18, 142, 92
IMAGES = os.path.join(HERE, 'images')

try:
    F = ImageFont.truetype('consola.ttf', 15)
    FB = ImageFont.truetype('consolab.ttf', 19)
    FH = ImageFont.truetype('consolab.ttf', 24)
except Exception:
    F = FB = FH = ImageFont.load_default()


def rgb(variant, tile=TILE, cx=CX, cy=CY):
    p = os.path.join(HERE, 'out', variant, tile, 'tex',
                     'Commonwealth.4.%d.%d.DDS' % (cx, cy))
    return S.Dds(p).level(0)[:, :, :3].astype(np.float64)


def van_rgb(cx=CX, cy=CY):
    return S.Dds(S.van_sheet(cx, cy)).level(0)[:, :, :3].astype(np.float64)


def L_of(a):
    return S.lum(np.dstack([a, np.full(a.shape[:2], 255.0)]))


def vis_of(a):
    return T.tiling_visibility(L_of(a), P)


def sheet_page(name, head, sub, panels, note=None):
    W = 2 * (CELL_W + PAD) + PAD
    H = HEAD + 2 * (CELL_H + CAP + PAD) + PAD
    img = Image.new('RGB', (W, H), (26, 26, 30))
    d = ImageDraw.Draw(img)
    d.text((PAD, 12), head, font=FH, fill=(240, 240, 235))
    for i, ln in enumerate(sub):
        d.text((PAD, 44 + i * 18), ln, font=F, fill=(170, 170, 180))
    for k, (title, arr, lines, col, ticks) in enumerate(panels):
        cx = PAD + (k % 2) * (CELL_W + PAD)
        cy = HEAD + (k // 2) * (CELL_H + CAP + PAD)
        p = Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8), 'RGB')
        p = p.resize((CELL_W, CELL_H), Image.NEAREST)
        img.paste(p, (cx, cy))
        d.rectangle([cx - 1, cy - 1, cx + CELL_W, cy + CELL_H], outline=(70, 70, 78))
        for (ax, off) in ticks:
            if not (0 <= off < CROP):
                continue
            if ax == 'x':
                X = cx + off * MAG
                d.line([X, cy - 8, X, cy - 2], fill=(255, 120, 120), width=2)
                d.line([X, cy + CELL_H + 2, X, cy + CELL_H + 8],
                       fill=(255, 120, 120), width=2)
            else:
                Y = cy + off * MAG
                d.line([cx - 8, Y, cx - 2, Y], fill=(255, 120, 120), width=2)
                d.line([cx + CELL_W + 2, Y, cx + CELL_W + 8, Y],
                       fill=(255, 120, 120), width=2)
        d.text((cx, cy + CELL_H + 6), title, font=FB, fill=col)
        for i, ln in enumerate(lines):
            d.text((cx, cy + CELL_H + 30 + i * 17), ln, font=F, fill=(170, 170, 180))
    if note:
        d.text((PAD, H - 16), note, font=F, fill=(140, 140, 150))
    os.makedirs(IMAGES, exist_ok=True)
    out = os.path.join(IMAGES, name)
    img.save(out)
    print('wrote %s  (%d x %d)' % (out, W, H))
    return out


def worst_vis_window(a, step=32):
    """The 128-texel window of THIS sheet that shows the repeat strongest.
    128 texels = exactly 12 repeats, so the FFT bin stays integer."""
    L = L_of(a)
    best, bx, by = -1.0, 0, 0
    for y in range(0, 512 - CROP + 1, step):
        for x in range(0, 512 - CROP + 1, step):
            v, _f = T.tiling_visibility(L[y:y + CROP, x:x + CROP], P)
            if v > best:
                best, bx, by = v, x, y
    return bx, by, best


def worst_seam_crossing(a):
    """The quadrant crossing whose two lines carry the most gradient in THIS
    sheet: the 128 crop is centred on it, so both lines are inside the crop."""
    L = L_of(a)
    _mx, _av, r = seam(L)
    rx, ry = r[:7], r[7:]
    ix = int(np.argmax(rx)) + 1
    iy = int(np.argmax(ry)) + 1
    x = min(max(ix * 64 - CROP // 2, 0), 512 - CROP)
    y = min(max(iy * 64 - CROP // 2, 0), 512 - CROP)
    return x, y, ix * 64, iy * 64, rx[ix - 1], ry[iy - 1]


def main():
    van = van_rgb()
    rung = rgb('rung')
    avg = rgb('avg')
    avgb = rgb('avgblend')
    blend = rgb('blend')
    res = {}

    # ------------------------------------------------------ 1. cmp_tiling2.png
    bx, by, bv = worst_vis_window(rung)
    print('worst-repeat 128 window of the RUNG bake at (%d,%d), visibility %.3f'
          % (bx, by, bv))

    def cut(a):
        return a[by:by + CROP, bx:bx + CROP]

    vV, fV = vis_of(cut(van))
    vR, fR = vis_of(cut(rung))
    vA, fA = vis_of(cut(avgb))
    diff = np.clip(np.abs(cut(rung) - cut(avgb)) * 8.0, 0, 255)
    res['tiling'] = dict(x=bx, y=by, van=vV, vanFloor=fV, rung=vR, rungFloor=fR,
                         avgb=vA, avgbFloor=fA)
    sheet_page(
        'cmp_tiling2.png',
        'The landscape texture repeat, measured out',
        ['chunk (%d,%d), dim 4, 512 texels at 32 world units each. The same %d x %d texels'
         % (CX, CY, CROP, CROP),
         'at (%d,%d) in every panel, %dx nearest neighbour: the window where the RUNG bake'
         % (bx, by, MAG),
         'shows the repeat strongest (128 texels = exactly 12 repeats, integer FFT bin).',
         'vis = the repeat`s amplitude in 8-bit luminance units at 10.667 texels. A 128 crop',
         'holds 12 repeats, not 48, so ITS null floor is high -- the verdict is the sheet row.'],
        [('VANILLA  Commonwealth.4.%d.%d.DDS' % (CX, CY), cut(van),
          ['Bethesda`s shipped chunk sheet',
           'vis %.3f  own floor %.3f' % (vV, fV),
           'whole sheet: vis 0.201; worst of 22 shipped = 0.264'],
          (225, 225, 230), []),
         ('OURS  the rung (--land-sample footprint = default)', cut(rung),
          ['the footprint mip at the engine`s own repeat:',
           'vis %.3f  own floor %.3f  = %.1fx vanilla`s crop'
           % (vR, fR, vR / max(vV, 1e-9)),
           'whole sheet vis 1.037 = 3.9x vanilla`s worst of 22'],
          (255, 210, 120), []),
         ('OURS  --land-sample average --blend-edges quadrant', cut(avgb),
          ['the landscape diffuse`s 1x1 mip = the exact average',
           'over one repeat. vis %.3f  own floor %.3f' % (vA, fA),
           'whole sheet vis 0.092, under vanilla`s worst by 2.9x'],
          (150, 230, 150), []),
         ('|RUNG - average+blend|  x8', diff,
          ['what the switch removes, at 8x contrast: the repeat',
           'itself. Whole-sheet local variance 12.29 -> 4.84,',
           'and that is the cost -- see cmp_detail_spectrum.png.'],
          (225, 225, 230), [])],
        note='All four panels are DDS files off disk, baked by release/NifSkope.exe. '
             'Nothing here is modelled.')

    # ------------------------------------------------------- 2. cmp_edges.png
    ex, ey, lx, ly, rrx, rry = worst_seam_crossing(avg)
    print('worst quadrant crossing of the AVG bake at x=%d y=%d (ratios %.3f/%.3f); '
          'crop (%d,%d)' % (lx, ly, rrx, rry, ex, ey))

    def ecut(a):
        return a[ey:ey + CROP, ex:ex + CROP]

    ticks = [('x', lx - ex), ('y', ly - ey)]
    sV = seam(L_of(van))
    sR = seam(L_of(rung))
    sA = seam(L_of(avg))
    sB = seam(L_of(avgb))
    sF = seam(L_of(blend))
    ediff = np.clip(np.abs(ecut(avg) - ecut(avgb)) * 8.0, 0, 255)
    res['edges'] = dict(x=ex, y=ey, lx=lx, ly=ly, vanAvg=sV[1], vanMax=sV[0],
                        rungAvg=sR[1], avgAvg=sA[1], avgMax=sA[0],
                        avgbAvg=sB[1], avgbMax=sB[0], blendAvg=sF[1])
    sheet_page(
        'cmp_edges.png',
        'The quadrant grid: the hard blend edges, and the cross-fade',
        ['chunk (%d,%d). The same %d x %d texels at (%d,%d) in every panel, %dx nearest'
         % (CX, CY, CROP, CROP, ex, ey, MAG),
         'neighbour. RED TICKS, drawn outside the image, mark the quadrant borders at',
         'x=%d and y=%d -- the 2,048-world-unit lines where the layer set changes. The crop'
         % (lx, ly),
         'is centred on the crossing the AVERAGE bake reads worst. seam = mean |gradient|',
         'along the 14 interior quadrant lines / the sheet`s own. 1.000 = lines not special.'],
        [('VANILLA  Commonwealth.4.%d.%d.DDS' % (CX, CY), ecut(van),
          ['Bethesda`s shipped sheet: no grid.',
           'seam %.3f  (median of 22 = 1.041, worst = 1.100)' % sV[1],
           'worst single line %.3f' % sV[0]],
          (225, 225, 230), ticks),
         ('OURS  --land-sample average  (no cross-fade)', ecut(avg),
          ['with each quadrant flat, the ONLY edges left in the',
           'sheet are the quadrant lines: seam %.3f,' % sA[1],
           'worst single line %.3f -- outside vanilla`s law' % sA[0]],
          (255, 140, 140), ticks),
         ('OURS  --land-sample average --blend-edges quadrant', ecut(avgb),
          ['the neighbour`s composite cross-faded over 128 world',
           'units either side, quintic, exactly 0.5 on the line:',
           'seam %.3f, worst line %.3f -- below vanilla`s median'
           % (sB[1], sB[0])],
          (150, 230, 150), ticks),
         ('|average - average+blend|  x8', ediff,
          ['the cross-fade`s whole footprint at 8x contrast: two',
           '4-texel strips and nothing else. On the rung`s own',
           'sampling it reads seam %.3f (was %.3f).' % (sF[1], sR[1])],
          (225, 225, 230), ticks)],
        note='All four panels are DDS files off disk. The ticks are drawn outside the '
             'image, never over it.')

    # -------------------------------------------- 3. cmp_detail_spectrum.png
    k15, k25, k50 = rgb('k0.15'), rgb('k0.25'), rgb('k0.50')
    vanL = L_of(van)

    def full(a):
        L = L_of(a)
        v, f = T.tiling_visibility(L, P)
        _c, _p, bt = T.bands(L)
        tot = sum(x for _n, x in bt)
        return dict(vis=v, lv=float(S.local_var(L).mean()),
                    fine=100.0 * sum(x for _n, x in bt[3:]) / tot,
                    spec=T.spectrum_distance(vanL, L))

    mV, m0, m25, m50 = full(van), full(avgb), full(k25), full(k50)
    res['detail'] = dict(van=mV, k0=m0, k25=m25, k50=m50, k15=full(k15))
    print(json.dumps(res['detail'], indent=1))
    sheet_page(
        'cmp_detail_spectrum.png',
        'The blur: what the detail knob buys, and what it costs',
        ['chunk (%d,%d). The same %d x %d texels at (%d,%d) in every panel, %dx nearest'
         % (CX, CY, CROP, CROP, bx, by, MAG),
         'neighbour -- the same crop as cmp_tiling2.png. Every number below is measured on',
         'the WHOLE sheet. locVar = 3x3 local variance of luminance; fine% = the share of',
         'the sheet`s variance finer than 8 texels; specD = spectrum distance to vanilla.',
         'vanilla`s repeat ceiling is 0.264 (worst of 22 shipped); the knob crosses it at 0.246.'],
        [('VANILLA  Commonwealth.4.%d.%d.DDS' % (CX, CY), cut(van),
          ['locVar %.2f   fine %.1f%%   vis %.3f' % (mV['lv'], mV['fine'], mV['vis']),
           'NO candidate explains this detail: ten sources,',
           '|r| <= 0.006 against floors of the same size (sec 1c)'],
          (225, 225, 230), []),
         ('OURS  --land-detail 0  (the average; knob default)', cut(avgb),
          ['locVar %.2f   fine %.1f%%   vis %.3f   specD %.3f'
           % (m0['lv'], m0['fine'], m0['vis'], m0['spec']),
           'the repeat is gone and so is the detail: %.0f%% of'
           % (100 * m0['lv'] / mV['lv']),
           'vanilla`s local variance. THIS IS STILL RED.'],
          (150, 230, 150), []),
         ('OURS  --land-detail 0.25', cut(k25),
          ['locVar %.2f   fine %.1f%%   vis %.3f   specD %.3f'
           % (m25['lv'], m25['fine'], m25['vis'], m25['spec']),
           'past vanilla`s repeat ceiling (%.3f > 0.264) and it' % m25['vis'],
           'has bought %.2f local-variance levels of the ~15' % (m25['lv'] - m0['lv'])],
          (255, 210, 120), []),
         ('OURS  --land-detail 0.50', cut(k50),
          ['locVar %.2f   fine %.1f%%   vis %.3f   specD %.3f'
           % (m50['lv'], m50['fine'], m50['vis'], m50['spec']),
           'twice vanilla`s repeat, for %.2f levels. The repeat'
           % (m50['lv'] - m0['lv']),
           'and the texture`s detail ARE THE SAME SIGNAL.'],
          (255, 140, 140), [])],
        note='All four panels are DDS files off disk. The detail term is REFUSED as a '
             'fix; the knob is the priced trade.')

    json.dump(res, open(os.path.join(HERE, 'pics.json'), 'w'), indent=1)


if __name__ == '__main__':
    main()
