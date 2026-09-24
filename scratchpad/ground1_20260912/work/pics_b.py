"""Lane GROUND1 Part B pictures.

Every panel is a real baked sheet off disk, read through the SAME code the gate
read it with (splatlib's DDS decoder, b1_vanilla's gradient field), so the
picture and the number in its caption cannot disagree.

  b_cmp_erosion_msn.png     the normal sheet's FINE BAND -- the relief finer
                            than four texels, which is the band our 128-u height
                            grid cannot carry and the band F1 measured vanilla's
                            in -- shaded by one fixed sun:
                              off | on | vanilla | what the pass itself added
  b_cmp_erosion_colour.png  the colour sheet:
                              off | on | the difference x8 | vanilla
  b_cmp_erosion_lit.png     the colour sheet LIT BY ITS OWN NORMAL SHEET, which
                            is what the ground is: off | on | vanilla. The
                            lighting is stated arithmetic, not a renderer -- see
                            section B5 of the report for the render that was
                            tried and what it measured.

The sun, the exaggeration and the amplification are the same in every panel of a
picture and are written into its title, because a picture whose panels are
stretched differently is an argument, not a measurement.
"""
import glob
import os
import re
import sys

import numpy as np
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, r'E:/Projects/NifskopeWildWastelandEdition/scratchpad/splat1_20260911')
import splatlib as S                                              # noqa: E402
import b1_vanilla as B                                            # noqa: E402

OUT = os.path.join(os.path.dirname(HERE), 'images')

# the sun, fixed for every panel of every picture here
AZ_DEG = 315.0      # from the north-west
EL_DEG = 28.0
RELIEF = 1.0        # gradient exaggeration before shading
COLDIFF = 8         # colour difference amplification

NAME = re.compile(r'Commonwealth\.(\d+)\.(-?\d+)\.(-?\d+)(_msn|_data)?\.DDS$', re.I)


def sheets(root, suffix):
    out = {}
    for p in sorted(glob.glob(os.path.join(root, 'tex', '*.DDS'))):
        m = NAME.search(os.path.basename(p))
        if m and (m.group(4) or '') == suffix:
            out[(int(m.group(2)), int(m.group(3)))] = p
    return out


def fine_gradient(path):
    """The gradient field with everything coarser than four texels removed."""
    gx, gy, _ = B.gradient_field(path)
    cx, cy, fx, fy = B.split(gx, gy)
    return fx, fy, gx, gy


def shade(gx, gy, k=RELIEF):
    """Lambert of the surface whose height gradient is (gx, gy), one fixed sun."""
    az, el = np.radians(AZ_DEG), np.radians(EL_DEG)
    lx, ly, lz = np.cos(el) * np.sin(az), np.cos(el) * np.cos(az), np.sin(el)
    nx, ny, nz = -gx * k, -gy * k, np.ones_like(gx)
    n = np.sqrt(nx * nx + ny * ny + nz * nz)
    d = (nx * lx + ny * ly + nz * lz) / n
    # the flat ground of a fine band sits at d = sin(el); rescale so flat is mid
    # grey and the same stretch is used in every panel
    v = 0.5 + (d - np.sin(el)) * 2.2
    return np.clip(v * 255.0, 0, 255).astype(np.uint8)


def fine_sd(path):
    fx, fy, _, _ = fine_gradient(path)
    return float(np.sqrt(fx.var() + fy.var()))


def fine_lum(path):
    a = S.Dds(path).level(0).astype(np.float64) / 255.0
    lum = 0.2126 * a[..., 0] + 0.7152 * a[..., 1] + 0.0722 * a[..., 2]
    return float((lum - S._box(lum, 2)).std())


def grid(name, title, rows, caps, pad=64, note=''):
    """rows = [[img, img, ...], ...]; every row the same column count."""
    h, w = rows[0][0].shape[:2]
    gap = pad // 2
    n = len(rows[0])
    rh = [r[0].shape[0] for r in rows]
    out = Image.new('RGB',
                    (gap + n * (w + gap), pad + sum(x + gap for x in rh) + 8),
                    (16, 16, 16))
    y = pad
    for r, hh in zip(rows, rh):
        for k, p in enumerate(r):
            im = Image.fromarray(p)
            if im.mode != 'RGB':
                im = im.convert('RGB')
            out.paste(im, (gap + k * (w + gap), y))
        y += hh + gap
    d = ImageDraw.Draw(out)
    d.text((6, 5), title, fill=(235, 235, 235))
    if note:
        import textwrap
        lines = textwrap.wrap(note, max(40, (out.width - 14) // 6))[:2]
        for i, ln in enumerate(lines):
            d.text((6, 20 + i * 12), ln, fill=(170, 170, 170))
    for k, t in enumerate(caps):
        d.text((gap + k * (w + gap) + 2, pad - 14), t, fill=(235, 235, 235))
    p = os.path.join(OUT, name)
    out.save(p)
    print('%s  %dx%d  %d bytes' % (p, out.width, out.height, os.path.getsize(p)))


def crop4(a, x0, y0, side=170, mag=3):
    """A square off the same place in every panel, magnified nearest-neighbour."""
    c = a[y0:y0 + side, x0:x0 + side]
    im = Image.fromarray(c)
    if im.mode != 'RGB':
        im = im.convert('RGB')
    return np.asarray(im.resize((side * mag, side * mag), Image.NEAREST))


def main(pick=None):
    off_m = sheets(os.path.join(HERE, 'f4_off'), '_msn')
    on_m = sheets(os.path.join(HERE, 'f4_on'), '_msn')
    off_c = sheets(os.path.join(HERE, 'f4_off'), '')
    on_c = sheets(os.path.join(HERE, 'f4_on'), '')

    # pick the chunk by the STEEPEST coarse ground, stated rather than eyeballed
    print('%-10s %9s %9s %9s %9s' % ('chunk', 'coarse|g|', 'fineSD off',
                                     'fineSD on', 'vanilla'))
    best, bestv = None, -1.0
    for key in sorted(on_m):
        gx, gy, _ = B.gradient_field(on_m[key])
        cm = float(np.sqrt(gx * gx + gy * gy).mean())
        vp = S.van_sheet(key[0], key[1], '_msn')
        v = fine_sd(vp) if os.path.exists(vp) else float('nan')
        print('%-10s %9.4f %9.4f %9.4f %9.4f'
              % ('%d,%d' % key, cm, fine_sd(off_m[key]), fine_sd(on_m[key]), v))
        if cm > bestv:
            best, bestv = key, cm
    key = best if pick is None else tuple(int(x) for x in pick.split(','))
    print('\nchunk %d,%d  (steepest coarse ground of the eight)' % key)

    vm = S.van_sheet(key[0], key[1], '_msn')
    vc = S.van_sheet(key[0], key[1], '')

    fo = fine_gradient(off_m[key])
    fn = fine_gradient(on_m[key])
    fv = fine_gradient(vm)
    dx, dy = fn[0] - fo[0], fn[1] - fo[1]

    CX, CY = 170, 170           # the same 170-texel square in every panel
    msn = [shade(*fo[:2]), shade(*fn[:2]), shade(fv[0], fv[1]), shade(dx, dy)]
    grid('b_cmp_erosion_msn.png',
         'the normal sheet, relief FINER than 4 texels (finer than the 128-u height grid), '
         'one sun az %d el %d, the same stretch in every panel -- '
         'Commonwealth dim 4 chunk %d,%d, --road-detail 1'
         % (AZ_DEG, EL_DEG, key[0], key[1]),
         [msn, [crop4(m, CX, CY) for m in msn]],
         ['ours, --erosion 0   (fine SD %.4f)' % fine_sd(off_m[key]),
          'ours, --erosion 1   (fine SD %.4f)' % fine_sd(on_m[key]),
          'vanilla             (fine SD %.4f)' % fine_sd(vm),
          'what the pass added (on minus off)'],
         note='top: the whole 512-texel sheet.  bottom: the same 170-texel square '
              '(5,440 u) in all four, magnified x3.  the vanilla channels run; ours are '
              'the right amount of relief pointing less well (anisotropy 1.18 against 1.48)')

    def rgb8(path):
        return np.clip(S.Dds(path).level(0)[:, :, :3], 0, 255).astype(np.uint8)

    co, cn, cv = rgb8(off_c[key]), rgb8(on_c[key]), rgb8(vc)
    d = np.clip(128 + (cn.astype(np.int32) - co.astype(np.int32)) * COLDIFF,
                0, 255).astype(np.uint8)
    print('colour: mean |on - off| = %.2f of 255, max %d'
          % (np.abs(cn.astype(np.int32) - co.astype(np.int32)).mean(),
             np.abs(cn.astype(np.int32) - co.astype(np.int32)).max()))

    col = [co, cn, d, cv]
    grid('b_cmp_erosion_colour.png',
         'the colour sheet -- a crevice shading and nothing else, no palette '
         '(same chunk %d,%d, --road-detail 1)' % key,
         [col, [crop4(c, CX, CY) for c in col]],
         ['ours, --erosion 0   (fine band %.5f)' % fine_lum(off_c[key]),
          'ours, --erosion 1   (fine band %.5f)' % fine_lum(on_c[key]),
          'the difference, x%d about mid grey' % COLDIFF,
          'vanilla             (fine band %.5f)' % fine_lum(vc)],
         note='mean |on - off| %.2f of 255, max %d.  The claim is only that the colour '
              'stops being flatter than the vanilla one; TILING3 put an R-squared ceiling of '
              '0.018-0.023 on any per-texel law from the fine normal to the vanilla fine colour'
              % (np.abs(cn.astype(np.int32) - co.astype(np.int32)).mean(),
                 np.abs(cn.astype(np.int32) - co.astype(np.int32)).max()))

    AMBIENT = 0.30

    def lit(cpath, mpath):
        gx, gy, _ = B.gradient_field(mpath)
        az, el = np.radians(AZ_DEG), np.radians(EL_DEG)
        lx, ly, lz = np.cos(el) * np.sin(az), np.cos(el) * np.cos(az), np.sin(el)
        nx, ny, nz = -gx, -gy, np.ones_like(gx)
        n = np.sqrt(nx * nx + ny * ny + nz * nz)
        lam = np.clip((nx * lx + ny * ly + nz * lz) / n, 0.0, 1.0)
        f = AMBIENT + (1.0 - AMBIENT) * lam
        c = np.clip(S.Dds(cpath).level(0)[:, :, :3], 0, 255).astype(np.float64)
        return np.clip(c * f[:, :, None] * 1.8, 0, 255).astype(np.uint8)

    lo = lit(off_c[key], off_m[key])
    ln = lit(on_c[key], on_m[key])
    lv = lit(vc, vm)
    grid('b_cmp_erosion_lit.png',
         'the colour sheet LIT BY ITS OWN NORMAL SHEET -- sun az %d el %d, '
         'ambient %.2f, gain 1.8, identical in all three -- '
         'Commonwealth dim 4 chunk %d,%d, --road-detail 1'
         % (AZ_DEG, EL_DEG, AMBIENT, key[0], key[1]),
         [[lo, ln, lv], [crop4(lo, CX, CY), crop4(ln, CX, CY), crop4(lv, CX, CY)]],
         ['ours, --erosion 0', 'ours, --erosion 1', 'vanilla'],
         note='This is arithmetic on the two sheets, not a render: colour x '
              '(ambient + (1-ambient) x Lambert of the sheet normal).  The same '
              'sun, ambient and gain in every panel.  The NifSkope viewport was '
              'tried too and is reported in B5.')

    return 0


if __name__ == '__main__':
    sys.exit(main(*sys.argv[1:]))
